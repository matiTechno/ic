#include "ic.h"

// lvalue expressions: variable / parameter identifiers, string literals, parenthesized expression if
// inner expression is lvalue, . operator if lhs operand is lvalue, -> operator, dereference operator *, subscription operator []

// lvalue can be used in: address_of operator, increment, decrement, lhs operand of assignment, lhs operand of member access '.'
// (in the last case lvalue is not required, but if present less stack memory / regs are used - I hope I got it right)

void compile(ic_function& function, ic_runtime& runtime)
{
    ic_compiler compiler;
    compiler.runtime = &runtime;
    compiler.function = &function;
    compiler.stack_size = 0;
    compiler.max_stack_size = 0;
    compiler.loop_level = 0;
    compiler.emit_bytecode = true;
    compiler.push_scope();

    for (int i = 0; i < function.param_count; ++i)
        compiler.declare_var(function.params[i].type, function.params[i].name);

    bool returned = compile_stmt(function.body, compiler);

    if (function.return_type.indirection_level || function.return_type.basic_type != IC_TYPE_VOID)
        assert(returned);
    else if(!returned)
        compiler.add_inst({ IC_OPC_RETURN });

    function.by_size = compiler.bytecode.size();
    function.bytecode = (ic_inst*)malloc(function.by_size * sizeof(ic_inst));
    memcpy(function.bytecode, compiler.bytecode.data(), function.by_size * sizeof(ic_inst));
    function.stack_size = compiler.max_stack_size;
}

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
            if (returned) // dead code elimination
                break;

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
            ic_value value = compile_expr(stmt->_for.header2, compiler);
            compile_implicit_conversion(non_pointer_type(IC_TYPE_S32), value.type, compiler);
            compiler.add_inst({ IC_OPC_JUMP_CONDITION_FAIL });
        }

        compile_stmt(stmt->_for.body, compiler);

        if (stmt->_for.header3)
            compile_expr(stmt->_for.header3, compiler);

        compiler.add_inst_number(IC_OPC_JUMP, loop_start_idx);

        // resolve jumps
        for (int i = loop_start_idx; i < compiler.bytecode.size(); ++i)
        {
            ic_inst& inst = compiler.bytecode[i];

            switch (inst.opcode)
            {
            case IC_OPC_JUMP_CONDITION_FAIL:
                inst.opcode = IC_OPC_JUMP_FALSE;
                inst.operand.number = compiler.bytecode.size();
                break;
            case IC_OPC_JUMP_START:
                inst.opcode = IC_OPC_JUMP;
                inst.operand.number = loop_start_idx;
                break;
            case IC_OPC_JUMP_END:
                inst.opcode = IC_OPC_JUMP;
                inst.operand.number = compiler.bytecode.size();
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
        ic_value value = compile_expr(stmt->_if.header, compiler);
        compile_implicit_conversion(non_pointer_type(IC_TYPE_S32), value.type, compiler);
        int idx_start = compiler.bytecode.size();
        compiler.add_inst(IC_OPC_JUMP_CONDITION_FAIL);
        compiler.push_scope();
        returned_if = compile_stmt(stmt->_if.body_if, compiler);
        compiler.pop_scope();
        int idx_else;

        if (stmt->_if.body_else)
        {
            compiler.add_inst(IC_OPC_JUMP_END); // don't enter else when if block was entered
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
                inst.operand.number = idx_else;
                break;
            case IC_OPC_JUMP_END:
                inst.opcode = IC_OPC_JUMP;
                inst.operand.number = compiler.bytecode.size();
                break;
            }
        }

        returned = returned_if && returned_else; // both branches must return, if there is no else branch return false
        break;
    }
    case IC_STMT_VAR_DECLARATION:
    {
        ic_var var = compiler.declare_var(stmt->_var_declaration.type, stmt->_var_declaration.token.string);

        if (stmt->_var_declaration.expr)
        {
            ic_value value = compile_expr(stmt->_var_declaration.expr, compiler);
            compile_implicit_conversion(var.type, value.type, compiler);
            compiler.add_inst_number(IC_OPC_ADDRESS_OF, var.idx);

            if (is_non_pointer_struct(var.type))
                compiler.add_inst_number(IC_OPC_STORE_STRUCT_AT, compiler.get_struct(var.type.struct_name)->num_data);
            else
                compiler.add_inst(IC_OPC_STORE_8_AT); // variables of any non-struct type occupy 8 bytes
        }
        else
            assert((var.type.const_mask & 1) == 0);

        break;
    }
    case IC_STMT_RETURN:
    {
        returned = true;
        ic_type return_type = compiler.function->return_type;

        if (stmt->_return.expr)
        {
            ic_value value = compile_expr(stmt->_return.expr, compiler);
            compile_implicit_conversion(return_type, value.type, compiler);
        }
        else
            assert(!return_type.indirection_level && return_type.basic_type == IC_TYPE_VOID);

        compiler.add_inst(IC_OPC_RETURN);
        break;
    }
    case IC_STMT_BREAK:
    {
        assert(compiler.loop_level);
        compiler.add_inst(IC_OPC_JUMP_END);
        break;
    }
    case IC_STMT_CONTINUE:
    {
        assert(compiler.loop_level);
        compiler.add_inst(IC_OPC_JUMP_START);
        break;
    }
    case IC_STMT_EXPR:
    {
        if (stmt->_expr)
        {
            compile_expr(stmt->_expr, compiler);
            compiler.add_inst(IC_OPC_CLEAR); // todo
        }

        break;
    }
    default:
        assert(false);
    }

    return returned;
}

ic_value compile_expr(ic_expr* expr, ic_compiler& compiler, bool substitute_lvalue)
{
    assert(expr);

    switch (expr->type)
    {
    case IC_EXPR_BINARY:
        return compile_binary(expr, compiler);

    case IC_EXPR_UNARY:
        return compile_unary(expr, compiler, substitute_lvalue);

    case IC_EXPR_SIZEOF:
    {
        ic_type type = expr->_sizeof.type;
        int size;

        if (type.indirection_level)
            size = 8;
        else
        {
            switch (type.basic_type)
            {
            case IC_TYPE_BOOL:
            case IC_TYPE_S8:
            case IC_TYPE_U8:
                size = 1;
                break;
            case IC_TYPE_S32:
            case IC_TYPE_F32:
                size = 4;
                break;
            case IC_TYPE_F64:
                size = 8;
                break;
            case IC_TYPE_STRUCT:
                size = compiler.get_struct(type.struct_name)->num_data * sizeof(ic_data);
                break;
            default:
                assert(false);
            }
        }
        compiler.add_inst_push({ .s32 = size });
        return { non_pointer_type(IC_TYPE_S32), false };
    }
    case IC_EXPR_CAST_OPERATOR:
    {
        ic_value value = compile_expr(expr->_cast_operator.expr, compiler);
        ic_type target_type = expr->_cast_operator.type;

        if (target_type.indirection_level)
            assert(value.type.indirection_level);
        else
            compile_implicit_conversion(target_type, value.type, compiler);

        return { target_type, false };
    }
    case IC_EXPR_SUBSCRIPT:
    {
        // todo, redundant with IC_TOK_PLUS
        ic_value lhs = compile_expr(expr->_subscript.lhs, compiler);
        assert(lhs.type.indirection_level);
        ic_value rhs = compile_expr(expr->_subscript.rhs, compiler);
        ic_type rhs_convert_type = get_numeric_expr_type(rhs.type);
        assert(rhs_convert_type.basic_type == IC_TYPE_S32);
        compile_implicit_conversion(rhs_convert_type, rhs.type, compiler);
        int type_size;

        if (lhs.type.indirection_level > 1)
            type_size = 8;
        else
        {
            switch (lhs.type.basic_type)
            {
            case IC_TYPE_BOOL:
            case IC_TYPE_S8:
            case IC_TYPE_U8:
                type_size = 1;
                break;
            case IC_TYPE_S32:
            case IC_TYPE_F32:
                type_size = 4;
                break;
            case IC_TYPE_F64:
                type_size = 8;
                break;
            case IC_TYPE_STRUCT:
                type_size = compiler.get_struct(lhs.type.struct_name)->num_data * sizeof(ic_data);
                break;
            default:
                assert(false); // e.g. pointer to void or nullptr
            }
        }

        compiler.add_inst_push({ .s32 = type_size }); // todo? pass type size as an operand of add_ptr instruction
        compiler.add_inst(IC_OPC_MUL_S32);
        compiler.add_inst(IC_OPC_ADD_PTR_S32);

        // todo, this is quite redundant with IC_TOK_STAR...
        lhs.type.const_mask = lhs.type.const_mask >> 1;
        lhs.type.indirection_level -= 1;

        if (substitute_lvalue)
            compile_dereference(lhs.type, compiler);

        return { lhs.type, !substitute_lvalue };
    }
    case IC_EXPR_MEMBER_ACCESS:
    {
        ic_value value = compile_expr(expr->_member_access.lhs, compiler, false);
        assert(value.type.basic_type == IC_TYPE_STRUCT);

        if (value.type.indirection_level)
        {
            assert(value.type.indirection_level == 1);
            assert(expr->token.type == IC_TOK_ARROW);

            if (value.lvalue)
                compile_dereference(value.type, compiler); // this is important, on operand stack there is currently an address of a pointer,
            // we need to dereference pointer to get a struct address
        }
        else
            assert(expr->token.type == IC_TOK_DOT);

        ic_string target_name = expr->_member_access.rhs_token.string;
        ic_struct* _struct = compiler.get_struct(value.type.struct_name);
        ic_type target_type;
        int data_offset = 0;
        bool match = false;

        for (int i = 0; i < _struct->num_members; ++i)
        {
            ic_struct_member member = _struct->members[i];

            if (ic_string_compare(target_name, member.name))
            {
                target_type = member.type;
                match = true;
                break;
            }

            if (is_non_pointer_struct(member.type))
                data_offset += compiler.get_struct(member.type.struct_name)->num_data;
            else
                data_offset += 1;

        }

        assert(match);
        
        if (value.lvalue || value.type.indirection_level)
        {
            compiler.add_inst_push({ .s32 = data_offset * (int)sizeof(ic_data) });
            compiler.add_inst(IC_OPC_ADD_PTR_S32);

            if (!substitute_lvalue)
            {
                // propagate constness to member data
                if (value.type.indirection_level)
                    target_type.const_mask |= ((value.type.const_mask >> 1) & 1);
                else
                    target_type.const_mask |= (value.type.const_mask & 1);

                return { target_type, true };
            }

            compile_dereference(target_type, compiler);
            return { target_type, false };
        }

        int target_type_size = is_non_pointer_struct(target_type) ?
            compiler.get_struct(target_type.struct_name)->num_data : 1;
        ic_inst inst;
        inst.opcode = IC_OPC_MEMMOVE;
        inst.operand.memmove.dst = _struct->num_data;
        inst.operand.memmove.src = _struct->num_data - data_offset;
        inst.operand.memmove.size = target_type_size;
        compiler.add_inst2(inst);
        compiler.add_inst_number(IC_OPC_POP_MANY, _struct->num_data - target_type_size);
        return { target_type, false };
    }
    case IC_EXPR_PARENTHESES:
    {
        return compile_expr(expr->_parentheses.expr, compiler, substitute_lvalue);
    }
    case IC_EXPR_FUNCTION_CALL:
    {
        ic_function* function = compiler.get_fun(expr->token.string);

        int argc = 0;
        ic_expr* expr_arg = expr->_function_call.arg;

        while (expr_arg)
        {
            ic_value value = compile_expr(expr_arg, compiler);
            compile_implicit_conversion(function->params[argc].type, value.type, compiler);
            expr_arg = expr_arg->next;
            ++argc;
        }

        assert(argc == function->param_count);

        compiler.add_inst_number(IC_OPC_CALL, function - compiler.runtime->_functions.data()); // todo, something nicer?
        return { function->return_type, false };
    }
    case IC_EXPR_PRIMARY:
    {
        ic_token token = expr->token;

        switch (token.type)
        {
        case IC_TOK_INT_NUMBER_LITERAL:
        {
            compiler.add_inst_push({ .s32 = (int)token.number });
            return { non_pointer_type(IC_TYPE_S32), false };
        }
        case IC_TOK_FLOAT_NUMBER_LITERAL:
        {
            compiler.add_inst_push({ .f64 = (double)token.number });
            return { non_pointer_type(IC_TYPE_F64), false };
        }
        case IC_TOK_IDENTIFIER:
        {
            ic_var var = compiler.get_var(token.string);
            compiler.add_inst_number(IC_OPC_ADDRESS_OF, var.idx);

            if (!substitute_lvalue)
                return { var.type, true };

            compile_dereference(var.type, compiler);
            return { var.type, false };

        }
        case IC_TOK_TRUE:
        {
            compiler.add_inst_push({ .s32 = 1 });
            return { non_pointer_type(IC_TYPE_S32), false };
        }
        case IC_TOK_FALSE:
        {
            compiler.add_inst_push({ .s32 = 0 });
            return { non_pointer_type(IC_TYPE_S32), false };
        }
        case IC_TOK_NULLPTR:
        {
            compiler.add_inst_push({ .pointer = nullptr });
            return { pointer1_type(IC_TYPE_NULLPTR), false };
        }
        case IC_TOK_STRING_LITERAL:
        {
            compiler.add_inst_push({ .pointer = (void*)token.string.data }); // ub? removing const
            return { pointer1_type(IC_TYPE_S8, true), false };
        }
        case IC_TOK_CHARACTER_LITERAL:
        {
            compiler.add_inst_push({ .s32 = (int)token.number });
            return { non_pointer_type(IC_TYPE_S32), false };
        }
        default:
            assert(false);
        }
    }
    default:
        assert(false);
    }
    return {};
}