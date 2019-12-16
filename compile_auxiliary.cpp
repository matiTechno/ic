#include "ic_impl.h"

bool compile_implicit_conversion_impl(ic_type to, ic_type from, ic_compiler& compiler)
{
    if (to.basic_type == IC_TYPE_STRUCT && from.basic_type == IC_TYPE_STRUCT && to._struct != from._struct)
        return false;

    if (to.indirection_level)
    {
        if (to.basic_type == IC_TYPE_NULLPTR)
            assert(compiler.error);

        if (!from.indirection_level)
            return false;

        if (from.basic_type == IC_TYPE_NULLPTR)
            return true;

        // this implementation is more restrictive than C++ permits, might fix later, http://c-faq.com/ansi/constmismatch.html
        if ((to.const_mask & 2) < (from.const_mask & 2))
            return false;

        if (to.basic_type == IC_TYPE_VOID && to.indirection_level == 1)
            return true;

        if (to.indirection_level != from.indirection_level || to.basic_type != from.basic_type)
            return false;

        for (int i = 2; i <= to.indirection_level; ++i)
        {
            int const_to = to.const_mask & (1 << i);
            int const_from = from.const_mask & (1 << i);
            if (const_to != const_from)
                return false;
        }
        return true;
    }
    assert(!to.indirection_level); // internal

    if (from.indirection_level)
    {
        if (to.basic_type == IC_TYPE_BOOL)
        {
            compiler.add_opcode(IC_OPC_B_PTR);
            return true;
        }
        return false;
    }

    if (to.basic_type == from.basic_type)
        return true;

    switch (to.basic_type)
    {
    case IC_TYPE_BOOL:
        switch (from.basic_type)
        {
        case IC_TYPE_S8:
            compiler.add_opcode(IC_OPC_B_S8);
            return true;
        case IC_TYPE_U8:
            compiler.add_opcode(IC_OPC_B_U8);
            return true;
        case IC_TYPE_S32:
            compiler.add_opcode(IC_OPC_B_S32);
            return true;
        case IC_TYPE_F32:
            compiler.add_opcode(IC_OPC_B_F32);
            return true;
        case IC_TYPE_F64:
            compiler.add_opcode(IC_OPC_B_F64);
            return true;
        }
        break;
    case IC_TYPE_S8:
        switch (from.basic_type)
        {
        case IC_TYPE_BOOL:
            return true;
        case IC_TYPE_U8:
            compiler.add_opcode(IC_OPC_S8_U8);
            return true;
        case IC_TYPE_S32:
            compiler.add_opcode(IC_OPC_S8_S32);
            return true;
        case IC_TYPE_F32:
            compiler.add_opcode(IC_OPC_S8_F32);
            return true;
        case IC_TYPE_F64:
            compiler.add_opcode(IC_OPC_S8_F64);
            return true;
        }
        break;
    case IC_TYPE_U8:
        switch (from.basic_type)
        {
        case IC_TYPE_BOOL:
        case IC_TYPE_S8:
            compiler.add_opcode(IC_OPC_U8_S8);
            return true;
        case IC_TYPE_S32:
            compiler.add_opcode(IC_OPC_U8_S32);
            return true;
        case IC_TYPE_F32:
            compiler.add_opcode(IC_OPC_U8_F32);
            return true;
        case IC_TYPE_F64:
            compiler.add_opcode(IC_OPC_U8_F64);
            return true;
        }
        break;
    case IC_TYPE_S32:
        switch (from.basic_type)
        {
        case IC_TYPE_BOOL:
        case IC_TYPE_S8:
            compiler.add_opcode(IC_OPC_S32_S8);
            return true;
        case IC_TYPE_U8:
            compiler.add_opcode(IC_OPC_S32_U8);
            return true;
        case IC_TYPE_F32:
            compiler.add_opcode(IC_OPC_S32_F32);
            return true;
        case IC_TYPE_F64:
            compiler.add_opcode(IC_OPC_S32_F64);
            return true;
        }
        break;
    case IC_TYPE_F32:
        switch (from.basic_type)
        {
        case IC_TYPE_BOOL:
        case IC_TYPE_S8:
            compiler.add_opcode(IC_OPC_F32_S8);
            return true;
        case IC_TYPE_U8:
            compiler.add_opcode(IC_OPC_F32_U8);
            return true;
        case IC_TYPE_S32:
            compiler.add_opcode(IC_OPC_F32_S32);
            return true;
        case IC_TYPE_F64:
            compiler.add_opcode(IC_OPC_F32_F64);
            return true;
        }
        break;
    case IC_TYPE_F64:
        switch (from.basic_type)
        {
        case IC_TYPE_BOOL:
        case IC_TYPE_S8:
            compiler.add_opcode(IC_OPC_F64_S8);
            return true;
        case IC_TYPE_U8:
            compiler.add_opcode(IC_OPC_F64_U8);
            return true;
        case IC_TYPE_S32:
            compiler.add_opcode(IC_OPC_F64_S32);
            return true;
        case IC_TYPE_F32:
            compiler.add_opcode(IC_OPC_F64_F32);
            return true;
        }
        break;
    }
    return false;
}

void compile_implicit_conversion(ic_type to, ic_type from, ic_compiler& compiler, ic_token token)
{
    if (!compile_implicit_conversion_impl(to, from, compiler))
        compiler.set_error(token, "conversion incompatible types");
}

ic_type get_expr_result_type(ic_expr* expr, ic_compiler& compiler)
{
    int prev = compiler.code_gen;
    compiler.code_gen = false;
    ic_type type = compile_expr(expr, compiler).type;
    compiler.code_gen = prev;
    return type;
}

ic_type arithmetic_expr_type(ic_type lhs, ic_type rhs, ic_compiler& compiler, ic_token token)
{
    if (lhs.indirection_level || rhs.indirection_level)
        compiler.set_error(token, "expected an arithmetic expression");

    switch (lhs.basic_type)
    {
    case IC_TYPE_BOOL:
    case IC_TYPE_S8:
    case IC_TYPE_U8:
    case IC_TYPE_S32:
    case IC_TYPE_F32:
    case IC_TYPE_F64:
        break;
    default:
        compiler.set_error(token, "expected an arithmetic expression");
    }

    if (lhs.basic_type <= IC_TYPE_S32 && rhs.basic_type <= IC_TYPE_S32)
        return non_pointer_type(IC_TYPE_S32);

    return lhs.basic_type > rhs.basic_type ? lhs : rhs;
}

ic_type arithmetic_expr_type(ic_type operand_type, ic_compiler& compiler, ic_token token)
{
    return arithmetic_expr_type(operand_type, non_pointer_type(IC_TYPE_BOOL), compiler, token);
}

void assert_modifiable_lvalue(ic_expr_result result, ic_compiler& compiler, ic_token token)
{
    if (!result.lvalue || (result.type.const_mask & 1))
        compiler.set_error(token, "expected a modifiable lvalue");
}

void compile_load(ic_type type, ic_compiler& compiler)
{
    if (type.indirection_level)
    {
        compiler.add_opcode(IC_OPC_LOAD_8);
        return;
    }

    switch (type.basic_type)
    {
    case IC_TYPE_BOOL:
    case IC_TYPE_S8:
    case IC_TYPE_U8:
        compiler.add_opcode(IC_OPC_LOAD_1);
        return;
    case IC_TYPE_S32:
    case IC_TYPE_F32:
        compiler.add_opcode(IC_OPC_LOAD_4);
        return;
    case IC_TYPE_F64:
        compiler.add_opcode(IC_OPC_LOAD_8);
        return;
    case IC_TYPE_STRUCT:
        compiler.add_opcode(IC_OPC_LOAD_STRUCT);
        compiler.add_s32(type._struct->byte_size);
        return;
    }
    assert(compiler.error);
}

void compile_store(ic_type type, ic_compiler& compiler)
{
    if (type.indirection_level)
    {
        compiler.add_opcode(IC_OPC_STORE_8);
        return;
    }

    switch (type.basic_type)
    {
    case IC_TYPE_BOOL:
    case IC_TYPE_S8:
    case IC_TYPE_U8:
        compiler.add_opcode(IC_OPC_STORE_1);
        return;
    case IC_TYPE_S32:
    case IC_TYPE_F32:
        compiler.add_opcode(IC_OPC_STORE_4);
        return;
    case IC_TYPE_F64:
        compiler.add_opcode(IC_OPC_STORE_8);
        return;
    case IC_TYPE_STRUCT:
        compiler.add_opcode(IC_OPC_STORE_STRUCT);
        compiler.add_s32(type._struct->byte_size);
        return;
    }
    assert(compiler.error);
}

void compile_pop_expr_result(ic_expr_result result, ic_compiler& compiler)
{
    // this function is called from stmt level and there is never a reason to have an expr return an lvalue
    assert(!result.lvalue);

    if (is_void(result.type))
        return;

    if (result.type.indirection_level)
    {
        compiler.add_opcode(IC_OPC_POP);
        return;
    }

    switch (result.type.basic_type)
    {
    case IC_TYPE_BOOL:
    case IC_TYPE_S8:
    case IC_TYPE_U8:
    case IC_TYPE_S32:
    case IC_TYPE_F32:
    case IC_TYPE_F64:
        compiler.add_opcode(IC_OPC_POP);
        return;
    case IC_TYPE_STRUCT:
        compiler.add_opcode(IC_OPC_POP_MANY);
        compiler.add_s32(bytes_to_data_size(result.type._struct->byte_size));
        return;
    }
    assert(compiler.error);
}

int pointed_type_byte_size(ic_type type, ic_compiler& compiler)
{
    if (!type.indirection_level)
        assert(compiler.error);

    if (type.indirection_level > 1)
        return sizeof(void*);
    switch (type.basic_type)
    {
    case IC_TYPE_BOOL:
    case IC_TYPE_S8:
    case IC_TYPE_U8:
        return sizeof(char);
    case IC_TYPE_S32:
        return sizeof(int);
    case IC_TYPE_F32:
        return sizeof(float);
    case IC_TYPE_F64:
        return sizeof(double);
    case IC_TYPE_STRUCT:
        return type._struct->byte_size;
    case IC_TYPE_VOID:
        return 0;
    }
    assert(compiler.error);
    return {};
}
