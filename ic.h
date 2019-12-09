#pragma once
#include <vector>

enum ic_lib_type
{
    IC_LIB_CORE = 1 << 0,
    // reserved
    IC_LIB_USER = 1 << 30,
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
            int bytecode_idx;
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
    // todo, combine strings with bytecode
    ic_vm_function* functions;
    unsigned char* bytecode;
    char* strings;
    int functions_size;
    int strings_byte_size;
    int bytecode_size;
    int global_data_size; // this does not need to be stored
};

struct ic_stack_frame
{
    int size;
    int bp;
    unsigned char* ip;
};

struct ic_vm
{
    std::vector<ic_stack_frame> stack_frames; // I don't like to pull vector in api header, strechy buffer might be good for this
    // stack_frame is now so small that we can allocate just one buffer for it with bounds checking
    ic_data* call_stack; // must not be invalidated during execution
    ic_data* operand_stack;
    int call_stack_size;
    int operand_stack_size;

    // define these functions outside of vm to not polute api
    void push_stack_frame(unsigned char* bytecode, int stack_size);
    void pop_stack_frame();
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
void serialize_program(std::vector<unsigned char>& buf, ic_program& program);
void disassmble(ic_program& program);
bool compile_to_bytecode(const char* source, ic_program* program, int libs, ic_host_function* host_functions, int host_functions_size);
void vm_init(ic_vm& vm, ic_program& program, ic_host_function* host_functions, int host_functions_size); // if vm runs only one program can be done once before many runs
void vm_run(ic_vm& vm, ic_program& program);