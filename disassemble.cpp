#include <stdio.h>
#include "ic_impl.h"

void print_instructions(unsigned char* bytecode, int bytecode_size, int strings_byte_size);

void ic_program_print_disassembly(ic_program& program)
{
    printf("host_functions_size: %d\n", program.host_functions_size);
    printf("bytecode_size (includes strings): %d\n", program.bytecode_size);
    printf("strings_byte_size: %d\n", program.strings_byte_size);
    printf("global_data_byte_size (includes strings): %d\n", program.global_data_byte_size);
    printf("strings: ");
    int str_idx = 0;

    while (str_idx < program.strings_byte_size)
    {
        str_idx += printf("%s", program.bytecode + str_idx);
        str_idx += 1; // skip null character
        printf(" ");
    }

    printf("\nhost_functions:\n\n");

    for (int i = 0; i < program.host_functions_size; ++i)
    {
        ic_host_function& fun = program.host_functions[i];
        printf("id: %d\n", i);
        printf("prototype_str: %s\n", fun.prototype_str);
        printf("hash: %x\n", fun.hash);
        printf("origin: %d\n", fun.origin);
        printf("return_size: %d\n", fun.return_size);
        printf("param_size: %d\n\n", fun.param_size);
    }

    print_instructions(program.bytecode, program.bytecode_size, program.strings_byte_size);
}

void print_instructions(unsigned char* bytecode, int bytecode_size, int strings_byte_size)
{
    printf("instructions:\n");
    unsigned char* it = bytecode + strings_byte_size;

    while (it < bytecode + bytecode_size)
    {
        printf("%-8d", (int)(it - bytecode));
        ic_opcode opcode = (ic_opcode)*it;
        ++it;

        switch (opcode)
        {
        case IC_OPC_PUSH_S8:
            printf("push_s8 %d", *(char*)it);
            ++it;
            break;
        case IC_OPC_PUSH_S32:
            printf("push_s32 %d", read_int(&it));
            break;
        case IC_OPC_PUSH_F32:
            printf("push_f32 %f", read_float(&it));
            break;
        case IC_OPC_PUSH_F64:
            printf("push_f64 %f", read_double(&it));
            break;
        case IC_OPC_PUSH_NULLPTR:
            printf("push_nullptr");
            break;
        case IC_OPC_PUSH:
            printf("push");
            break;
        case IC_OPC_PUSH_MANY:
            printf("push_many %d", read_int(&it));
            break;
        case IC_OPC_POP:
            printf("pop");
            break;
        case IC_OPC_POP_MANY:
            printf("pop_many %d", read_int(&it));
            break;
        case IC_OPC_SWAP:
            printf("swap");
            break;
        case IC_OPC_MEMMOVE:
        {
            int op1 = read_int(&it);
            int op2 = read_int(&it);
            int op3 = read_int(&it);
            printf("memmove %d %d %d", op1, op2, op3);
            break;
        }
        case IC_OPC_CLONE:
            printf("clone");
            break;
        case IC_OPC_CALL:
            printf("call %d", read_int(&it));
            break;
        case IC_OPC_CALL_HOST:
            printf("call_host %d", read_int(&it));
            break;
        case IC_OPC_RETURN:
            printf("return");
            break;
        case IC_OPC_JUMP_TRUE:
            printf("jump_true %d", read_int(&it));
            break;
        case IC_OPC_JUMP_FALSE:
            printf("jump_false %d", read_int(&it));
            break;
        case IC_OPC_JUMP:
            printf("jump %d", read_int(&it));
            break;
        case IC_LOGICAL_NOT:
            printf("logical_not");
            break;
        case IC_OPC_ADDRESS:
            printf("address %d", read_int(&it));
            break;
        case IC_OPC_ADDRESS_GLOBAL:
            printf("address_global %d", read_int(&it));
            break;
        case IC_OPC_STORE_1:
            printf("store_1");
            break;
        case IC_OPC_STORE_4:
            printf("store_4");
            break;
        case IC_OPC_STORE_8:
            printf("store_8");
            break;
        case IC_OPC_STORE_STRUCT:
            printf("store_struct %d", read_int(&it));
            break;
        case IC_OPC_LOAD_1:
            printf("load_1");
            break;
        case IC_OPC_LOAD_4:
            printf("load_4");
            break;
        case IC_OPC_LOAD_8:
            printf("load_8");
            break;
        case IC_OPC_LOAD_STRUCT:
            printf("load_struct %d", read_int(&it));
            break;
        case IC_OPC_COMPARE_E_S32:
            printf("compare_e_s32");
            break;
        case IC_OPC_COMPARE_NE_S32:
            printf("compare_ne_s32");
            break;
        case IC_OPC_COMPARE_G_S32:
            printf("compare_g_s32");
            break;
        case IC_OPC_COMPARE_GE_S32:
            printf("compare_ge_s32");
            break;
        case IC_OPC_COMPARE_L_S32:
            printf("compare_l_s32");
            break;
        case IC_OPC_COMPARE_LE_S32:
            printf("compare_le_s32");
            break;
        case IC_OPC_NEGATE_S32:
            printf("negate_s32");
            break;
        case IC_OPC_ADD_S32:
            printf("add_s32");
            break;
        case IC_OPC_SUB_S32:
            printf("sub_s32");
            break;
        case IC_OPC_MUL_S32:
            printf("mul_s32");
            break;
        case IC_OPC_DIV_S32:
            printf("div_s32");
            break;
        case IC_OPC_MODULO_S32:
            printf("modulo_s32");
            break;
        case IC_OPC_COMPARE_E_F32:
            printf("compare_e_f32");
            break;
        case IC_OPC_COMPARE_NE_F32:
            printf("compare_ne_f32");
            break;
        case IC_OPC_COMPARE_G_F32:
            printf("compare_g_f32");
            break;
        case IC_OPC_COMPARE_GE_F32:
            printf("compare_ge_f32");
            break;
        case IC_OPC_COMPARE_L_F32:
            printf("compare_l_f32");
            break;
        case IC_OPC_COMPARE_LE_F32:
            printf("compare_le_f32");
            break;
        case IC_OPC_NEGATE_F32:
            printf("negate_f32");
            break;
        case IC_OPC_ADD_F32:
            printf("add_f32");
            break;
        case IC_OPC_SUB_F32:
            printf("sub_f32");
            break;
        case IC_OPC_MUL_F32:
            printf("mul_f32");
            break;
        case IC_OPC_DIV_F32:
            printf("div_f32");
            break;
        case IC_OPC_COMPARE_E_F64:
            printf("compare_e_f64");
            break;
        case IC_OPC_COMPARE_NE_F64:
            printf("compare_ne_f64");
            break;
        case IC_OPC_COMPARE_G_F64:
            printf("compare_g_f64");
            break;
        case IC_OPC_COMPARE_GE_F64:
            printf("compare_ge_f64");
            break;
        case IC_OPC_COMPARE_L_F64:
            printf("compare_l_f64");
            break;
        case IC_OPC_COMPARE_LE_F64:
            printf("compare_le_f64");
            break;
        case IC_OPC_NEGATE_F64:
            printf("negate_f64");
            break;
        case IC_OPC_ADD_F64:
            printf("add_f64");
            break;
        case IC_OPC_SUB_F64:
            printf("sub_f64");
            break;
        case IC_OPC_MUL_F64:
            printf("mul_f64");
            break;
        case IC_OPC_DIV_F64:
            printf("div_f64");
            break;
        case IC_OPC_COMPARE_E_PTR:
            printf("compare_e_ptr");
            break;
        case IC_OPC_COMPARE_NE_PTR:
            printf("compare_ne_ptr");
            break;
        case IC_OPC_COMPARE_G_PTR:
            printf("compare_g_ptr");
            break;
        case IC_OPC_COMPARE_GE_PTR:
            printf("compare_ge_ptr");
            break;
        case IC_OPC_COMPARE_L_PTR:
            printf("compare_l_ptr");
            break;
        case IC_OPC_COMPARE_LE_PTR:
            printf("compare_le_ptr");
            break;
        case IC_OPC_SUB_PTR_PTR:
            printf("sub_ptr_ptr %d", read_int(&it));
            break;
        case IC_OPC_ADD_PTR_S32:
            printf("add_ptr_s32 %d", read_int(&it));
            break;
        case IC_OPC_SUB_PTR_S32:
            printf("sub_ptr_s32 %d", read_int(&it));
            break;
        case IC_OPC_B_S8:
            printf("b_s8");
            break;
        case IC_OPC_B_U8:
            printf("b_u8");
            break;
        case IC_OPC_B_S32:
            printf("b_s32");
            break;
        case IC_OPC_B_F32:
            printf("b_f32");
            break;
        case IC_OPC_B_F64:
            printf("b_f64");
            break;
        case IC_OPC_B_PTR:
            printf("b_ptr");
            break;
        case IC_OPC_S8_U8:
            printf("s8_u8");
            break;
        case IC_OPC_S8_S32:
            printf("s8_s32");
            break;
        case IC_OPC_S8_F32:
            printf("s8_f32");
            break;
        case IC_OPC_S8_F64:
            printf("s8_f64");
            break;
        case IC_OPC_U8_S8:
            printf("u8_s8");
            break;
        case IC_OPC_U8_S32:
            printf("u8_s32");
            break;
        case IC_OPC_U8_F32:
            printf("u8_f32");
            break;
        case IC_OPC_U8_F64:
            printf("u8_f64");
            break;
        case IC_OPC_S32_S8:
            printf("s32_s8");
            break;
        case IC_OPC_S32_U8:
            printf("s32_u8");
            break;
        case IC_OPC_S32_F32:
            printf("s32_f32");
            break;
        case IC_OPC_S32_F64:
            printf("s32_f64");
            break;
        case IC_OPC_F32_S8:
            printf("f32_s8");
            break;
        case IC_OPC_F32_U8:
            printf("f32_u8");
            break;
        case IC_OPC_F32_S32:
            printf("f32_s32");
            break;
        case IC_OPC_F32_F64:
            printf("f32_f64");
            break;
        case IC_OPC_F64_S8:
            printf("f64_s8");
            break;
        case IC_OPC_F64_U8:
            printf("f64_u8");
            break;
        case IC_OPC_F64_S32:
            printf("f64_s32");
            break;
        case IC_OPC_F64_F32:
            printf("f64_f32");
            break;
        default:
            assert(false);
        }
        printf("\n");
    } // while
}
