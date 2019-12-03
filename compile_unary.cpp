#include "ic.h"

// todo, rename expr_type to something else

ic_value compile_unary(ic_expr* expr, ic_compiler& compiler, bool substitute_lvalue)
{
    switch (expr->token.type)
    {
    case IC_TOK_MINUS:
    {
        ic_value value = compile_expr(expr->_unary.expr, compiler);
        ic_type expr_type = get_numeric_expr_type(value.type);
        compile_implicit_conversion(expr_type, value.type, compiler);

        switch (expr_type.basic_type)
        {
        case IC_TYPE_S32:
            compiler.add_inst(IC_OPC_NEGATE_S32);
            break;
        case IC_TYPE_F32:
            compiler.add_inst(IC_OPC_NEGATE_F32);
            break;
        case IC_TYPE_F64:
            compiler.add_inst(IC_OPC_NEGATE_F64);
            break;
        }
        return { expr_type, false };
    }
    case IC_TOK_BANG:
    {
        ic_value value = compile_expr(expr->_unary.expr, compiler);

        if (value.type.indirection_level)
            compiler.add_inst(IC_OPC_LOGICAL_NOT_PTR);
        else
        {
            ic_type expr_type = get_numeric_expr_type(value.type);
            compile_implicit_conversion(expr_type, value.type, compiler);

            switch (expr_type.basic_type)
            {
            case IC_TYPE_S32:
                compiler.add_inst(IC_OPC_LOGICAL_NOT_S32);
                break;
            case IC_TYPE_F32:
                compiler.add_inst(IC_OPC_LOGICAL_NOT_F32);
                break;
            case IC_TYPE_F64:
                compiler.add_inst(IC_OPC_LOGICAL_NOT_F64);
                break;
            }
        }
        return { non_pointer_type(IC_TYPE_S32), false }; // logical not pushes s32 onto operand stack
    }
    case IC_TOK_PLUS_PLUS:
    case IC_TOK_MINUS_MINUS:
    {
        int offset = expr->token.type == IC_TOK_PLUS_PLUS ? 1 : -1;
        ic_value value = compile_expr(expr->_unary.expr, compiler, false);
        assert_modifiable_lvalue(value);
        compiler.add_inst(IC_OPC_CLONE);
        compile_dereference(value.type, compiler);
        ic_type expr_type = get_numeric_expr_type(value.type);
        compile_implicit_conversion(expr_type, value.type, compiler);

        switch (expr_type.basic_type)
        {
        case IC_TYPE_S32:
            compiler.add_inst_push({ .s32 = offset });
            compiler.add_inst(IC_OPC_ADD_S32);
            break;
        case IC_TYPE_F32:
            compiler.add_inst_push({ .f32 = (float)offset }); // fuck this, narrowing conversion errors are such a pain in the ass
            compiler.add_inst(IC_OPC_ADD_F32);
            break;
        case IC_TYPE_F64:
            compiler.add_inst_push({ .f64 = (double)offset });
            compiler.add_inst(IC_OPC_ADD_F64);
            break;
        }

        compile_implicit_conversion(value.type, expr_type, compiler);
        compiler.add_inst(IC_OPC_SWAP);
        compile_store_at(value.type, compiler);

        if (substitute_lvalue)
        {
            compiler.add_inst(IC_OPC_POP);
            return { value.type, false };
        }

        compiler.add_inst(IC_OPC_SWAP);
        compiler.add_inst(IC_OPC_POP);
        return value;
    }
    case IC_TOK_AMPERSAND:
    {
        ic_value value = compile_expr(expr->_unary.expr, compiler, false);
        assert(value.lvalue);
        value.type.indirection_level += 1;
        value.type.const_mask = value.type.const_mask << 1;
        return { value.type, false };
    }
    case IC_TOK_STAR:
    {
        ic_value value = compile_expr(expr->_unary.expr, compiler);
        assert(value.type.indirection_level);
        value.type.const_mask = value.type.const_mask >> 1;
        value.type.indirection_level -= 1;

        if (substitute_lvalue)
            compile_dereference(value.type, compiler);

        return { value.type, !substitute_lvalue };
    }
    default:
        assert(false);
    }
    return {};
}
