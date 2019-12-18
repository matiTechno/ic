#include "ic_impl.h"

bool compile_function(ic_function& function, ic_memory& memory, bool code_gen)
{
    assert(function.type == IC_FUN_SOURCE);
    assert(!memory.scopes.size);
    assert(!memory.vars.size);
    assert(!memory.break_ops.size);
    assert(!memory.cont_ops.size);

    ic_compiler compiler;
    compiler.memory = &memory;
    compiler.function = &function;
    compiler.code_gen = code_gen;
    compiler.stack_size = 0;
    compiler.max_stack_size = 0;
    compiler.loop_count = 0;
    compiler.error = false;

    if (code_gen)
        function.data_idx = memory.program_data.size;
    compiler.push_scope();

    for (int i = 0; i < function.param_count; ++i)
    {
        ic_param& param = function.params[i];

        if(param.name.data)
            compiler.declare_var(param.type, param.name, function.token); // todo, param.token
        else
        {
            compiler.warn(function.token, "unused function parameter"); // todo, param.token would be better
            // even if unused must be declared to work with VM
            compiler.declare_unused_param(function.params[i].type);
        }
    }
    ic_stmt_result result = compile_stmt(function.body, compiler);
    compiler.pop_scope();

    if (!is_void(function.return_type) && result != IC_STMT_RESULT_RETURN)
        compiler.set_error(function.token, "all branches of a non-void return type function must return a value");

    if(is_void(function.return_type) && result != IC_STMT_RESULT_RETURN)
        compiler.add_opcode(IC_OPC_RETURN);

    function.stack_size = compiler.max_stack_size;
    return !compiler.error;
}

ic_stmt_result compile_stmt(ic_stmt* stmt, ic_compiler& compiler)
{
    assert(stmt);

    switch (stmt->type)
    {
    case IC_STMT_COMPOUND:
    {
        if (stmt->compound.push_scope)
            compiler.push_scope();

        ic_stmt* stmt_it = stmt->compound.body;
        ic_stmt_result result = IC_STMT_RESULT_NULL;
        int prev_gen_bc = compiler.code_gen;

        while (stmt_it && !compiler.error)
        {
            ic_stmt_result inner_result = compile_stmt(stmt_it, compiler);

            if (result == IC_STMT_RESULT_NULL && inner_result != IC_STMT_RESULT_NULL)
            {
                result = inner_result;
                if (stmt_it->next) // if this is not the last statement of a compound statement
                {
                    compiler.code_gen = false; // don't generate unreachable code, but compile for correctness
                    compiler.warn(stmt_it->next->token, "unreachable code");
                }
            }
            stmt_it = stmt_it->next;
        }

        if (stmt->compound.push_scope)
            compiler.pop_scope();
        compiler.code_gen = prev_gen_bc;
        return result;
    }
    case IC_STMT_FOR:
    {
        // outer loops may have unresolved break and continue operands
        int break_ops_begin = compiler.memory->break_ops.size;
        int cont_ops_begin = compiler.memory->cont_ops.size;
        compiler.loop_count += 1;
        compiler.push_scope();

        if (stmt->_for.header1)
            compile_stmt(stmt->_for.header1, compiler);

        int idx_begin = compiler.bc_size();
        int idx_resolve_end;

        if (stmt->_for.header2)
        {
            // no need to pop result, JUMP_FALSE instr pops it
            ic_expr_result result = compile_expr(stmt->_for.header2, compiler);
            compile_implicit_conversion(non_pointer_type(IC_TYPE_BOOL), result.type, compiler, stmt->_for.header2->token);
            compiler.add_opcode(IC_OPC_JUMP_FALSE);
            idx_resolve_end = compiler.bc_size();
            compiler.add_s32({});
        }

        compile_stmt(stmt->_for.body, compiler);
        int idx_continue = compiler.bc_size();

        if (stmt->_for.header3)
        {
            ic_expr_result result = compile_expr(stmt->_for.header3, compiler);
            compile_pop_expr_result(result, compiler);
        }

        compiler.add_opcode(IC_OPC_JUMP);
        compiler.add_s32(idx_begin);
        int idx_end = compiler.bc_size();

        if (stmt->_for.header2)
            compiler.bc_set_int(idx_resolve_end, idx_end);

        for(int i = break_ops_begin; i < compiler.memory->break_ops.size; ++i)
            compiler.bc_set_int(compiler.memory->break_ops.buf[i], idx_end);

        for(int i = cont_ops_begin; i < compiler.memory->cont_ops.size; ++i)
            compiler.bc_set_int(compiler.memory->cont_ops.buf[i], idx_continue);

        compiler.pop_scope();
        compiler.loop_count -= 1;
        compiler.memory->break_ops.resize(break_ops_begin);
        compiler.memory->cont_ops.resize(cont_ops_begin);
        return IC_STMT_RESULT_NULL;
    }
    case IC_STMT_IF:
    {
        ic_stmt_result result_if;
        ic_stmt_result result_else = IC_STMT_RESULT_NULL;
        // no need to pop result, JUMP_FALSE instr pops it
        ic_expr_result result = compile_expr(stmt->_if.header, compiler); // no need to push a scope here, expr can't declare a new variable
        compile_implicit_conversion(non_pointer_type(IC_TYPE_BOOL), result.type, compiler, stmt->_if.header->token);
        compiler.add_opcode(IC_OPC_JUMP_FALSE);
        int idx_resolve_else = compiler.bc_size();
        compiler.add_s32({});
        compiler.push_scope();
        result_if = compile_stmt(stmt->_if.body_if, compiler);
        compiler.pop_scope();
        int idx_else;

        if (stmt->_if.body_else)
        {
            compiler.add_opcode(IC_OPC_JUMP);
            int idx_resolve_end = compiler.bc_size();
            compiler.add_s32({});
            idx_else = compiler.bc_size();
            compiler.push_scope();
            result_else = compile_stmt(stmt->_if.body_else, compiler);
            compiler.pop_scope();
            int idx_end = compiler.bc_size();
            compiler.bc_set_int(idx_resolve_end, idx_end);
        }
        else
            idx_else = compiler.bc_size();

        compiler.bc_set_int(idx_resolve_else, idx_else);
        return result_if < result_else ? result_if : result_else;
    }
    case IC_STMT_VAR_DECL:
    {
        ic_var var = compiler.declare_var(stmt->var_decl.type, stmt->var_decl.token.string, stmt->var_decl.token);

        if (stmt->var_decl.expr)
        {
            ic_expr_result result = compile_expr(stmt->var_decl.expr, compiler);
            compile_implicit_conversion(var.type, result.type, compiler, stmt->token);
            compiler.add_opcode(IC_OPC_ADDRESS);
            compiler.add_s32(var.idx);

            if (is_struct(var.type))
            {
                compiler.add_opcode(IC_OPC_STORE_STRUCT);
                compiler.add_s32(var.type._struct->byte_size);
            }
            else
                compiler.add_opcode(IC_OPC_STORE_8); // variables of any non-struct type occupy 8 bytes

            // STORE poppes an address, data is still left on the operand stack
            compile_pop_expr_result(result, compiler);
        }
        else if (var.type.const_mask & 1)
            compiler.set_error(stmt->token, "const variable must be initialized");

        return IC_STMT_RESULT_NULL;
    }
    case IC_STMT_RETURN:
    {
        ic_type return_type = compiler.function->return_type;

        if (stmt->_return.expr)
        {
            ic_expr_result result = compile_expr(stmt->_return.expr, compiler);
            compile_implicit_conversion(return_type, result.type, compiler, stmt->token);
            // don't pop the result, VM passes it to a parent stack_frame
        }
        else if (!is_void(return_type))
            compiler.set_error(stmt->token, "function with a non-void return type must return a value");

        compiler.add_opcode(IC_OPC_RETURN);
        return IC_STMT_RESULT_RETURN;
    }
    case IC_STMT_BREAK:
    case IC_STMT_CONTINUE:
    {
        if (!compiler.loop_count)
            compiler.set_error(stmt->token, "break / continue statements can be used only inside loops");

        compiler.add_opcode(IC_OPC_JUMP);

        if (stmt->type == IC_STMT_BREAK)
            compiler.add_resolve_break_operand();
        else
            compiler.add_resolve_cont_operand();
        compiler.add_s32({});
        return IC_STMT_RESULT_BREAK_CONT;
    }
    case IC_STMT_EXPR:
    {
        if (stmt->expr)
        {
            ic_expr_result result = compile_expr(stmt->expr, compiler);
            compile_pop_expr_result(result, compiler);
        }
        return IC_STMT_RESULT_NULL;
    }
    default:
        assert(false);
    }
    return {};
}

ic_expr_result compile_expr(ic_expr* expr, ic_compiler& compiler, bool load_lvalue)
{
    assert(expr);

    switch (expr->type)
    {
    case IC_EXPR_BINARY:
        return compile_binary(expr, compiler);

    case IC_EXPR_UNARY:
        return compile_unary(expr, compiler, load_lvalue);

    case IC_EXPR_SIZEOF:
    {
        int size = type_byte_size(expr->_sizeof.type);
        assert(size); // incomplete type is not allowed, parser already handles it
        compiler.add_opcode(IC_OPC_PUSH_S32);
        compiler.add_s32(size);
        return { non_pointer_type(IC_TYPE_S32), false };
    }
    case IC_EXPR_CAST_OPERATOR:
    {
        ic_expr_result result = compile_expr(expr->cast_operator.expr, compiler);
        ic_type target_type = expr->cast_operator.type;

        if (target_type.indirection_level)
        {
            if (!result.type.indirection_level)
                compiler.set_error(expr->token, "pointer types can't be casted to non-pointer types");
        }
        else
            compile_implicit_conversion(target_type, result.type, compiler, expr->token);

        return { target_type, false };
    }
    case IC_EXPR_SUBSCRIPT:
    {
        ic_type lhs_type = get_expr_result_type(expr->subscript.lhs, compiler);
        ic_type result_type;
        if (lhs_type.indirection_level)
            result_type = compile_pointer_offset_expr(expr->subscript.lhs, expr->subscript.rhs, IC_OPC_ADD_PTR_S32, compiler).type;
        else
            result_type = compile_pointer_offset_expr(expr->subscript.rhs, expr->subscript.lhs, IC_OPC_ADD_PTR_S32, compiler).type;
        return compile_dereference(result_type, compiler, load_lvalue, expr->token);
    }
    case IC_EXPR_MEMBER_ACCESS:
    {
        ic_expr_result result;

        switch (expr->token.type)
        {
        case IC_TOK_DOT:
            result = compile_expr(expr->member_access.lhs, compiler, false);
            break;
        case IC_TOK_ARROW:
            result = compile_expr(expr->member_access.lhs, compiler);
            result = compile_dereference(result.type, compiler, false, expr->token);
            break;
        default:
            assert(false);
        }

        if (!is_struct(result.type))
        {
            compiler.set_error(expr->token, "member access operators can be used only on structs and struct pointers");
            // return to not dereference an invalid struct pointer
            // it is important to not return a STRUCT type which could result in an invalid pointer dereference further in the execution
            return { non_pointer_type(IC_TYPE_S32), false };
        }
        assert(result.type._struct->defined); // this would be an internal error
        // same, returning uninitialized value may crash the compiler, e.g. dereferencing an invalid struct pointer
        ic_type target_type = non_pointer_type(IC_TYPE_S32);

        ic_struct* _struct = result.type._struct;
        ic_string target_name = expr->member_access.rhs_token.string;
        int byte_offset = 0;
        bool match = false;

        for (int i = 0; i < _struct->members_size; ++i)
        {
            const ic_param& member = _struct->members[i];
            int align_size = is_struct(member.type) ? member.type._struct->alignment : type_byte_size(member.type);
            byte_offset = align(byte_offset, align_size);

            if (string_compare(target_name, member.name))
            {
                target_type = member.type;
                match = true;
                break;
            }
            byte_offset += type_byte_size(member.type);
        }

        if(!match)
            compiler.set_error(expr->token, "an invalid struct member name");
        if (result.lvalue)
        {
            // todo, don't pollute the codebase with if statements, this could be handled by some optimization pass
            if (byte_offset)
            {
                compiler.add_opcode(IC_OPC_PUSH_S32);
                compiler.add_s32(byte_offset);
                compiler.add_opcode(IC_OPC_ADD_PTR_S32);
                compiler.add_s32(sizeof(char));
            }

            if (!load_lvalue)
            {
                target_type.const_mask |= result.type.const_mask & 1; // propagate constness to a member
                return { target_type, true };
            }

            compile_load(target_type, compiler);
            return { target_type, false };
        }
        int byte_size = type_byte_size(target_type);

        if (byte_offset)
        {
            int begin_byte = bytes_to_data_size(_struct->byte_size) * sizeof(ic_data);
            compiler.add_opcode(IC_OPC_MEMMOVE);
            compiler.add_s32(begin_byte);
            compiler.add_s32(begin_byte - byte_offset);
            compiler.add_s32(byte_size);
        }
        int pop_size = (_struct->byte_size - byte_size) / sizeof(ic_data);

        if (pop_size)
        {
            compiler.add_opcode(IC_OPC_POP_MANY);
            compiler.add_s32(pop_size);
        }
        return { target_type, false };
    }
    case IC_EXPR_PARENTHESES:
    {
        return compile_expr(expr->parentheses.expr, compiler, load_lvalue);
    }
    case IC_EXPR_FUNCTION_CALL:
    {
        int argc = 0;
        int idx;
        ic_function* function = compiler.get_function(expr->token.string, &idx, expr->token);
        ic_expr* expr_arg = expr->function_call.arg;

        while (expr_arg)
        {
            ic_type arg_type = compile_expr(expr_arg, compiler).type;
            compile_implicit_conversion(function->params[argc].type, arg_type, compiler, expr_arg->token);
            expr_arg = expr_arg->next;
            ++argc;
        }

        if (argc != function->param_count)
            compiler.set_error(expr->token, "the number of arguments does not match the number of parameters");

        compiler.add_opcode(IC_OPC_CALL);
        compiler.add_s32(idx);
        return { function->return_type, false };
    }
    case IC_EXPR_PRIMARY:
    {
        ic_token token = expr->token;

        switch (token.type)
        {
        case IC_TOK_IDENTIFIER:
        {
            bool is_global;
            ic_var var = compiler.get_var(token.string, &is_global, expr->token);
            compiler.add_opcode(is_global ? IC_OPC_ADDRESS_GLOBAL : IC_OPC_ADDRESS);
            compiler.add_s32(var.idx);

            if (!load_lvalue)
                return { var.type, true };

            compile_load(var.type, compiler);
            return { var.type, false };
        }
        case IC_TOK_TRUE:
        case IC_TOK_FALSE:
            compiler.add_opcode(IC_OPC_PUSH_S32);
            compiler.add_s32(token.type == IC_TOK_TRUE ? 1 : 0);
            return { non_pointer_type(IC_TYPE_S32), false };

        case IC_TOK_NULLPTR:
            compiler.add_opcode(IC_OPC_PUSH_NULLPTR);
            return { pointer1_type(IC_TYPE_NULLPTR), false };

        case IC_TOK_INT_NUMBER_LITERAL:
        case IC_TOK_CHARACTER_LITERAL:
            compiler.add_opcode(IC_OPC_PUSH_S32);
            compiler.add_s32(token.number);
            return { non_pointer_type(IC_TYPE_S32), false };

        case IC_TOK_FLOAT_NUMBER_LITERAL:
            compiler.add_opcode(IC_OPC_PUSH_F64);
            compiler.add_f64(token.number);
            return { non_pointer_type(IC_TYPE_F64), false };

        case IC_TOK_STRING_LITERAL:
            compiler.add_opcode(IC_OPC_ADDRESS_GLOBAL);
            compiler.add_s32(token.number);
            return { const_pointer1_type(IC_TYPE_S8), false };

        default:
            assert(false);
        }
    }
    default:
        assert(false);
    }
    return {};
}
