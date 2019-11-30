#include "ic.h"

void ic_vm::push_stack_frame(ic_inst* bytecode, int stack_size, int return_size)
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

void run_bytecode(ic_vm& vm)
{
    assert(vm.stack_frames.size() == 1);
    ic_stack_frame* frame = &vm.stack_frames[0];

    while (vm.stack_frames.size())
    {
        ic_inst inst = *frame->ip;
        ++frame->ip;

        switch (inst.opcode)
        {
        case IC_OPC_PUSH:
        {
            vm.push_op(inst.operand.push_data);
            break;
        }
        case IC_OPC_CALL:
        {
            ic_vm_function& function = vm.functions[inst.operand.idx];
            int param_size = function.param_size;

            if (function.is_host)
            {
                ic_data return_data = function.host_callback(vm.end_op() - param_size);
                vm.pop_op_many(function.param_size);

                if (function.return_size)
                    vm.push_op(return_data);
            }
            else
            {
                vm.push_stack_frame(function.source.bytecode, function.source.stack_size, function.return_size);
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
            frame = &vm.stack_frames.back();
            break;
        }
        case IC_OPC_LOGICAL_NOT:
        {
            vm.top_op().s8 = !vm.top_op().s8;
        }
        case IC_OPC_JUMP_TRUE:
        {
            if(vm.pop_op().s8)
                frame->ip = frame->bytecode + inst.operand.idx;

            break;
        }
        case IC_OPC_JUMP_FALSE:
        {
            if(!vm.pop_op().s8)
                frame->ip = frame->bytecode + inst.operand.idx;

            break;
        }
        case IC_OPC_JUMP:
        {
            frame->ip = frame->bytecode + inst.operand.idx;
            break;
        }
        case IC_OPC_ADDRESS_OF:
        {
            vm.push_op();
            vm.top_op().pointer = &frame->bp[inst.operand.idx];
            break;
        }
        case IC_OPC_ADDRESS_OF_GLOBAL:
        {
            vm.push_op();
            vm.top_op().pointer = &vm.global_data[inst.operand.idx];
            break;
        }
        case IC_OPC_LOAD:
        {
            vm.push_op(frame->bp[inst.operand.load.idx]);
            break;
        }
        case IC_OPC_LOAD_GLOBAL:
        {
            vm.push_op(vm.global_data[inst.operand.load.idx]);
            break;
        }
        case IC_OPC_LOAD_STRUCT:
        {
            void* dst = vm.end_op();
            int size = inst.operand.load.size;
            vm.push_op_many(size);
            memcpy(dst, &frame->bp[inst.operand.load.idx], size * sizeof(ic_data));
        }
        case IC_OPC_LOAD_STRUCT_GLOBAL:
        {
            void* dst = vm.end_op();
            int size = inst.operand.load.size;
            vm.push_op_many(size);
            memcpy(dst, &vm.global_data[inst.operand.load.idx], size * sizeof(ic_data));
        }
        case IC_OPC_STORE:
        {
            frame->bp[inst.operand.store.idx] = vm.top_op();
            break;
        }
        case IC_OPC_STORE_GLOBAL:
        {
            vm.global_data[inst.operand.store.idx] = vm.top_op();
            break;
        }
        case IC_OPC_STORE_STRUCT:
        {
            int size = inst.operand.store.size;
            memcpy(&frame->bp[inst.operand.store.idx], vm.end_op() - size, size * sizeof(ic_data));
            break;
        }
        case IC_OPC_STORE_STRUCT_GLOBAL:
        {
            int size = inst.operand.store.size;
            memcpy(&vm.global_data[inst.operand.store.idx], vm.end_op() - size, size * sizeof(ic_data));
            break;
        }
        case IC_OPC_STORE_1_AT:
        {
            void* ptr = vm.pop_op().pointer;
            *(char*)ptr = vm.top_op().s8;
            break;
        }
        case IC_OPC_STORE_4_AT:
        {
            void* ptr = vm.pop_op().pointer;
            *(int*)ptr = vm.top_op().s32;
            break;
        }
        case IC_OPC_STORE_8_AT:
        {
            void* ptr = vm.pop_op().pointer;
            *(double*)ptr = vm.top_op().f64;
            break;
        }
        case IC_OPC_STORE_STRUCT_AT:
        {
            void* dst = vm.pop_op().pointer;
            int size = inst.operand.size;
            memcpy(dst, vm.end_op() - size, size * sizeof(ic_data));
            break;
        }
        case IC_OPC_DEREFERENCE_1:
        {
            void* ptr = vm.top_op().pointer;
            vm.top_op().s8 = *(char*)ptr;
            break;
        }
        case IC_OPC_DEREFERENCE_4:
        {
            void* ptr = vm.top_op().pointer;
            vm.top_op().s32 = *(int*)ptr;
            break;
        }
        case IC_OPC_DEREFERENCE_8:
        {
            void* ptr = vm.top_op().pointer;
            vm.top_op().f64 = *(double*)ptr;
            break;
        }
        case IC_OPC_DEREFERENCE_STRUCT:
        {
            void* ptr = vm.pop_op().pointer;
            int size = inst.operand.size;
            vm.push_op_many(size);
            memcpy(vm.end_op() - size, ptr, size * sizeof(ic_data));
            break;
        }
        case IC_OPC_COPMARE_E_S32:
        {
            int rhs = vm.pop_op().s32;
            vm.top_op().s8 = vm.top_op().s32 == rhs;
            break;
        }
        case IC_OPC_COPMARE_NE_S32:
        {
            int rhs = vm.pop_op().s32;
            vm.top_op().s8 = vm.top_op().s32 != rhs;
            break;
        }
        case IC_OPC_COPMARE_G_S32:
        {
            int rhs = vm.pop_op().s32;
            vm.top_op().s8 = vm.top_op().s32 > rhs;
            break;
        }
        case IC_OPC_COPMARE_GE_S32:
        {
            int rhs = vm.pop_op().s32;
            vm.top_op().s8 = vm.top_op().s32 >= rhs;
            break;
        }
        case IC_OPC_COPMARE_L_S32:
        {
            int rhs = vm.pop_op().s32;
            vm.top_op().s8 = vm.top_op().s32 < rhs;
            break;
        }
        case IC_OPC_COPMARE_LE_S32:
        {
            int rhs = vm.pop_op().s32;
            vm.top_op().s8 = vm.top_op().s32 <= rhs;
            break;
        }
        case IC_OPC_NEGATE_S32:
        {
            vm.top_op().s32 = !vm.top_op().s32;
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
        case IC_OPC_COPMARE_E_F32:
        {
            float rhs = vm.pop_op().f32;
            vm.top_op().s8 = vm.top_op().f32 == rhs;
            break;
        }
        case IC_OPC_COPMARE_NE_F32:
        {
            float rhs = vm.pop_op().f32;
            vm.top_op().s8 = vm.top_op().f32 != rhs;
            break;
        }
        case IC_OPC_COPMARE_G_F32:
        {
            float rhs = vm.pop_op().f32;
            vm.top_op().s8 = vm.top_op().f32 > rhs;
            break;
        }
        case IC_OPC_COPMARE_GE_F32:
        {
            float rhs = vm.pop_op().f32;
            vm.top_op().s8 = vm.top_op().f32 >= rhs;
            break;
        }
        case IC_OPC_COPMARE_L_F32:
        {
            float rhs = vm.pop_op().f32;
            vm.top_op().s8 = vm.top_op().f32 < rhs;
            break;
        }
        case IC_OPC_COPMARE_LE_F32:
        {
            float rhs = vm.pop_op().f32;
            vm.top_op().s8 = vm.top_op().f32 <= rhs;
            break;
        }
        case IC_OPC_NEGATE_F32:
        {
            vm.top_op().f32 = !vm.top_op().f32;
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
        case IC_OPC_COPMARE_E_F64:
        {
            double rhs = vm.pop_op().f64;
            vm.top_op().s8 = vm.top_op().f64 == rhs;
            break;
        }
        case IC_OPC_COPMARE_NE_F64:
        {
            double rhs = vm.pop_op().f64;
            vm.top_op().s8 = vm.top_op().f64 != rhs;
            break;
        }
        case IC_OPC_COPMARE_G_F64:
        {
            double rhs = vm.pop_op().f64;
            vm.top_op().s8 = vm.top_op().f64 > rhs;
            break;
        }
        case IC_OPC_COPMARE_GE_F64:
        {
            double rhs = vm.pop_op().f64;
            vm.top_op().s8 = vm.top_op().f64 >= rhs;
            break;
        }
        case IC_OPC_COPMARE_L_F64:
        {
            double rhs = vm.pop_op().f64;
            vm.top_op().s8 = vm.top_op().f64 < rhs;
            break;
        }
        case IC_OPC_COPMARE_LE_F64:
        {
            double rhs = vm.pop_op().f64;
            vm.top_op().s8 = vm.top_op().f64 <= rhs;
            break;
        }
        case IC_OPC_NEGATE_F64:
        {
            vm.top_op().f64 = !vm.top_op().f64;
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
        case IC_OPC_DIV_f64:
        {
            double rhs = vm.pop_op().f64;
            vm.top_op().f64 /= rhs;
            break;
        }
        case IC_OPC_ADD_PTR_S32:
        {
            int bytes = vm.pop_op().s32 * inst.operand.size;
            vm.top_op().pointer = (char*)vm.top_op().pointer + bytes;
            break;
        }
        case IC_OPC_SUB_PTR_S32:
        {
            int bytes = vm.pop_op().s32 * inst.operand.size;
            vm.top_op().pointer = (char*)vm.top_op().pointer - bytes;
            break;
        }
        case IC_OPC_MODULO_S32:
        {
            int rhs = vm.pop_op().s32;
            vm.top_op().s32 = vm.top_op().s32 % rhs;
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
