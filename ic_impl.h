#pragma once
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ic.h"

// ic - interpreted C
// todo
// comma, ternary, bitwise operators, switch, else if
// somehow support multithreading (interpreter)? run function ast on a separate thread? (what about mutexes and atomics?)
// function pointers, typedefs (or better 'using = '), initializer-list, automatic array, escape sequences, preprocessor, enum, union, /* comments
// , structures and unions can be anonymous inside other structures and unions (I very like this feature)
// ptrdiff_t ?; implicit type conversions warnings (overflows, etc.)
// tail call optimization
// imgui bytecode debugger, text editor with colors and error reporting using our very own ast technology
// exit function should exit bytecode execution, not a host program (exit instruction would be helpful)
// do some benchmarks against jvm, python and lua
// use float instead of f32, same for other types? current type naming convention is not clear and it needs to be fixed
// bytecode endianness
// I would like to support simple generics and plain struct functions
// self-hosting
// error messages may sometimes point slightly off, at a wrong token
// constant folding

static_assert(sizeof(int) == 4, "sizeof(int) == 4");
static_assert(sizeof(float) == 4, "sizeof(float) == 4");
static_assert(sizeof(double) == 8, "sizeof(double) == 8");
static_assert(sizeof(void*) == 8, "sizeof(void*) == 8");

#define IC_MAX_ARGC 10

// this is quite important to know:
// local variables are packed with a proper alignment and padded as a whole to ic_data
// each expression operand is padded to ic_data

// calling convention
// caller: pushes space for a return value, pushes arguments, issues call
// VM call: pushes bp and ip, sets bp to sp, sets ip to a call operand
// callee: pushes space for local variables, issues return
// VM return: sets sp to bp, pops and restores ip and bp
// caller: pops function arguments, uses or pops the returned value
enum ic_opcode
{
    IC_OPC_PUSH_S8,
    IC_OPC_PUSH_S32,
    IC_OPC_PUSH_F32,
    IC_OPC_PUSH_F64,
    IC_OPC_PUSH_NULLPTR,
    // push, pop operate on ic_data, not bytes
    IC_OPC_PUSH,
    IC_OPC_PUSH_MANY,
    IC_OPC_POP,
    IC_OPC_POP_MANY,
    IC_OPC_SWAP,
    IC_OPC_MEMMOVE, // needed for member access of an rvalue struct
    IC_OPC_CLONE,
    IC_OPC_CALL,
    IC_OPC_CALL_HOST,
    IC_OPC_RETURN,
    IC_OPC_JUMP_TRUE,
    IC_OPC_JUMP_FALSE,
    IC_LOGICAL_NOT,
    IC_OPC_JUMP,
    IC_OPC_ADDRESS, // operand is a byte offset from a base pointer
    IC_OPC_ADDRESS_GLOBAL, // similar

    // order of operands on the operand stack is reversed (data before address)
    // address is popped, data is left
    IC_OPC_STORE_1,
    IC_OPC_STORE_4,
    IC_OPC_STORE_8,
    IC_OPC_STORE_STRUCT,

    // address is popped, data is pushed
    IC_OPC_LOAD_1,
    IC_OPC_LOAD_4,
    IC_OPC_LOAD_8,
    IC_OPC_LOAD_STRUCT,

    IC_OPC_COMPARE_E_S32,
    IC_OPC_COMPARE_NE_S32,
    IC_OPC_COMPARE_G_S32,
    IC_OPC_COMPARE_GE_S32,
    IC_OPC_COMPARE_L_S32,
    IC_OPC_COMPARE_LE_S32,
    IC_OPC_NEGATE_S32,
    IC_OPC_ADD_S32,
    IC_OPC_SUB_S32,
    IC_OPC_MUL_S32,
    IC_OPC_DIV_S32,
    IC_OPC_MODULO_S32,

    IC_OPC_COMPARE_E_F32,
    IC_OPC_COMPARE_NE_F32,
    IC_OPC_COMPARE_G_F32,
    IC_OPC_COMPARE_GE_F32,
    IC_OPC_COMPARE_L_F32,
    IC_OPC_COMPARE_LE_F32,
    IC_OPC_NEGATE_F32,
    IC_OPC_ADD_F32,
    IC_OPC_SUB_F32,
    IC_OPC_MUL_F32,
    IC_OPC_DIV_F32,

    IC_OPC_COMPARE_E_F64,
    IC_OPC_COMPARE_NE_F64,
    IC_OPC_COMPARE_G_F64,
    IC_OPC_COMPARE_GE_F64,
    IC_OPC_COMPARE_L_F64,
    IC_OPC_COMPARE_LE_F64,
    IC_OPC_NEGATE_F64,
    IC_OPC_ADD_F64,
    IC_OPC_SUB_F64,
    IC_OPC_MUL_F64,
    IC_OPC_DIV_F64,

    IC_OPC_COMPARE_E_PTR,
    IC_OPC_COMPARE_NE_PTR,
    IC_OPC_COMPARE_G_PTR,
    IC_OPC_COMPARE_GE_PTR,
    IC_OPC_COMPARE_L_PTR,
    IC_OPC_COMPARE_LE_PTR,
    IC_OPC_SUB_PTR_PTR,
    IC_OPC_ADD_PTR_S32,
    IC_OPC_SUB_PTR_S32,

    // convert to from

    IC_OPC_B_S8,
    IC_OPC_B_U8,
    IC_OPC_B_S32,
    IC_OPC_B_F32,
    IC_OPC_B_F64,
    IC_OPC_B_PTR,

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
};

// todo, is unsigned char any better than int? memory-wise yes,
// but there are other aspects, I will need to profile
enum ic_token_type: unsigned char
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
    IC_TOK_ARROW,
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
};

// order is important here
enum ic_basic_type: unsigned char
{
    IC_TYPE_BOOL, // stores only 0 or 1 at .s8
    IC_TYPE_S8,
    IC_TYPE_U8,
    IC_TYPE_S32,
    IC_TYPE_F32,
    IC_TYPE_F64,
    IC_TYPE_VOID,
    IC_TYPE_NULLPTR,
    IC_TYPE_STRUCT,
};

enum ic_expr_type: unsigned char
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

enum ic_stmt_type: unsigned char
{
    IC_STMT_COMPOUND,
    IC_STMT_FOR,
    IC_STMT_IF,
    IC_STMT_VAR_DECL,
    IC_STMT_RETURN,
    IC_STMT_BREAK,
    IC_STMT_CONTINUE,
    IC_STMT_EXPR,
};

enum ic_function_type: unsigned char
{
    IC_FUN_HOST,
    IC_FUN_SOURCE,
};

enum ic_decl_type: unsigned char
{
    IC_DECL_FUNCTION,
    IC_DECL_STRUCT,
    IC_DECL_VAR,
};

enum ic_stmt_result
{
    IC_STMT_RESULT_NULL = 0,
    IC_STMT_RESULT_BREAK_CONT = 1,
    IC_STMT_RESULT_RETURN = 2,
};

enum ic_print_type
{
    IC_PERROR,
    IC_PWARNING,
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

struct ic_struct;

struct ic_type
{
    ic_basic_type basic_type;
    char indirection_level;
    unsigned char const_mask;
    ic_struct* _struct;
};

// todo, combine ic_expr and ic_stmt into a single ast_node structure
// maybe this way is better?
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
        } binary;

        struct
        {
            ic_expr* expr;
        } unary;

        struct
        {
            ic_type type;
        } _sizeof;

        struct
        {
            ic_type type;
            ic_expr* expr;
        } cast_operator;

        struct
        {
            ic_expr* lhs;
            ic_expr* rhs;
        } subscript;

        struct
        {
            ic_expr* lhs;
            ic_token rhs_token;
        } member_access;

        struct
        {
            ic_expr* expr;
        } parentheses;

        struct
        {
            ic_expr* arg;
        } function_call;
    };
};

struct ic_stmt
{
    ic_stmt_type type;
    ic_token token;
    ic_stmt* next;

    union
    {
        struct
        {
            bool push_scope;
            ic_stmt* body;
        } compound;

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
            ic_type type;
            ic_token token;
            ic_expr* expr;
        } var_decl;

        struct
        {
            ic_expr* expr;
        } _return;
        
        ic_expr* expr;
    };
};

struct ic_param
{
    ic_type type;
    ic_string name;
};

struct ic_function
{
    ic_function_type type;
    ic_type return_type;
    ic_token token;
    int param_count;
    // todo, allocate, same as struct members?
    ic_param params[IC_MAX_ARGC];

    union
    {
        struct
        {
            ic_stmt* body;
            int instr_idx;
        };
        ic_host_function* host_function;
    };
};

struct ic_struct
{
    ic_token token;
    ic_param* members;
    int members_size;
    bool defined;
    int byte_size;
    int alignment;
};

struct ic_decl
{
    ic_decl_type type;
    union
    {
        ic_struct _struct;
        ic_function function;
        struct
        {
            ic_type type;
            ic_token token;
        } var;
    };
};

struct ic_var
{
    ic_type type;
    ic_string name;
    int byte_idx;
};

template<typename T>
struct ic_array
{
    int size;
    int capacity;
    T* buf;

    void init()
    {
        size = 0;
        capacity = 0;
        buf = nullptr;
        // give it a start
        resize(20);
        size = 0;
    }

    T* transfer()
    {
        T* temp = buf;
        buf = nullptr;
        return temp;
    }

    void free()
    {
        ::free(buf);
    }

    void clear()
    {
        size = 0;
    }

    void resize(int new_size)
    {
        size = new_size;
        _maybe_grow();
    }

    void push_back()
    {
        size += 1;
        _maybe_grow();
    }

    void push_back(T t)
    {
        size += 1;
        _maybe_grow();
        back() = t;
    }

    void pop_back()
    {
        size -= 1;
    }

    T& back()
    {
        return *(buf + size - 1);
    }

    T* begin()
    {
        return buf;
    }

    T* end()
    {
        return buf + size;
    }

    void _maybe_grow()
    {
        if (size > capacity)
        {
            capacity = size * 2;
            buf = (T*)realloc(buf, capacity * sizeof(T));
            assert(buf);
        }
    }
};

// used for data that must not be invalidated
template<typename T, int N>
struct ic_deque
{
    ic_array<T*> pools;
    int size;

    void init()
    {
        size = 0;
        pools.init();
    }

    void free()
    {
        for (T* pool : pools)
            ::free(pool);

        pools.free();
    }

    T& get(int idx)
    {
        assert(idx < size);
        return pools.buf[idx / N][idx % N];
    }

    T* allocate()
    {
        if (!pools.size || size == pools.size * N)
        {
            T* new_pool = (T*)malloc(N * sizeof(T));
            pools.push_back(new_pool);
        }

        size += 1;
        return &get(size - 1);
    }

    T* allocate_chunk(int chunk_size)
    {
        assert(chunk_size <= N);
        assert(chunk_size);

        if (!pools.size || size + chunk_size > pools.size * N)
        {
            T* new_pool = (T*)malloc(N * sizeof(T));
            pools.push_back(new_pool);
        }

        int current_pool_size = size % N;

        if (current_pool_size + chunk_size > N)
            size += N - current_pool_size; // move to the next pool (we want continuous memory)

        size += chunk_size;
        return &get(size - chunk_size);
    }
};

inline int bytes_to_data_size(int bytes)
{
    return (bytes + sizeof(ic_data) - 1 ) / sizeof(ic_data);
}

inline int read_int(unsigned char** buf_it)
{
    int v;
    memcpy(&v, *buf_it, sizeof(int));
    *buf_it += sizeof(int);
    return v;
}

inline float read_float(unsigned char** buf_it)
{
    float v;
    memcpy(&v, *buf_it, sizeof(float));
    *buf_it += sizeof(float);
    return v;
}

inline double read_double(unsigned char** buf_it)
{
    double v;
    memcpy(&v, *buf_it, sizeof(double));
    *buf_it += sizeof(double);
    return v;
}

bool string_compare(ic_string str1, ic_string str2);
bool is_struct(ic_type type);
bool is_void(ic_type type);
ic_type non_pointer_type(ic_basic_type type);
ic_type const_pointer1_type(ic_basic_type type);
ic_type pointer1_type(ic_basic_type type);
void print(ic_print_type type, int line, int col, ic_array<ic_string>& source_lines, const char* err_msg);
int type_data_size(ic_type type);
int type_byte_size(ic_type type);
int align(int bytes, int type_size);
struct ic_memory;
ic_function* get_function(ic_string name, ic_memory& memory);
ic_var* get_global_var(ic_string name, ic_memory& memory);

struct ic_scope
{
    int prev_stack_size;
    int prev_var_count;
};

struct ic_memory
{
    ic_deque<char, 10000> generic_pool;
    ic_deque<ic_struct, 100> structs;
    ic_array<ic_string> source_lines;
    ic_array<ic_token> tokens;
    ic_array<ic_function*> active_source_functions;
    ic_array<ic_function*> active_host_functions;
    ic_array<unsigned char> bytecode;
    ic_array<ic_function> functions;
    ic_array<ic_var> global_vars;
    ic_array<ic_scope> scopes;
    ic_array<ic_var> vars;
    ic_array<int> break_ops;
    ic_array<int> cont_ops;
    ic_array<int> call_ops;

    void init()
    {
        generic_pool.init();
        structs.init();
        source_lines.init();
        tokens.init();
        active_source_functions.init();
        active_host_functions.init();
        bytecode.init();
        functions.init();
        global_vars.init();
        scopes.init();
        vars.init();
        break_ops.init();
        cont_ops.init();
        call_ops.init();
    }

    void free()
    {
        generic_pool.free();
        structs.free();
        source_lines.free();
        tokens.free();
        active_source_functions.free();
        active_host_functions.free();
        bytecode.free();
        functions.free();
        global_vars.free();
        scopes.free();
        vars.free();
        break_ops.free();
        cont_ops.free();
        call_ops.free();
    }

    // add a padding so the next allocation is aligned to double (the largest type this code is using)
    // todo, this is architecture specific
    char* allocate_generic(int bytes)
    {
        return generic_pool.allocate_chunk(align(bytes, sizeof(double)));
    }
};

struct ic_expr_result
{
    ic_type type;
    bool lvalue;
};

struct ic_compiler
{
    ic_memory* memory;
    ic_function* function;
    bool code_gen;
    int stack_byte_size;
    int max_stack_byte_size;
    int loop_count;
    bool error;
    int return_byte_idx;

    void set_error(ic_token token, const char* err_msg)
    {
        // there is no real need to disable code gen on error, it won't be used anyway
        if (error)
            return;
        error = true;
        print(IC_PERROR, token.line, token.col, memory->source_lines, err_msg);
    }

    void warn(ic_token token, const char* msg)
    {
        if (error)
            return;
        print(IC_PWARNING, token.line, token.col, memory->source_lines, msg);
    }

    int bc_size()
    {
        return memory->bytecode.size;
    }

    void bc_set_int(int idx, int data)
    {
        if (!code_gen)
            return;
        memcpy(memory->bytecode.buf + idx, &data, sizeof(int));
    }

    void push_scope()
    {
        ic_scope scope;
        scope.prev_var_count = memory->vars.size;
        scope.prev_stack_size = stack_byte_size;
        memory->scopes.push_back(scope);
    }

    void pop_scope()
    {
        memory->vars.resize(memory->scopes.back().prev_var_count);
        stack_byte_size = memory->scopes.back().prev_stack_size;
        memory->scopes.pop_back();
    }

    void add_opcode(ic_opcode opcode)
    {
        assert(opcode >= 0 && opcode <= 255);
        if (!code_gen)
            return;
        memory->bytecode.push_back(opcode);
    }

    void add_s8(char data)
    {
        if (!code_gen)
            return;
        memory->bytecode.push_back();
        *(char*)&memory->bytecode.back() = data;
    }

    void add_u8(unsigned char data)
    {
        if (!code_gen)
            return;
        memory->bytecode.push_back(data);
    }

    void add_s32(int data)
    {
        if (!code_gen)
            return;
        int size = sizeof(int);
        memory->bytecode.resize(memory->bytecode.size + size);
        memcpy(memory->bytecode.end() - size, &data, size);
    }
    void add_f32(float data)
    {
        if (!code_gen)
            return;
        int size = sizeof(float);
        memory->bytecode.resize(memory->bytecode.size + size);
        memcpy(memory->bytecode.end() - size, &data, size);
    }

    void add_f64(double data)
    {
        if (!code_gen)
            return;
        int size = sizeof(double);
        memory->bytecode.resize(memory->bytecode.size + size);
        memcpy(memory->bytecode.end() - size, &data, size);
    }

    void add_resolve_break_operand()
    {
        if (!code_gen)
            return;
        memory->break_ops.push_back(bc_size());
    }

    void add_resolve_cont_operand()
    {
        if (!code_gen)
            return;
        memory->cont_ops.push_back(bc_size());
    }

    void add_resolve_call_operand()
    {
        if (!code_gen)
            return;
        memory->call_ops.push_back(bc_size());
    }

    ic_var declare_var(ic_type type, ic_string name, ic_token token)
    {
        assert(memory->scopes.size);

        for (int i = memory->vars.size - 1; i >= memory->scopes.back().prev_var_count; --i)
        {
            if (string_compare(memory->vars.buf[i].name, name))
            {
                set_error(token, "variable with such name is already declared in the current scope");
                break;
            }
        }
        ic_var var;
        var.type = type;
        var.name = name;
        int byte_size = type_byte_size(var.type);
        int align_size = is_struct(var.type) ? var.type._struct->alignment : byte_size;
        stack_byte_size = align(stack_byte_size, align_size);
        var.byte_idx = stack_byte_size;
        memory->vars.push_back(var);
        stack_byte_size += byte_size;
        max_stack_byte_size = stack_byte_size > max_stack_byte_size ? stack_byte_size : max_stack_byte_size;
        return var;
    }

    ic_function* get_function(ic_string name, int* idx, ic_token token)
    {
        assert(idx);
        ic_function* function = ::get_function(name, *memory);

        if (!function)
        {
            set_error(token, "function with such name is not declared");
            assert(memory->functions.size); // there must be at least one function (main); do not return nullptr
            return memory->functions.buf;
        }

        if (!code_gen)
            return function;

        ic_array<ic_function*>& active_functions = function->type == IC_FUN_HOST ? memory->active_host_functions : memory->active_source_functions;

        for(int i = 0; i < active_functions.size; ++i)
        {
            ic_function* active = active_functions.buf[i];
            if (active == function)
            {
                *idx = i;
                return function;
            }
        }
        *idx = active_functions.size;
        active_functions.push_back(function);
        return function;
    }

    ic_var get_var(ic_string name, bool* is_global, ic_token token)
    {
        for (int i = memory->vars.size - 1; i >= 0; --i)
        {
            if (string_compare(memory->vars.buf[i].name, name))
            {
                *is_global = false;
                return memory->vars.buf[i];
            }
        }

        ic_var* global_var = get_global_var(name, *memory);

        if (!global_var)
        {
            set_error(token, "variable with such name is not declared");
            ic_var var;
            var.type = non_pointer_type(IC_TYPE_S32); // returning an uninitialized type may cause a crash (_struct pointer)
            return var;
        }

        *is_global = true;
        return *global_var;
    }
};

bool compile_function(ic_function& function, ic_memory& memory, bool code_gen);
// todo, pass compiler as a first argument to be consistent with rest of the code
ic_stmt_result compile_stmt(ic_stmt* stmt, ic_compiler& compiler);
ic_expr_result compile_expr(ic_expr* expr, ic_compiler& compiler, bool load_lvalue = true);
ic_expr_result compile_binary(ic_expr* expr, ic_compiler& compiler);
ic_expr_result compile_unary(ic_expr* expr, ic_compiler& compiler, bool load_lvalue);
ic_expr_result compile_pointer_offset_expr(ic_expr* ptr_expr, ic_expr* offset_expr, ic_opcode opc, ic_compiler& compiler);
ic_expr_result compile_dereference(ic_type type, ic_compiler& compiler, bool load_lvalue, ic_token token);
// compile_auxiliary.cpp
void compile_implicit_conversion(ic_type to, ic_type from, ic_compiler& compiler, ic_token token);
ic_type get_expr_result_type(ic_expr* expr, ic_compiler& compiler);
ic_type arithmetic_expr_type(ic_type lhs, ic_type rhs, ic_compiler& compiler, ic_token token);
ic_type arithmetic_expr_type(ic_type operand_type, ic_compiler& compiler, ic_token token);
void assert_modifiable_lvalue(ic_expr_result result, ic_compiler& compiler, ic_token token);
void compile_load(ic_type type, ic_compiler& compiler);
void compile_store(ic_type type, ic_compiler& compiler);
void compile_pop_expr_result(ic_expr_result result, ic_compiler& compiler);
int pointed_type_byte_size(ic_type type, ic_compiler& compiler);
