#include "ic_impl.h"

ic_expr_result compile_dereference(ic_type type, ic_compiler& compiler, bool load_lvalue, ic_token token)
{
    if (!type.indirection_level)
        compiler.set_error(token, "expected a pointer type");

    type.indirection_level -= 1;
    type.const_mask = type.const_mask >> 1;

    if ( (is_struct(type) && !type._struct->defined) || is_void(type) || type.basic_type == IC_TYPE_NULLPTR)
        compiler.set_error(token, "cannot dereference an incomplete type");

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
        ic_type type = compile_expr(expr->unary.expr, compiler).type;
        ic_type atype = arithmetic_expr_type(type, compiler, expr->token);
        compile_implicit_conversion(atype, type, compiler, expr->token);

        switch (atype.basic_type)
        {
        case IC_TYPE_S32:
            compiler.add_opcode(IC_OPC_NEGATE_S32);
            break;
        case IC_TYPE_F32:
            compiler.add_opcode(IC_OPC_NEGATE_F32);
            break;
        case IC_TYPE_F64:
            compiler.add_opcode(IC_OPC_NEGATE_F64);
            break;
        }
        return { atype, false };
    }
    case IC_TOK_BANG:
    {
        ic_type type = compile_expr(expr->unary.expr, compiler).type;
        compile_implicit_conversion(non_pointer_type(IC_TYPE_BOOL), type, compiler, expr->token);
        compiler.add_opcode(IC_LOGICAL_NOT);
        return { non_pointer_type(IC_TYPE_BOOL), false };
    }
    case IC_TOK_PLUS_PLUS:
    case IC_TOK_MINUS_MINUS:
    {
        // this is quite a complicated operation
        int add_value = expr->token.type == IC_TOK_PLUS_PLUS ? 1 : -1;
        ic_expr_result result = compile_expr(expr->unary.expr, compiler, false);
        assert_modifiable_lvalue(result, compiler, expr->token);
        compiler.add_opcode(IC_OPC_CLONE);
        compiler.add_opcode(IC_OPC_CLONE);
        compile_load(result.type, compiler);

        if (result.type.indirection_level)
        {
            int size = pointed_type_byte_size(result.type, compiler);
            
            if (!size)
                compiler.set_error(expr->token, "void pointers can't be incremented / decremented");
            compiler.add_opcode(IC_OPC_PUSH_S32);
            compiler.add_s32(add_value);
            compiler.add_opcode(IC_OPC_ADD_PTR_S32);
            compiler.add_s32(size);
        }
        else
        {
            ic_type atype = arithmetic_expr_type(result.type, compiler, expr->token);
            compile_implicit_conversion(atype, result.type, compiler, expr->token);

            switch (atype.basic_type)
            {
            case IC_TYPE_S32:
                compiler.add_opcode(IC_OPC_PUSH_S32);
                compiler.add_s32(add_value);
                compiler.add_opcode(IC_OPC_ADD_S32);
                break;
            case IC_TYPE_F32:
                compiler.add_opcode(IC_OPC_PUSH_F32);
                compiler.add_f32(add_value);
                compiler.add_opcode(IC_OPC_ADD_F32);
                break;
            case IC_TYPE_F64:
                compiler.add_opcode(IC_OPC_PUSH_F64);
                compiler.add_f64(add_value);
                compiler.add_opcode(IC_OPC_ADD_F64);
                break;
            }
            compile_implicit_conversion(result.type, atype, compiler, expr->token);
        }
        compiler.add_opcode(IC_OPC_SWAP);
        compile_store(result.type, compiler);

        if (load_lvalue)
        {
            compiler.add_opcode(IC_OPC_SWAP);
            compiler.add_opcode(IC_OPC_POP);
            return { result.type, false };
        }
        compiler.add_opcode(IC_OPC_POP);
        return result;
    }
    case IC_TOK_AMPERSAND:
    {
        ic_expr_result result = compile_expr(expr->unary.expr, compiler, false);

        if (!result.lvalue)
            compiler.set_error(expr->token, "expected an lvalue expression");
        result.type.indirection_level += 1;
        result.type.const_mask = result.type.const_mask << 1;
        return { result.type, false };
    }
    case IC_TOK_STAR:
    {
        ic_type type = compile_expr(expr->unary.expr, compiler).type;
        return compile_dereference(type, compiler, load_lvalue, expr->token);
    }
    default:
        assert(false);
    }
    return {};
}
