#pragma once
#include <vector> // todo

enum ic_lib_type
{
    IC_LIB_CORE = 1 << 0,
};

// todo, rename to _char, _uchar, _int, _ptr etc.
union ic_data
{
    char s8;
    unsigned char u8;
    int s32;
    float f32;
    double f64;
    void* pointer;
};

// add pointer to void*, so host can change its internal state without using global variables
using ic_host_function_ptr = ic_data(*)(ic_data* argv);

struct ic_host_function
{
    const char* declaration; // todo not declaration but prototype
    ic_host_function_ptr callback;
    // void* data
};

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
            int data_idx;
            int stack_size;
        };
        struct
        {
            ic_host_function_ptr callback;
            unsigned int hash;
            int lib;
            // data
        };
    };
};

struct ic_program
{
    ic_vm_function* functions;
    unsigned char* data;
    int functions_size;
    int strings_byte_size;
    int data_size;
    int global_data_size;
};

struct ic_stack_frame
{
    int size;
    int bp;
    unsigned char* ip;
};

struct ic_vm
{
    ic_stack_frame* stack_frames;
    ic_data* call_stack;
    ic_data* operand_stack;
    int stack_frames_size;
    int call_stack_size;
    int operand_stack_size;

    void push_stack_frame(unsigned char* bytecode, int stack_size);
    void pop_stack_frame();
    ic_stack_frame& top_frame();
    void push_op(ic_data data);
    void push_op();
    void push_op_many(int size);
    ic_data pop_op();
    void pop_op_many(int size);
    ic_data& top_op();
    ic_data* end_op();
};

// todo, polish these functions
ic_program load_program(unsigned char* buf);
// read entire struct and not member by member
void serialize_program(std::vector<unsigned char>& buf, ic_program& program); // remove vector
void disassmble(ic_program& program);
bool compile_to_bytecode(const char* source, ic_program* program, int libs, ic_host_function* host_functions, int host_functions_size);
void vm_init(ic_vm& vm, ic_program& program, ic_host_function* host_functions, int host_functions_size); // if vm runs only one program can be done once before many runs
void vm_run(ic_vm& vm, ic_program& program);