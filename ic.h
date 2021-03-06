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

using ic_callback = void(*)(ic_data* argv, ic_data* retv, void* host_data); // returning a struct: *(struct_type*)retv = value;

// these are helpful if a callback has struct parameters, see main.cpp for a usage example

inline ic_data ic_get_arg(ic_data*& argv)
{
    ++argv;
    return *(argv - 1);
}

inline void* ic_get_struct_arg(ic_data*& argv, int byte_size)
{
    int data_size = (byte_size + sizeof(ic_data) - 1) / sizeof(ic_data);
    ic_data* begin = argv;
    argv += data_size;
    return begin;
}

struct ic_host_function
{
    const char* prototype_str;
    ic_callback callback;
    void* host_data;
    // implementation
    unsigned int hash;
    int origin;
    int return_size;
    int param_size;
};

struct ic_program
{
    unsigned char* bytecode; // includes strings
    ic_host_function* host_functions;
    int host_functions_size;
    int bytecode_size;
    int strings_byte_size;
    int global_data_byte_size; // includes strings
};

struct ic_vm
{
    ic_data* stack;
    ic_data* sp; // stack pointer
    ic_data* bp; // base pointer
    unsigned char* ip; // instruction pointer

    // todo, make sure these are inlined
    void push();
    void push_many(int size);
    ic_data pop();
    void pop_many(int size);
    ic_data& top();
};

// host_functions should end with a nullptr prototype_str; if host functions use structures, they should be declared in struct_decls
bool ic_program_init_compile(ic_program& program, const char* source, int libs, ic_host_function* host_functions, const char* struct_decls);
void ic_program_init_load(ic_program& program, unsigned char* buf, int libs, ic_host_function* host_functions);
void ic_program_free(ic_program& program);
void ic_program_print_disassembly(ic_program& program);
void ic_program_serialize(ic_program& program, unsigned char*& buf, int& size);
void ic_buf_free(unsigned char* buf);
void ic_vm_init(ic_vm& vm);
int ic_vm_run(ic_vm& vm, ic_program& program);
void ic_vm_free(ic_vm& vm);
