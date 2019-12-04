#include "ic.h"

ic_expr_result compile_compound_assignment(ic_expr* expr, int opc_s32, int opc_f32, int opc_f64, ic_compiler& compiler)
{
    ic_expr_result lhs = compile_expr(expr->_binary.lhs, compiler, false);
    assert_modifiable_lvalue(lhs);
    ic_type rhs_type = get_expr_result_type(expr->_binary.rhs, compiler);
    ic_type atype = arithmetic_expr_type(lhs.type, rhs_type);
    compiler.add_instr(IC_OPC_CLONE);
    compile_load(lhs.type, compiler);
    compile_implicit_conversion(atype, lhs.type, compiler);
    compile_expr(expr->_binary.rhs, compiler);
    compile_implicit_conversion(atype, rhs_type, compiler);

    switch (atype.basic_type)
    {
    case IC_TYPE_S32:
        compiler.add_instr(opc_s32);
        break;
    case IC_TYPE_F32:
        compiler.add_instr(opc_f32);
        break;
    case IC_TYPE_F64:
        compiler.add_instr(opc_f64);
        break;
    }
    compile_implicit_conversion(lhs.type, atype, compiler);
    compiler.add_instr(IC_OPC_SWAP);
    compile_store(lhs.type, compiler);
    return { lhs.type, false };
}

ic_expr_result compile_comparison(ic_expr* expr, int opc_s32, int opc_f32, int opc_f64, int opc_ptr, ic_compiler& compiler)
{
    ic_type lhs_type = compile_expr(expr->_binary.lhs, compiler).type;

    if (lhs_type.indirection_level)
    {
        ic_type rhs_type = compile_expr(expr->_binary.rhs, compiler).type;
        assert_comparison_compatible_pointer_types(lhs_type, rhs_type);
        compiler.add_instr(opc_ptr);
    }
    else
    {
        ic_type rhs_type = get_expr_result_type(expr->_binary.rhs, compiler);
        ic_type atype = arithmetic_expr_type(lhs_type, rhs_type);
        compile_implicit_conversion(atype, lhs_type, compiler);
        compile_expr(expr->_binary.rhs, compiler);
        compile_implicit_conversion(atype, rhs_type, compiler);

        switch (atype.basic_type)
        {
        case IC_TYPE_S32:
            compiler.add_instr(opc_s32);
            break;
        case IC_TYPE_F32:
            compiler.add_instr(opc_f32);
            break;
        case IC_TYPE_F64:
            compiler.add_instr(opc_f64);
            break;
        }
    }
    return { non_pointer_type(IC_TYPE_S32), false };
}

// lhs expr should be compiled before calling this function
ic_expr_result compile_arithmetic(ic_type lhs_type, ic_expr* rhs_expr, int opc_s32, int opc_f32, int opc_f64, ic_compiler& compiler)
{
    ic_type rhs_type = get_expr_result_type(rhs_expr, compiler);
    ic_type atype = arithmetic_expr_type(lhs_type, rhs_type);
    compile_implicit_conversion(atype, lhs_type, compiler);
    compile_expr(rhs_expr, compiler);
    compile_implicit_conversion(atype, rhs_type, compiler);

    switch (atype.basic_type)
    {
    case IC_TYPE_S32:
        compiler.add_instr(opc_s32);
        break;
    case IC_TYPE_F32:
        compiler.add_instr(opc_f32);
        break;
    case IC_TYPE_F64:
        compiler.add_instr(opc_f64);
        break;
    }
    return { atype, false };
}

// lhs expr should be compiled before calling this function
ic_expr_result compile_pointer_additive_expr(ic_type lhs_type, ic_expr* rhs_expr, int opc, ic_compiler& compiler)
{
    ic_type rhs_type = compile_expr(rhs_expr, compiler).type;
    ic_type atype = arithmetic_expr_type(rhs_type);
    assert(atype.basic_type == IC_TYPE_S32);
    compile_implicit_conversion(atype, rhs_type, compiler);
    int size;

    if (lhs_type.indirection_level > 1)
        size = 8;
    else
    {
        switch (lhs_type.basic_type)
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
            size = compiler.get_struct(lhs_type.struct_name)->num_data * sizeof(ic_data);
            break;
        default:
            assert(false); // e.g. pointer to void
        }
    }
    compiler.add_instr(opc, size);
    return { lhs_type, false };
}

ic_expr_result compile_binary(ic_expr* expr, ic_compiler& compiler)
{
    switch (expr->token.type)
    {
    case IC_TOK_EQUAL:
    {
        ic_type lhs_type = get_expr_result_type(expr->_binary.lhs, compiler); // this is needed to convert rhs before lhs lvalue is pushed
        ic_type rhs_type = compile_expr(expr->_binary.rhs, compiler).type;
        compile_implicit_conversion(lhs_type, rhs_type, compiler);
        ic_expr_result lhs = compile_expr(expr->_binary.lhs, compiler, false);
        assert_modifiable_lvalue(lhs);
        compile_store(lhs.type, compiler);
        return { lhs_type, false };
    }

    case IC_TOK_PLUS_EQUAL:
        return compile_compound_assignment(expr, IC_OPC_ADD_S32, IC_OPC_ADD_F32, IC_OPC_ADD_F64, compiler);
    case IC_TOK_MINUS_EQUAL:
        return compile_compound_assignment(expr, IC_OPC_SUB_S32, IC_OPC_SUB_F32, IC_OPC_SUB_F64, compiler);
    case IC_TOK_STAR_EQUAL:
        return compile_compound_assignment(expr, IC_OPC_MUL_S32, IC_OPC_MUL_F32, IC_OPC_MUL_F64, compiler);
    case IC_TOK_SLASH_EQUAL:
        return compile_compound_assignment(expr, IC_OPC_DIV_S32, IC_OPC_DIV_F32, IC_OPC_DIV_F64, compiler);

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
        return compile_comparison(expr, IC_OPC_COMPARE_E_S32, IC_OPC_COMPARE_E_F32, IC_OPC_COMPARE_E_F64, IC_OPC_COMPARE_E_PTR, compiler);
    case IC_TOK_BANG_EQUAL:
        return compile_comparison(expr, IC_OPC_COMPARE_NE_S32, IC_OPC_COMPARE_NE_F32, IC_OPC_COMPARE_NE_F64, IC_OPC_COMPARE_NE_PTR, compiler);
    case IC_TOK_GREATER:
        return compile_comparison(expr, IC_OPC_COMPARE_G_S32, IC_OPC_COMPARE_G_F32, IC_OPC_COMPARE_G_F64, IC_OPC_COMPARE_G_PTR, compiler);
    case IC_TOK_GREATER_EQUAL:
        return compile_comparison(expr, IC_OPC_COMPARE_GE_S32, IC_OPC_COMPARE_GE_F32, IC_OPC_COMPARE_GE_F64, IC_OPC_COMPARE_GE_PTR, compiler);
    case IC_TOK_LESS:
        return compile_comparison(expr, IC_OPC_COMPARE_L_S32, IC_OPC_COMPARE_L_F32, IC_OPC_COMPARE_L_F64, IC_OPC_COMPARE_L_PTR, compiler);
    case IC_TOK_LESS_EQUAL:
        return compile_comparison(expr, IC_OPC_COMPARE_LE_S32, IC_OPC_COMPARE_LE_F32, IC_OPC_COMPARE_LE_F64, IC_OPC_COMPARE_LE_PTR, compiler);

    case IC_TOK_PLUS:
    {
        ic_type lhs_type = compile_expr(expr->_binary.lhs, compiler).type;
        if(!lhs_type.indirection_level)
            return compile_arithmetic(lhs_type, expr->_binary.rhs, IC_OPC_ADD_S32, IC_OPC_ADD_F32, IC_OPC_ADD_F64, compiler);
        return compile_pointer_additive_expr(lhs_type, expr->_binary.rhs, IC_OPC_ADD_PTR_S32, compiler);
    }
    case IC_TOK_MINUS:
    {
        ic_type lhs_type = compile_expr(expr->_binary.lhs, compiler).type;
        if(!lhs_type.indirection_level)
            return compile_arithmetic(lhs_type, expr->_binary.rhs, IC_OPC_SUB_S32, IC_OPC_SUB_F32, IC_OPC_SUB_F64, compiler);
        return compile_pointer_additive_expr(lhs_type, expr->_binary.rhs, IC_OPC_SUB_PTR_S32, compiler);
    }
    case IC_TOK_STAR:
    {
        ic_type lhs_type = compile_expr(expr->_binary.lhs, compiler).type;
        return compile_arithmetic(lhs_type, expr->_binary.rhs, IC_OPC_MUL_S32, IC_OPC_MUL_F32, IC_OPC_MUL_F64, compiler);
    }
    case IC_TOK_SLASH:
    {
        ic_type lhs_type = compile_expr(expr->_binary.lhs, compiler).type;
        return compile_arithmetic(lhs_type, expr->_binary.rhs, IC_OPC_DIV_S32, IC_OPC_DIV_F32, IC_OPC_DIV_F64, compiler);
    }
    case IC_TOK_PERCENT:
    {
        ic_type lhs_type = compile_expr(expr->_binary.lhs, compiler).type;
        ic_type rhs_type = get_expr_result_type(expr->_binary.rhs, compiler);
        ic_type atype = arithmetic_expr_type(lhs_type, rhs_type);
        assert(atype.basic_type == IC_TYPE_S32);
        compile_implicit_conversion(atype, lhs_type, compiler);
        compile_expr(expr->_binary.rhs, compiler);
        compile_implicit_conversion(atype, rhs_type, compiler);
        compiler.add_instr(IC_OPC_MODULO_S32);
        return { atype, false };
    }
    default:
        assert(false);
    }
    return {};
}
