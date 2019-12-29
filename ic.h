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

// these are helpful if a callback has struct parameters

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
    ic_callback callback;
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

// todo, better naming of these whole host stuff
struct ic_vm_function
{
    ic_callback callback;
    void* host_data;
    unsigned int hash;
    int origin;
    int return_size;
    int param_size;
};

struct ic_program
{
    unsigned char* bytecode;
    ic_vm_function* functions;
    int functions_size;
    int bytecode_size;
    int strings_byte_size;
    int global_data_size;
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

bool ic_program_init_compile(ic_program& program, const char* source, int libs, ic_host_decl host_decl);
void ic_program_init_load(ic_program& program, unsigned char* buf, int libs, ic_host_decl host_decl);
void ic_program_free(ic_program& program);
void ic_program_print_disassembly(ic_program& program);
void ic_program_serialize(ic_program& program, unsigned char*& buf, int& size);
void ic_buf_free(unsigned char* buf);
void ic_vm_init(ic_vm& vm);
int ic_vm_run(ic_vm& vm, ic_program& program);
void ic_vm_free(ic_vm& vm);
