#include "ic.h"

ic_expr_result compile_dereference(ic_type type, ic_compiler& compiler, bool load_lvalue)
{
    assert(type.indirection_level);
    type.indirection_level -= 1;
    type.const_mask = type.const_mask >> 1;

    if (load_lvalue)
        compile_load(type, compiler);

    return { type, !load_lvalue };
}

ic_expr_result compile_unary(ic_expr* expr, ic_compiler& compiler, bool load_lvalue)
{
    switch (expr->token.type)
    {
    case IC_TOK_MINUS:
    {
        ic_type type = compile_expr(expr->_unary.expr, compiler).type;
        ic_type atype = arithmetic_expr_type(type);
        compile_implicit_conversion(atype, type, compiler);

        switch (atype.basic_type)
        {
        case IC_TYPE_S32:
            compiler.add_instr(IC_OPC_NEGATE_S32);
            break;
        case IC_TYPE_F32:
            compiler.add_instr(IC_OPC_NEGATE_F32);
            break;
        case IC_TYPE_F64:
            compiler.add_instr(IC_OPC_NEGATE_F64);
            break;
        }
        return { atype, false };
    }
    case IC_TOK_BANG:
    {
        ic_type type = compile_expr(expr->_unary.expr, compiler).type;

        if (type.indirection_level)
            compiler.add_instr(IC_OPC_LOGICAL_NOT_PTR);
        else
        {
            ic_type atype = arithmetic_expr_type(type);
            compile_implicit_conversion(atype, type, compiler);

            switch (atype.basic_type)
            {
            case IC_TYPE_S32:
                compiler.add_instr(IC_OPC_LOGICAL_NOT_S32);
                break;
            case IC_TYPE_F32:
                compiler.add_instr(IC_OPC_LOGICAL_NOT_F32);
                break;
            case IC_TYPE_F64:
                compiler.add_instr(IC_OPC_LOGICAL_NOT_F64);
                break;
            }
        }
        return { non_pointer_type(IC_TYPE_S32), false }; // logical not pushes s32 onto an operand stack
    }
    case IC_TOK_PLUS_PLUS:
    case IC_TOK_MINUS_MINUS:
    {
        // this is a quite complicated operation
        int add_value = expr->token.type == IC_TOK_PLUS_PLUS ? 1 : -1;
        ic_expr_result result = compile_expr(expr->_unary.expr, compiler, false);
        assert_modifiable_lvalue(result);
        compiler.add_instr(IC_OPC_CLONE);
        compiler.add_instr(IC_OPC_CLONE);
        compile_load(result.type, compiler);

        if (result.type.indirection_level)
        {
            int size = pointed_type_byte_size(result.type, compiler);
            assert(size);
            compiler.add_instr_push({ .s32 = add_value });
            compiler.add_instr(IC_OPC_ADD_PTR_S32, size);
        }
        else
        {
            ic_type atype = arithmetic_expr_type(result.type);
            compile_implicit_conversion(atype, result.type, compiler);

            switch (atype.basic_type)
            {
            case IC_TYPE_S32:
                compiler.add_instr_push({ .s32 = add_value });
                compiler.add_instr(IC_OPC_ADD_S32);
                break;
            case IC_TYPE_F32:
                compiler.add_instr_push({ .f32 = (float)add_value });
                compiler.add_instr(IC_OPC_ADD_F32);
                break;
            case IC_TYPE_F64:
                compiler.add_instr_push({ .f64 = (double)add_value });
                compiler.add_instr(IC_OPC_ADD_F64);
                break;
            }
            compile_implicit_conversion(result.type, atype, compiler);
        }
        compiler.add_instr(IC_OPC_SWAP);
        compile_store(result.type, compiler);

        if (load_lvalue)
        {
            compiler.add_instr(IC_OPC_SWAP);
            compiler.add_instr(IC_OPC_POP);
            return { result.type, false };
        }
        compiler.add_instr(IC_OPC_POP);
        return result;
    }
    case IC_TOK_AMPERSAND:
    {
        ic_expr_result result = compile_expr(expr->_unary.expr, compiler, false);
        assert(result.lvalue);
        result.type.indirection_level += 1;
        result.type.const_mask = result.type.const_mask << 1;
        return { result.type, false };
    }
    case IC_TOK_STAR:
    {
        ic_type type = compile_expr(expr->_unary.expr, compiler).type;
        return compile_dereference(type, compiler, load_lvalue);
    }
    default:
        assert(false);
    }
    return {};
}
