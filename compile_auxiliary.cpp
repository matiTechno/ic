#include "ic_impl.h"

void compile_implicit_conversion(ic_type to, ic_type from, ic_compiler& compiler)
{
    assert(to.indirection_level == from.indirection_level);

    if (to.basic_type == IC_TYPE_STRUCT && from.basic_type == IC_TYPE_STRUCT)
        assert(ic_string_compare(to.struct_name, from.struct_name));

    if (to.indirection_level)
    {
        assert(to.basic_type != IC_TYPE_NULLPTR);

        // TODO, this is not correct, 
        // http://c-faq.com/ansi/constmismatch.html
        // needs t o be fixed
        // to_type must be more const than from_type, except constness of a pointer itself
        for (int i = 1; i <= to.indirection_level; ++i)
        {
            int const_to = to.const_mask & (1 << i);
            int const_from = from.const_mask & (1 << i);
            assert(const_to >= const_from);
        }

        if (to.basic_type == IC_TYPE_VOID || from.basic_type == IC_TYPE_NULLPTR || to.basic_type == from.basic_type)
            return;
        assert(false);
    }

    if (to.basic_type == from.basic_type)
        return;

    switch (to.basic_type)
    {
    case IC_TYPE_BOOL:
        switch (from.basic_type)
        {
        case IC_TYPE_S8:
            compiler.add_opcode(IC_OPC_B_S8);
            return;
        case IC_TYPE_U8:
            compiler.add_opcode(IC_OPC_B_U8);
            return;
        case IC_TYPE_S32:
            compiler.add_opcode(IC_OPC_B_S32);
            return;
        case IC_TYPE_F32:
            compiler.add_opcode(IC_OPC_B_F32);
            return;
        case IC_TYPE_F64:
            compiler.add_opcode(IC_OPC_B_F64);
            return;
        }
        assert(false);
    case IC_TYPE_S8:
        switch (from.basic_type)
        {
        case IC_TYPE_BOOL:
            return;
        case IC_TYPE_U8:
            compiler.add_opcode(IC_OPC_S8_U8);
            return;
        case IC_TYPE_S32:
            compiler.add_opcode(IC_OPC_S8_S32);
            return;
        case IC_TYPE_F32:
            compiler.add_opcode(IC_OPC_S8_F32);
            return;
        case IC_TYPE_F64:
            compiler.add_opcode(IC_OPC_S8_F64);
            return;
        }
        assert(false);
    case IC_TYPE_U8:
        switch (from.basic_type)
        {
        case IC_TYPE_BOOL:
        case IC_TYPE_S8:
            compiler.add_opcode(IC_OPC_U8_S8);
            return;
        case IC_TYPE_S32:
            compiler.add_opcode(IC_OPC_U8_S32);
            return;
        case IC_TYPE_F32:
            compiler.add_opcode(IC_OPC_U8_F32);
            return;
        case IC_TYPE_F64:
            compiler.add_opcode(IC_OPC_U8_F64);
            return;
        }
        assert(false);
    case IC_TYPE_S32:
        switch (from.basic_type)
        {
        case IC_TYPE_BOOL:
        case IC_TYPE_S8:
            compiler.add_opcode(IC_OPC_S32_S8);
            return;
        case IC_TYPE_U8:
            compiler.add_opcode(IC_OPC_S32_U8);
            return;
        case IC_TYPE_F32:
            compiler.add_opcode(IC_OPC_S32_F32);
            return;
        case IC_TYPE_F64:
            compiler.add_opcode(IC_OPC_S32_F64);
            return;
        }
        assert(false);
    case IC_TYPE_F32:
        switch (from.basic_type)
        {
        case IC_TYPE_BOOL:
        case IC_TYPE_S8:
            compiler.add_opcode(IC_OPC_F32_S8);
            return;
        case IC_TYPE_U8:
            compiler.add_opcode(IC_OPC_F32_U8);
            return;
        case IC_TYPE_S32:
            compiler.add_opcode(IC_OPC_F32_S32);
            return;
        case IC_TYPE_F64:
            compiler.add_opcode(IC_OPC_F32_F64);
            return;
        }
        assert(false);
    case IC_TYPE_F64:
        switch (from.basic_type)
        {
        case IC_TYPE_BOOL:
        case IC_TYPE_S8:
            compiler.add_opcode(IC_OPC_F64_S8);
            return;
        case IC_TYPE_U8:
            compiler.add_opcode(IC_OPC_F64_U8);
            return;
        case IC_TYPE_S32:
            compiler.add_opcode(IC_OPC_F64_S32);
            return;
        case IC_TYPE_F32:
            compiler.add_opcode(IC_OPC_F64_F32);
            return;
        }
        assert(false);
    }
    assert(false);
}

ic_type get_expr_result_type(ic_expr* expr, ic_compiler& compiler)
{
    int prev = compiler.generate_bytecode;
    compiler.generate_bytecode = false;
    ic_type type = compile_expr(expr, compiler).type;
    compiler.generate_bytecode = prev;
    return type;
}

ic_type arithmetic_expr_type(ic_type lhs, ic_type rhs)
{
    assert(!lhs.indirection_level && !rhs.indirection_level);

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
        assert(false);
    }

    if (lhs.basic_type <= IC_TYPE_S32 && rhs.basic_type <= IC_TYPE_S32)
        return non_pointer_type(IC_TYPE_S32);

    return lhs.basic_type > rhs.basic_type ? lhs : rhs;
}

ic_type arithmetic_expr_type(ic_type operand_type)
{
    return arithmetic_expr_type(operand_type, non_pointer_type(IC_TYPE_BOOL));
}


void assert_comparison_compatible_pointer_types(ic_type lhs, ic_type rhs)
{
    assert(lhs.indirection_level && rhs.indirection_level);
    bool is_void = lhs.basic_type == IC_TYPE_VOID || rhs.basic_type == IC_TYPE_VOID;
    bool is_nullptr = lhs.basic_type == IC_TYPE_NULLPTR || rhs.basic_type == IC_TYPE_NULLPTR;

    if (is_void || is_nullptr)
        return;

    assert(lhs.basic_type == rhs.basic_type);

    if (lhs.basic_type == IC_TYPE_STRUCT)
        assert(ic_string_compare(lhs.struct_name, rhs.struct_name));
}

void assert_modifiable_lvalue(ic_expr_result result)
{
    assert(result.lvalue);
    assert((result.type.const_mask & 1) == 0); // watch out for operator precedence
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
        compiler.add_s32(compiler.get_struct(type.struct_name)->num_data);
        return;
    }
    assert(false);
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
        compiler.add_s32(compiler.get_struct(type.struct_name)->num_data);
        return;
    }
    assert(false);
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
        compiler.add_s32(compiler.get_struct(result.type.struct_name)->num_data);
        return;
    }
    assert(false);
}

int pointed_type_byte_size(ic_type type, ic_compiler& compiler)
{
    if (type.indirection_level > 1)
        return 8;
    switch (type.basic_type)
    {
    case IC_TYPE_BOOL:
    case IC_TYPE_S8:
    case IC_TYPE_U8:
        return 1;
    case IC_TYPE_S32:
    case IC_TYPE_F32:
        return 4;
    case IC_TYPE_F64:
        return 8;
    case IC_TYPE_STRUCT:
        return compiler.get_struct(type.struct_name)->num_data * sizeof(ic_data);
    case IC_TYPE_VOID:
        return 0;
    }
    assert(false);
    return {};
}
