#include "ic.h"

struct ic_vm_scope
{
    int prev_var_count;
};

struct ic_vm_expr
{
    bool lvalue;
    ic_value_type type;
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
ic_vm_expr compile_expr(ic_expr* expr, ic_compiler& compiler);
void compile_implicit_conversion(ic_value_type to, ic_value_type from, ic_compiler& compiler);

void compile(ic_function& function, ic_runtime& runtime)
{
    ic_compiler compiler;
    compiler.runtime = &runtime;
    compiler.function = &function;
    compiler.stack_size = 0;
    compiler.current_stack_size = 0;
    compiler.loop_level = 0;

    bool returned = compile_stmt(function.body, compiler);

    if (function.return_type.indirection_level || function.return_type.basic_type != IC_TYPE_VOID)
        assert(returned);
    else
        compiler.add_inst({ .opcode = IC_OPC_RETURN });

    function.bytecode = (ic_inst*)malloc(compiler.bytecode.size() * sizeof(ic_inst));
    memcpy(function.bytecode, compiler.bytecode.data(), compiler.bytecode.size() * sizeof(ic_inst));
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
        compiler.add_inst({ .opcode = IC_OPC_JUMP_CONDITION_FAIL });
        int idx_start = compiler.bytecode.size();
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

ic_vm_expr compile_expr(ic_expr* expr, ic_compiler& compiler)
{
    assert(expr);

    switch (expr->type)
    {
    case IC_EXPR_BINARY:
    {
        break;
    }
    case IC_EXPR_UNARY:
    {
        break;
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
        break;
    }
    } // switch
    
    assert(false);
    return {};
}

void compile_implicit_conversion(ic_value_type to, ic_value_type from, ic_compiler& compiler)
{

}
