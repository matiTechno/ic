#include "ic.h"

void ic_vm::push_stack_frame(unsigned char* bytecode, int stack_size, int return_size)
{
    ic_stack_frame frame;
    frame.prev_operand_stack_size = operand_stack_size;
    frame.bp = call_stack + call_stack_size;
    frame.ip = bytecode;
    frame.bytecode = bytecode;
    frame.size = stack_size;
    frame.return_size = return_size;
    stack_frames.push_back(frame);

    call_stack_size += stack_size;
    assert(call_stack_size <= IC_CALL_STACK_SIZE);
}

void ic_vm::pop_stack_frame()
{
    int prev_operand_stack_size = stack_frames.back().prev_operand_stack_size;
    int return_size = stack_frames.back().return_size;
    void* dst = operand_stack + prev_operand_stack_size;
    void* src = end_op() - return_size;
    memmove(dst, src, return_size * sizeof(ic_data));
    operand_stack_size = prev_operand_stack_size + return_size;
    call_stack_size -= stack_frames.back().size;
    stack_frames.pop_back();
}

int read_s32_operand(unsigned char** ip)
{
    int operand;
    memcpy(&operand, *ip, sizeof(int));
    *ip += sizeof(int);
    return operand;
}

void run_bytecode(ic_vm& vm)
{
    assert(vm.stack_frames.size() == 1);
    ic_stack_frame* frame = &vm.stack_frames[0];

    for(;;)
    {
        ic_opcode opcode = (ic_opcode)*frame->ip;
        ++frame->ip;

        switch (opcode)
        {
        case IC_OPC_PUSH_S8:
        {
            vm.push_op();
            vm.top_op().s8 = *(char*)frame->ip;
            frame->ip += 1;
            break;
        }
        case IC_OPC_PUSH_U8:
        {
            vm.push_op();
            vm.top_op().u8 = *frame->ip;
            frame->ip += 1;
            break;
        }
        case IC_OPC_PUSH_S32:
        case IC_OPC_PUSH_F32:
        {
            vm.push_op(); // ic_data is 8 bytes so one is enough to hold any immediate operand
            memcpy(vm.end_op() - 1, frame->ip, 4); // implicit conversions or casting operator violate alignment rules
            frame->ip += 4;
            break;
        }
        case IC_OPC_PUSH_F64:
        {
            vm.push_op();
            memcpy(vm.end_op() - 1, frame->ip, 8);
            frame->ip += 8;
            break;
        }
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
            int size = read_s32_operand(&frame->ip);
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
            void* dst = vm.end_op() - read_s32_operand(&frame->ip);
            void* src = vm.end_op() - read_s32_operand(&frame->ip);
            int size = read_s32_operand(&frame->ip);
            memmove(dst, src, size * sizeof(ic_data));
            break;
        }
        case IC_OPC_CLONE:
        {
            vm.push_op(vm.top_op());
            break;
        }
        case IC_OPC_CALL:
        {
            int fun_idx = read_s32_operand(&frame->ip);
            ic_function& function = vm.functions[fun_idx];
            int param_size = function.param_size;

            if (function.type == IC_FUN_HOST)
            {
                ic_data return_data = function.callback(vm.end_op() - param_size);
                vm.pop_op_many(function.param_size);

                if (function.return_size)
                    vm.push_op(return_data);
            }
            else
            {
                vm.push_stack_frame(function.bytecode, function.stack_size, function.return_size);
                frame = &vm.stack_frames.back();
                memcpy(frame->bp, vm.end_op() - param_size, param_size * sizeof(ic_data));
                frame->prev_operand_stack_size -= param_size;
                vm.pop_op_many(param_size);
            }
            break;
        }
        case IC_OPC_RETURN:
        {
            vm.pop_stack_frame();
            if (!vm.stack_frames.size())
                return;
            frame = &vm.stack_frames.back();
            break;
        }
        case IC_OPC_JUMP_TRUE:
        {
            int idx = read_s32_operand(&frame->ip);
            if(vm.pop_op().s32)
                frame->ip = frame->bytecode + idx;
            break;
        }
        case IC_OPC_JUMP_FALSE:
        {
            int idx = read_s32_operand(&frame->ip);
            if(!vm.pop_op().s32)
                frame->ip = frame->bytecode + idx;
            break;
        }
        case IC_OPC_JUMP:
        {
            int idx = read_s32_operand(&frame->ip);
            frame->ip = frame->bytecode + idx;
            break;
        }
        case IC_OPC_ADDRESS:
        {
            int idx = read_s32_operand(&frame->ip);
            void* addr = frame->bp + idx;
            assert(addr >= frame->bp && addr < vm.call_stack + vm.call_stack_size);
            vm.push_op();
            vm.top_op().pointer = addr;
            break;
        }
        case IC_OPC_ADDRESS_GLOBAL:
        {
            int idx = read_s32_operand(&frame->ip);
            void* addr = vm.call_stack + idx;
            assert(addr >= 0 && addr < vm.stack_frames[0].bp);
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
            int size = read_s32_operand(&frame->ip);
            memcpy(dst, vm.end_op() - size, size * sizeof(ic_data));
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
            int size = read_s32_operand(&frame->ip);
            vm.push_op_many(size);
            memcpy(vm.end_op() - size, ptr, size * sizeof(ic_data));
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
        case IC_OPC_ADD_PTR_S32:
        {
            int type_byte_size = read_s32_operand(&frame->ip);
            int bytes = vm.pop_op().s32 * type_byte_size;
            vm.top_op().pointer = (char*)vm.top_op().pointer + bytes;
            break;
        }
        case IC_OPC_SUB_PTR_S32:
        {
            int type_byte_size = read_s32_operand(&frame->ip);
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
