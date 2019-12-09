#include "ic_impl.h"

void compile_function(ic_function& function, ic_parser* parser, std::vector<ic_function*>* active_functions,
    std::vector<unsigned char>* bytecode)
{
    assert(function.type == IC_FUN_SOURCE);

    if(bytecode)
        function.data_idx = bytecode->size();

    ic_compiler compiler;
    compiler.bytecode = bytecode;
    compiler.function = &function;
    compiler.parser = parser;
    compiler.active_functions = active_functions;
    compiler.generate_bytecode = bytecode != nullptr;
    compiler.stack_size = 0;
    compiler.max_stack_size = 0;
    compiler.loop_count = 0;
    compiler.push_scope();

    if (compiler.generate_bytecode)
        assert(active_functions);

    for (int i = 0; i < function.param_count; ++i)
    {
        if(function.params[i].name.data)
            compiler.declare_var(function.params[i].type, function.params[i].name);
        else
        {
            printf("warning unused param\n");
            compiler.declare_unused_param(function.params[i].type);
        }
    }
    ic_stmt_result result = compile_stmt(function.body, compiler);
    assert(compiler.scopes.size() == 1);
    assert(compiler.loop_count == 0);

    if(!is_void(function.return_type))
        assert(result == IC_STMT_RESULT_RETURN);
    else if(result != IC_STMT_RESULT_RETURN)
        compiler.add_opcode(IC_OPC_RETURN);

    function.stack_size = compiler.max_stack_size;
}

ic_stmt_result compile_stmt(ic_stmt* stmt, ic_compiler& compiler)
{
    assert(stmt);

    switch (stmt->type)
    {
    case IC_STMT_COMPOUND:
    {
        if (stmt->_compound.push_scope)
            compiler.push_scope();

        ic_stmt* stmt_it = stmt->_compound.body;
        ic_stmt_result result = IC_STMT_RESULT_NULL;
        int prev_gen_bc = compiler.generate_bytecode;

        while (stmt_it)
        {
            ic_stmt_result inner_result = compile_stmt(stmt_it, compiler);

            if (result == IC_STMT_RESULT_NULL && inner_result != IC_STMT_RESULT_NULL)
            {
                result = inner_result;
                if (stmt_it->next) // if this is not the last statement of a compound statement
                {
                    compiler.generate_bytecode = false; // don't generate unreachable code, but compile for correctness
                    printf("warning: compiling unreachable code\n"); // todo, print on which line
                }
            }
            stmt_it = stmt_it->next;
        }

        if (stmt->_compound.push_scope)
            compiler.pop_scope();
        compiler.generate_bytecode = prev_gen_bc;
        return result;
    }
    case IC_STMT_FOR:
    {
        // outer loops may have unresolved break and continue operands
        int break_ops_begin = compiler.break_ops.size();
        int cont_ops_begin = compiler.cont_ops.size();
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
            compile_implicit_conversion(non_pointer_type(IC_TYPE_S32), result.type, compiler);
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

        for(int i = break_ops_begin; i < compiler.break_ops.size(); ++i)
            compiler.bc_set_int(compiler.break_ops[i], idx_end);

        for(int i = cont_ops_begin; i < compiler.cont_ops.size(); ++i)
            compiler.bc_set_int(compiler.cont_ops[i], idx_continue);

        compiler.pop_scope();
        compiler.loop_count -= 1;
        compiler.break_ops.resize(break_ops_begin);
        compiler.cont_ops.resize(cont_ops_begin);
        return IC_STMT_RESULT_NULL;
    }
    case IC_STMT_IF:
    {
        ic_stmt_result result_if;
        ic_stmt_result result_else = IC_STMT_RESULT_NULL;
        // no need to pop result, JUMP_FALSE instr pops it
        ic_expr_result result = compile_expr(stmt->_if.header, compiler); // no need to push a scope here, expr can't declare a new variable
        compile_implicit_conversion(non_pointer_type(IC_TYPE_S32), result.type, compiler);
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
    case IC_STMT_VAR_DECLARATION:
    {
        ic_var var = compiler.declare_var(stmt->_var_declaration.type, stmt->_var_declaration.token.string);

        if (stmt->_var_declaration.expr)
        {
            ic_expr_result result = compile_expr(stmt->_var_declaration.expr, compiler);
            compile_implicit_conversion(var.type, result.type, compiler);
            compiler.add_opcode(IC_OPC_ADDRESS);
            compiler.add_s32(var.idx);

            if (is_struct(var.type))
            {
                compiler.add_opcode(IC_OPC_STORE_STRUCT);
                compiler.add_s32(compiler.get_struct(var.type.struct_name)->num_data);
            }
            else
                compiler.add_opcode(IC_OPC_STORE_8); // variables of any non-struct type occupy 8 bytes

            // STORE poppes an address, data is still left on the operand stack
            compile_pop_expr_result(result, compiler);
        }
        else
            assert((var.type.const_mask & 1) == 0); // const variable must be initialized
        return IC_STMT_RESULT_NULL;
    }
    case IC_STMT_RETURN:
    {
        ic_type return_type = compiler.function->return_type;

        if (stmt->_return.expr)
        {
            ic_expr_result result = compile_expr(stmt->_return.expr, compiler);
            compile_implicit_conversion(return_type, result.type, compiler);
            // don't pop the result, VM passes it to a parent stack_frame
        }
        else
            assert(is_void(return_type));

        compiler.add_opcode(IC_OPC_RETURN);
        return IC_STMT_RESULT_RETURN;
    }
    case IC_STMT_BREAK:
    case IC_STMT_CONTINUE:
    {
        assert(compiler.loop_count);
        compiler.add_opcode(IC_OPC_JUMP);

        if(stmt->type == IC_STMT_BREAK)
            compiler.break_ops.push_back(compiler.bc_size());
        else
            compiler.cont_ops.push_back(compiler.bc_size());

        compiler.add_s32({});
        return IC_STMT_RESULT_BREAK_CONT;
    }
    case IC_STMT_EXPR:
    {
        if (stmt->_expr)
        {
            ic_expr_result result = compile_expr(stmt->_expr, compiler);
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
        ic_type type = expr->_sizeof.type;
        type.indirection_level += 1; // this is quite a hack
        int size = pointed_type_byte_size(type, compiler);
        assert(size); // incomplete type is not allowed
        compiler.add_opcode(IC_OPC_PUSH_S32);
        compiler.add_s32(size);
        return { non_pointer_type(IC_TYPE_S32), false };
    }
    case IC_EXPR_CAST_OPERATOR:
    {
        ic_expr_result result = compile_expr(expr->_cast_operator.expr, compiler);
        ic_type target_type = expr->_cast_operator.type;

        if (target_type.indirection_level)
            assert(result.type.indirection_level);
        else
            compile_implicit_conversion(target_type, result.type, compiler);

        return { target_type, false };
    }
    case IC_EXPR_SUBSCRIPT:
    {
        ic_type lhs_type = get_expr_result_type(expr->_subscript.lhs, compiler);
        ic_type rhs_type = get_expr_result_type(expr->_subscript.rhs, compiler);
        ic_type ptr_type;
        if (lhs_type.indirection_level)
            ptr_type = compile_pointer_offset_expr(expr->_subscript.lhs, expr->_subscript.rhs, IC_OPC_ADD_PTR_S32, compiler).type;
        else
            ptr_type = compile_pointer_offset_expr(expr->_subscript.rhs, expr->_subscript.lhs, IC_OPC_ADD_PTR_S32, compiler).type;
        return compile_dereference(ptr_type, compiler, load_lvalue);
    }
    case IC_EXPR_MEMBER_ACCESS:
    {
        ic_expr_result result;

        switch (expr->token.type)
        {
        case IC_TOK_DOT:
            result = compile_expr(expr->_member_access.lhs, compiler, false);
            break;
        case IC_TOK_ARROW:
            result = compile_expr(expr->_member_access.lhs, compiler);
            result = compile_dereference(result.type, compiler, false);
            assert(!result.type.indirection_level);
            break;
        default:
            assert(false);
        }

        assert(result.type.basic_type == IC_TYPE_STRUCT);
        ic_string target_name = expr->_member_access.rhs_token.string;
        ic_type target_type;
        ic_struct* _struct = compiler.get_struct(result.type.struct_name);
        int data_offset = 0;
        bool match = false;

        for (int i = 0; i < _struct->num_members; ++i)
        {
            const ic_param& member = _struct->members[i];

            if (string_compare(target_name, member.name))
            {
                target_type = member.type;
                match = true;
                break;
            }

            if (is_struct(member.type))
                data_offset += compiler.get_struct(member.type.struct_name)->num_data;
            else
                data_offset += 1;
        }

        assert(match);
        
        if (result.lvalue)
        {
            compiler.add_opcode(IC_OPC_PUSH_S32);
            compiler.add_s32(data_offset);
            compiler.add_opcode(IC_OPC_ADD_PTR_S32);
            compiler.add_s32(sizeof(ic_data));

            if (!load_lvalue)
            {
                // propagate constness to accessed data; non pointer type can have only one first bit set in a const mask
                assert(result.type.const_mask == 0 || result.type.const_mask == 1);
                target_type.const_mask |= result.type.const_mask; // & 1 is missing because we know only first bit may be set
                return { target_type, true };
            }

            compile_load(target_type, compiler);
            return { target_type, false };
        }

        int size = is_struct(target_type) ? compiler.get_struct(target_type.struct_name)->num_data : 1;
        compiler.add_opcode(IC_OPC_MEMMOVE);
        compiler.add_s32(_struct->num_data);
        compiler.add_s32(_struct->num_data - data_offset);
        compiler.add_s32(size);
        compiler.add_opcode(IC_OPC_POP_MANY);
        compiler.add_s32(_struct->num_data - size);
        return { target_type, false };
    }
    case IC_EXPR_PARENTHESES:
    {
        return compile_expr(expr->_parentheses.expr, compiler, load_lvalue);
    }
    case IC_EXPR_FUNCTION_CALL:
    {
        int argc = 0;
        int idx;
        ic_function* function = compiler.get_function(expr->token.string, &idx);
        ic_expr* expr_arg = expr->_function_call.arg;

        while (expr_arg)
        {
            ic_type arg_type = compile_expr(expr_arg, compiler).type;
            compile_implicit_conversion(function->params[argc].type, arg_type, compiler);
            expr_arg = expr_arg->next;
            ++argc;
        }

        assert(argc == function->param_count);
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
            ic_var var = compiler.get_var(token.string, &is_global);
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
