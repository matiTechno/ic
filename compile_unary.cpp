#include "ic.h"

// todo, rename expr_type to something else, promoted_type,..., promotion_type

ic_expr_result compile_unary(ic_expr* expr, ic_compiler& compiler, bool load_lvalue)
{
    switch (expr->token.type)
    {
    case IC_TOK_MINUS:
    {
        ic_expr_result result = compile_expr(expr->_unary.expr, compiler);
        ic_type expr_type = get_numeric_expr_type(result.type);
        compile_implicit_conversion(expr_type, result.type, compiler);

        switch (expr_type.basic_type)
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
        return { expr_type, false };
    }
    case IC_TOK_BANG:
    {
        ic_expr_result result = compile_expr(expr->_unary.expr, compiler);

        if (result.type.indirection_level)
            compiler.add_instr(IC_OPC_LOGICAL_NOT_PTR);
        else
        {
            ic_type expr_type = get_numeric_expr_type(result.type);
            compile_implicit_conversion(expr_type, result.type, compiler);

            switch (expr_type.basic_type)
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
        return { non_pointer_type(IC_TYPE_S32), false }; // logical not pushes s32 onto operand stack
    }
    // this produces terrible code
    case IC_TOK_PLUS_PLUS:
    case IC_TOK_MINUS_MINUS:
    {
        // todo, support pointer arithmetic
        int offset = expr->token.type == IC_TOK_PLUS_PLUS ? 1 : -1;
        ic_expr_result result = compile_expr(expr->_unary.expr, compiler, false);
        assert_modifiable_lvalue(result);
        compiler.add_instr(IC_OPC_CLONE);
        compiler.add_instr(IC_OPC_CLONE);
        compile_load(result.type, compiler);
        ic_type expr_type = get_numeric_expr_type(result.type);
        compile_implicit_conversion(expr_type, result.type, compiler);

        switch (expr_type.basic_type)
        {
        case IC_TYPE_S32:
            compiler.add_instr_push({ .s32 = offset });
            compiler.add_instr(IC_OPC_ADD_S32);
            break;
        case IC_TYPE_F32:
            compiler.add_instr_push({ .f32 = (float)offset }); // fuck this, narrowing conversion errors are such a pain in the ass
            compiler.add_instr(IC_OPC_ADD_F32);
            break;
        case IC_TYPE_F64:
            compiler.add_instr_push({ .f64 = (double)offset });
            compiler.add_instr(IC_OPC_ADD_F64);
            break;
        }

        compile_implicit_conversion(result.type, expr_type, compiler);
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
        ic_expr_result result = compile_expr(expr->_unary.expr, compiler);
        assert(result.type.indirection_level);
        result.type.const_mask = result.type.const_mask >> 1;
        result.type.indirection_level -= 1;

        if (load_lvalue)
            compile_load(result.type, compiler);

        return { result.type, !load_lvalue };
    }
    default:
        assert(false);
    }
    return {};
}
