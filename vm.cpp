#include "ic_impl.h"

#define IC_STACK_SIZE (1024 * 1024)

void ic_vm_init(ic_vm& vm)
{
    vm.stack = (ic_data*)malloc(IC_STACK_SIZE * sizeof(ic_data));
}

void ic_vm_free(ic_vm& vm)
{
    free(vm.stack);
}

void ic_vm::push()
{
    ++sp;
    assert(sp <= stack + IC_STACK_SIZE);
}

void ic_vm::push_many(int size)
{
    sp += size;
    assert(sp <= stack + IC_STACK_SIZE);
}

ic_data ic_vm::pop()
{
    --sp;
    return *sp;
}

void ic_vm::pop_many(int size)
{
    sp -= size;
}

ic_data& ic_vm::top()
{
    return *(sp - 1);
}

int ic_vm_run(ic_vm& _vm, ic_program& program)
{
    ic_vm vm = _vm; // 20% perf gain in visual studio; but there is no gain if a parameter is passed by value, why?
    memcpy(vm.stack, program.bytecode, program.strings_byte_size);
    // set global non-string data to 0
    memset(vm.stack + program.strings_byte_size, 0, program.global_data_size * sizeof(ic_data) - program.strings_byte_size);
    vm.sp = vm.stack + program.global_data_size;
    vm.push_many(3); // main() return value, bp, ip
    vm.top().pointer = nullptr; // set a return address, see IC_OPC_RETURN for an explanation
    vm.bp = vm.sp;
    vm.ip = program.bytecode + program.strings_byte_size;

    for(;;)
    {
        ic_opcode opcode = (ic_opcode)*vm.ip;
        ++vm.ip;

        switch (opcode)
        {
        case IC_OPC_PUSH_S8:
            vm.push();
            vm.top().s8 = *(char*)vm.ip;
            ++vm.ip;
            break;
        case IC_OPC_PUSH_S32:
            vm.push();
            vm.top().s32 = read_int(&vm.ip);
            break;
        case IC_OPC_PUSH_F32:
            vm.push();
            vm.top().f32 = read_float(&vm.ip);
            break;
        case IC_OPC_PUSH_F64:
            vm.push();
            vm.top().f64 = read_double(&vm.ip);
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
            vm.push_many(read_int(&vm.ip));
            break;
        }
        case IC_OPC_POP:
        {
            vm.pop();
            break;
        }
        case IC_OPC_POP_MANY:
        {
            int size = read_int(&vm.ip);
            vm.pop_many(size);
            break;
        }
        case IC_OPC_SWAP:
        {
            ic_data tmp = *(vm.sp - 2);
            *(vm.sp - 2) = vm.top();
            vm.top() = tmp;
            break;
        }
        case IC_OPC_MEMMOVE:
        {
            void* dst = (char*)vm.sp - read_int(&vm.ip);
            void* src = (char*)vm.sp - read_int(&vm.ip);
            int byte_size = read_int(&vm.ip);
            memmove(dst, src, byte_size);
            break;
        }
        case IC_OPC_CLONE:
        {
            vm.push();
            vm.top() = *(vm.sp - 2);
            break;
        }
        case IC_OPC_CALL:
        {
            int idx = read_int(&vm.ip);
            vm.push();
            vm.top().pointer = vm.bp;
            vm.push();
            vm.top().pointer = vm.ip;
            vm.bp = vm.sp;
            vm.ip = program.bytecode + idx;
            break;
        }
        case IC_OPC_CALL_HOST:
        {
            int idx = read_int(&vm.ip);
            ic_vm_function& function = program.functions[idx];
            ic_data* argv = vm.sp - function.param_size;
            ic_data* retv = argv - function.return_size;
            function.callback(argv, retv, function.host_data);
            break;
        }
        case IC_OPC_RETURN:
        {
            vm.sp = vm.bp;
            vm.ip = (unsigned char*)vm.pop().pointer;
            vm.bp = (ic_data*)vm.pop().pointer;

            if (!vm.ip)
                return vm.top().s32;
            break;
        }
        case IC_OPC_JUMP_TRUE:
        {
            int idx = read_int(&vm.ip);
            if(vm.pop().s8)
                vm.ip = program.bytecode + idx;
            break;
        }
        case IC_OPC_JUMP_FALSE:
        {
            int idx = read_int(&vm.ip);
            if(!vm.pop().s8)
                vm.ip = program.bytecode + idx;
            break;
        }
        case IC_OPC_JUMP:
        {
            int idx = read_int(&vm.ip);
            vm.ip = program.bytecode + idx;
            break;
        }
        case IC_LOGICAL_NOT:
        {
            vm.top().s8 = !vm.top().s8;
            break;
        }
        case IC_OPC_ADDRESS:
        {
            int byte_offset = read_int(&vm.ip);
            vm.push();
            vm.top().pointer = (char*)vm.bp + byte_offset;
            break;
        }
        case IC_OPC_ADDRESS_GLOBAL:
        {
            int byte_offset = read_int(&vm.ip);
            vm.push();
            vm.top().pointer = (char*)vm.stack + byte_offset;
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
            int byte_size = read_int(&vm.ip);
            int data_size = bytes_to_data_size(byte_size);
            memcpy(dst, vm.sp - data_size, byte_size);
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
            int byte_size = read_int(&vm.ip);
            int data_size = bytes_to_data_size(byte_size);
            vm.push_many(data_size);
            memcpy(vm.sp - data_size, ptr, byte_size);
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
            int type_byte_size = read_int(&vm.ip);
            assert(type_byte_size);
            void* rhs = vm.pop().pointer;
            vm.top().s32 = ((char*)vm.top().pointer - (char*)rhs) / type_byte_size;
            break;
        }
        case IC_OPC_ADD_PTR_S32:
        {
            int type_byte_size = read_int(&vm.ip);
            assert(type_byte_size);
            int bytes = vm.pop().s32 * type_byte_size;
            vm.top().pointer = (char*)vm.top().pointer + bytes;
            break;
        }
        case IC_OPC_SUB_PTR_S32:
        {
            int type_byte_size = read_int(&vm.ip);
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
