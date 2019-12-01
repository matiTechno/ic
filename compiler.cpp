#include "ic.h"

struct ic_vm_scope
{
    int prev_var_count;
};

enum ic_lvalue_type
{
    IC_LVALUE_NON,
    IC_LVALUE_ADDRESS,
    IC_LVALUE_LOCAL,
    IC_LVALUE_GLOBAL,
};

struct ic_vm_expr
{
    ic_value_type type;
    ic_lvalue_type lvalue_type;
    int var_idx;
};

struct ic_compiler // this name is wrong, maybe compile_state is better or something
{
    std::vector<ic_inst> bytecode;
    std::vector<ic_vm_scope> scopes;
    std::vector<ic_vm_var> vars;
    ic_runtime* runtime;
    ic_function* function;
    int current_stack_size;
    int stack_size; // e.g. max of if and else branches
    int loop_level;
    bool disable_bytecode_emition;

    void push_scope();
    void pop_scope();
    // todo: pass token and print error message on error
    void add_inst(ic_inst inst);
    int declare_var(ic_value_type type, ic_string name);
    // true if local scope, false if global scope
    bool get_var(ic_string name, ic_vm_var& var);
    ic_function* get_fun(ic_string name);
    ic_struct* get_struct(ic_string name);
};

void ic_compiler::push_scope()
{
    ic_vm_scope scope;
    scope.prev_var_count = vars.size();
    scopes.push_back(scope);
}

void ic_compiler::pop_scope()
{
    vars.resize(scopes.back().prev_var_count);
    scopes.pop_back();
}

void ic_compiler::add_inst(ic_inst inst)
{
    assert(inst.opcode <= IC_OPC_JUMP_END && inst.opcode >= 0);
    if(!disable_bytecode_emition)
        bytecode.push_back(inst);
}

int ic_compiler::declare_var(ic_value_type type, ic_string name)
{
    assert(scopes.size());
    bool present = false;

    for (int i = vars.size() - 1; i >= scopes.back().prev_var_count; --i)
    {
        ic_vm_var var = vars[i];

        if (ic_string_compare(var.name, name))
        {
            present = true;
            break;
        }
    }

    if (present)
        assert(false);

    int idx = current_stack_size;

    if (is_non_pointer_struct(type))
    {
        ic_struct* s = get_struct(type.struct_name);
        current_stack_size += s->num_data;
    }
    else
        current_stack_size += 1;

    stack_size = current_stack_size > stack_size ? current_stack_size : stack_size;
    ic_vm_var v;
    v.name = name;
    v.type = type;
    v.idx = idx;
    vars.push_back(v);
    return idx;
}

bool ic_compiler::get_var(ic_string name, ic_vm_var& output)
{
    assert(scopes.size());
    int idx_var = vars.size() - 1;

    for (int idx_scope = scopes.size() - 1; idx_scope >= 0; --idx_scope)
    {
        ic_vm_scope& scope = scopes[idx_scope];

        for (; idx_var >= scope.prev_var_count; --idx_var)
        {
            ic_vm_var var = vars[idx_var];

            if (ic_string_compare(var.name, name))
            {
                output = var;
                return true;
            }
        }
    }

    for (ic_vm_var& global : runtime->_global_vars)
    {
        if (ic_string_compare(name, global.name))
        {
            output = global;
            return false;
        }
    }

    assert(false);
    return {};
}

ic_function* ic_compiler::get_fun(ic_string name)
{
    ic_function* f = runtime->get_function(name);
    assert(f);
    return f;
}

ic_struct* ic_compiler::get_struct(ic_string name)
{
    ic_struct* s = runtime->get_struct(name);
    assert(s);
    return s;
}

bool compile_stmt(ic_stmt* stmt, ic_compiler& compiler);
ic_vm_expr compile_expr(ic_expr* expr, ic_compiler& compiler, bool substitute_lvalue = true);
void compile_implicit_conversion(ic_value_type to, ic_value_type from, ic_compiler& compiler);

void compile(ic_function& function, ic_runtime& runtime)
{
    ic_compiler compiler;
    compiler.runtime = &runtime;
    compiler.function = &function;
    compiler.stack_size = 0;
    compiler.current_stack_size = 0;
    compiler.loop_level = 0;
    compiler.disable_bytecode_emition = false;

    compiler.push_scope();

    for (int i = 0; i < function.param_count; ++i)
        compiler.declare_var(function.params[i].type, function.params[i].name);

    bool returned = compile_stmt(function.body, compiler);

    if (function.return_type.indirection_level || function.return_type.basic_type != IC_TYPE_VOID)
        assert(returned);
    else if(compiler.bytecode.back().opcode != IC_OPC_RETURN)
        compiler.add_inst({ .opcode = IC_OPC_RETURN });

    function.bytecode = (ic_inst*)malloc(compiler.bytecode.size() * sizeof(ic_inst));
    memcpy(function.bytecode, compiler.bytecode.data(), compiler.bytecode.size() * sizeof(ic_inst));
    function.by_size = compiler.bytecode.size();
}

// returns true if return statement occured
bool compile_stmt(ic_stmt* stmt, ic_compiler& compiler)
{
    assert(stmt);
    bool returned = false;

    switch (stmt->type)
    {
    case IC_STMT_COMPOUND:
    {
        if (stmt->_compound.push_scope)
            compiler.push_scope();

        ic_stmt* body_stmt = stmt->_compound.body;

        while (body_stmt)
        {
            assert(!returned); // don't allow code that is after a return statement (dead code)
            returned = compile_stmt(body_stmt, compiler);
            body_stmt = body_stmt->next;
        }

        if (stmt->_compound.push_scope)
            compiler.pop_scope();

        break;
    }
    case IC_STMT_FOR:
    {
        // don't set returned flag here, loop may never be executed
        compiler.loop_level += 1;
        compiler.push_scope();

        if (stmt->_for.header1)
            compile_stmt(stmt->_for.header1, compiler);

        int loop_start_idx = compiler.bytecode.size();

        if (stmt->_for.header2)
        {
            ic_vm_expr expr = compile_expr(stmt->_for.header2, compiler);
            compile_implicit_conversion(non_pointer_type(IC_TYPE_BOOL), expr.type, compiler);
            compiler.add_inst({ .opcode = IC_OPC_JUMP_CONDITION_FAIL });
        }

        compile_stmt(stmt->_for.body, compiler);

        if (stmt->_for.header3)
            compile_expr(stmt->_for.header3, compiler);

        // go back to the beginning
        {
            ic_inst inst;
            inst.opcode = IC_OPC_JUMP;
            inst.operand.idx = loop_start_idx;
            compiler.add_inst(inst);
        }

        // resolve jumps
        for (int i = loop_start_idx + 1; i < compiler.bytecode.size(); ++i)
        {
            ic_inst& inst = compiler.bytecode[i];

            switch (inst.opcode)
            {
            case IC_OPC_JUMP_CONDITION_FAIL:
                inst.opcode = IC_OPC_JUMP_FALSE;
                inst.operand.idx = compiler.bytecode.size();
                break;
            case IC_OPC_JUMP_START:
                inst.opcode = IC_OPC_JUMP;
                inst.operand.idx = loop_start_idx;
                break;
            case IC_OPC_JUMP_END:
                inst.opcode = IC_OPC_JUMP;
                inst.operand.idx = compiler.bytecode.size();
                break;
            }
        }

        compiler.pop_scope();
        compiler.loop_level -= 1;
        break;
    }
    case IC_STMT_IF:
    {
        int returned_if = false;
        int returned_else = false;
        ic_vm_expr expr = compile_expr(stmt->_if.header, compiler);
        compile_implicit_conversion(non_pointer_type(IC_TYPE_BOOL), expr.type, compiler);
        int idx_start = compiler.bytecode.size();
        compiler.add_inst({ .opcode = IC_OPC_JUMP_CONDITION_FAIL });
        compiler.push_scope();
        returned_if = compile_stmt(stmt->_if.body_if, compiler);
        compiler.pop_scope();
        int idx_else;

        if (stmt->_if.body_else)
        {
            compiler.add_inst({ .opcode = IC_OPC_JUMP_END }); // don't enter else when if block was entered
            idx_else = compiler.bytecode.size();
            compiler.push_scope();
            returned_else = compile_stmt(stmt->_if.body_else, compiler);
            compiler.pop_scope();
        }
        else
            idx_else = compiler.bytecode.size();

        // resolve jumps
        for (int i = idx_start; i < compiler.bytecode.size(); ++i)
        {
            ic_inst& inst = compiler.bytecode[i];

            switch (inst.opcode)
            {
            case IC_OPC_JUMP_CONDITION_FAIL:
                inst.opcode = IC_OPC_JUMP_FALSE;
                inst.operand.idx = idx_else;
                break;
            case IC_OPC_JUMP_END:
                inst.opcode = IC_OPC_JUMP;
                inst.operand.idx = compiler.bytecode.size();
                break;
            }
        }

        returned = returned_if && returned_else; // both branches must return, if there is no else branch return false
        break;
    }
    case IC_STMT_VAR_DECLARATION:
    {
        int idx = compiler.declare_var(stmt->_var_declaration.type, stmt->_var_declaration.token.string);

        if (stmt->_var_declaration.expr)
        {
            ic_vm_expr expr = compile_expr(stmt->_var_declaration.expr, compiler);
            compile_implicit_conversion(stmt->_var_declaration.type, expr.type, compiler);
            
            ic_inst inst;
            inst.operand.store.idx = idx;

            if (is_non_pointer_struct(expr.type))
            {
                inst.opcode = IC_OPC_STORE_STRUCT;
                ic_struct* s = compiler.get_struct(expr.type.struct_name);
                inst.operand.store.size = s->num_data;
            }
            else
                inst.opcode = IC_OPC_STORE;

            compiler.add_inst(inst);
        }

        break;
    }
    case IC_STMT_RETURN:
    {
        returned = true;
        ic_value_type return_type = compiler.function->return_type;

        if (stmt->_return.expr)
        {
            ic_vm_expr expr = compile_expr(stmt->_return.expr, compiler);
            compile_implicit_conversion(return_type, expr.type, compiler);
        }
        else
            assert(!return_type.indirection_level && return_type.basic_type == IC_TYPE_VOID);

        compiler.add_inst({ .opcode = IC_OPC_RETURN });
        break;
    }
    case IC_STMT_BREAK:
    {
        assert(compiler.loop_level);
        compiler.add_inst({ .opcode = IC_OPC_JUMP_END });
        break;
    }
    case IC_STMT_CONTINUE:
    {
        assert(compiler.loop_level);
        compiler.add_inst({ .opcode = IC_OPC_JUMP_START });
        break;
    }
    case IC_STMT_EXPR:
    {
        if (stmt->_expr)
            compile_expr(stmt->_expr, compiler);

        break;
    }
    default:
        assert(false);
    }

    return returned;
}

// these expressions must respet substitute_lvalue parameter
// following expressions are lvalue: variable / parameter identifiers, string literals, parenthesized expression if
// inner expression is lvalue, . operator if lhs operand is lvalue, -> operator, dereference operator *, subscription operator []

// lvalue can be used in: address_of operator, increment, decrement, lhs operand of assignment
// cppreference mentioned also lhs operand of member access . operator but I don't see why - this expression may return lvalue
// but is not really using it.

void compile_dereference(ic_value_type type, ic_compiler& compiler)
{
    assert(type.indirection_level);
    ic_inst inst;

    if (type.indirection_level > 1 || type.basic_type == IC_TYPE_F64)
        inst.opcode = IC_OPC_DEREFERENCE_8;
    else
    {
        switch (type.basic_type)
        {
        case IC_TYPE_BOOL:
        case IC_TYPE_S8:
            inst.opcode = IC_OPC_DEREFERENCE_1;
            break;
        case IC_TYPE_S32:
        case IC_TYPE_F32:
            inst.opcode = IC_OPC_DEREFERENCE_4;
            break;
        case IC_TYPE_STRUCT:
        {
            ic_struct* s = compiler.get_struct(type.struct_name);
            inst.opcode = IC_OPC_DEREFERENCE_STRUCT;
            inst.operand.size = s->num_data;
            break;
        }
        default:
            assert(false);
        }
    }

    compiler.add_inst(inst);
}

void assert_numeric_type(ic_value_type type)
{
    assert(type.indirection_level == 0);
    switch (type.basic_type)
    {
    case IC_TYPE_BOOL:
    case IC_TYPE_S8:
    case IC_TYPE_U8:
    case IC_TYPE_S32:
    case IC_TYPE_F32:
    case IC_TYPE_F64:
        return;
    }
    assert(false);
}

void compile_store_at(ic_value_type type, ic_compiler& compiler)
{
    ic_inst inst;
    if (type.indirection_level)
    {
        inst.opcode = IC_OPC_STORE_8_AT;
    }
    else
    {
        switch (type.basic_type)
        {
        case IC_TYPE_BOOL:
        case IC_TYPE_S8:
        case IC_TYPE_U8:
            inst.opcode = IC_OPC_STORE_1_AT;
            break;
        case IC_TYPE_S32:
        case IC_TYPE_F32:
            inst.opcode = IC_OPC_STORE_4_AT;
            break;
        case IC_TYPE_F64:
            inst.opcode = IC_OPC_STORE_8_AT;
            break;
        case IC_TYPE_STRUCT:
        {
            ic_struct* _struct = compiler.get_struct(type.struct_name);
            inst.opcode = IC_OPC_STORE_STRUCT_AT;
            inst.operand.size = _struct->num_data;
            break;
        }
        default:
            assert(false);
        }
    }

    compiler.add_inst(inst);
}

ic_vm_expr compile_lhs_assignment(ic_expr* lhs_expr, ic_value_type rhs_type, ic_compiler& compiler);
ic_value_type get_numeric_expression_type(ic_value_type lhs, ic_value_type rhs);
ic_value_type get_comparison_expression_type(ic_value_type lhs, ic_value_type rhs);

ic_value_type get_expr_type(ic_expr* expr, ic_compiler& compiler)
{
    compiler.disable_bytecode_emition = true;
    ic_value_type t = compile_expr(expr, compiler).type;
    compiler.disable_bytecode_emition = false;
    return t;
}

ic_vm_expr compile_expr(ic_expr* expr, ic_compiler& compiler, bool substitute_lvalue)
{
    assert(expr);

    switch (expr->type)
    {
    case IC_EXPR_BINARY:
    {
        switch (expr->token.type)
        {
        case IC_TOK_EQUAL: // order of lhs rhs is reversed on the operand stack for assignment
        {
            ic_vm_expr rhs = compile_expr(expr->_binary.rhs, compiler);
            return compile_lhs_assignment(expr->_binary.lhs, rhs.type, compiler);
        }
        case IC_TOK_PLUS_EQUAL:
        {
            // todo redundancy in compile_lhs_assignment - we don't
            // need to perform non emitive compile to discover lhs type - we have it here
            ic_value_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
            ic_value_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
            ic_value_type expr_type = get_numeric_expression_type(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_implicit_conversion(expr_type, lhs_type, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(expr_type, rhs_type, compiler);

            if (expr_type.basic_type == IC_TYPE_S32)
                compiler.add_inst({ IC_OPC_ADD_S32 });
            else if (expr_type.basic_type == IC_TYPE_F32)
                compiler.add_inst({ IC_OPC_ADD_F32 });
            else if (expr_type.basic_type == IC_TYPE_F64)
                compiler.add_inst({ IC_OPC_ADD_F64 });
            else
                assert(false);

            return compile_lhs_assignment(expr->_binary.lhs, expr_type, compiler); // there is some big redundancy here,
            // after getting function it needs to be fixed, todo
        }
        case IC_TOK_MINUS_EQUAL:
        {
            ic_value_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
            ic_value_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
            ic_value_type expr_type = get_numeric_expression_type(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_implicit_conversion(expr_type, lhs_type, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(expr_type, rhs_type, compiler);

            if (expr_type.basic_type == IC_TYPE_S32)
                compiler.add_inst({ IC_OPC_SUB_S32 });
            else if (expr_type.basic_type == IC_TYPE_F32)
                compiler.add_inst({ IC_OPC_SUB_F32 });
            else if (expr_type.basic_type == IC_TYPE_F64)
                compiler.add_inst({ IC_OPC_SUB_F64 });
            else
                assert(false);

            return compile_lhs_assignment(expr->_binary.lhs, expr_type, compiler);
        }
        case IC_TOK_STAR_EQUAL:
        {
            ic_value_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
            ic_value_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
            ic_value_type expr_type = get_numeric_expression_type(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_implicit_conversion(expr_type, lhs_type, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(expr_type, rhs_type, compiler);

            if (expr_type.basic_type == IC_TYPE_S32)
                compiler.add_inst({ IC_OPC_MUL_S32 });
            else if (expr_type.basic_type == IC_TYPE_F32)
                compiler.add_inst({ IC_OPC_MUL_F32 });
            else if (expr_type.basic_type == IC_TYPE_F64)
                compiler.add_inst({ IC_OPC_MUL_F64 });
            else
                assert(false);

            return compile_lhs_assignment(expr->_binary.lhs, expr_type, compiler);
        }
        case IC_TOK_SLASH_EQUAL:
        {
            ic_value_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
            ic_value_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
            ic_value_type expr_type = get_numeric_expression_type(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_implicit_conversion(expr_type, lhs_type, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(expr_type, rhs_type, compiler);

            if (expr_type.basic_type == IC_TYPE_S32)
                compiler.add_inst({ IC_OPC_DIV_S32 });
            else if (expr_type.basic_type == IC_TYPE_F32)
                compiler.add_inst({ IC_OPC_DIV_F32 });
            else if (expr_type.basic_type == IC_TYPE_F64)
                compiler.add_inst({ IC_OPC_DIV_F64 });
            else
                assert(false);

            return compile_lhs_assignment(expr->_binary.lhs, expr_type, compiler);
        }
        case IC_TOK_VBAR_VBAR:
        {
            ic_value_type lhs_type = compile_expr(expr->_binary.lhs, compiler).type;
            compile_implicit_conversion(non_pointer_type(IC_TYPE_BOOL), lhs_type, compiler);
            int jump_idx = compiler.bytecode.size();
            compiler.add_inst({ IC_OPC_JUMP_TRUE });
            ic_value_type rhs_type = compile_expr(expr->_binary.rhs, compiler).type;
            compile_implicit_conversion(non_pointer_type(IC_TYPE_BOOL), rhs_type, compiler);
            compiler.bytecode[jump_idx].operand.idx = compiler.bytecode.size();
            return { non_pointer_type(IC_TYPE_BOOL), IC_LVALUE_NON };
        }
        case IC_TOK_AMPERSAND_AMPERSAND:
        {
            ic_value_type lhs_type = compile_expr(expr->_binary.lhs, compiler).type;
            compile_implicit_conversion(non_pointer_type(IC_TYPE_BOOL), lhs_type, compiler);
            int jump_idx = compiler.bytecode.size();
            compiler.add_inst({ IC_OPC_JUMP_FALSE });
            ic_value_type rhs_type = compile_expr(expr->_binary.rhs, compiler).type;
            compile_implicit_conversion(non_pointer_type(IC_TYPE_BOOL), rhs_type, compiler);
            compiler.bytecode[jump_idx].operand.idx = compiler.bytecode.size();
            return { non_pointer_type(IC_TYPE_BOOL), IC_LVALUE_NON };
        }
        case IC_TOK_EQUAL_EQUAL:
        {
            ic_value_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
            ic_value_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
            ic_value_type expr_type = get_comparison_expression_type(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_implicit_conversion(expr_type, lhs_type, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(expr_type, rhs_type, compiler);
            int opcode;

            if (expr_type.indirection_level)
                opcode = IC_OPC_COMPARE_E_PTR;
            else
            {
                switch (expr_type.basic_type)
                {
                case IC_TYPE_S32:
                    opcode = IC_OPC_COMPARE_E_S32;
                    break;
                case IC_TYPE_F32:
                    opcode = IC_OPC_COMPARE_E_F32;
                    break;
                case IC_TYPE_F64:
                    opcode = IC_OPC_COMPARE_E_F64;
                    break;
                default:
                    assert(false);
                }
            }

            compiler.add_inst({ opcode });
            return { non_pointer_type(IC_TYPE_BOOL), IC_LVALUE_NON };
        }
        case IC_TOK_BANG_EQUAL:
        {
            ic_value_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
            ic_value_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
            ic_value_type expr_type = get_comparison_expression_type(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_implicit_conversion(expr_type, lhs_type, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(expr_type, rhs_type, compiler);
            int opcode;

            if (expr_type.indirection_level)
                opcode = IC_OPC_COMPARE_NE_PTR;
            else
            {
                switch (expr_type.basic_type)
                {
                case IC_TYPE_S32:
                    opcode = IC_OPC_COMPARE_NE_S32;
                    break;
                case IC_TYPE_F32:
                    opcode = IC_OPC_COMPARE_NE_F32;
                    break;
                case IC_TYPE_F64:
                    opcode = IC_OPC_COMPARE_NE_F64;
                    break;
                default:
                    assert(false);
                }
            }

            compiler.add_inst({ opcode });
            return { non_pointer_type(IC_TYPE_BOOL), IC_LVALUE_NON };
        }
        case IC_TOK_GREATER:
        {
            ic_value_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
            ic_value_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
            ic_value_type expr_type = get_comparison_expression_type(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_implicit_conversion(expr_type, lhs_type, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(expr_type, rhs_type, compiler);
            int opcode;

            if (expr_type.indirection_level)
                opcode = IC_OPC_COMPARE_G_PTR;
            else
            {
                switch (expr_type.basic_type)
                {
                case IC_TYPE_S32:
                    opcode = IC_OPC_COMPARE_G_S32;
                    break;
                case IC_TYPE_F32:
                    opcode = IC_OPC_COMPARE_G_F32;
                    break;
                case IC_TYPE_F64:
                    opcode = IC_OPC_COMPARE_G_F64;
                    break;
                default:
                    assert(false);
                }
            }

            compiler.add_inst({ opcode });
            return { non_pointer_type(IC_TYPE_BOOL), IC_LVALUE_NON };
        }
        case IC_TOK_GREATER_EQUAL:
        {
            ic_value_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
            ic_value_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
            ic_value_type expr_type = get_comparison_expression_type(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_implicit_conversion(expr_type, lhs_type, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(expr_type, rhs_type, compiler);
            int opcode;

            if (expr_type.indirection_level)
                opcode = IC_OPC_COMPARE_GE_PTR;
            else
            {
                switch (expr_type.basic_type)
                {
                case IC_TYPE_S32:
                    opcode = IC_OPC_COMPARE_GE_S32;
                    break;
                case IC_TYPE_F32:
                    opcode = IC_OPC_COMPARE_GE_F32;
                    break;
                case IC_TYPE_F64:
                    opcode = IC_OPC_COMPARE_GE_F64;
                    break;
                default:
                    assert(false);
                }
            }

            compiler.add_inst({ opcode });
            return { non_pointer_type(IC_TYPE_BOOL), IC_LVALUE_NON };
        }
        case IC_TOK_LESS:
        {
            ic_value_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
            ic_value_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
            ic_value_type expr_type = get_comparison_expression_type(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_implicit_conversion(expr_type, lhs_type, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(expr_type, rhs_type, compiler);
            int opcode;

            if (expr_type.indirection_level)
                opcode = IC_OPC_COMPARE_L_PTR;
            else
            {
                switch (expr_type.basic_type)
                {
                case IC_TYPE_S32:
                    opcode = IC_OPC_COMPARE_L_S32;
                    break;
                case IC_TYPE_F32:
                    opcode = IC_OPC_COMPARE_L_F32;
                    break;
                case IC_TYPE_F64:
                    opcode = IC_OPC_COMPARE_L_F64;
                    break;
                default:
                    assert(false);
                }
            }

            compiler.add_inst({ opcode });
            return { non_pointer_type(IC_TYPE_BOOL), IC_LVALUE_NON };
        }
        case IC_TOK_LESS_EQUAL:
        {
            ic_value_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
            ic_value_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
            ic_value_type expr_type = get_comparison_expression_type(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_implicit_conversion(expr_type, lhs_type, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(expr_type, rhs_type, compiler);
            int opcode;

            if (expr_type.indirection_level)
                opcode = IC_OPC_COMPARE_LE_PTR;
            else
            {
                switch (expr_type.basic_type)
                {
                case IC_TYPE_S32:
                    opcode = IC_OPC_COMPARE_LE_S32;
                    break;
                case IC_TYPE_F32:
                    opcode = IC_OPC_COMPARE_LE_F32;
                    break;
                case IC_TYPE_F64:
                    opcode = IC_OPC_COMPARE_LE_F64;
                    break;
                default:
                    assert(false);
                }
            }

            compiler.add_inst({ opcode });
            return { non_pointer_type(IC_TYPE_BOOL), IC_LVALUE_NON };
        }
        // todo pointer arithmetic
        case IC_TOK_PLUS:
        {
            ic_value_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
            ic_value_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
            ic_value_type expr_type = get_numeric_expression_type(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_implicit_conversion(expr_type, lhs_type, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(expr_type, rhs_type, compiler);

            int opcode;

            switch (expr_type.basic_type)
            {
            case IC_TYPE_S32:
                opcode = IC_OPC_ADD_S32;
                break;
            case IC_TYPE_F32:
                opcode = IC_OPC_ADD_F32;
                break;
            case IC_TYPE_F64:
                opcode = IC_OPC_ADD_F64;
                break;
            default:
                assert(false);
            }

            compiler.add_inst({ opcode });
            return { expr_type, IC_LVALUE_NON };

        }
        // todo pointer arithmetic
        case IC_TOK_MINUS:
        {
            ic_value_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
            ic_value_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
            ic_value_type expr_type = get_numeric_expression_type(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_implicit_conversion(expr_type, lhs_type, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(expr_type, rhs_type, compiler);

            int opcode;

            switch (expr_type.basic_type)
            {
            case IC_TYPE_S32:
                opcode = IC_OPC_SUB_S32;
                break;
            case IC_TYPE_F32:
                opcode = IC_OPC_SUB_F32;
                break;
            case IC_TYPE_F64:
                opcode = IC_OPC_SUB_F64;
                break;
            default:
                assert(false);
            }

            compiler.add_inst({ opcode });
            return { expr_type, IC_LVALUE_NON };

        }
        case IC_TOK_STAR:
        {
            ic_value_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
            ic_value_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
            ic_value_type expr_type = get_numeric_expression_type(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_implicit_conversion(expr_type, lhs_type, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(expr_type, rhs_type, compiler);

            int opcode;

            switch (expr_type.basic_type)
            {
            case IC_TYPE_S32:
                opcode = IC_OPC_MUL_S32;
                break;
            case IC_TYPE_F32:
                opcode = IC_OPC_MUL_F32;
                break;
            case IC_TYPE_F64:
                opcode = IC_OPC_MUL_F64;
                break;
            default:
                assert(false);
            }

            compiler.add_inst({ opcode });
            return { expr_type, IC_LVALUE_NON };

        }
        case IC_TOK_SLASH:
        {
            ic_value_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
            ic_value_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
            ic_value_type expr_type = get_numeric_expression_type(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_implicit_conversion(expr_type, lhs_type, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(expr_type, rhs_type, compiler);

            int opcode;

            switch (expr_type.basic_type)
            {
            case IC_TYPE_S32:
                opcode = IC_OPC_DIV_S32;
                break;
            case IC_TYPE_F32:
                opcode = IC_OPC_DIV_F32;
                break;
            case IC_TYPE_F64:
                opcode = IC_OPC_DIV_F64;
                break;
            default:
                assert(false);
            }

            compiler.add_inst({ opcode });
            return { expr_type, IC_LVALUE_NON };

        }
        case IC_TOK_PERCENT:
        {
            ic_value_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
            ic_value_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
            ic_value_type expr_type = get_numeric_expression_type(lhs_type, rhs_type);
            assert(expr_type.basic_type = IC_TYPE_S32); // only integer types + the only integer type returned by get_numeric_expr is s32
            compile_expr(expr->_binary.lhs, compiler);
            compile_implicit_conversion(expr_type, lhs_type, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(expr_type, rhs_type, compiler);
            compiler.add_inst({ IC_OPC_MODULO_S32 });
            return { expr_type, IC_LVALUE_NON };
        }
        default:
            assert(false);
        }
    }
    case IC_EXPR_UNARY:
    {
        switch (expr->token.type)
        {
        case IC_TOK_MINUS:
        {
            ic_value_type operand_type = get_expr_type(expr->_unary.expr, compiler);
            ic_value_type expr_type = get_numeric_expression_type(operand_type, non_pointer_type(IC_TYPE_BOOL));
            compile_expr(expr->_unary.expr, compiler);
            compile_implicit_conversion(expr_type, operand_type, compiler);

            switch (expr_type.basic_type)
            {
            case IC_TYPE_S32:
                compiler.add_inst({ IC_OPC_NEGATE_S32 });
                break;
            case IC_TYPE_F32:
                compiler.add_inst({ IC_OPC_NEGATE_F32 });
                break;
            case IC_TYPE_F64:
                compiler.add_inst({ IC_OPC_NEGATE_F64 });
                break;
            default:
                assert(false);
            }
            return { expr_type, IC_LVALUE_NON };
        }
        case IC_TOK_BANG:
        {
            ic_value_type operand_type = get_expr_type(expr->_unary.expr, compiler);
            compile_expr(expr->_unary.expr, compiler);
            compile_implicit_conversion(non_pointer_type(IC_TYPE_BOOL), operand_type, compiler);
            compiler.add_inst({ IC_OPC_LOGICAL_NOT });
            return { non_pointer_type(IC_TYPE_BOOL), IC_LVALUE_NON };
        }
        // for operand substitute_lvalue = false;
        // operand does not need to be lvalue
        case IC_TOK_MINUS_MINUS:
        case IC_TOK_PLUS_PLUS:
        {
            ic_vm_expr vm_expr = compile_expr(expr->_unary.expr, compiler, false);
            ic_value_type num_expr_type = get_numeric_expression_type(vm_expr.type, non_pointer_type(IC_TYPE_BOOL));

            switch (vm_expr.lvalue_type)
            {
            case IC_LVALUE_ADDRESS:
            {
                compiler.add_inst({ IC_OPC_CLONE });
                compile_dereference(vm_expr.type, compiler);
                compile_implicit_conversion(num_expr_type, vm_expr.type, compiler);
                ic_inst inst;
                ic_inst inst2;
                inst.opcode = IC_OPC_PUSH;

                switch (num_expr_type.basic_type)
                {
                case IC_TYPE_S32:
                    inst.operand.push_data.s32 = expr->token.type == IC_TOK_MINUS_MINUS ? -1 : 1;
                    inst2.opcode = IC_OPC_ADD_S32;
                    break;
                case IC_TYPE_F32:
                    inst.operand.push_data.f32 = expr->token.type == IC_TOK_MINUS_MINUS ? -1 : 1;
                    inst2.opcode = IC_OPC_ADD_F32;
                    break;
                case IC_TYPE_F64:
                    inst.operand.push_data.f64 = expr->token.type == IC_TOK_MINUS_MINUS ? -1 : 1;
                    inst2.opcode = IC_OPC_ADD_F64;
                    break;
                default:
                    assert(false);
                }
                compiler.add_inst(inst);
                compiler.add_inst(inst2);
                compile_implicit_conversion(vm_expr.type, num_expr_type, compiler);
                compiler.add_inst({ IC_OPC_SWAP });
                compile_store_at(vm_expr.type, compiler);

                if (substitute_lvalue)
                    compiler.add_inst({ IC_OPC_POP }); // pop address
                else
                {
                    compiler.add_inst({ IC_OPC_SWAP });
                    compiler.add_inst({ IC_OPC_POP }); // pop data, leave address
                }

                break;
            }
            case IC_LVALUE_LOCAL:
            case IC_LVALUE_GLOBAL:
            {
                {
                    ic_inst inst;
                    inst.opcode = vm_expr.lvalue_type == IC_LVALUE_LOCAL ? IC_OPC_LOAD : IC_OPC_LOAD_GLOBAL;
                    inst.operand.load.idx = vm_expr.var_idx;
                    compiler.add_inst(inst);
                }
                ic_value_type num_expr_type = get_numeric_expression_type(vm_expr.type, non_pointer_type(IC_TYPE_BOOL));
                compile_implicit_conversion(num_expr_type, vm_expr.type, compiler);
                ic_inst inst;
                ic_inst inst2;
                inst.opcode = IC_OPC_PUSH;

                switch (num_expr_type.basic_type)
                {
                case IC_TYPE_S32:
                    inst.operand.push_data.s32 = expr->token.type == IC_TOK_MINUS_MINUS ? -1 : 1;
                    inst2.opcode = IC_OPC_ADD_S32;
                    break;
                case IC_TYPE_F32:
                    inst.operand.push_data.f32 = expr->token.type == IC_TOK_MINUS_MINUS ? -1 : 1;
                    inst2.opcode = IC_OPC_ADD_F32;
                    break;
                case IC_TYPE_F64:
                    inst.operand.push_data.f64 = expr->token.type == IC_TOK_MINUS_MINUS ? -1 : 1;
                    inst2.opcode = IC_OPC_ADD_F64;
                    break;
                default:
                    assert(false);
                }
                compiler.add_inst(inst);
                compiler.add_inst(inst2);
                compile_implicit_conversion(vm_expr.type, num_expr_type, compiler);
                {
                    ic_inst inst;
                    inst.opcode = vm_expr.lvalue_type == IC_LVALUE_LOCAL ? IC_OPC_STORE : IC_OPC_STORE_GLOBAL;
                    inst.operand.store.idx = vm_expr.var_idx;
                    compiler.add_inst(inst);
                }
                if (!substitute_lvalue)
                    compiler.add_inst({ IC_OPC_POP });
                break;
            }
            case IC_LVALUE_NON:
            {
                ic_value_type num_expr_type = get_numeric_expression_type(vm_expr.type, non_pointer_type(IC_TYPE_BOOL));
                compile_implicit_conversion(num_expr_type, vm_expr.type, compiler);
                ic_inst inst;
                ic_inst inst2;
                inst.opcode = IC_OPC_PUSH;

                switch (num_expr_type.basic_type)
                {
                case IC_TYPE_S32:
                    inst.operand.push_data.s32 = expr->token.type == IC_TOK_MINUS_MINUS ? -1 : 1;
                    inst2.opcode = IC_OPC_ADD_S32;
                    break;
                case IC_TYPE_F32:
                    inst.operand.push_data.f32 = expr->token.type == IC_TOK_MINUS_MINUS ? -1 : 1;
                    inst2.opcode = IC_OPC_ADD_F32;
                    break;
                case IC_TYPE_F64:
                    inst.operand.push_data.f64 = expr->token.type == IC_TOK_MINUS_MINUS ? -1 : 1;
                    inst2.opcode = IC_OPC_ADD_F64;
                    break;
                default:
                    assert(false);
                }
                compiler.add_inst(inst);
                compiler.add_inst(inst2);
                break;
            }
            default:
                assert(false);
            }
            return vm_expr;
        }
        case IC_TOK_AMPERSAND:
        {
            ic_vm_expr vm_expr = compile_expr(expr->_unary.expr, compiler, false);

            switch (vm_expr.lvalue_type)
            {
            case IC_LVALUE_ADDRESS:
                break;
            case IC_LVALUE_LOCAL:
            {
                ic_inst inst;
                inst.opcode = IC_OPC_ADDRESS_OF;
                inst.operand.idx = vm_expr.var_idx;
                compiler.add_inst(inst);
                break;
            }
            case IC_LVALUE_GLOBAL:
            {
                ic_inst inst;
                inst.opcode = IC_OPC_ADDRESS_OF_GLOBAL;
                inst.operand.idx = vm_expr.var_idx;
                compiler.add_inst(inst);
                break;
            }
            default:
                assert(false);
            }

            ic_value_type new_type = vm_expr.type;
            new_type.indirection_level += 1;
            new_type.const_mask = new_type.const_mask << 1;
            return { new_type, IC_LVALUE_NON };
        }
        case IC_TOK_STAR:
        {
            ic_vm_expr vm_expr = compile_expr(expr->_unary.expr, compiler);
            assert(vm_expr.type.indirection_level);

            if (substitute_lvalue)
            {
                compile_dereference(vm_expr.type, compiler);
                ic_value_type new_type = vm_expr.type;
                new_type.const_mask = new_type.const_mask >> 1;
                new_type.indirection_level -= 1;
                vm_expr.type = new_type;
            }

            return { vm_expr.type, IC_LVALUE_ADDRESS };
        }
        default:
            assert(false);

        }
    }
    case IC_EXPR_SIZEOF:
    {
        break;
    }
    case IC_EXPR_CAST_OPERATOR:
    {
        break;
    }
    case IC_EXPR_SUBSCRIPT:
    {
        break;
    }
    case IC_EXPR_MEMBER_ACCESS:
    {
        break;
    }
    case IC_EXPR_PARENTHESES:
    {
        break;
    }
    case IC_EXPR_FUNCTION_CALL:
    {
        break;
    }
    case IC_EXPR_PRIMARY:
    {
        ic_token token = expr->token;

        switch (token.type)
        {
        case IC_TOK_INT_NUMBER_LITERAL:
        {
            ic_inst inst;
            inst.opcode = IC_OPC_PUSH;
            inst.operand.push_data.s32 = token.number;
            compiler.add_inst(inst);
            return { non_pointer_type(IC_TYPE_S32), IC_LVALUE_NON };
        }
        case IC_TOK_FLOAT_NUMBER_LITERAL:
        {
            ic_inst inst;
            inst.opcode = IC_OPC_PUSH;
            inst.operand.push_data.f32 = token.number;
            compiler.add_inst(inst);
            return { non_pointer_type(IC_TYPE_F32), IC_LVALUE_NON };
        }
        case IC_TOK_IDENTIFIER:
        {
            ic_vm_var var;
            bool local = compiler.get_var(token.string, var);
            ic_vm_expr e;
            e.lvalue_type = local ? IC_LVALUE_LOCAL : IC_LVALUE_GLOBAL;
            e.type = var.type;
            e.var_idx = var.idx;

            if (substitute_lvalue)
            {
                if (is_non_pointer_struct(var.type))
                {
                    ic_struct* s = compiler.get_struct(var.type.struct_name);
                    ic_inst inst;
                    inst.opcode = local ? IC_OPC_LOAD_STRUCT : IC_OPC_LOAD_STRUCT_GLOBAL;
                    inst.operand.load.idx = var.idx;
                    inst.operand.load.size = s->num_data;
                    compiler.add_inst(inst);

                }
                else
                {
                    ic_inst inst;
                    inst.opcode = local ? IC_OPC_LOAD : IC_OPC_LOAD_GLOBAL;
                    inst.operand.load.idx = var.idx;
                    compiler.add_inst(inst);
                }
            }

            return e;
        }
        case IC_TOK_TRUE:
        case IC_TOK_FALSE:
        {
            ic_inst inst;
            inst.opcode = IC_OPC_PUSH;
            inst.operand.push_data.s8 = token.type == IC_TOK_TRUE ? 1 : 0;
            compiler.add_inst(inst);
            return { non_pointer_type(IC_TYPE_BOOL), IC_LVALUE_NON };
        }
        case IC_TOK_NULLPTR:
        {
            ic_inst inst;
            inst.opcode = IC_OPC_PUSH;
            inst.operand.push_data.pointer = nullptr;
            compiler.add_inst(inst);
            return { pointer1_type(IC_TYPE_NULLPTR), IC_LVALUE_NON };
        }
        case IC_TOK_STRING_LITERAL:
        {
            assert(false); // todo
        }
        case IC_TOK_CHARACTER_LITERAL:
        {
            assert(false); // todo
        }
        default:
            assert(false);
        }
    default:
        assert(false);
    }
    } // switch
    
    assert(false);
    return {};
}

void compile_implicit_conversion(ic_value_type to, ic_value_type from, ic_compiler& compiler)
{
    assert(to.indirection_level == from.indirection_level);

    if (to.indirection_level)
    {
        assert(to.basic_type == from.basic_type);
        return;
    }

    if (to.basic_type == from.basic_type)
        return;

    switch (to.basic_type)
    {
    case IC_TYPE_BOOL:
        switch (from.basic_type)
        {
        case IC_TYPE_S8:
            compiler.add_inst({ IC_OPC_B_S8 });
            return;
        case IC_TYPE_U8:
            compiler.add_inst({ IC_OPC_B_U8 });
            return;
        case IC_TYPE_S32:
            compiler.add_inst({ IC_OPC_B_S32 });
            return;
        case IC_TYPE_F32:
            compiler.add_inst({ IC_OPC_B_F32 });
            return;
        case IC_TYPE_F64:
            compiler.add_inst({ IC_OPC_B_F64 });
            return;
        }
        break;
    case IC_TYPE_S8:
        switch (from.basic_type)
        {
        case IC_TYPE_BOOL:
            return;
        case IC_TYPE_U8:
            compiler.add_inst({ IC_OPC_S8_U8 });
            return;
        case IC_TYPE_S32:
            compiler.add_inst({ IC_OPC_S8_S32 });
            return;
        case IC_TYPE_F32:
            compiler.add_inst({ IC_OPC_S8_F32 });
            return;
        case IC_TYPE_F64:
            compiler.add_inst({ IC_OPC_S8_F64 });
            return;
        }
        break;
    case IC_TYPE_U8:
        switch (from.basic_type)
        {
        case IC_TYPE_BOOL:
        case IC_TYPE_S8:
            compiler.add_inst({ IC_OPC_U8_S8 });
            return;
        case IC_TYPE_S32:
            compiler.add_inst({ IC_OPC_U8_S32 });
            return;
        case IC_TYPE_F32:
            compiler.add_inst({ IC_OPC_U8_F32 });
            return;
        case IC_TYPE_F64:
            compiler.add_inst({ IC_OPC_U8_F64 });
            return;
        }
        break;
    case IC_TYPE_S32:
        switch (from.basic_type)
        {
        case IC_TYPE_BOOL:
        case IC_TYPE_S8:
            compiler.add_inst({ IC_OPC_S32_S8 });
            return;
        case IC_TYPE_U8:
            compiler.add_inst({ IC_OPC_S32_U8 });
            return;
        case IC_TYPE_F32:
            compiler.add_inst({ IC_OPC_S32_F32 });
            return;
        case IC_TYPE_F64:
            compiler.add_inst({ IC_OPC_S32_F64 });
            return;
        }
        break;
    case IC_TYPE_F32:
        switch (from.basic_type)
        {
        case IC_TYPE_BOOL:
        case IC_TYPE_S8:
            compiler.add_inst({ IC_OPC_F32_S8 });
            return;
        case IC_TYPE_U8:
            compiler.add_inst({ IC_OPC_F32_U8 });
            return;
        case IC_TYPE_S32:
            compiler.add_inst({ IC_OPC_F32_S32 });
            return;
        case IC_TYPE_F64:
            compiler.add_inst({ IC_OPC_F32_F64 });
            return;
        }
        break;
    case IC_TYPE_F64:
        switch (from.basic_type)
        {
        case IC_TYPE_BOOL:
        case IC_TYPE_S8:
            compiler.add_inst({ IC_OPC_F64_S8 });
            return;
        case IC_TYPE_U8:
            compiler.add_inst({ IC_OPC_F64_U8 });
            return;
        case IC_TYPE_S32:
            compiler.add_inst({ IC_OPC_F64_S32 });
            return;
        case IC_TYPE_F32:
            compiler.add_inst({ IC_OPC_F64_F32 });
            return;
        }
        break;
    }

    assert(false);
}

ic_value_type get_numeric_expression_type(ic_value_type lhs, ic_value_type rhs)
{
    assert(!lhs.indirection_level && !rhs.indirection_level);
    switch (lhs.basic_type)
    {
    case IC_TYPE_BOOL:
    case IC_TYPE_S8:
    case IC_TYPE_U8:
    case IC_TYPE_S32:
    case IC_TYPE_F32:
    case IC_TYPE_F64:
        break;
    default:
        assert(false);
    }

    if (lhs.basic_type < IC_TYPE_S32 && rhs.basic_type < IC_TYPE_S32)
        return non_pointer_type(IC_TYPE_S32);

    return lhs.basic_type > rhs.basic_type ? lhs : rhs;
}

// TODO
ic_value_type get_comparison_expression_type(ic_value_type lhs, ic_value_type rhs)
{
    assert(lhs.indirection_level == rhs.indirection_level);
    if (lhs.indirection_level)
    {
        return lhs;
    }

    return get_numeric_expression_type(lhs, rhs);
}

ic_vm_expr compile_lhs_assignment(ic_expr* lhs_expr, ic_value_type rhs_type, ic_compiler& compiler)
{
    ic_value_type lhs_type = get_expr_type(lhs_expr, compiler);
    compile_implicit_conversion(lhs_type, rhs_type, compiler);
    ic_vm_expr lhs = compile_expr(lhs_expr, compiler, false); // only now compile, after rhs and conversions are emitted
    ic_inst inst;

    switch (lhs.lvalue_type)
    {
    case IC_LVALUE_ADDRESS:
    {
        if (lhs.type.indirection_level)
        {
            inst.opcode = IC_OPC_STORE_8_AT;
            break;
        }

        switch (lhs.type.basic_type)
        {
        case IC_TYPE_BOOL:
        case IC_TYPE_S8:
        case IC_TYPE_U8:
            inst.opcode = IC_OPC_STORE_1_AT;
            break;
        case IC_TYPE_S32:
        case IC_TYPE_F32:
            inst.opcode = IC_OPC_STORE_4_AT;
            break;
        case IC_TYPE_F64:
            inst.opcode = IC_OPC_STORE_8_AT;
            break;
        case IC_TYPE_STRUCT:
        {
            ic_struct* _struct = compiler.get_struct(lhs.type.struct_name);
            inst.opcode = IC_OPC_STORE_STRUCT_AT;
            inst.operand.size = _struct->num_data;
            break;
        }
        default:
            assert(false);
        }

        compiler.add_inst(inst);
        compiler.add_inst({ IC_OPC_POP });
    }
    case IC_LVALUE_LOCAL:
    {
        inst.operand.store.idx = lhs.var_idx;

        if (is_non_pointer_struct(lhs.type))
        {
            inst.opcode = IC_OPC_STORE_STRUCT;
            ic_struct* _struct = compiler.get_struct(lhs.type.struct_name);
            inst.operand.size = _struct->num_data;
        }
        else
            inst.opcode = IC_OPC_STORE;

        compiler.add_inst(inst);
        break;
    }
    case IC_LVALUE_GLOBAL:
    {
        inst.operand.store.idx = lhs.var_idx;

        if (is_non_pointer_struct(lhs.type))
        {
            inst.opcode = IC_OPC_STORE_STRUCT_GLOBAL;
            ic_struct* _struct = compiler.get_struct(lhs.type.struct_name);
            inst.operand.size = _struct->num_data;
        }
        else
            inst.opcode = IC_OPC_STORE_GLOBAL;

        compiler.add_inst(inst);
        break;
    }
    default:
        assert(false);
    }

    return lhs;
}
