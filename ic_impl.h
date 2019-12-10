#pragma once
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "ic.h"

// ic - interpreted C
// todo
// use common prefix for all declarations (at least for interface)
// comma, ternary, bitwise operators, switch, else if
// somehow support multithreading (interpreter)? run function ast on a separate thread? (what about mutexes and atomics?)
// function pointers, typedefs (or better 'using = '), initializer-list, automatic array, escape sequences, preprocessor, enum, union
// , structures and unions can be anonymous inside other structures and unions (I very like this feature)
// host structures; struct alignment, packing; exposing ast - seamless data structure sharing between host and VM
// not only struct alignment but also basic types alingment, so e.g. u8 does not consume 8 bytes of a call stack
// ptrdiff_t ?; implicit type conversions warnings (overflows, etc.)
// tail call optimization
// imgui bytecode debugger, text editor with colors and error reporting using our very own ast technology
// exit function should exit bytecode execution, not a host program
// do some benchmarks against jvm, python and lua
// use float instead of f32, same for other types?
// bytecode endianness
// I would like to support simple generics and plain struct functions
// self-hosting

static_assert(sizeof(int) == 4, "sizeof(int) == 4");
static_assert(sizeof(float) == 4, "sizeof(float) == 4");
static_assert(sizeof(double) == 8, "sizeof(double) == 8");
static_assert(sizeof(void*) == 8, "sizeof(void*) == 8");

#define IC_MAX_ARGC 10
#define IC_USER_FUNCTION -1

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

enum ic_global_type: unsigned char
{
    IC_GLOBAL_FUNCTION,
    IC_GLOBAL_STRUCT,
    IC_GLOBAL_VAR_DECLARATION,
};

enum ic_function_type: unsigned char
{
    IC_FUN_HOST,
    IC_FUN_SOURCE,
};

enum ic_stmt_type: unsigned char
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

// order is important here
enum ic_basic_type: unsigned char
{
    IC_TYPE_BOOL, // stores only 0 or 1; 1 byte at .s8
    IC_TYPE_S8,
    IC_TYPE_U8,
    IC_TYPE_S32,
    IC_TYPE_F32,
    IC_TYPE_F64,
    IC_TYPE_VOID,
    IC_TYPE_NULLPTR,
    IC_TYPE_STRUCT,
};

enum ic_stmt_result
{
    IC_STMT_RESULT_NULL = 0,
    IC_STMT_RESULT_BREAK_CONT = 1,
    IC_STMT_RESULT_RETURN = 2,
};

template<typename T, int N>
struct ic_deque
{
    std::vector<T*> pools;
    int size;

    void init()
    {
        size = 0;
    }

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
        // todo, int type for storing string literal idx and integer values
        double number;
        ic_string string;
    };
};

struct ic_type
{
    ic_basic_type basic_type;
    char indirection_level;
    unsigned char const_mask;
    ic_string struct_name; // keep pointer instead
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
            ic_type type;
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
            ic_type type;
        } _sizeof;

        struct
        {
            ic_type type;
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
};

struct ic_global
{
    ic_global_type type;

    union
    {
        ic_function function;
        ic_struct _struct;

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

bool string_compare(ic_string str1, ic_string str2);
bool is_struct(ic_type type);
bool is_void(ic_type type);
ic_type non_pointer_type(ic_basic_type type);
ic_type const_pointer1_type(ic_basic_type type);
ic_type pointer1_type(ic_basic_type type);
int read_int(unsigned char** buf_it);
float read_float(unsigned char** buf_it);
double read_double(unsigned char** buf_it);

struct ic_exception_parse {};
struct ic_exception_compie {};

struct ic_parser
{
    ic_deque<ic_expr, 1000> expressions;
    ic_deque<ic_stmt, 1000> statements;
    std::vector<ic_struct> structs;
    std::vector<ic_function> functions;
    std::vector<ic_var> global_vars;

    void init()
    {
        expressions.init();
        statements.init();
    }

    void free()
    {
        expressions.free();
        statements.free();
    }

    ic_stmt* allocate_stmt(ic_stmt_type type)
    {
        ic_stmt* stmt = statements.allocate();
        memset(stmt, 0, sizeof(ic_stmt));
        stmt->type = type;
        return stmt;
    }

    ic_expr* allocate_expr(ic_expr_type type, ic_token token)
    {
        ic_expr* expr = expressions.allocate();
        memset(expr, 0, sizeof(ic_expr));
        expr->type = type;
        expr->token = token;
        return expr;
    }

    ic_struct* get_struct(ic_string name)
    {
        for (ic_struct& _struct : structs)
        {
            if (string_compare(_struct.token.string, name))
                return &_struct;
        }
        return nullptr;
    }

    ic_function* get_function(ic_string name)
    {
        for (ic_function& function : functions)
        {
            if (string_compare(function.token.string, name))
                return &function;
        }
        return nullptr;
    }

    ic_var* get_global_var(ic_string name)
    {
        for (ic_var& var : global_vars)
        {
            if (string_compare(var.name, name))
                return &var;
        }
        return nullptr;
    }
};

struct ic_scope
{
    int prev_stack_size;
    int prev_var_count;
};

struct ic_expr_result
{
    ic_type type;
    bool lvalue;
};

struct ic_compiler
{
    std::vector<unsigned char>* bytecode;
    ic_function* function;
    ic_parser* parser;
    std::vector<ic_function*>* active_functions;
    bool generate_bytecode;
    std::vector<ic_scope> scopes;
    std::vector<ic_var> vars;
    std::vector<int> break_ops;
    std::vector<int> cont_ops;
    int stack_size;
    int max_stack_size;
    int loop_count;

    int bc_size()
    {
        if (!generate_bytecode)
            return {};
        return bytecode->size();
    }

    void bc_set_int(int idx, int data)
    {
        if (!generate_bytecode)
            return;
        void* dst = bytecode->data() + idx;
        memcpy(dst, &data, sizeof(int));
    }

    void push_scope()
    {
        ic_scope scope;
        scope.prev_var_count = vars.size();
        scope.prev_stack_size = stack_size;
        scopes.push_back(scope);
    }

    void pop_scope()
    {
        vars.resize(scopes.back().prev_var_count);
        stack_size = scopes.back().prev_stack_size;
        scopes.pop_back();
    }

    void add_opcode(ic_opcode opcode)
    {
        assert(opcode >= 0 && opcode <= 255);
        if (!generate_bytecode)
            return;
        bytecode->push_back(opcode);
    }

    void add_s8(char data)
    {
        if (!generate_bytecode)
            return;
        bytecode->emplace_back();
        *(char*)&bytecode->back() = data;
    }

    void add_u8(unsigned char data)
    {
        if (!generate_bytecode)
            return;
        bytecode->push_back(data);
    }

    void add_s32(int data)
    {
        if (!generate_bytecode)
            return;
        int size = sizeof(int);
        bytecode->resize(bytecode->size() + size);
        memcpy(&bytecode->back() - size + 1, &data, size);
    }
    void add_f32(float data)
    {
        if (!generate_bytecode)
            return;
        int size = sizeof(float);
        bytecode->resize(bytecode->size() + size);
        memcpy(&bytecode->back() - size + 1, &data, size);
    }

    void add_f64(double data)
    {
        if (!generate_bytecode)
            return;
        int size = sizeof(double);
        bytecode->resize(bytecode->size() + size);
        memcpy(&bytecode->back() - size + 1, &data, size);
    }

    void declare_unused_param(ic_type type)
    {
        stack_size += is_struct(type) ? get_struct(type.struct_name)->num_data : 1;
        max_stack_size = stack_size > max_stack_size ? stack_size : max_stack_size;
    }

    ic_var declare_var(ic_type type, ic_string name)
    {
        assert(scopes.size());

        for (int i = vars.size() - 1; i >= scopes.back().prev_var_count; --i)
        {
            if (string_compare(vars[i].name, name))
                assert(false);
        }

        int byte_idx = stack_size * sizeof(ic_data);
        stack_size += is_struct(type) ? get_struct(type.struct_name)->num_data : 1;
        max_stack_size = stack_size > max_stack_size ? stack_size : max_stack_size;
        ic_var var;
        var.idx = byte_idx;
        var.name = name;
        var.type = type;
        vars.push_back(var);
        return var;
    }

    ic_struct* get_struct(ic_string name)
    {
        ic_struct* _struct = parser->get_struct(name);
        assert(_struct);
        return _struct;
    }

    ic_function* get_function(ic_string name, int* idx)
    {
        assert(idx);
        ic_function* function = parser->get_function(name);
        assert(function);

        if (!generate_bytecode)
            return function;

        for(int i = 0; i < active_functions->size(); ++i)
        {
            ic_function* active = (*active_functions)[i];

            if (active == function)
            {
                *idx = i;
                return function;
            }
        }
        *idx = active_functions->size();
        active_functions->push_back(function);
        return function;
    }

    ic_var get_var(ic_string name, bool* is_global)
    {
        for (int i = vars.size() - 1; i >= 0; --i)
        {
            if (string_compare(vars[i].name, name))
            {
                *is_global = false;
                return vars[i];
            }
        }

        ic_var* global_var = parser->get_global_var(name);
        assert(global_var);
        *is_global = true;
        return *global_var;
    }
};

void compile_function(ic_function& function, ic_parser* parser, std::vector<ic_function*>* active_functions,
    std::vector<unsigned char>* bytecode);
ic_stmt_result compile_stmt(ic_stmt* stmt, ic_compiler& compiler);
ic_expr_result compile_expr(ic_expr* expr, ic_compiler& compiler, bool load_lvalue = true);
ic_expr_result compile_binary(ic_expr* expr, ic_compiler& compiler);
ic_expr_result compile_unary(ic_expr* expr, ic_compiler& compiler, bool load_lvalue);
ic_expr_result compile_pointer_offset_expr(ic_expr* ptr_expr, ic_expr* offset_expr, ic_opcode opc, ic_compiler& compiler);
ic_expr_result compile_dereference(ic_type type, ic_compiler& compiler, bool load_lvalue);
// compile_auxiliary.cpp
void compile_implicit_conversion(ic_type to, ic_type from, ic_compiler& compiler);
ic_type get_expr_result_type(ic_expr* expr, ic_compiler& compiler);
ic_type arithmetic_expr_type(ic_type lhs, ic_type rhs);
ic_type arithmetic_expr_type(ic_type operand_type);
void assert_comparison_compatible_pointer_types(ic_type lhs, ic_type rhs);
void assert_modifiable_lvalue(ic_expr_result result);
void compile_load(ic_type type, ic_compiler& compiler);
void compile_store(ic_type type, ic_compiler& compiler);
void compile_pop_expr_result(ic_expr_result result, ic_compiler& compiler);
int pointed_type_byte_size(ic_type type, ic_compiler& compiler);
