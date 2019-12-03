#include "ic.h"

// todo, get_expr_type() may be redundant in many cases
// todo, rename expr type, to e.g. numeric_type, to not confuse with ic_expr.type

ic_value compile_assignment(ic_expr* lhs_expr, ic_type rhs_type, ic_compiler& compiler)
{
    {
        ic_type lhs_type = get_expr_type(lhs_expr, compiler);
        compile_implicit_conversion(lhs_type, rhs_type, compiler);
    }
    ic_value lhs = compile_expr(lhs_expr, compiler, false);
    assert_modifiable_lvalue(lhs);
    compile_store_at(lhs.type, compiler);
    return lhs;
}

ic_value compile_binary(ic_expr* expr, ic_compiler& compiler)
{
    switch (expr->token.type)
    {
    case IC_TOK_EQUAL: // order of lhs rhs is reversed on the operand stack for assignment
    {
        ic_value rhs = compile_expr(expr->_binary.rhs, compiler);
        return compile_assignment(expr->_binary.lhs, rhs.type, compiler);
    }
    case IC_TOK_PLUS_EQUAL: // there is a lot of redundancy with get_expr_type() and compile_assignment(), fix it
    {
        ic_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
        ic_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
        ic_type expr_type = get_numeric_expr_type(lhs_type, rhs_type);
        compile_expr(expr->_binary.lhs, compiler);
        compile_implicit_conversion(expr_type, lhs_type, compiler);
        compile_expr(expr->_binary.rhs, compiler);
        compile_implicit_conversion(expr_type, rhs_type, compiler);

        switch (expr_type.basic_type)
        {
        case IC_TYPE_S32:
            compiler.add_instr(IC_OPC_ADD_S32);
            break;
        case IC_TYPE_F32:
            compiler.add_instr(IC_OPC_ADD_F32);
            break;
        case IC_TYPE_F64:
            compiler.add_instr(IC_OPC_ADD_F64);
            break;
        }
        return compile_assignment(expr->_binary.lhs, expr_type, compiler);
    }
    case IC_TOK_MINUS_EQUAL:
    {
        ic_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
        ic_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
        ic_type expr_type = get_numeric_expr_type(lhs_type, rhs_type);
        compile_expr(expr->_binary.lhs, compiler);
        compile_implicit_conversion(expr_type, lhs_type, compiler);
        compile_expr(expr->_binary.rhs, compiler);
        compile_implicit_conversion(expr_type, rhs_type, compiler);

        switch (expr_type.basic_type)
        {
        case IC_TYPE_S32:
            compiler.add_instr(IC_OPC_SUB_S32);
            break;
        case IC_TYPE_F32:
            compiler.add_instr(IC_OPC_SUB_F32);
            break;
        case IC_TYPE_F64:
            compiler.add_instr(IC_OPC_SUB_F64);
            break;
        }
        return compile_assignment(expr->_binary.lhs, expr_type, compiler);
    }
    case IC_TOK_STAR_EQUAL:
    {
        ic_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
        ic_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
        ic_type expr_type = get_numeric_expr_type(lhs_type, rhs_type);
        compile_expr(expr->_binary.lhs, compiler);
        compile_implicit_conversion(expr_type, lhs_type, compiler);
        compile_expr(expr->_binary.rhs, compiler);
        compile_implicit_conversion(expr_type, rhs_type, compiler);

        switch (expr_type.basic_type)
        {
        case IC_TYPE_S32:
            compiler.add_instr(IC_OPC_MUL_S32);
            break;
        case IC_TYPE_F32:
            compiler.add_instr(IC_OPC_MUL_F32);
            break;
        case IC_TYPE_F64:
            compiler.add_instr(IC_OPC_MUL_F64);
            break;
        }
        return compile_assignment(expr->_binary.lhs, expr_type, compiler);
    }
    case IC_TOK_SLASH_EQUAL:
    {
        ic_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
        ic_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
        ic_type expr_type = get_numeric_expr_type(lhs_type, rhs_type);
        compile_expr(expr->_binary.lhs, compiler);
        compile_implicit_conversion(expr_type, lhs_type, compiler);
        compile_expr(expr->_binary.rhs, compiler);
        compile_implicit_conversion(expr_type, rhs_type, compiler);

        switch (expr_type.basic_type)
        {
        case IC_TYPE_S32:
            compiler.add_instr(IC_OPC_DIV_S32);
            break;
        case IC_TYPE_F32:
            compiler.add_instr(IC_OPC_DIV_F32);
            break;
        case IC_TYPE_F64:
            compiler.add_instr(IC_OPC_DIV_F64);
            break;
        }
        return compile_assignment(expr->_binary.lhs, expr_type, compiler);
    }
    case IC_TOK_VBAR_VBAR:
    {
        ic_type lhs_type = compile_expr(expr->_binary.lhs, compiler).type;
        compile_implicit_conversion(non_pointer_type(IC_TYPE_S32), lhs_type, compiler);
        int idx_jump_true = compiler.bytecode.size();
        compiler.add_instr(IC_OPC_JUMP_TRUE);
        ic_type rhs_type = compile_expr(expr->_binary.rhs, compiler).type;
        compile_implicit_conversion(non_pointer_type(IC_TYPE_S32), rhs_type, compiler);
        compiler.add_instr(IC_OPC_JUMP, compiler.bytecode.size() + 2);
        compiler.bytecode[idx_jump_true].op1 = compiler.bytecode.size();
        compiler.add_instr_push({ .s32 = 1 });
        return { non_pointer_type(IC_TYPE_S32), false };
    }
    case IC_TOK_AMPERSAND_AMPERSAND:
    {
        ic_type lhs_type = compile_expr(expr->_binary.lhs, compiler).type;
        compile_implicit_conversion(non_pointer_type(IC_TYPE_S32), lhs_type, compiler);
        int idx_jump_false = compiler.bytecode.size();
        compiler.add_instr(IC_OPC_JUMP_FALSE);
        ic_type rhs_type = compile_expr(expr->_binary.rhs, compiler).type;
        compile_implicit_conversion(non_pointer_type(IC_TYPE_S32), rhs_type, compiler);
        compiler.add_instr(IC_OPC_JUMP, compiler.bytecode.size() + 2);
        compiler.bytecode[idx_jump_false].op1 = compiler.bytecode.size();
        compiler.add_instr_push({ .s32 = 0 });
        return { non_pointer_type(IC_TYPE_S32), false };
    }
    case IC_TOK_EQUAL_EQUAL:
    {
        ic_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
        ic_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);

        if (lhs_type.indirection_level)
        {
            assert_comparison_compatible_pointer_types(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compiler.add_instr(IC_OPC_COMPARE_E_PTR);
        }
        else
        {
            ic_type expr_type = get_numeric_expr_type(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_implicit_conversion(expr_type, lhs_type, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(expr_type, rhs_type, compiler);

            switch (expr_type.basic_type)
            {
            case IC_TYPE_S32:
                compiler.add_instr(IC_OPC_COMPARE_E_S32);
                break;
            case IC_TYPE_F32:
                compiler.add_instr(IC_OPC_COMPARE_E_F32);
                break;
            case IC_TYPE_F64:
                compiler.add_instr(IC_OPC_COMPARE_E_F64);
                break;
            }
        }
        return { non_pointer_type(IC_TYPE_S32), false };
    }
    case IC_TOK_BANG_EQUAL:
    {
        ic_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
        ic_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);

        if (lhs_type.indirection_level)
        {
            assert_comparison_compatible_pointer_types(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compiler.add_instr(IC_OPC_COMPARE_NE_PTR);
        }
        else
        {
            ic_type expr_type = get_numeric_expr_type(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_implicit_conversion(expr_type, lhs_type, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(expr_type, rhs_type, compiler);

            switch (expr_type.basic_type)
            {
            case IC_TYPE_S32:
                compiler.add_instr(IC_OPC_COMPARE_NE_S32);
                break;
            case IC_TYPE_F32:
                compiler.add_instr(IC_OPC_COMPARE_NE_F32);
                break;
            case IC_TYPE_F64:
                compiler.add_instr(IC_OPC_COMPARE_NE_F64);
                break;
            }
        }
        return { non_pointer_type(IC_TYPE_S32), false };
    }
    case IC_TOK_GREATER:
    {
        ic_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
        ic_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);

        if (lhs_type.indirection_level)
        {
            assert_comparison_compatible_pointer_types(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compiler.add_instr(IC_OPC_COMPARE_G_PTR);
        }
        else
        {
            ic_type expr_type = get_numeric_expr_type(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_implicit_conversion(expr_type, lhs_type, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(expr_type, rhs_type, compiler);

            switch (expr_type.basic_type)
            {
            case IC_TYPE_S32:
                compiler.add_instr(IC_OPC_COMPARE_G_S32);
                break;
            case IC_TYPE_F32:
                compiler.add_instr(IC_OPC_COMPARE_G_F32);
                break;
            case IC_TYPE_F64:
                compiler.add_instr(IC_OPC_COMPARE_G_F64);
                break;
            }
        }
        return { non_pointer_type(IC_TYPE_S32), false };
    }
    case IC_TOK_GREATER_EQUAL:
    {
        ic_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
        ic_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);

        if (lhs_type.indirection_level)
        {
            assert_comparison_compatible_pointer_types(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compiler.add_instr(IC_OPC_COMPARE_GE_PTR);
        }
        else
        {
            ic_type expr_type = get_numeric_expr_type(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_implicit_conversion(expr_type, lhs_type, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(expr_type, rhs_type, compiler);

            switch (expr_type.basic_type)
            {
            case IC_TYPE_S32:
                compiler.add_instr(IC_OPC_COMPARE_GE_S32);
                break;
            case IC_TYPE_F32:
                compiler.add_instr(IC_OPC_COMPARE_GE_F32);
                break;
            case IC_TYPE_F64:
                compiler.add_instr(IC_OPC_COMPARE_GE_F64);
                break;
            }
        }
        return { non_pointer_type(IC_TYPE_S32), false };
    }
    case IC_TOK_LESS:
    {
        ic_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
        ic_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);

        if (lhs_type.indirection_level)
        {
            assert_comparison_compatible_pointer_types(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compiler.add_instr(IC_OPC_COMPARE_L_PTR);
        }
        else
        {
            ic_type expr_type = get_numeric_expr_type(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_implicit_conversion(expr_type, lhs_type, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(expr_type, rhs_type, compiler);

            switch (expr_type.basic_type)
            {
            case IC_TYPE_S32:
                compiler.add_instr(IC_OPC_COMPARE_L_S32);
                break;
            case IC_TYPE_F32:
                compiler.add_instr(IC_OPC_COMPARE_L_F32);
                break;
            case IC_TYPE_F64:
                compiler.add_instr(IC_OPC_COMPARE_L_F64);
                break;
            }
        }
        return { non_pointer_type(IC_TYPE_S32), false };
    }
    case IC_TOK_LESS_EQUAL:
    {
        ic_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
        ic_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);

        if (lhs_type.indirection_level)
        {
            assert_comparison_compatible_pointer_types(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compiler.add_instr(IC_OPC_COMPARE_LE_PTR);
        }
        else
        {
            ic_type expr_type = get_numeric_expr_type(lhs_type, rhs_type);
            compile_expr(expr->_binary.lhs, compiler);
            compile_implicit_conversion(expr_type, lhs_type, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(expr_type, rhs_type, compiler);

            switch (expr_type.basic_type)
            {
            case IC_TYPE_S32:
                compiler.add_instr(IC_OPC_COMPARE_LE_S32);
                break;
            case IC_TYPE_F32:
                compiler.add_instr(IC_OPC_COMPARE_LE_F32);
                break;
            case IC_TYPE_F64:
                compiler.add_instr(IC_OPC_COMPARE_LE_F64);
                break;
            }
        }
        return { non_pointer_type(IC_TYPE_S32), false };
    }
    case IC_TOK_PLUS:
    {
        // this is unnecessary
        ic_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
        ic_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);

        if (lhs_type.indirection_level)
        {
            ic_type rhs_convert_type = get_numeric_expr_type(rhs_type);
            assert(rhs_convert_type.basic_type == IC_TYPE_S32); // only integer types can be pointer offsets + every integer type is promoted to s32
            compile_expr(expr->_binary.lhs, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(rhs_convert_type, rhs_type, compiler);
            int type_size;

            if (lhs_type.indirection_level > 1)
                type_size = 8;
            else
            {
                switch (lhs_type.basic_type)
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
                    type_size = compiler.get_struct(lhs_type.struct_name)->num_data * sizeof(ic_data);
                    break;
                default:
                    assert(false); // e.g. pointer to void or nullptr
                }
            }

            compiler.add_instr(IC_OPC_ADD_PTR_S32, type_size);
            return { lhs_type, false };
        }
        ic_type expr_type = get_numeric_expr_type(lhs_type, rhs_type);
        compile_expr(expr->_binary.lhs, compiler);
        compile_implicit_conversion(expr_type, lhs_type, compiler);
        compile_expr(expr->_binary.rhs, compiler);
        compile_implicit_conversion(expr_type, rhs_type, compiler);

        switch (expr_type.basic_type)
        {
        case IC_TYPE_S32:
            compiler.add_instr(IC_OPC_ADD_S32);
            break;
        case IC_TYPE_F32:
            compiler.add_instr(IC_OPC_ADD_F32);
            break;
        case IC_TYPE_F64:
            compiler.add_instr(IC_OPC_ADD_F64);
            break;
        }
        return { expr_type, false };
    }
    case IC_TOK_MINUS:
    {
        ic_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
        ic_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);

        if (lhs_type.indirection_level)
        {
            ic_type rhs_convert_type = get_numeric_expr_type(rhs_type);
            assert(rhs_convert_type.basic_type == IC_TYPE_S32);
            compile_expr(expr->_binary.lhs, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compile_implicit_conversion(rhs_convert_type, rhs_type, compiler);
            int type_size;

            if (lhs_type.indirection_level > 1)
                type_size = 8;
            else
            {
                switch (lhs_type.basic_type)
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
                    type_size = compiler.get_struct(lhs_type.struct_name)->num_data * sizeof(ic_data);
                    break;
                default:
                    assert(false);
                }
            }

            compiler.add_instr(IC_OPC_SUB_PTR_S32, type_size);
            return { lhs_type, false };
        }
        ic_type expr_type = get_numeric_expr_type(lhs_type, rhs_type);
        compile_expr(expr->_binary.lhs, compiler);
        compile_implicit_conversion(expr_type, lhs_type, compiler);
        compile_expr(expr->_binary.rhs, compiler);
        compile_implicit_conversion(expr_type, rhs_type, compiler);

        switch (expr_type.basic_type)
        {
        case IC_TYPE_S32:
            compiler.add_instr(IC_OPC_SUB_S32);
            break;
        case IC_TYPE_F32:
            compiler.add_instr(IC_OPC_SUB_F32);
            break;
        case IC_TYPE_F64:
            compiler.add_instr(IC_OPC_SUB_F64);
            break;
        }
        return { expr_type, false };
    }
    case IC_TOK_STAR:
    {
        ic_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
        ic_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
        ic_type expr_type = get_numeric_expr_type(lhs_type, rhs_type);
        compile_expr(expr->_binary.lhs, compiler);
        compile_implicit_conversion(expr_type, lhs_type, compiler);
        compile_expr(expr->_binary.rhs, compiler);
        compile_implicit_conversion(expr_type, rhs_type, compiler);

        switch (expr_type.basic_type)
        {
        case IC_TYPE_S32:
            compiler.add_instr(IC_OPC_MUL_S32);
            break;
        case IC_TYPE_F32:
            compiler.add_instr(IC_OPC_MUL_F32);
            break;
        case IC_TYPE_F64:
            compiler.add_instr(IC_OPC_MUL_F64);
            break;
        }
        return { expr_type, false };
    }
    case IC_TOK_SLASH:
    {
        ic_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
        ic_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
        ic_type expr_type = get_numeric_expr_type(lhs_type, rhs_type);
        compile_expr(expr->_binary.lhs, compiler);
        compile_implicit_conversion(expr_type, lhs_type, compiler);
        compile_expr(expr->_binary.rhs, compiler);
        compile_implicit_conversion(expr_type, rhs_type, compiler);

        switch (expr_type.basic_type)
        {
        case IC_TYPE_S32:
            compiler.add_instr(IC_OPC_DIV_S32);
            break;
        case IC_TYPE_F32:
            compiler.add_instr(IC_OPC_DIV_F32);
            break;
        case IC_TYPE_F64:
            compiler.add_instr(IC_OPC_DIV_F64);
            break;
        }
        return { expr_type, false };
    }
    case IC_TOK_PERCENT:
    {
        ic_type lhs_type = get_expr_type(expr->_binary.lhs, compiler);
        ic_type rhs_type = get_expr_type(expr->_binary.rhs, compiler);
        ic_type expr_type = get_numeric_expr_type(lhs_type, rhs_type);
        assert(expr_type.basic_type == IC_TYPE_S32); // only integer types can be modulo operands + every integer type is promoted to s32
        compile_expr(expr->_binary.lhs, compiler);
        compile_implicit_conversion(expr_type, lhs_type, compiler);
        compile_expr(expr->_binary.rhs, compiler);
        compile_implicit_conversion(expr_type, rhs_type, compiler);
        compiler.add_instr(IC_OPC_MODULO_S32);
        return { expr_type, false };
    }
    default:
        assert(false);
    }
    return {};
}
