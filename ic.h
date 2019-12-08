#pragma once
#include <vector>
#include <assert.h> // tod, remove

enum ic_lib_type
{
    IC_LIB_CORE = 1 << 0,
    // reserved
    IC_LIB_USER = 1 << 30,
};

union ic_data
{
    char s8;
    unsigned char u8;
    int s32;
    float f32;
    double f64;
    void* pointer;
};

// todo, remove
#define IC_CALL_STACK_SIZE (1024 * 1024)
#define IC_OPERAND_STACK_SIZE 1024

// add pointer to void*, so host can change its internal state without using global variables
using ic_host_function_ptr = ic_data(*)(ic_data* argv);

// todo, some better naming would be nice (ic_function, and current ic_function rename to ic_function_desc)
struct ic_vm_function
{
    bool host_impl;
    int return_size;
    int param_size;

    union
    {
        struct
        {
            unsigned char* bytecode;
            int bytecode_size;
            int stack_size;
        };
        struct
        {
            ic_host_function_ptr callback;
            unsigned int hash;
            int lib; // rename to origin?
        };
    };
};

// todo keep one buffer for all functions bytecode
struct ic_program
{
    ic_vm_function* functions;
    int functions_size;
    char* strings;
    int strings_byte_size;
    int global_data_size;
};

struct ic_stack_frame
{
    int size;
    int return_size;
    int prev_operand_stack_size;
    ic_data* bp; // base pointer
    unsigned char* ip; // instruction pointer
    unsigned char* bytecode; // this is needed for jumps
};

struct ic_vm
{
    // important, ic_data buffers must not be invalidated during execution
    std::vector<ic_stack_frame> stack_frames;
    ic_data* call_stack;
    ic_data* operand_stack;
    int call_stack_size;
    int operand_stack_size;

    void push_stack_frame(unsigned char* bytecode, int stack_size, int return_size);
    void pop_stack_frame();

    void push_op(ic_data data)
    {
        assert(operand_stack_size < IC_OPERAND_STACK_SIZE);
        operand_stack[operand_stack_size] = data;
        operand_stack_size += 1;
    }

    void push_op()
    {
        assert(operand_stack_size < IC_OPERAND_STACK_SIZE);
        operand_stack_size += 1;
    }

    void push_op_many(int size)
    {
        operand_stack_size += size;
        assert(operand_stack_size <= IC_OPERAND_STACK_SIZE);
    }

    ic_data pop_op()
    {
        operand_stack_size -= 1;
        return operand_stack[operand_stack_size];
    }

    void pop_op_many(int size)
    {
        operand_stack_size -= size;
    }

    ic_data& top_op()
    {
        return operand_stack[operand_stack_size - 1];
    }

    ic_data* end_op()
    {
        return operand_stack + operand_stack_size;
    }
};

// todo, rename to ic_host_declaration
struct ic_host_function
{
    const char* declaration;
    ic_host_function_ptr callback;
};

ic_program load_program(unsigned char* buf);
void serialize_program(std::vector<unsigned char>& buf, ic_program& program);
void disassmble(ic_program& program);
bool compile_to_bytecode(const char* source, ic_program* program, int libs, ic_host_function* host_functions, int host_functions_size);
void vm_init(ic_vm& vm, ic_program& program, ic_host_function* host_functions, int host_functions_size); // if vm runs only one program can be done once before many runs
void vm_run(ic_vm& vm, ic_program& program);