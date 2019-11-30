#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <stdarg.h>
#include <stdint.h>
#include <chrono>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// ic - interpreted C
// todo
// final goal: dump assembly that can be assembled by e.g. nasm to an object file (or dump LLVM IR, or AOT compilation + execution)
// use common prefix for all declaration names
// comma, ternary operators
// print source code line on error
// somehow support multithreading (interpreter)? run function ast on a separate thread? (what about mutexes and atomics?)
// function pointers, typedefs (or better 'using = ')
// host structures registering ?
// ptrdiff_t ?; implicit type conversions warnings (overflows, etc.)
// one of the next goals will be to change execution into type pass with byte code generation and then execute bytecode
// escape sequences
// automatic array
// auto generate code for registering host functions (parse target function, generate warapper, register wrapper)
// data of returned struct value is leaked until function call scope is popped (except when value is used to initialize a variable); this is not a huge deal
// tail call optimization
// initializer-list
// bitwise operators
// imgui debugger
// software rasterizer benchmark
// change type keywords? (char, uchar, int, float, double)
// struct allignment (currently each member consumes 8 bytes)
// struct in value_type should be a pointer to a struct and not string

template<typename T, int N>
struct ic_deque
{
    std::vector<T*> pools;
    int size = 0; // todo; after replacing vector initialize externally

    void free()
    {
        for (T* pool : pools)
            ::free(pool);
    }

    T& get(int idx)
    {
        assert(idx < size);
        return pools[idx / N][idx % N];
    }

    T* allocate()
    {
        if (pools.empty() || size == pools.size() * N)
        {
            T* new_pool = (T*)malloc(N * sizeof(T));
            pools.push_back(new_pool);
        }

        size += 1;
        return &get(size - 1);
    }

    // this is quite tricky
    T* allocate_continuous(int num) // todo; change size to _size; num so it does not collide with member size
    {
        assert(num <= N);
        num = num ? num : 1; // this is to support non-member structs - allocate for them one dummy data

        if (pools.empty() || size + num > pools.size() * N)
        {
            T* new_pool = (T*)malloc(N * sizeof(T));
            pools.push_back(new_pool);
        }

        int current_pool_size = size % N;

        if (current_pool_size + num > N)
            size += N - current_pool_size; // move to the next pool (we want continuous memory)

        size += num;
        return &get(size - num);
    }
};

static_assert(sizeof(int) == 4, "sizeof(int) == 4");
static_assert(sizeof(float) == 4, "sizeof(float) == 4");
static_assert(sizeof(double) == 8, "sizeof(double) == 8");
// it is asserted that a write at ic_value.double will cover ic_value.pointer and vice versa (union)
static_assert(sizeof(double) == sizeof(void*), "sizeof(double) == sizeof(void*)");

#define IC_MAX_ARGC 10

enum ic_token_type
{
    IC_TOK_EOF,
    IC_TOK_IDENTIFIER,
    // keywords
    IC_TOK_FOR,
    IC_TOK_WHILE,
    IC_TOK_IF,
    IC_TOK_ELSE,
    IC_TOK_RETURN,
    IC_TOK_BREAK,
    IC_TOK_CONTINUE,
    IC_TOK_TRUE,
    IC_TOK_FALSE,
    IC_TOK_BOOL,
    IC_TOK_S8,
    IC_TOK_U8,
    IC_TOK_S32,
    IC_TOK_U32,
    IC_TOK_F32,
    IC_TOK_F64,
    IC_TOK_VOID,
    IC_TOK_NULLPTR,
    IC_TOK_CONST,
    IC_TOK_STRUCT,
    IC_TOK_SIZEOF,
    // literals
    IC_TOK_INT_NUMBER_LITERAL,
    IC_TOK_FLOAT_NUMBER_LITERAL,
    IC_TOK_STRING_LITERAL,
    IC_TOK_CHARACTER_LITERAL,
    // double character
    IC_TOK_PLUS_EQUAL,
    IC_TOK_MINUS_EQUAL,
    IC_TOK_STAR_EQUAL,
    IC_TOK_SLASH_EQUAL,
    IC_TOK_VBAR_VBAR,
    IC_TOK_AMPERSAND_AMPERSAND,
    IC_TOK_EQUAL_EQUAL,
    IC_TOK_BANG_EQUAL,
    IC_TOK_GREATER_EQUAL,
    IC_TOK_LESS_EQUAL,
    IC_TOK_PLUS_PLUS,
    IC_TOK_MINUS_MINUS,
    // single character
    IC_TOK_LEFT_PAREN,
    IC_TOK_RIGHT_PAREN,
    IC_TOK_LEFT_BRACE,
    IC_TOK_RIGHT_BRACE,
    IC_TOK_SEMICOLON,
    IC_TOK_COMMA,
    IC_TOK_EQUAL,
    IC_TOK_GREATER,
    IC_TOK_LESS,
    IC_TOK_PLUS,
    IC_TOK_MINUS,
    IC_TOK_STAR,
    IC_TOK_SLASH,
    IC_TOK_BANG,
    IC_TOK_AMPERSAND,
    IC_TOK_PERCENT,
    IC_TOK_LEFT_BRACKET,
    IC_TOK_RIGHT_BRACKET,
    IC_TOK_DOT,
    IC_TOK_ARROW,
};

enum ic_stmt_type
{
    IC_STMT_COMPOUND,
    IC_STMT_FOR,
    IC_STMT_IF,
    IC_STMT_VAR_DECLARATION,
    IC_STMT_RETURN,
    IC_STMT_BREAK,
    IC_STMT_CONTINUE,
    IC_STMT_EXPR,
};

enum ic_expr_type
{
    IC_EXPR_BINARY,
    IC_EXPR_UNARY,
    IC_EXPR_SIZEOF,
    IC_EXPR_CAST_OPERATOR,
    IC_EXPR_SUBSCRIPT,
    IC_EXPR_MEMBER_ACCESS,
    // parentheses expr exists so if(x=3) doesn't compile
    // and if((x=3)) compiles
    IC_EXPR_PARENTHESES,
    IC_EXPR_FUNCTION_CALL,
    IC_EXPR_PRIMARY,
};

// order is important here
enum ic_basic_type
{
    IC_TYPE_BOOL, // stores only 0 or 1; 1 byte at .s8
    IC_TYPE_S8,
    IC_TYPE_U8,
    IC_TYPE_S32,
    IC_TYPE_U32,
    IC_TYPE_F32,
    IC_TYPE_F64,
    IC_TYPE_VOID,
    IC_TYPE_NULLPTR,
    IC_TYPE_STRUCT,
};

enum ic_error_type
{
    IC_ERR_LEXING,
    IC_ERR_PARSING,
};

enum ic_function_type
{
    IC_FUN_HOST,
    IC_FUN_SOURCE,
};

// todo; union, enum
enum ic_global_type
{
    IC_GLOBAL_FUNCTION,
    IC_GLOBAL_STRUCT,
    IC_GLOBAL_VAR_DECLARATION,
};

enum ic_stmt_result_type
{
    IC_STMT_RESULT_BREAK,
    IC_STMT_RESULT_CONTINUE,
    IC_STMT_RESULT_RETURN,
    IC_STMT_RESULT_NOP,
};

struct ic_keyword
{
    const char* str;
    ic_token_type token_type;
};

static ic_keyword _keywords[] = {
    {"for", IC_TOK_FOR},
    {"while", IC_TOK_WHILE},
    {"if", IC_TOK_IF},
    {"else", IC_TOK_ELSE},
    {"return", IC_TOK_RETURN},
    {"break", IC_TOK_BREAK},
    {"continue", IC_TOK_CONTINUE},
    {"true", IC_TOK_TRUE},
    {"false", IC_TOK_FALSE},
    {"bool", IC_TOK_BOOL},
    {"s8", IC_TOK_S8},
    {"u8", IC_TOK_U8},
    {"s32", IC_TOK_S32},
    {"u32", IC_TOK_U32},
    {"f32", IC_TOK_F32},
    {"f64", IC_TOK_F64},
    {"void", IC_TOK_VOID},
    {"nullptr", IC_TOK_NULLPTR},
    {"const", IC_TOK_CONST},
    {"struct", IC_TOK_STRUCT},
    {"sizeof", IC_TOK_SIZEOF},
};

struct ic_string
{
    const char* data;
    int len;
};

struct ic_token
{
    ic_token_type type;
    int line;
    int col;

    union
    {
        double number;
        ic_string string;
    };
};

struct ic_value_type
{
    ic_basic_type basic_type;
    int indirection_level;
    unsigned int const_mask;
    ic_string struct_name;
};

struct ic_expr;

struct ic_stmt
{
    ic_stmt_type type;
    ic_stmt* next;

    union
    {
        struct
        {
            bool push_scope;
            ic_stmt* body;
        } _compound;

        struct
        {
            ic_stmt* header1;
            ic_expr* header2;
            ic_expr* header3;
            ic_stmt* body;
        } _for;

        struct
        {
            ic_expr* header;
            ic_stmt* body_if;
            ic_stmt* body_else;
        } _if;

        struct
        {
            ic_value_type type;
            ic_token token;
            ic_expr* expr;
        } _var_declaration;

        struct
        {
            ic_expr* expr;
        } _return;
        
        ic_expr* _expr;
    };
};

struct ic_expr
{
    ic_expr_type type;
    ic_token token;
    ic_expr* next;

    union
    {
        struct
        {
            ic_expr* lhs;
            ic_expr* rhs;
        } _binary;

        struct
        {
            ic_expr* expr;
        } _unary;

        struct
        {
            ic_value_type type;
        } _sizeof;

        struct
        {
            ic_value_type type;
            ic_expr* expr;
        } _cast_operator;

        struct
        {
            ic_expr* lhs;
            ic_expr* rhs;
        } _subscript;

        struct
        {
            ic_expr* lhs;
            ic_token rhs_token;
        } _member_access;

        struct
        {
            ic_expr* expr;
        } _parentheses;

        struct
        {
            ic_expr* arg;
        } _function_call;
    };
};

struct ic_value
{
    ic_value_type type;

    union
    {
        char s8;
        unsigned char u8;
        int s32;
        unsigned int u32;
        float f32;
        double f64;
        void* pointer;
    };
};

struct ic_var
{
    ic_string name;
    ic_value value;
};

struct ic_scope
{
    int prev_var_count;
    int prev_struct_data_count;
    bool new_stack_frame; // so variables do not leak to lower functions
};

struct ic_param
{
    ic_value_type type;
    ic_string name;
};

// partially redundant with ic_value
// ic_value can inherit this union to avoid redundancy
union ic_data
{
    char s8;
    unsigned char u8;
    int s32;
    float f32;
    double f64;
    void* pointer;
};

using ic_host_function = ic_data(*)(ic_data* argv);

struct ic_inst;

struct ic_function
{
    ic_function_type type;
    ic_value_type return_type;
    ic_token token;
    int param_count;
    ic_param params[IC_MAX_ARGC];
    ic_inst* bytecode; // todo move to a union
    int stack_size; // same

    union
    {
        ic_host_function callback;
        ic_stmt* body;
    };
};

struct ic_struct_member
{
    ic_value_type type;
    ic_string name;
};

struct ic_struct
{
    ic_token token;
    ic_struct_member* members;
    int num_members;
    int num_data;
};

struct ic_global
{
    ic_global_type type;

    union
    {
        ic_function function;
        ic_struct _struct;
        ic_stmt stmt; // var declaration
    };
};

struct ic_vm_var
{
    ic_value_type type;
    ic_string name;
    int idx;
};

struct ic_stmt_result
{
    ic_stmt_result_type type;
    ic_value value;
};

struct ic_expr_result
{
    void* lvalue_data;
    ic_value value;
};

struct ic_exception_parsing {};

#define IC_CALL_STACK_SIZE (1024 * 1024)
#define IC_OPERAND_STACK_SIZE 1024

enum ic_opcode
{
    IC_OPC_PUSH,
    IC_OPC_CALL, // last function argument is at the top of a operand stack (order is not reversed)
    IC_OPC_RETURN,
    IC_OPC_LOGICAL_NOT,
    IC_OPC_JUMP_TRUE,
    IC_OPC_JUMP_FALSE,
    IC_OPC_JUMP,
    IC_OPC_ADDRESS_OF,
    IC_OPC_ADDRESS_OF_GLOBAL,

    IC_OPC_LOAD,
    IC_OPC_LOAD_GLOBAL,
    IC_OPC_LOAD_STRUCT,
    IC_OPC_LOAD_STRUCT_GLOBAL,

    // data is not popped
    IC_OPC_STORE,
    IC_OPC_STORE_GLOBAL,
    IC_OPC_STORE_STRUCT,
    IC_OPC_STORE_STRUCT_GLOBAL,

    // order of operands is reversed (data before address)
    // address is popped, data is not popped
    IC_OPC_STORE_1_AT,
    IC_OPC_STORE_4_AT,
    IC_OPC_STORE_8_AT,
    IC_OPC_STORE_STRUCT_AT,

    // address is popped, data is pushed
    IC_OPC_DEREFERENCE_1,
    IC_OPC_DEREFERENCE_4,
    IC_OPC_DEREFERENCE_8,
    IC_OPC_DEREFERENCE_STRUCT,

    IC_OPC_COPMARE_E_S32,
    IC_OPC_COPMARE_NE_S32,
    IC_OPC_COPMARE_G_S32,
    IC_OPC_COPMARE_GE_S32,
    IC_OPC_COPMARE_L_S32,
    IC_OPC_COPMARE_LE_S32,
    IC_OPC_NEGATE_S32,
    IC_OPC_ADD_S32,
    IC_OPC_SUB_S32,
    IC_OPC_MUL_S32,
    IC_OPC_DIV_S32,

    IC_OPC_COPMARE_E_F32,
    IC_OPC_COPMARE_NE_F32,
    IC_OPC_COPMARE_G_F32,
    IC_OPC_COPMARE_GE_F32,
    IC_OPC_COPMARE_L_F32,
    IC_OPC_COPMARE_LE_F32,
    IC_OPC_NEGATE_F32,
    IC_OPC_ADD_F32,
    IC_OPC_SUB_F32,
    IC_OPC_MUL_F32,
    IC_OPC_DIV_F32,

    IC_OPC_COPMARE_E_F64,
    IC_OPC_COPMARE_NE_F64,
    IC_OPC_COPMARE_G_F64,
    IC_OPC_COPMARE_GE_F64,
    IC_OPC_COPMARE_L_F64,
    IC_OPC_COPMARE_LE_F64,
    IC_OPC_NEGATE_F64,
    IC_OPC_ADD_F64,
    IC_OPC_SUB_F64,
    IC_OPC_MUL_F64,
    IC_OPC_DIV_f64,

    IC_OPC_ADD_PTR_S32,
    IC_OPC_SUB_PTR_S32,
    IC_OPC_MODULO_S32,

    // convert to from

    IC_OPC_B_S8,
    IC_OPC_B_U8,
    IC_OPC_B_S32,
    IC_OPC_B_F32,
    IC_OPC_B_F64,

    IC_OPC_S8_U8,
    IC_OPC_S8_S32,
    IC_OPC_S8_F32,
    IC_OPC_S8_F64,

    IC_OPC_U8_S8,
    IC_OPC_U8_S32,
    IC_OPC_U8_F32,
    IC_OPC_U8_F64,

    IC_OPC_S32_S8,
    IC_OPC_S32_U8,
    IC_OPC_S32_F32,
    IC_OPC_S32_F64,

    IC_OPC_F32_S8,
    IC_OPC_F32_U8,
    IC_OPC_F32_S32,
    IC_OPC_F32_F64,

    IC_OPC_F64_S8,
    IC_OPC_F64_U8,
    IC_OPC_F64_S32,
    IC_OPC_F64_F32,

    // I don't like these (resolved during compilation)
    IC_OPC_JUMP_CONDITION_FAIL,
    IC_OPC_JUMP_START,
    IC_OPC_JUMP_END,
};

union ic_inst_operand
{
    ic_data push_data;
    int idx;
    int size;

    struct
    {
        int idx;
        int size;
    } load;

    struct
    {
        int idx;
        int size;
    } store;
};

struct ic_inst
{
    unsigned char opcode;
    ic_inst_operand operand;
};

struct ic_stack_frame
{
    int prev_operand_stack_size;
    int size;
    int return_size;
    ic_data* bp; // base pointer
    ic_inst* ip; // instruction pointer
    ic_inst* bytecode; // needed for jumps
};

struct ic_vm_function
{
    bool is_host;
    int param_size;
    int return_size;

    union
    {
        ic_host_function host_callback;

        struct
        {
            ic_inst* bytecode;
            int stack_size;
        } source;
    };
};

struct ic_vm
{
    // important, ic_data buffers must not be invalidated during execution
    std::vector<ic_stack_frame> stack_frames;
    ic_vm_function* functions;
    ic_data* global_data;
    ic_data* call_stack;
    ic_data* operand_stack;
    int call_stack_size;
    int operand_stack_size;

    void push_stack_frame(ic_inst* bytecode, int stack_size, int return_size);
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

struct ic_runtime;
void compile(ic_function& function, ic_runtime& runtime);
void run_bytecode(ic_vm& vm);

struct ic_runtime
{
    void init();
    bool add_global_var(const char* name, ic_value value);
    // if a function with the same name exists it is replaced
    void add_host_function(const char* name, ic_host_function function);
    bool run(const char* source); // after run() variables can be extracted
    ic_value* get_global_var(const char* name);
    void clear_global_vars(); // use this before next run()
    void clear_host_functions(); // useful when e.g. running different script
    void free(); // todo; after free() next init() will not put runtime into correct state (not all vectors are cleared, etc.)

    // implementation
    ic_deque<ic_stmt, 1000> _stmt_deque; // same
    ic_deque<ic_expr, 1000> _expr_deque; // same
    ic_deque<ic_var, 1000> _var_deque; // deque is used because variables must not be invalidated (pointers are supported)
    ic_deque<ic_data, 1000> _struct_data_deque; // same
    std::vector<char*> _string_literals;
    std::vector<ic_token> _tokens;
    std::vector<ic_scope> _scopes;
    std::vector<ic_function> _functions;
    std::vector<ic_struct> _structs;
    int _global_size;
    std::vector<ic_vm_var> _global_vars;

    void push_scope(bool new_stack_frame = false);
    void pop_scope();
    bool add_var(ic_string name, ic_value value);
    ic_value* get_var(ic_string name);
    ic_function* get_function(ic_string name);
    ic_struct* get_struct(ic_string name);
    ic_stmt* allocate_stmt(ic_stmt_type type);
    ic_expr* allocate_expr(ic_expr_type type, ic_token token);
};

bool ic_string_compare(ic_string str1, ic_string str2);
bool is_non_pointer_struct(ic_value_type& type);
ic_value_type non_pointer_type(ic_basic_type type);
