#pragma once
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ic.h"

// ic - interpreted C
// todo
// comma, ternary, bitwise operators, switch, else if
// somehow support multithreading (interpreter)? run function ast on a separate thread? (what about mutexes and atomics?)
// function pointers, typedefs (or better 'using = '), initializer-list, automatic array, escape sequences, preprocessor, enum, union,
//  /* comments
// , structures and unions can be anonymous inside other structures and unions (I very like this feature)
// host structures; struct alignment, packing; exposing ast - seamless data structure sharing between host and VM
// not only struct alignment but also basic types alingment, so e.g. u8 does not consume 8 bytes of a call stack and operand stack
// ptrdiff_t ?; implicit type conversions warnings (overflows, etc.)
// tail call optimization
// imgui bytecode debugger, text editor with colors and error reporting using our very own ast technology
// exit function should exit bytecode execution, not a host program
// do some benchmarks against jvm, python and lua
// use float instead of f32, same for other types?
// bytecode endianness
// I would like to support simple generics and plain struct functions
// self-hosting
// remove std::vector dependency - to keep POD and explicit initialization style consistent
// error messages may point slightly off, at a wrong token

static_assert(sizeof(int) == 4, "sizeof(int) == 4");
static_assert(sizeof(float) == 4, "sizeof(float) == 4");
static_assert(sizeof(double) == 8, "sizeof(double) == 8");
static_assert(sizeof(void*) == 8, "sizeof(void*) == 8");

#define IC_MAX_ARGC 10 // todo

// important: compare and logical_not push s32 not bool
enum ic_opcode
{
    IC_OPC_PUSH_S32,
    IC_OPC_PUSH_F32,
    IC_OPC_PUSH_F64,
    IC_OPC_PUSH_NULLPTR,

    IC_OPC_POP,
    IC_OPC_POP_MANY,
    IC_OPC_SWAP,
    IC_OPC_MEMMOVE, // needed for member access of an rvalue struct
    IC_OPC_CLONE,
    IC_OPC_CALL, // last function argument is at the top of an operand stack (order is not reversed)
    IC_OPC_RETURN,
    IC_OPC_JUMP_TRUE, // expects s32, operand is popped
    IC_OPC_JUMP_FALSE, // expects s32, operand is popped
    IC_OPC_JUMP,
    // operand is a BYTE index of a variable (this is to simplify string literals stuff)
    IC_OPC_ADDRESS,
    IC_OPC_ADDRESS_GLOBAL,

    // order of stack operands is reversed (data before address)
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
    IC_OPC_LOGICAL_NOT_S32,
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
    IC_OPC_LOGICAL_NOT_F32,
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
    IC_OPC_LOGICAL_NOT_F64,
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
    IC_OPC_LOGICAL_NOT_PTR,
    IC_OPC_SUB_PTR_PTR,
    IC_OPC_ADD_PTR_S32,
    IC_OPC_SUB_PTR_S32,

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
            ic_host_function* host_function;
            int origin;
        };
        struct
        {
            ic_stmt* body;
            int data_idx;
            int stack_size;
        };
    };
};

struct ic_struct
{
    ic_token token;
    ic_param* members;
    int num_members;
    int num_data;
    bool defined;
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
    int idx;
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

    T* tansfer()
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

// this is used for data that must not be invalidated
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

    T* allocate_continuous(int num)
    {
        assert(num <= N);
        assert(num);

        if (!pools.size || size + num > pools.size * N)
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
    ic_array<ic_function*> active_functions;
    ic_array<unsigned char> program_data;
    ic_array<ic_function> functions;
    ic_array<ic_var> global_vars;
    ic_array<ic_scope> scopes;
    ic_array<ic_var> vars;
    ic_array<int> break_ops;
    ic_array<int> cont_ops;

    void init()
    {
        generic_pool.init();
        structs.init();
        source_lines.init();
        tokens.init();
        active_functions.init();
        program_data.init();
        functions.init();
        global_vars.init();
        scopes.init();
        vars.init();
        break_ops.init();
        cont_ops.init();
    }

    void free()
    {
        generic_pool.free();
        structs.free();
        source_lines.free();
        tokens.free();
        active_functions.free();
        program_data.free();
        functions.free();
        global_vars.free();
        scopes.free();
        vars.free();
        break_ops.free();
        cont_ops.free();
    }
};

bool string_compare(ic_string str1, ic_string str2);
bool is_struct(ic_type type);
bool is_void(ic_type type);
ic_type non_pointer_type(ic_basic_type type);
ic_type const_pointer1_type(ic_basic_type type);
ic_type pointer1_type(ic_basic_type type);
int read_int(unsigned char** buf_it);
float read_float(unsigned char** buf_it);
double read_double(unsigned char** buf_it);
void print(ic_print_type type, int line, int col, ic_array<ic_string>& source_lines, const char* err_msg);
int type_size(ic_type type);
ic_var* get_global_var(ic_string name, ic_memory& memory);
ic_function* get_function(ic_string name, ic_memory& memory);

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
    int stack_size;
    int max_stack_size;
    int loop_count;
    bool error;

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
        if (!code_gen)
            return {};
        return memory->program_data.size;
    }

    void bc_set_int(int idx, int data)
    {
        if (!code_gen)
            return;
        memcpy(memory->program_data.buf + idx, &data, sizeof(int));
    }

    void push_scope()
    {
        ic_scope scope;
        scope.prev_var_count = memory->vars.size;
        scope.prev_stack_size = stack_size;
        memory->scopes.push_back(scope);
    }

    void pop_scope()
    {
        memory->vars.resize(memory->scopes.back().prev_var_count);
        stack_size = memory->scopes.back().prev_stack_size;
        memory->scopes.pop_back();
    }

    void add_opcode(ic_opcode opcode)
    {
        assert(opcode >= 0 && opcode <= 255);
        if (!code_gen)
            return;
        memory->program_data.push_back(opcode);
    }

    void add_s8(char data)
    {
        if (!code_gen)
            return;
        memory->program_data.push_back();
        *(char*)&memory->program_data.back() = data;
    }

    void add_u8(unsigned char data)
    {
        if (!code_gen)
            return;
        memory->program_data.push_back(data);
    }

    void add_s32(int data)
    {
        if (!code_gen)
            return;
        int size = sizeof(int);
        memory->program_data.resize(memory->program_data.size + size);
        memcpy(memory->program_data.end() - size, &data, size);
    }
    void add_f32(float data)
    {
        if (!code_gen)
            return;
        int size = sizeof(float);
        memory->program_data.resize(memory->program_data.size + size);
        memcpy(memory->program_data.end() - size, &data, size);
    }

    void add_f64(double data)
    {
        if (!code_gen)
            return;
        int size = sizeof(double);
        memory->program_data.resize(memory->program_data.size + size);
        memcpy(memory->program_data.end() - size, &data, size);
    }

    void declare_unused_param(ic_type type)
    {
        stack_size += type_size(type);
        max_stack_size = stack_size > max_stack_size ? stack_size : max_stack_size;
    }

    ic_var declare_var(ic_type type, ic_string name)
    {
        assert(memory->scopes.size);

        for (int i = memory->vars.size - 1; i >= memory->scopes.back().prev_var_count; --i)
        {
            if (string_compare(memory->vars.buf[i].name, name))
                assert(false); // todo
        }

        int byte_idx = stack_size * sizeof(ic_data);
        stack_size += type_size(type);
        max_stack_size = stack_size > max_stack_size ? stack_size : max_stack_size;
        ic_var var;
        var.idx = byte_idx;
        var.name = name;
        var.type = type;
        memory->vars.push_back(var);
        return var;
    }

    ic_function* get_function(ic_string name, int* idx)
    {
        assert(idx);
        ic_function* function = ::get_function(name, *memory);
        assert(function); // todo

        if (!code_gen)
            return function;

        for(int i = 0; i < memory->active_functions.size; ++i)
        {
            ic_function* active = memory->active_functions.buf[i];
            if (active == function)
            {
                *idx = i;
                return function;
            }
        }
        *idx = memory->active_functions.size;
        memory->active_functions.push_back(function);
        return function;
    }

    ic_var get_var(ic_string name, bool* is_global)
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
        assert(global_var);
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
int pointed_type_byte_size(ic_type type);
