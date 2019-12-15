#pragma once

enum ic_lib_type
{
    IC_LIB_CORE = 1 << 0,
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

using ic_host_function_ptr = ic_data(*)(ic_data* argv, void* host_data);

struct ic_host_function
{
    const char* prototype_str;
    ic_host_function_ptr callback;
    void* host_data;
};

struct ic_vm_function
{
    bool host_impl;
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
            void* host_data;
            unsigned int hash;
            int origin;
            bool returns_value;
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

bool ic_program_init_compile(ic_program& program, const char* source, int libs, ic_host_function* host_functions);
void ic_program_init_load(ic_program& program, unsigned char* buf, int libs, ic_host_function* host_functions);
void ic_program_free(ic_program& program);
void ic_program_print_disassembly(ic_program& program);
void ic_program_serialize(ic_program& program, unsigned char*& buf, int& size);
void ic_buf_free(unsigned char* buf);
void ic_vm_init(ic_vm& vm);
void ic_vm_run(ic_vm& vm, ic_program& program);
void ic_vm_free(ic_vm& vm);
