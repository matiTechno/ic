#include "ic_impl.h"

ic_expr_result compile_compound_assignment_mul_div(ic_expr* expr, ic_opcode opc_s32, ic_opcode opc_f32, ic_opcode opc_f64, ic_compiler& compiler)
{
    ic_expr_result lhs = compile_expr(expr->_binary.lhs, compiler, false);
    assert_modifiable_lvalue(lhs);
    compiler.add_opcode(IC_OPC_CLONE);
    compile_load(lhs.type, compiler);
    ic_type rhs_type = get_expr_result_type(expr->_binary.rhs, compiler);
    ic_type atype = arithmetic_expr_type(lhs.type, rhs_type);
    compile_implicit_conversion(atype, lhs.type, compiler);
    compile_expr(expr->_binary.rhs, compiler);
    compile_implicit_conversion(atype, rhs_type, compiler);

    switch (atype.basic_type)
    {
    case IC_TYPE_S32:
        compiler.add_opcode(opc_s32);
        break;
    case IC_TYPE_F32:
        compiler.add_opcode(opc_f32);
        break;
    case IC_TYPE_F64:
        compiler.add_opcode(opc_f64);
        break;
    }
    compile_implicit_conversion(lhs.type, atype, compiler);
    compiler.add_opcode(IC_OPC_SWAP);
    compile_store(lhs.type, compiler);
    return { lhs.type, false };
}

ic_expr_result compile_compound_assignment_add_sub(ic_expr* expr, ic_opcode opc_s32, ic_opcode opc_f32, ic_opcode opc_f64, ic_opcode opc_ptr,
    ic_compiler& compiler)
{
    ic_expr_result lhs = compile_expr(expr->_binary.lhs, compiler, false);
    assert_modifiable_lvalue(lhs);
    compiler.add_opcode(IC_OPC_CLONE);
    compile_load(lhs.type, compiler);

    if (lhs.type.indirection_level)
    {
        ic_type rhs_type = compile_expr(expr->_binary.rhs, compiler).type;
        ic_type atype = arithmetic_expr_type(rhs_type);
        assert(atype.basic_type == IC_TYPE_S32);
        compile_implicit_conversion(atype, rhs_type, compiler);
        int size = pointed_type_byte_size(lhs.type, compiler);
        assert(size);
        compiler.add_opcode(opc_ptr);
        compiler.add_s32(size);
    }
    else
    {
        ic_type rhs_type = get_expr_result_type(expr->_binary.rhs, compiler);
        ic_type atype = arithmetic_expr_type(lhs.type, rhs_type);
        compile_implicit_conversion(atype, lhs.type, compiler);
        compile_expr(expr->_binary.rhs, compiler);
        compile_implicit_conversion(atype, rhs_type, compiler);

        switch (atype.basic_type)
        {
        case IC_TYPE_S32:
            compiler.add_opcode(opc_s32);
            break;
        case IC_TYPE_F32:
            compiler.add_opcode(opc_f32);
            break;
        case IC_TYPE_F64:
            compiler.add_opcode(opc_f64);
            break;
        }
        compile_implicit_conversion(lhs.type, atype, compiler);
    }

    compiler.add_opcode(IC_OPC_SWAP);
    compile_store(lhs.type, compiler);
    return { lhs.type, false };
}

ic_expr_result compile_comparison(ic_expr* expr, ic_opcode opc_s32, ic_opcode opc_f32, ic_opcode opc_f64, ic_opcode opc_ptr, ic_compiler& compiler)
{
    ic_type lhs_type = compile_expr(expr->_binary.lhs, compiler).type;

    if (lhs_type.indirection_level)
    {
        ic_type rhs_type = compile_expr(expr->_binary.rhs, compiler).type;
        assert_comparison_compatible_pointer_types(lhs_type, rhs_type);
        compiler.add_opcode(opc_ptr);
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
            compiler.add_opcode(opc_s32);
            break;
        case IC_TYPE_F32:
            compiler.add_opcode(opc_f32);
            break;
        case IC_TYPE_F64:
            compiler.add_opcode(opc_f64);
            break;
        }
    }
    return { non_pointer_type(IC_TYPE_S32), false };
}

// rhs_type is passed to avoid redundant get_expr_type() call in a case of add/sub expression
ic_expr_result compile_binary_arithmetic(ic_expr* expr, ic_type rhs_type, ic_opcode opc_s32, ic_opcode opc_f32, ic_opcode opc_f64,
    ic_compiler& compiler)
{
    assert(expr->type == IC_EXPR_BINARY);
    ic_type lhs_type = compile_expr(expr->_binary.lhs, compiler).type;
    ic_type atype = arithmetic_expr_type(lhs_type, rhs_type);
    compile_implicit_conversion(atype, lhs_type, compiler);
    compile_expr(expr->_binary.rhs, compiler);
    compile_implicit_conversion(atype, rhs_type, compiler);

    switch (atype.basic_type)
    {
    case IC_TYPE_S32:
        compiler.add_opcode(opc_s32);
        break;
    case IC_TYPE_F32:
        compiler.add_opcode(opc_f32);
        break;
    case IC_TYPE_F64:
        compiler.add_opcode(opc_f64);
        break;
    }
    return { atype, false };
}

ic_expr_result compile_binary_logical(ic_expr* expr, ic_opcode opc_jump, int value_early_jump, ic_compiler& compiler)
{
    // lhs condition
    ic_type lhs_type = compile_expr(expr->_binary.lhs, compiler).type;
    compile_implicit_conversion(non_pointer_type(IC_TYPE_S32), lhs_type, compiler);
    compiler.add_opcode(opc_jump);
    int idx_resolve_condition = compiler.bc_size();
    compiler.add_s32({});
    // rhs condition
    ic_type rhs_type = compile_expr(expr->_binary.rhs, compiler).type;
    compile_implicit_conversion(non_pointer_type(IC_TYPE_S32), rhs_type, compiler);
    compiler.add_opcode(IC_OPC_JUMP);
    int idx_resolve_end = compiler.bc_size();
    compiler.add_s32({});
    // if (lhs)
    int idx_condition = compiler.bc_size();
    compiler.bc_set_int(idx_resolve_condition, idx_condition);
    compiler.add_opcode(IC_OPC_PUSH_S32);
    compiler.add_s32(value_early_jump);

    int idx_end = compiler.bc_size();
    compiler.bc_set_int(idx_resolve_end, idx_end);

    return { non_pointer_type(IC_TYPE_S32), false };
}

ic_expr_result compile_pointer_offset_expr(ic_expr* ptr_expr, ic_expr* offset_expr, ic_opcode opc, ic_compiler& compiler)
{
    ic_type ptr_type = compile_expr(ptr_expr, compiler).type;
    ic_type offset_type = compile_expr(offset_expr, compiler).type;
    ic_type atype = arithmetic_expr_type(offset_type);
    assert(atype.basic_type == IC_TYPE_S32);
    compile_implicit_conversion(atype, offset_type, compiler);
    int size = pointed_type_byte_size(ptr_type, compiler);
    assert(size);
    compiler.add_opcode(opc);
    compiler.add_s32(size);
    return { ptr_type, false };
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
        return compile_compound_assignment_add_sub(expr, IC_OPC_ADD_S32, IC_OPC_ADD_F32, IC_OPC_ADD_F64, IC_OPC_ADD_PTR_S32, compiler);
    case IC_TOK_MINUS_EQUAL:
        return compile_compound_assignment_add_sub(expr, IC_OPC_SUB_S32, IC_OPC_SUB_F32, IC_OPC_SUB_F64, IC_OPC_SUB_PTR_S32, compiler);
    case IC_TOK_STAR_EQUAL:
        return compile_compound_assignment_mul_div(expr, IC_OPC_MUL_S32, IC_OPC_MUL_F32, IC_OPC_MUL_F64, compiler);
    case IC_TOK_SLASH_EQUAL:
        return compile_compound_assignment_mul_div(expr, IC_OPC_DIV_S32, IC_OPC_DIV_F32, IC_OPC_DIV_F64, compiler);
    case IC_TOK_VBAR_VBAR:
        return compile_binary_logical(expr, IC_OPC_JUMP_TRUE, 1, compiler);
    case IC_TOK_AMPERSAND_AMPERSAND:
        return compile_binary_logical(expr, IC_OPC_JUMP_FALSE, 0, compiler);
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
        ic_type lhs_type = get_expr_result_type(expr->_binary.lhs, compiler);
        ic_type rhs_type = get_expr_result_type(expr->_binary.rhs, compiler);
        // both ptr + 1 and 1 + ptr expressions are valid
        if (lhs_type.indirection_level)
            return compile_pointer_offset_expr(expr->_binary.lhs, expr->_binary.rhs, IC_OPC_ADD_PTR_S32, compiler);
        if (rhs_type.indirection_level)
            return compile_pointer_offset_expr(expr->_binary.rhs, expr->_binary.lhs, IC_OPC_ADD_PTR_S32, compiler);
        return compile_binary_arithmetic(expr, rhs_type, IC_OPC_ADD_S32, IC_OPC_ADD_F32, IC_OPC_ADD_F64, compiler);
    }
    case IC_TOK_MINUS:
    {
        ic_type lhs_type = get_expr_result_type(expr->_binary.lhs, compiler);
        ic_type rhs_type = get_expr_result_type(expr->_binary.rhs, compiler);
        // 1 - ptr is not a valid expression; ptr - ptr is valid

        if (lhs_type.indirection_level && rhs_type.indirection_level)
        {
            assert(lhs_type.indirection_level == rhs_type.indirection_level);
            assert(lhs_type.basic_type == rhs_type.basic_type);
            if (lhs_type.basic_type == IC_TYPE_STRUCT)
                assert(ic_string_compare(lhs_type.struct_name, rhs_type.struct_name));

            compile_expr(expr->_binary.lhs, compiler);
            compile_expr(expr->_binary.rhs, compiler);
            compiler.add_opcode(IC_OPC_SUB_PTR_PTR);
            int size = pointed_type_byte_size(lhs_type, compiler);
            assert(size);
            compiler.add_s32(size);
            return { IC_TYPE_S32, false };
        }

        if (lhs_type.indirection_level)
            return compile_pointer_offset_expr(expr->_binary.lhs, expr->_binary.rhs, IC_OPC_SUB_PTR_S32, compiler);
        return compile_binary_arithmetic(expr, rhs_type, IC_OPC_SUB_S32, IC_OPC_SUB_F32, IC_OPC_SUB_F64, compiler);
    }
    case IC_TOK_STAR:
    {
        ic_type rhs_type = get_expr_result_type(expr->_binary.rhs, compiler);
        return compile_binary_arithmetic(expr, rhs_type, IC_OPC_MUL_S32, IC_OPC_MUL_F32, IC_OPC_MUL_F64, compiler);
    }
    case IC_TOK_SLASH:
    {
        ic_type rhs_type = get_expr_result_type(expr->_binary.rhs, compiler);
        return compile_binary_arithmetic(expr, rhs_type, IC_OPC_DIV_S32, IC_OPC_DIV_F32, IC_OPC_DIV_F64, compiler);
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
        compiler.add_opcode(IC_OPC_MODULO_S32);
        return { atype, false };
    }
    default:
        assert(false);
    }
    return {};
}
