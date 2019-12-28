#include "ic_impl.h"

#define IC_STACK_SIZE (1024 * 1024)

struct ic_stack_frame
{
    int bp; // replace with actual pointer
    unsigned char* ip;
};

void ic_vm_init(ic_vm& vm)
{
    vm.stack = (ic_data*)malloc(IC_STACK_SIZE * sizeof(ic_data));
}

void ic_vm_free(ic_vm& vm)
{
    free(vm.stack);
}

void ic_vm::push(ic_data data)
{
    assert(stack_size < IC_STACK_SIZE);
    stack[stack_size] = data;
    stack_size += 1;
}

void ic_vm::push()
{
    assert(stack_size < IC_STACK_SIZE);
    stack_size += 1;
}

void ic_vm::push_many(int size)
{
    stack_size += size;
    assert(stack_size <= IC_STACK_SIZE);
}

ic_data ic_vm::pop()
{
    stack_size -= 1;
    return stack[stack_size];
}

void ic_vm::pop_many(int size)
{
    stack_size -= size;
}

ic_data& ic_vm::top()
{
    return stack[stack_size - 1];
}

ic_data* ic_vm::end()
{
    return stack + stack_size;
}

int ic_vm_run(ic_vm& vm, ic_program& program)
{
    memcpy(vm.stack, program.data, program.strings_byte_size);
    // todo, is memset 0 setting all values to 0? (e.g. is double with all bits zero 0?); clear global non string data
    memset(vm.stack + program.strings_byte_size, 0, program.global_data_size * sizeof(ic_data) - program.strings_byte_size);
    vm.stack_size = program.global_data_size; // this is important
    vm.push_many(3); // main() return value, bp, ip
    vm.top().pointer = nullptr; // set a return address, see IC_OPC_RETURN for a why
    ic_stack_frame frame;
    frame.bp = vm.stack_size;
    frame.ip = program.data + program.functions[0].data_idx;

    for(;;)
    {
        ic_opcode opcode = (ic_opcode)*frame.ip;
        ++frame.ip;

        switch (opcode)
        {
        case IC_OPC_PUSH_S8:
            vm.push();
            vm.top().s8 = *(char*)frame.ip;
            ++frame.ip;
            break;
        case IC_OPC_PUSH_S32:
            vm.push();
            vm.top().s32 = read_int(&frame.ip);
            break;
        case IC_OPC_PUSH_F32:
            vm.push();
            vm.top().f32 = read_float(&frame.ip);
            break;
        case IC_OPC_PUSH_F64:
            vm.push();
            vm.top().f64 = read_double(&frame.ip);
            break;
        case IC_OPC_PUSH_NULLPTR:
        {
            vm.push();
            vm.top().pointer = nullptr;
            break;
        }
        case IC_OPC_PUSH:
        {
            vm.push();
            break;
        }
        case IC_OPC_PUSH_MANY:
        {
            vm.push_many(read_int(&frame.ip));
            break;
        }
        case IC_OPC_POP:
        {
            vm.pop();
            break;
        }
        case IC_OPC_POP_MANY:
        {
            int size = read_int(&frame.ip);
            vm.pop_many(size);
            break;
        }
        case IC_OPC_SWAP:
        {
            ic_data top = vm.top();
            ic_data* second = vm.end() - 2;
            vm.top() = *second;
            *second = top;
            break;
        }
        case IC_OPC_MEMMOVE:
        {
            void* dst = (char*)vm.end() - read_int(&frame.ip);
            void* src = (char*)vm.end() - read_int(&frame.ip);
            int byte_size = read_int(&frame.ip);
            memmove(dst, src, byte_size);
            break;
        }
        case IC_OPC_CLONE:
        {
            vm.push(vm.top());
            break;
        }
        case IC_OPC_CALL:
        {
            int fun_idx = read_int(&frame.ip);
            ic_vm_function& function = program.functions[fun_idx];

            if (function.host_impl)
            {
                ic_data* argv = vm.end() - function.param_size;
                ic_data* retv = argv - function.return_size;
                function.callback(argv, retv, function.host_data);
            }
            else
            {
                vm.push();
                vm.top().s32 = frame.bp;
                vm.push();
                vm.top().pointer = frame.ip;
                frame.bp = vm.stack_size;
                frame.ip = program.data + function.data_idx;
            }
            break;
        }
        case IC_OPC_RETURN:
        {
            vm.stack_size = frame.bp;
            frame.ip = (unsigned char*)vm.pop().pointer;
            frame.bp = vm.pop().s32;

            if (!frame.ip)
                return vm.top().s32;
            break;
        }
        case IC_OPC_JUMP_TRUE:
        {
            int idx = read_int(&frame.ip);
            if(vm.pop().s8)
                frame.ip = program.data + idx;
            break;
        }
        case IC_OPC_JUMP_FALSE:
        {
            int idx = read_int(&frame.ip);
            if(!vm.pop().s8)
                frame.ip = program.data + idx;
            break;
        }
        case IC_OPC_JUMP:
        {
            int idx = read_int(&frame.ip);
            frame.ip = program.data + idx;
            break;
        }
        case IC_LOGICAL_NOT:
        {
            vm.top().s8 = !vm.top().s8;
            break;
        }
        case IC_OPC_ADDRESS:
        {
            int byte_offset = read_int(&frame.ip);
            ic_data* base_addr = vm.stack + frame.bp;
            void* addr = (char*)base_addr + byte_offset;
            vm.push();
            vm.top().pointer = addr;
            break;
        }
        case IC_OPC_ADDRESS_GLOBAL:
        {
            int byte_offset = read_int(&frame.ip);
            void* addr = (char*)vm.stack + byte_offset;
            assert(addr >= vm.stack && addr < vm.stack + program.global_data_size);
            vm.push();
            vm.top().pointer = addr;
            break;
        }
        case IC_OPC_STORE_1:
        {
            void* ptr = vm.pop().pointer;
            *(char*)ptr = vm.top().s8;
            break;
        }
        case IC_OPC_STORE_4:
        {
            void* ptr = vm.pop().pointer;
            *(int*)ptr = vm.top().s32;
            break;
        }
        case IC_OPC_STORE_8:
        {
            void* ptr = vm.pop().pointer;
            *(double*)ptr = vm.top().f64;
            break;
        }
        case IC_OPC_STORE_STRUCT:
        {
            void* dst = vm.pop().pointer;
            int byte_size = read_int(&frame.ip);
            int data_size = bytes_to_data_size(byte_size);
            memcpy(dst, vm.end() - data_size, byte_size);
            break;
        }
        case IC_OPC_LOAD_1:
        {
            void* ptr = vm.top().pointer;
            vm.top().s8 = *(char*)ptr;
            break;
        }
        case IC_OPC_LOAD_4:
        {
            void* ptr = vm.top().pointer;
            vm.top().s32 = *(int*)ptr;
            break;
        }
        case IC_OPC_LOAD_8:
        {
            void* ptr = vm.top().pointer;
            vm.top().f64 = *(double*)ptr;
            break;
        }
        case IC_OPC_LOAD_STRUCT:
        {
            void* ptr = vm.pop().pointer;
            int byte_size = read_int(&frame.ip);
            int data_size = bytes_to_data_size(byte_size);
            vm.push_many(data_size);
            memcpy(vm.end() - data_size, ptr, byte_size);
            break;
        }
        case IC_OPC_COMPARE_E_S32:
        {
            int rhs = vm.pop().s32;
            vm.top().s8 = vm.top().s32 == rhs;
            break;
        }
        case IC_OPC_COMPARE_NE_S32:
        {
            int rhs = vm.pop().s32;
            vm.top().s8 = vm.top().s32 != rhs;
            break;
        }
        case IC_OPC_COMPARE_G_S32:
        {
            int rhs = vm.pop().s32;
            vm.top().s8 = vm.top().s32 > rhs;
            break;
        }
        case IC_OPC_COMPARE_GE_S32:
        {
            int rhs = vm.pop().s32;
            vm.top().s8 = vm.top().s32 >= rhs;
            break;
        }
        case IC_OPC_COMPARE_L_S32:
        {
            int rhs = vm.pop().s32;
            vm.top().s8 = vm.top().s32 < rhs;
            break;
        }
        case IC_OPC_COMPARE_LE_S32:
        {
            int rhs = vm.pop().s32;
            vm.top().s8 = vm.top().s32 <= rhs;
            break;
        }
        case IC_OPC_NEGATE_S32:
        {
            vm.top().s32 = -vm.top().s32;
            break;
        }
        case IC_OPC_ADD_S32:
        {
            int rhs = vm.pop().s32;
            vm.top().s32 += rhs;
            break;
        }
        case IC_OPC_SUB_S32:
        {
            int rhs = vm.pop().s32;
            vm.top().s32 -= rhs;
            break;
        }
        case IC_OPC_MUL_S32:
        {
            int rhs = vm.pop().s32;
            vm.top().s32 *= rhs;
            break;
        }
        case IC_OPC_DIV_S32:
        {
            int rhs = vm.pop().s32;
            vm.top().s32 /= rhs;
            break;
        }
        case IC_OPC_MODULO_S32:
        {
            int rhs = vm.pop().s32;
            vm.top().s32 = vm.top().s32 % rhs;
            break;
        }
        case IC_OPC_COMPARE_E_F32:
        {
            float rhs = vm.pop().f32;
            vm.top().s8 = vm.top().f32 == rhs;
            break;
        }
        case IC_OPC_COMPARE_NE_F32:
        {
            float rhs = vm.pop().f32;
            vm.top().s8 = vm.top().f32 != rhs;
            break;
        }
        case IC_OPC_COMPARE_G_F32:
        {
            float rhs = vm.pop().f32;
            vm.top().s8 = vm.top().f32 > rhs;
            break;
        }
        case IC_OPC_COMPARE_GE_F32:
        {
            float rhs = vm.pop().f32;
            vm.top().s8 = vm.top().f32 >= rhs;
            break;
        }
        case IC_OPC_COMPARE_L_F32:
        {
            float rhs = vm.pop().f32;
            vm.top().s8 = vm.top().f32 < rhs;
            break;
        }
        case IC_OPC_COMPARE_LE_F32:
        {
            float rhs = vm.pop().f32;
            vm.top().s8 = vm.top().f32 <= rhs;
            break;
        }
        case IC_OPC_NEGATE_F32:
        {
            vm.top().f32 = -vm.top().f32;
            break;
        }
        case IC_OPC_ADD_F32:
        {
            float rhs = vm.pop().f32;
            vm.top().f32 += rhs;
            break;
        }
        case IC_OPC_SUB_F32:
        {
            float rhs = vm.pop().f32;
            vm.top().f32 -= rhs;
            break;
        }
        case IC_OPC_MUL_F32:
        {
            float rhs = vm.pop().f32;
            vm.top().f32 *= rhs;
            break;
        }
        case IC_OPC_DIV_F32:
        {
            float rhs = vm.pop().f32;
            vm.top().f32 /= rhs;
            break;
        }
        case IC_OPC_COMPARE_E_F64:
        {
            double rhs = vm.pop().f64;
            vm.top().s8 = vm.top().f64 == rhs;
            break;
        }
        case IC_OPC_COMPARE_NE_F64:
        {
            double rhs = vm.pop().f64;
            vm.top().s8 = vm.top().f64 != rhs;
            break;
        }
        case IC_OPC_COMPARE_G_F64:
        {
            double rhs = vm.pop().f64;
            vm.top().s8 = vm.top().f64 > rhs;
            break;
        }
        case IC_OPC_COMPARE_GE_F64:
        {
            double rhs = vm.pop().f64;
            vm.top().s8 = vm.top().f64 >= rhs;
            break;
        }
        case IC_OPC_COMPARE_L_F64:
        {
            double rhs = vm.pop().f64;
            vm.top().s8 = vm.top().f64 < rhs;
            break;
        }
        case IC_OPC_COMPARE_LE_F64:
        {
            double rhs = vm.pop().f64;
            vm.top().s8 = vm.top().f64 <= rhs;
            break;
        }
        case IC_OPC_NEGATE_F64:
        {
            vm.top().f64 = -vm.top().f64;
            break;
        }
        case IC_OPC_ADD_F64:
        {
            double rhs = vm.pop().f64;
            vm.top().f64 += rhs;
            break;
        }
        case IC_OPC_SUB_F64:
        {
            double rhs = vm.pop().f64;
            vm.top().f64 -= rhs;
            break;
        }
        case IC_OPC_MUL_F64:
        {
            double rhs = vm.pop().f64;
            vm.top().f64 *= rhs;
            break;
        }
        case IC_OPC_DIV_F64:
        {
            double rhs = vm.pop().f64;
            vm.top().f64 /= rhs;
            break;
        }
        case IC_OPC_COMPARE_E_PTR:
        {
            void* rhs = vm.pop().pointer;
            vm.top().s8 = vm.top().pointer == rhs;
            break;
        }
        case IC_OPC_COMPARE_NE_PTR:
        {
            void* rhs = vm.pop().pointer;
            vm.top().s8 = vm.top().pointer != rhs;
            break;
        }
        case IC_OPC_COMPARE_G_PTR:
        {
            void* rhs = vm.pop().pointer;
            vm.top().s8 = vm.top().pointer > rhs;
            break;
        }
        case IC_OPC_COMPARE_GE_PTR:
        {
            void* rhs = vm.pop().pointer;
            vm.top().s8 = vm.top().pointer >= rhs;
            break;
        }
        case IC_OPC_COMPARE_L_PTR:
        {
            void* rhs = vm.pop().pointer;
            vm.top().s8 = vm.top().pointer < rhs;
            break;
        }
        case IC_OPC_COMPARE_LE_PTR:
        {
            void* rhs = vm.pop().pointer;
            vm.top().s8 = vm.top().pointer <= rhs;
            break;
        }
        case IC_OPC_SUB_PTR_PTR:
        {
            int type_byte_size = read_int(&frame.ip);
            assert(type_byte_size);
            void* rhs = vm.pop().pointer;
            vm.top().s32 = ((char*)vm.top().pointer - (char*)rhs) / type_byte_size;
            break;
        }
        case IC_OPC_ADD_PTR_S32:
        {
            int type_byte_size = read_int(&frame.ip);
            assert(type_byte_size);
            int bytes = vm.pop().s32 * type_byte_size;
            vm.top().pointer = (char*)vm.top().pointer + bytes;
            break;
        }
        case IC_OPC_SUB_PTR_S32:
        {
            int type_byte_size = read_int(&frame.ip);
            assert(type_byte_size);
            int bytes = vm.pop().s32 * type_byte_size;
            vm.top().pointer = (char*)vm.top().pointer - bytes;
            break;
        }
        case IC_OPC_B_S8:
            vm.top().s8 = (bool)vm.top().s8;
            break;
        case IC_OPC_B_U8:
            vm.top().s8 = (bool)vm.top().u8;
            break;
        case IC_OPC_B_S32:
            vm.top().s8 = (bool)vm.top().s32;
            break;
        case IC_OPC_B_F32:
            vm.top().s8 = (bool)vm.top().f32;
            break;
        case IC_OPC_B_F64:
            vm.top().s8 = (bool)vm.top().f64;
            break;
        case IC_OPC_B_PTR:
            vm.top().s8 = (bool)vm.top().pointer;
            break;
        case IC_OPC_S8_U8:
            vm.top().s8 = vm.top().u8;
            break;
        case IC_OPC_S8_S32:
            vm.top().s8 = vm.top().s32;
            break;
        case IC_OPC_S8_F32:
            vm.top().s8 = vm.top().f32;
            break;
        case IC_OPC_S8_F64:
            vm.top().s8 = vm.top().f64;
            break;
        case IC_OPC_U8_S8:
            vm.top().u8 = vm.top().s8;
            break;
        case IC_OPC_U8_S32:
            vm.top().u8 = vm.top().s32;
            break;
        case IC_OPC_U8_F32:
            vm.top().u8 = vm.top().f32;
            break;
        case IC_OPC_U8_F64:
            vm.top().u8 = vm.top().f64;
            break;
        case IC_OPC_S32_S8:
            vm.top().s32 = vm.top().s8;
            break;
        case IC_OPC_S32_U8:
            vm.top().s32 = vm.top().u8;
            break;
        case IC_OPC_S32_F32:
            vm.top().s32 = vm.top().f32;
            break;
        case IC_OPC_S32_F64:
            vm.top().s32 = vm.top().f64;
            break;
        case IC_OPC_F32_S8:
            vm.top().f32 = vm.top().s8;
            break;
        case IC_OPC_F32_U8:
            vm.top().f32 = vm.top().u8;
            break;
        case IC_OPC_F32_S32:
            vm.top().f32 = vm.top().s32;
            break;
        case IC_OPC_F32_F64:
            vm.top().f32 = vm.top().f64;
            break;
        case IC_OPC_F64_S8:
            vm.top().f64 = vm.top().s8;
            break;
        case IC_OPC_F64_U8:
            vm.top().f64 = vm.top().u8;
            break;
        case IC_OPC_F64_S32:
            vm.top().f64 = vm.top().s32;
            break;
        case IC_OPC_F64_F32:
            vm.top().f64 = vm.top().f32;
            break;
        default:
            assert(false);
        }
    } // while
}
