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

// write to retv only after reading all the arguments
// returning struct: *(copmlex*)retv = value;
using ic_host_function_ptr = void(*)(ic_data* argv, ic_data* retv, void* host_data);

// these are helpful if a function has struct parameters

inline void* ic_get_arg(ic_data*& argv, int size)
{
    int data_size = (size + sizeof(ic_data) - 1) / sizeof(ic_data);
    ic_data* begin = argv;
    argv += data_size;
    return begin;
}

inline int ic_get_bool(ic_data*& argv) { return *(char*)ic_get_arg(argv, sizeof(char)); }
inline int ic_get_char(ic_data*& argv) { return *(char*)ic_get_arg(argv, sizeof(char)); }
inline int ic_get_uchar(ic_data*& argv) { return *(unsigned char*)ic_get_arg(argv, sizeof(char)); }
inline int ic_get_int(ic_data*& argv) { return *(int*)ic_get_arg(argv, sizeof(int)); }
inline float ic_get_float(ic_data*& argv) { return *(float*)ic_get_arg(argv, sizeof(float)); }
inline double ic_get_double(ic_data*& argv) { return *(double*)ic_get_arg(argv, sizeof(double)); }
inline void* ic_get_ptr(ic_data*& argv) { return ic_get_arg(argv, sizeof(void*)); }

struct ic_host_function
{
    const char* prototype_str;
    ic_host_function_ptr callback;
    void* host_data;
};

// structures are parsed only if functions ptr is not null,
// they are only to support struct types in host function declarations
struct ic_host_decl
{
    ic_host_function* functions;
    // both definitions and forward declarations are fine
    const char* structures;
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
            int return_size;
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

struct ic_vm
{
    ic_data* stack;
    int stack_size;

    // todo, check if these are inlined and if not how much do we get from inlining them
    void push(ic_data data);
    void push();
    void push_many(int size);
    ic_data pop();
    void pop_many(int size);
    ic_data& top();
    ic_data* end();
};

bool ic_program_init_compile(ic_program& program, const char* source, int libs, ic_host_decl host_decl);
void ic_program_init_load(ic_program& program, unsigned char* buf, int libs, ic_host_decl host_decl);
void ic_program_free(ic_program& program);
void ic_program_print_disassembly(ic_program& program);
void ic_program_serialize(ic_program& program, unsigned char*& buf, int& size);
void ic_buf_free(unsigned char* buf);
void ic_vm_init(ic_vm& vm);
int ic_vm_run(ic_vm& vm, ic_program& program);
void ic_vm_free(ic_vm& vm);
