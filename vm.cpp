#include "ic_impl.h"

#define IC_CALL_STACK_SIZE (1024 * 1024)
#define IC_OPERAND_STACK_SIZE 1024
#define IC_STACK_FRAMES_SIZE 512

void ic_vm_init(ic_vm& vm)
{
    vm.stack_frames = (ic_stack_frame*)malloc(IC_STACK_FRAMES_SIZE * sizeof(ic_stack_frame));
    vm.call_stack = (ic_data*)malloc(IC_CALL_STACK_SIZE * sizeof(ic_data));
    vm.operand_stack = (ic_data*)malloc(IC_OPERAND_STACK_SIZE * sizeof(ic_data));
}

void ic_vm_free(ic_vm& vm)
{
    free(vm.stack_frames);
    free(vm.call_stack);
    free(vm.operand_stack);
}

void ic_vm::push_stack_frame(unsigned char* bytecode, int stack_size)
{
    assert(stack_frames_size < IC_STACK_FRAMES_SIZE);
    stack_frames_size += 1;
    ic_stack_frame& frame = top_frame();
    frame.size = stack_size;
    frame.bp = call_stack_size;
    frame.ip = bytecode;
    call_stack_size += stack_size;
    assert(call_stack_size <= IC_CALL_STACK_SIZE);
}

void ic_vm::pop_stack_frame()
{
    call_stack_size -= top_frame().size;
    stack_frames_size -= 1;
}

void ic_vm::push_op(ic_data data)
{
    assert(operand_stack_size < IC_OPERAND_STACK_SIZE);
    operand_stack[operand_stack_size] = data;
    operand_stack_size += 1;
}

void ic_vm::push_op()
{
    assert(operand_stack_size < IC_OPERAND_STACK_SIZE);
    operand_stack_size += 1;
}

void ic_vm::push_op_many(int size)
{
    operand_stack_size += size;
    assert(operand_stack_size <= IC_OPERAND_STACK_SIZE);
}

ic_data ic_vm::pop_op()
{
    operand_stack_size -= 1;
    return operand_stack[operand_stack_size];
}

void ic_vm::pop_op_many(int size)
{
    operand_stack_size -= size;
}

ic_data& ic_vm::top_op()
{
    return operand_stack[operand_stack_size - 1];
}

ic_data* ic_vm::end_op()
{
    return operand_stack + operand_stack_size;
}

ic_stack_frame& ic_vm::top_frame()
{
    return stack_frames[stack_frames_size - 1];
}

void ic_vm_run(ic_vm& vm, ic_program& program)
{
    memcpy(vm.call_stack, program.data, program.strings_byte_size);
    // todo, is memset 0 setting all values to 0? (e.g. is double with all bits zero 0?); clear global non string data
    memset(vm.call_stack + program.strings_byte_size, 0, program.global_data_size * sizeof(ic_data) - program.strings_byte_size);
    vm.stack_frames_size = 0;
    vm.call_stack_size = program.global_data_size; // this is important
    vm.operand_stack_size = 0;
    vm.push_stack_frame(program.data + program.functions[0].data_idx, program.functions[0].stack_size);
    assert(vm.stack_frames_size == 1);
    ic_stack_frame* frame = &vm.stack_frames[0];

    for(;;)
    {
        ic_opcode opcode = (ic_opcode)*frame->ip;
        ++frame->ip;

        switch (opcode)
        {
        case IC_OPC_PUSH_S32:
            vm.push_op();
            vm.top_op().s32 = read_int(&frame->ip);
            break;
        case IC_OPC_PUSH_F32:
            vm.push_op();
            vm.top_op().f32 = read_float(&frame->ip);
            break;
        case IC_OPC_PUSH_F64:
            vm.push_op();
            vm.top_op().f64 = read_double(&frame->ip);
            break;
        case IC_OPC_PUSH_NULLPTR:
        {
            vm.push_op();
            vm.top_op().pointer = nullptr;
        }
        case IC_OPC_POP:
        {
            vm.pop_op();
            break;
        }
        case IC_OPC_POP_MANY:
        {
            int size = read_int(&frame->ip);
            vm.pop_op_many(size);
            break;
        }
        case IC_OPC_SWAP:
        {
            ic_data top = vm.top_op();
            ic_data* second = vm.end_op() - 2;
            vm.top_op() = *second;
            *second = top;
            break;
        }
        case IC_OPC_MEMMOVE:
        {
            void* dst = (char*)vm.end_op() - read_int(&frame->ip);
            void* src = (char*)vm.end_op() - read_int(&frame->ip);
            int byte_size = read_int(&frame->ip);
            memmove(dst, src, byte_size);
            break;
        }
        case IC_OPC_CLONE:
        {
            vm.push_op(vm.top_op());
            break;
        }
        case IC_OPC_CALL:
        {
            int fun_idx = read_int(&frame->ip);
            ic_vm_function& function = program.functions[fun_idx];
            int param_size = function.param_size;

            if (function.host_impl)
            {
                ic_data return_data = function.callback(vm.end_op() - param_size, function.host_data);
                vm.pop_op_many(function.param_size);

                if (function.returns_value)
                    vm.push_op(return_data);
            }
            else
            {
                vm.push_stack_frame(program.data + function.data_idx, function.stack_size);
                frame = &vm.top_frame();
                memcpy(vm.call_stack + frame->bp, vm.end_op() - param_size, param_size * sizeof(ic_data));
                vm.pop_op_many(param_size);
            }
            break;
        }
        case IC_OPC_RETURN:
        {
            vm.pop_stack_frame();
            if (!vm.stack_frames_size)
                return;
            frame = &vm.top_frame();
            break;
        }
        case IC_OPC_JUMP_TRUE:
        {
            int idx = read_int(&frame->ip);
            if(vm.pop_op().s32)
                frame->ip = program.data + idx;
            break;
        }
        case IC_OPC_JUMP_FALSE:
        {
            int idx = read_int(&frame->ip);
            if(!vm.pop_op().s32)
                frame->ip = program.data + idx;
            break;
        }
        case IC_OPC_JUMP:
        {
            int idx = read_int(&frame->ip);
            frame->ip = program.data + idx;
            break;
        }
        case IC_OPC_ADDRESS:
        {
            int byte_offset = read_int(&frame->ip);
            void* base_addr = vm.call_stack + frame->bp;
            void* addr = (char*)base_addr + byte_offset;
            assert(addr >= base_addr && addr < vm.call_stack + vm.call_stack_size);
            vm.push_op();
            vm.top_op().pointer = addr;
            break;
        }
        case IC_OPC_ADDRESS_GLOBAL:
        {
            int byte_offset = read_int(&frame->ip);
            void* addr = (char*)vm.call_stack + byte_offset;
            assert(addr >= vm.call_stack && addr < vm.call_stack + program.global_data_size);
            vm.push_op();
            vm.top_op().pointer = addr;
            break;
        }
        case IC_OPC_STORE_1:
        {
            void* ptr = vm.pop_op().pointer;
            *(char*)ptr = vm.top_op().s8;
            break;
        }
        case IC_OPC_STORE_4:
        {
            void* ptr = vm.pop_op().pointer;
            *(int*)ptr = vm.top_op().s32;
            break;
        }
        case IC_OPC_STORE_8:
        {
            void* ptr = vm.pop_op().pointer;
            *(double*)ptr = vm.top_op().f64;
            break;
        }
        case IC_OPC_STORE_STRUCT:
        {
            void* dst = vm.pop_op().pointer;
            int byte_size = read_int(&frame->ip);
            int data_size = bytes_to_data_size(byte_size);
            memcpy(dst, vm.end_op() - data_size, byte_size);
            break;
        }
        case IC_OPC_LOAD_1:
        {
            void* ptr = vm.top_op().pointer;
            vm.top_op().s8 = *(char*)ptr;
            break;
        }
        case IC_OPC_LOAD_4:
        {
            void* ptr = vm.top_op().pointer;
            vm.top_op().s32 = *(int*)ptr;
            break;
        }
        case IC_OPC_LOAD_8:
        {
            void* ptr = vm.top_op().pointer;
            vm.top_op().f64 = *(double*)ptr;
            break;
        }
        case IC_OPC_LOAD_STRUCT:
        {
            void* ptr = vm.pop_op().pointer;
            int byte_size = read_int(&frame->ip);
            int data_size = bytes_to_data_size(byte_size);
            vm.push_op_many(data_size);
            memcpy(vm.end_op() - data_size, ptr, byte_size);
            break;
        }
        case IC_OPC_COMPARE_E_S32:
        {
            int rhs = vm.pop_op().s32;
            vm.top_op().s32 = vm.top_op().s32 == rhs;
            break;
        }
        case IC_OPC_COMPARE_NE_S32:
        {
            int rhs = vm.pop_op().s32;
            vm.top_op().s32 = vm.top_op().s32 != rhs;
            break;
        }
        case IC_OPC_COMPARE_G_S32:
        {
            int rhs = vm.pop_op().s32;
            vm.top_op().s32 = vm.top_op().s32 > rhs;
            break;
        }
        case IC_OPC_COMPARE_GE_S32:
        {
            int rhs = vm.pop_op().s32;
            vm.top_op().s32 = vm.top_op().s32 >= rhs;
            break;
        }
        case IC_OPC_COMPARE_L_S32:
        {
            int rhs = vm.pop_op().s32;
            vm.top_op().s32 = vm.top_op().s32 < rhs;
            break;
        }
        case IC_OPC_COMPARE_LE_S32:
        {
            int rhs = vm.pop_op().s32;
            vm.top_op().s32 = vm.top_op().s32 <= rhs;
            break;
        }
        case IC_OPC_LOGICAL_NOT_S32:
        {
            vm.top_op().s32 = !vm.top_op().s32;
            break;
        }
        case IC_OPC_NEGATE_S32:
        {
            vm.top_op().s32 = -vm.top_op().s32;
            break;
        }
        case IC_OPC_ADD_S32:
        {
            int rhs = vm.pop_op().s32;
            vm.top_op().s32 += rhs;
            break;
        }
        case IC_OPC_SUB_S32:
        {
            int rhs = vm.pop_op().s32;
            vm.top_op().s32 -= rhs;
            break;
        }
        case IC_OPC_MUL_S32:
        {
            int rhs = vm.pop_op().s32;
            vm.top_op().s32 *= rhs;
            break;
        }
        case IC_OPC_DIV_S32:
        {
            int rhs = vm.pop_op().s32;
            vm.top_op().s32 /= rhs;
            break;
        }
        case IC_OPC_MODULO_S32:
        {
            int rhs = vm.pop_op().s32;
            vm.top_op().s32 = vm.top_op().s32 % rhs;
            break;
        }
        case IC_OPC_COMPARE_E_F32:
        {
            float rhs = vm.pop_op().f32;
            vm.top_op().s32 = vm.top_op().f32 == rhs;
            break;
        }
        case IC_OPC_COMPARE_NE_F32:
        {
            float rhs = vm.pop_op().f32;
            vm.top_op().s32 = vm.top_op().f32 != rhs;
            break;
        }
        case IC_OPC_COMPARE_G_F32:
        {
            float rhs = vm.pop_op().f32;
            vm.top_op().s32 = vm.top_op().f32 > rhs;
            break;
        }
        case IC_OPC_COMPARE_GE_F32:
        {
            float rhs = vm.pop_op().f32;
            vm.top_op().s32 = vm.top_op().f32 >= rhs;
            break;
        }
        case IC_OPC_COMPARE_L_F32:
        {
            float rhs = vm.pop_op().f32;
            vm.top_op().s32 = vm.top_op().f32 < rhs;
            break;
        }
        case IC_OPC_COMPARE_LE_F32:
        {
            float rhs = vm.pop_op().f32;
            vm.top_op().s32 = vm.top_op().f32 <= rhs;
            break;
        }
        case IC_OPC_LOGICAL_NOT_F32:
        {
            vm.top_op().s32 = !vm.top_op().f32;
            break;
        }
        case IC_OPC_NEGATE_F32:
        {
            vm.top_op().f32 = -vm.top_op().f32;
            break;
        }
        case IC_OPC_ADD_F32:
        {
            float rhs = vm.pop_op().f32;
            vm.top_op().f32 += rhs;
            break;
        }
        case IC_OPC_SUB_F32:
        {
            float rhs = vm.pop_op().f32;
            vm.top_op().f32 -= rhs;
            break;
        }
        case IC_OPC_MUL_F32:
        {
            float rhs = vm.pop_op().f32;
            vm.top_op().f32 *= rhs;
            break;
        }
        case IC_OPC_DIV_F32:
        {
            float rhs = vm.pop_op().f32;
            vm.top_op().f32 /= rhs;
            break;
        }
        case IC_OPC_COMPARE_E_F64:
        {
            double rhs = vm.pop_op().f64;
            vm.top_op().s32 = vm.top_op().f64 == rhs;
            break;
        }
        case IC_OPC_COMPARE_NE_F64:
        {
            double rhs = vm.pop_op().f64;
            vm.top_op().s32 = vm.top_op().f64 != rhs;
            break;
        }
        case IC_OPC_COMPARE_G_F64:
        {
            double rhs = vm.pop_op().f64;
            vm.top_op().s32 = vm.top_op().f64 > rhs;
            break;
        }
        case IC_OPC_COMPARE_GE_F64:
        {
            double rhs = vm.pop_op().f64;
            vm.top_op().s32 = vm.top_op().f64 >= rhs;
            break;
        }
        case IC_OPC_COMPARE_L_F64:
        {
            double rhs = vm.pop_op().f64;
            vm.top_op().s32 = vm.top_op().f64 < rhs;
            break;
        }
        case IC_OPC_COMPARE_LE_F64:
        {
            double rhs = vm.pop_op().f64;
            vm.top_op().s32 = vm.top_op().f64 <= rhs;
            break;
        }
        case IC_OPC_LOGICAL_NOT_F64:
        {
            vm.top_op().s32 = !vm.top_op().f64;
            break;
        }
        case IC_OPC_NEGATE_F64:
        {
            vm.top_op().f64 = -vm.top_op().f64;
            break;
        }
        case IC_OPC_ADD_F64:
        {
            double rhs = vm.pop_op().f64;
            vm.top_op().f64 += rhs;
            break;
        }
        case IC_OPC_SUB_F64:
        {
            double rhs = vm.pop_op().f64;
            vm.top_op().f64 -= rhs;
            break;
        }
        case IC_OPC_MUL_F64:
        {
            double rhs = vm.pop_op().f64;
            vm.top_op().f64 *= rhs;
            break;
        }
        case IC_OPC_DIV_F64:
        {
            double rhs = vm.pop_op().f64;
            vm.top_op().f64 /= rhs;
            break;
        }
        case IC_OPC_COMPARE_E_PTR:
        {
            void* rhs = vm.pop_op().pointer;
            vm.top_op().s32 = vm.top_op().pointer == rhs;
            break;
        }
        case IC_OPC_COMPARE_NE_PTR:
        {
            void* rhs = vm.pop_op().pointer;
            vm.top_op().s32 = vm.top_op().pointer != rhs;
            break;
        }
        case IC_OPC_COMPARE_G_PTR:
        {
            void* rhs = vm.pop_op().pointer;
            vm.top_op().s32 = vm.top_op().pointer > rhs;
            break;
        }
        case IC_OPC_COMPARE_GE_PTR:
        {
            void* rhs = vm.pop_op().pointer;
            vm.top_op().s32 = vm.top_op().pointer >= rhs;
            break;
        }
        case IC_OPC_COMPARE_L_PTR:
        {
            void* rhs = vm.pop_op().pointer;
            vm.top_op().s32 = vm.top_op().pointer < rhs;
            break;
        }
        case IC_OPC_COMPARE_LE_PTR:
        {
            void* rhs = vm.pop_op().pointer;
            vm.top_op().s32 = vm.top_op().pointer <= rhs;
            break;
        }
        case IC_OPC_LOGICAL_NOT_PTR:
        {
            vm.top_op().s32 = !vm.top_op().pointer;
            break;
        }
        case IC_OPC_SUB_PTR_PTR:
        {
            int type_byte_size = read_int(&frame->ip);
            assert(type_byte_size);
            void* rhs = vm.pop_op().pointer;
            vm.top_op().s32 = ((char*)vm.top_op().pointer - (char*)rhs) / type_byte_size;
            break;
        }
        case IC_OPC_ADD_PTR_S32:
        {
            int type_byte_size = read_int(&frame->ip);
            assert(type_byte_size);
            int bytes = vm.pop_op().s32 * type_byte_size;
            vm.top_op().pointer = (char*)vm.top_op().pointer + bytes;
            break;
        }
        case IC_OPC_SUB_PTR_S32:
        {
            int type_byte_size = read_int(&frame->ip);
            assert(type_byte_size);
            int bytes = vm.pop_op().s32 * type_byte_size;
            vm.top_op().pointer = (char*)vm.top_op().pointer - bytes;
            break;
        }
        case IC_OPC_B_S8:
            vm.top_op().s8 = (bool)vm.top_op().s8;
            break;
        case IC_OPC_B_U8:
            vm.top_op().s8 = (bool)vm.top_op().u8;
            break;
        case IC_OPC_B_S32:
            vm.top_op().s8 = (bool)vm.top_op().s32;
            break;
        case IC_OPC_B_F32:
            vm.top_op().s8 = (bool)vm.top_op().f32;
            break;
        case IC_OPC_B_F64:
            vm.top_op().s8 = (bool)vm.top_op().f64;
            break;
        case IC_OPC_S8_U8:
            vm.top_op().s8 = vm.top_op().u8;
            break;
        case IC_OPC_S8_S32:
            vm.top_op().s8 = vm.top_op().s32;
            break;
        case IC_OPC_S8_F32:
            vm.top_op().s8 = vm.top_op().f32;
            break;
        case IC_OPC_S8_F64:
            vm.top_op().s8 = vm.top_op().f64;
            break;
        case IC_OPC_U8_S8:
            vm.top_op().u8 = vm.top_op().s8;
            break;
        case IC_OPC_U8_S32:
            vm.top_op().u8 = vm.top_op().s32;
            break;
        case IC_OPC_U8_F32:
            vm.top_op().u8 = vm.top_op().f32;
            break;
        case IC_OPC_U8_F64:
            vm.top_op().u8 = vm.top_op().f64;
            break;
        case IC_OPC_S32_S8:
            vm.top_op().s32 = vm.top_op().s8;
            break;
        case IC_OPC_S32_U8:
            vm.top_op().s32 = vm.top_op().u8;
            break;
        case IC_OPC_S32_F32:
            vm.top_op().s32 = vm.top_op().f32;
            break;
        case IC_OPC_S32_F64:
            vm.top_op().s32 = vm.top_op().f64;
            break;
        case IC_OPC_F32_S8:
            vm.top_op().f32 = vm.top_op().s8;
            break;
        case IC_OPC_F32_U8:
            vm.top_op().f32 = vm.top_op().u8;
            break;
        case IC_OPC_F32_S32:
            vm.top_op().f32 = vm.top_op().s32;
            break;
        case IC_OPC_F32_F64:
            vm.top_op().f32 = vm.top_op().f64;
            break;
        case IC_OPC_F64_S8:
            vm.top_op().f64 = vm.top_op().s8;
            break;
        case IC_OPC_F64_U8:
            vm.top_op().f64 = vm.top_op().u8;
            break;
        case IC_OPC_F64_S32:
            vm.top_op().f64 = vm.top_op().s32;
            break;
        case IC_OPC_F64_F32:
            vm.top_op().f64 = vm.top_op().f32;
            break;
        default:
            assert(false);
        }
    } // while
}
