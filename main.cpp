#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <stdarg.h>

// ic - interpreted C
// todo
// final goal: dump assembly that can be assembled by e.g. nasm to an object file
// use common prefix for all declaration names
// comma, [], sizeof operators
// print source code line on error
// somehow support multithreading (interpreter)? run function ast on a separate thread? (what about mutexes and atomics?)
// string literals should be null terminated
// function and void pointers, typedefs (or better 'using = ')
// host structures registering ?
// ptrdiff_t ?; implicit type conversions warnings (overflows, etc.)
// rename left, right to lhs and rhs
// one of the next goals will be to change execution into type pass with byte code generation and then execute bytecode

template<typename T, int N>
struct ic_deque
{
    std::vector<T*> pools;
    int size = 0;

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
    // literals
    IC_TOK_INT_NUMBER,
    IC_TOK_FLOAT_NUMBER,
    IC_TOK_STRING,
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

// todo; union?
enum ic_global_type
{
    IC_GLOBAL_FUNCTION,
    IC_GLOBAL_FUNCTION_FORWARD_DECLARATION,
    IC_GLOBAL_STRUCTURE,
    IC_GLOBAL_STRUCTURE_FORWARD_DECLARATION,
    IC_GLOBAL_VAR_DECLARATION,
    IC_GLOBAL_ENUM,
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
    {"nullptr", IC_TOK_NULLPTR},
    {"bool", IC_TOK_BOOL},
    {"s8", IC_TOK_S8},
    {"u8", IC_TOK_U8},
    {"s32", IC_TOK_S32},
    {"u32", IC_TOK_U32},
    {"f32", IC_TOK_F32},
    {"f64", IC_TOK_F64},
    {"void", IC_TOK_VOID},
};

struct ic_string
{
    const char* data;
    int len;
};

struct ic_token
{
    ic_token_type type;
    int col;
    int line;

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

    // without this visual c++ does not compile, I have no idea why...
    // I should test with gcc
    bool operator==(const ic_value_type& other) const
    {
        return basic_type == other.basic_type && indirection_level == other.indirection_level;
    }
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
            ic_expr* left;
            ic_expr* right;
        } _binary;

        struct
        {
            ic_expr* expr;
        } _unary;

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
    bool new_call_frame; // so variables do not leak to lower functions
};

struct ic_param
{
    ic_value_type type;
    ic_string name;
};

using ic_host_function = ic_value(*)(int argc, ic_value* argv);

struct ic_function
{
    ic_function_type type;
    ic_value_type return_type;
    ic_token token;
    int param_count;
    ic_param params[IC_MAX_ARGC];

    union
    {
        struct
        {
            ic_host_function callback;
            bool type_check;
        } host;

        ic_stmt* body;
    };
};

struct ic_global
{
    ic_global_type type;

    union
    {
        ic_function function;
    };
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
    void free();

    // implementation
    ic_deque<ic_stmt, 1000> _stmt_deque;
    ic_deque<ic_expr, 1000> _expr_deque;
    ic_deque<ic_var, 1000> _var_deque; // deque is used because variables must not be invalidated (pointers are supported)
    std::vector<ic_token> _tokens;
    std::vector<ic_scope> _scopes;
    std::vector<ic_function> _functions;

    void push_scope(bool new_call_frame = false);
    void pop_scope();
    bool add_var(ic_string name, ic_value value);
    ic_value* get_var(ic_string name);
    ic_function* get_function(ic_string name);
    ic_stmt* allocate_stmt(ic_stmt_type type);
    ic_expr* allocate_expr(ic_expr_type type, ic_token token);
};

int main(int argc, const char** argv)
{
    assert(argc == 2);
    FILE* file = fopen(argv[1], "r");
    assert(file);
    int rc = fseek(file, 0, SEEK_END);
    assert(rc == 0);
    int size = ftell(file);
    assert(size != EOF);
    assert(size != 0);
    rewind(file);
    std::vector<char> source_code;
    source_code.resize(size + 1);
    fread(source_code.data(), 1, size, file);
    source_code.push_back('\0');
    fclose(file);

    ic_runtime runtime;
    runtime.init();
    runtime.run(source_code.data());
    return 0;
}

void ic_print_error(ic_error_type error_type, int line, int col, const char* fmt, ...)
{
    const char* str_err_type = nullptr;

    if (error_type == IC_ERR_LEXING)
        str_err_type = "lexing";
    else if (error_type == IC_ERR_PARSING)
        str_err_type = "parsing";

    assert(str_err_type);

    printf("%s error; line: %d; col: %d; ", str_err_type, line, col);
    va_list list;
    va_start(list, fmt);
    vprintf(fmt, list);
    va_end(list);
    printf("\n");
}

ic_stmt* ic_runtime::allocate_stmt(ic_stmt_type type)
{
    ic_stmt* stmt = _stmt_deque.allocate();
    memset(stmt, 0, sizeof(ic_stmt));
    stmt->type = type;
    return stmt;
}

ic_expr* ic_runtime::allocate_expr(ic_expr_type type, ic_token token)
{
    ic_expr* expr = _expr_deque.allocate();
    memset(expr, 0, sizeof(ic_expr));
    expr->type = type;
    expr->token = token;
    return expr;
}

// todo: support format string and arguments, but how to do it? parse the format and
// call printf by printf?
ic_value ic_host_print(int argc, ic_value* argv)
{
    assert(argc == 1);
    assert(argv[0].type.basic_type == IC_TYPE_F64);
    assert(argv[0].type.indirection_level == 0);
    printf("print: %f\n", argv[0].f64);
    return { IC_TYPE_VOID, 0 }; // todo; void should be implicit?
}

void ic_runtime::init()
{
    push_scope(); // global scope

    // todo; use add_host_function()
    const char* str = "print";
    ic_string name = { str, strlen(str) };
    ic_function function;
    function.type = IC_FUN_HOST;
    function.token.string = name;
    function.return_type = { IC_TYPE_VOID, 0 };
    function.param_count = 1;
    function.params[0].type = { IC_TYPE_F64, 0 };
    function.host.callback = ic_host_print;
    function.host.type_check = true;
    _functions.push_back(function);
}

void ic_runtime::free()
{
    _stmt_deque.free();
    _expr_deque.free();
    _var_deque.free();
}

// todo; these functions should allocate and then free memory for host strings (which can be temporary variables)

void ic_runtime::clear_host_functions()
{
    assert(false);
}

void ic_runtime::clear_global_vars()
{
    assert(_scopes.size() == 1);
    _var_deque.size = 0;
}

bool ic_runtime::add_global_var(const char* name, ic_value value)
{
    assert(false);
    return {};
}

ic_value* ic_runtime::get_global_var(const char* name)
{
    assert(false);
    return {};
}

// todo; overwrite if exists; parameters can't be of type void
void ic_runtime::add_host_function(const char* name, ic_host_function function)
{
    assert(false);
}

bool ic_string_compare(ic_string str1, ic_string str2)
{
    if (str1.len != str2.len)
        return false;

    return (strncmp(str1.data, str2.data, str1.len) == 0);
}

bool ic_tokenize(std::vector<ic_token>& tokens, const char* source_code);
ic_global produce_global(const ic_token** it, ic_runtime& runtime);
ic_expr_result ic_evaluate_expr(const ic_expr* expr, ic_runtime& runtime);

bool ic_runtime::run(const char* source_code)
{
    assert(_scopes.size() == 1); // assert ic_runtime initialized
    assert(source_code);
    _tokens.clear();
    _stmt_deque.size = 0;
    _expr_deque.size = 0;

    if (!ic_tokenize(_tokens, source_code))
        return false;

    const ic_token* token_it = _tokens.data();
    while (token_it->type != IC_TOK_EOF)
    {
        ic_global global;

        try
        {
            global = produce_global(&token_it, *this);
        }
        catch (ic_exception_parsing)
        {
            return false;
        }

        assert(global.type == IC_GLOBAL_FUNCTION);
        ic_token token = global.function.token;

        if (get_function(token.string))
        {
            ic_print_error(IC_ERR_PARSING, token.line, token.col, "function with such name already exists");
            return false;
        }

        _functions.push_back(global.function);
    }

    // execute main funtion
    const char* str = "main";
    ic_string name = { str, strlen(str) };
    ic_token token;
    token.type = IC_TOK_IDENTIFIER;
    token.string = name;
    ic_expr* expr = allocate_expr(IC_EXPR_FUNCTION_CALL, token);
    ic_evaluate_expr(expr, *this);

    assert(_scopes.size() == 1); // all non global scopes are gone

    // remove source functions so they don't collide with next run()
    {
        auto it = _functions.begin();
        for (; it != _functions.end(); ++it)
        {
            if (it->type == IC_FUN_SOURCE)
                break;
        }
        _functions.erase(it, _functions.end()); // source function are always stored after host ones
    }

    // todo return value from main()?
    return true;
}

void ic_runtime::push_scope(bool new_call_frame)
{
    ic_scope scope;
    scope.prev_var_count = _var_deque.size;
    scope.new_call_frame = new_call_frame;
    _scopes.push_back(scope);
}

void ic_runtime::pop_scope()
{
    assert(_scopes.size());
    _var_deque.size = _scopes.back().prev_var_count;
    _scopes.pop_back();
}

bool ic_runtime::add_var(ic_string name, ic_value value)
{
    assert(_scopes.size());
    bool present = false;

    for (int i = _var_deque.size - 1; i >= _scopes.back().prev_var_count; --i)
    {
        ic_var& var = _var_deque.get(i);

        if (ic_string_compare(var.name, name))
        {
            present = true;
            break;
        }
    }

    if (present)
        return false;

    ic_var* var = _var_deque.allocate();
    var->name = name;
    var->value = value;
    return true;
}

ic_value* ic_runtime::get_var(ic_string name)
{
    assert(_scopes.size());
    int idx_var = _var_deque.size - 1;

    for (int idx_scope = _scopes.size() - 1; idx_scope >= 0; --idx_scope)
    {
        ic_scope& scope = _scopes[idx_scope];

        for (; idx_var >= scope.prev_var_count; --idx_var)
        {
            ic_var& var = _var_deque.get(idx_var);

            if (ic_string_compare(var.name, name))
                return &var.value;
        }

        // go to the global scope
        if (scope.new_call_frame)
        {
            assert(!_scopes[0].new_call_frame); // avoid infinite loop
            idx_scope = 1;
        }
    }

    return nullptr;
}

ic_function* ic_runtime::get_function(ic_string name)
{
    for (ic_function& function : _functions)
    {
        if (ic_string_compare(function.token.string, name))
            return& function;
    }
    return nullptr;
}

bool to_boolean(ic_value value)
{
    if (value.type.indirection_level)
        return value.pointer;

    switch (value.type.basic_type)
    {
    case IC_TYPE_BOOL:
    case IC_TYPE_S8:
        return value.s8;
    case IC_TYPE_U8:
        return value.u8;
    case IC_TYPE_S32:
        return value.s32;
    case IC_TYPE_U32:
        return value.u32;
    case IC_TYPE_F32:
        return value.f32;
    case IC_TYPE_F64:
        return value.f64;
    }

    assert(false);
    return {};
}

ic_stmt_result ic_execute_stmt(const ic_stmt* stmt, ic_runtime& runtime)
{
    assert(stmt);

    switch (stmt->type)
    {
    case IC_STMT_COMPOUND:
    {
        if (stmt->_compound.push_scope)
            runtime.push_scope();

        ic_stmt_result output = { IC_STMT_RESULT_NOP };
        const ic_stmt* body_stmt = stmt->_compound.body;

        while (body_stmt)
        {
            output = ic_execute_stmt(body_stmt, runtime);

            if (output.type != IC_STMT_RESULT_NOP)
                break;

            body_stmt = body_stmt->next;
        }

        if (stmt->_compound.push_scope)
            runtime.pop_scope();

        return output;
    }
    case IC_STMT_FOR:
    {
        runtime.push_scope();
        int num_scopes = runtime._scopes.size();
        int num_variables = runtime._scopes.back().prev_var_count;

        if (stmt->_for.header1)
            ic_execute_stmt(stmt->_for.header1, runtime);

        ic_stmt_result output = { IC_STMT_RESULT_NOP };

        for (;;)
        {
            if (stmt->_for.header2)
            {
                ic_expr_result header2_result = ic_evaluate_expr(stmt->_for.header2, runtime);

                if (!to_boolean(header2_result.value))
                    break;
            }

            ic_stmt_result body_result = ic_execute_stmt(stmt->_for.body, runtime);

            if (body_result.type == IC_STMT_RESULT_BREAK)
                break;
            else if (body_result.type == IC_STMT_RESULT_RETURN)
            {
                output = body_result;
                break;
            }
            // else do nothing on NOP and CONTINUE

            if (stmt->_for.header3)
                ic_evaluate_expr(stmt->_for.header3, runtime);
        }

        runtime.pop_scope();
        return output;
    }
    case IC_STMT_IF:
    {
        runtime.push_scope();
        ic_stmt_result output = { IC_STMT_RESULT_NOP };
        ic_expr_result header_result = ic_evaluate_expr(stmt->_if.header, runtime);

        if (to_boolean(header_result.value))
            output = ic_execute_stmt(stmt->_if.body_if, runtime);
        else if (stmt->_if.body_else)
            output = ic_execute_stmt(stmt->_if.body_else, runtime);

        runtime.pop_scope();
        return output;
    }
    case IC_STMT_RETURN:
    {
        ic_stmt_result output = {IC_STMT_RESULT_RETURN};

        if (stmt->_return.expr)
            output.value = ic_evaluate_expr(stmt->_return.expr, runtime).value;
        else
        {
            // this value should never be used (type pass should terminate)
            // this is only for bug detection, e.g. failing in to_boolean()
            output.value.type.basic_type = IC_TYPE_VOID;
            output.value.type.indirection_level = 0;
        }

        return output;
    }
    case IC_STMT_BREAK:
    {
        return ic_stmt_result{ IC_STMT_RESULT_BREAK };
    }
    case IC_STMT_CONTINUE:
    {
        return ic_stmt_result{ IC_STMT_RESULT_CONTINUE };
    }
    case IC_STMT_VAR_DECLARATION:
    {
        ic_value value;
        value.type = stmt->_var_declaration.type;

        if (stmt->_var_declaration.expr)
        {
            ic_value expr_value = ic_evaluate_expr(stmt->_var_declaration.expr, runtime).value;
            assert(value.type == expr_value.type);
            value = expr_value;
        }

        assert(runtime.add_var(stmt->_var_declaration.token.string, value));
        return ic_stmt_result{ IC_STMT_RESULT_NOP };
    }
    case IC_STMT_EXPR:
    {
        ic_evaluate_expr(stmt->_expr, runtime);
        return ic_stmt_result{ IC_STMT_RESULT_NOP };
    }
    } // switch
    
    assert(false);
    return {};
}

// interpreter does calculations only in double type and then converts value to expression result type
// this is to avoid boilerplate code + double can contain all other supported types
// why not just store everything as f64? in short - host data is supported
// why can't every numeric expression result in f64? only integer types can be added to pointers; ptr + (5 + 6); would fail then

double get_numeric_data(ic_value value);
void set_numeric_data(ic_value& value, double number);
ic_basic_type get_numeric_expr_result_type(ic_value left, ic_value right);
ic_value produce_numeric_value(ic_basic_type btype, double number);
// use before set_lvalue_data(); for only numeric expressions use set_numeric_data() instead
ic_value implicit_convert_type(ic_value_type target_type, ic_value value);
void set_lvalue_data(void* lvalue_data, ic_value value);
ic_value evaluate_add(ic_value left, ic_value right, bool subtract = false);
bool compare_equal(ic_value left, ic_value right);
bool compare_greater(ic_value left, ic_value right);

ic_expr_result ic_evaluate_expr(const ic_expr* expr, ic_runtime& runtime)
{
    assert(expr);

    switch(expr->type)
    {
    case IC_EXPR_BINARY:
    {
        ic_value right = ic_evaluate_expr(expr->_binary.right, runtime).value;
        void* lvalue_data;
        ic_value left;
        {
            ic_expr_result result = ic_evaluate_expr(expr->_binary.left, runtime);
            lvalue_data = result.lvalue_data;
            left = result.value;
        }

        switch (expr->token.type)
        {
        case IC_TOK_EQUAL:
        {
            ic_value output = implicit_convert_type(left.type, right);
            set_lvalue_data(lvalue_data, output);
            return { lvalue_data, output };
        }
        case IC_TOK_PLUS_EQUAL:
        {
            ic_value output = evaluate_add(left, right);
            output = implicit_convert_type(left.type, output);
            set_lvalue_data(lvalue_data, output);
            return { lvalue_data, output };
        }
        case IC_TOK_MINUS_EQUAL:
        {
            ic_value output = evaluate_add(left, right, true);
            output = implicit_convert_type(left.type, output);
            set_lvalue_data(lvalue_data, output);
            return { lvalue_data, output };
        }
        case IC_TOK_STAR_EQUAL:
        {
            double number = get_numeric_data(left) * get_numeric_data(right);
            set_numeric_data(left, number);
            set_lvalue_data(lvalue_data, left);
            return { lvalue_data, left };
        }
        case IC_TOK_SLASH_EQUAL:
        {
            double number = get_numeric_data(left) * get_numeric_data(right);
            set_numeric_data(left, number);
            set_lvalue_data(lvalue_data, left);
            return { lvalue_data, left };
        }
        case IC_TOK_VBAR_VBAR:
        {
            ic_value output;
            output.type = { IC_TYPE_BOOL, 0 };
            output.s8 = to_boolean(left) || to_boolean(right);
            return { nullptr, output };
        }
        case IC_TOK_AMPERSAND_AMPERSAND:
        {
            ic_value output;
            output.type = { IC_TYPE_BOOL, 0 };
            output.s8 = to_boolean(left) && to_boolean(right);
            return { nullptr, output };
        }
        case IC_TOK_EQUAL_EQUAL:
        {
            ic_value output;
            output.type = { IC_TYPE_BOOL, 0 };
            output.s8 = compare_equal(left, right);
            return { nullptr, output};
        }
        case IC_TOK_BANG_EQUAL:
        {
            ic_value output;
            output.type = { IC_TYPE_BOOL, 0 };
            output.s8 = !compare_equal(left, right);
            return { nullptr, output};
        }
        case IC_TOK_GREATER:
        {
            ic_value output;
            output.type = { IC_TYPE_BOOL, 0 };
            output.s8 = compare_greater(left, right);
            return { nullptr, output};
        }
        case IC_TOK_GREATER_EQUAL:
        {
            ic_value output;
            output.type = { IC_TYPE_BOOL, 0 };
            output.s8 = compare_greater(left, right) || compare_equal(left, right);
            return { nullptr, output};
        }
        case IC_TOK_LESS:
        {
            ic_value output;
            output.type = { IC_TYPE_BOOL, 0 };
            output.s8 = !compare_greater(left, right) && !compare_equal(left, right);
            return { nullptr, output};
        }
        case IC_TOK_LESS_EQUAL:
        {
            ic_value output;
            output.type = { IC_TYPE_BOOL, 0 };
            output.s8 = !compare_greater(left, right) || compare_equal(left, right);
            return { nullptr, output};
        }
        case IC_TOK_PLUS:
        {
            return { nullptr, evaluate_add(left, right) };
        }
        case IC_TOK_MINUS:
        {
            return { nullptr, evaluate_add(left, right, true) };
        }
        case IC_TOK_STAR:
        {
            double number = get_numeric_data(left) * get_numeric_data(right);
            return { nullptr, produce_numeric_value(get_numeric_expr_result_type(left, right), number) };
        }
        case IC_TOK_SLASH:
        {
            double number = get_numeric_data(left) / get_numeric_data(right);
            return { nullptr, produce_numeric_value(get_numeric_expr_result_type(left, right), number) };
        }
        } // switch

        assert(false);
    }
    case IC_EXPR_UNARY:
    {
        void* lvalue_data;
        ic_value value;
        {
            ic_expr_result result = ic_evaluate_expr(expr->_unary.expr, runtime);
            lvalue_data = result.lvalue_data;
            value = result.value;
        }

        switch (expr->token.type)
        {
        case IC_TOK_MINUS:
        {
            return { nullptr, evaluate_add(value, produce_numeric_value(IC_TYPE_S8, -1))};
        }
        case IC_TOK_BANG:
        {
            ic_value output;
            output.type = { IC_TYPE_BOOL, 0 };
            output.s8 = !to_boolean(value);
            return { nullptr, output };
        }
        case IC_TOK_MINUS_MINUS:
        {
            ic_value output = evaluate_add(value, produce_numeric_value(IC_TYPE_S8, -1));
            assert(output.type == value.type);
            set_lvalue_data(lvalue_data, output);
            return { lvalue_data, output };
        }
        case IC_TOK_PLUS_PLUS:
        {
            ic_value output = evaluate_add(value, produce_numeric_value(IC_TYPE_S8, 1));
            assert(output.type == value.type);
            set_lvalue_data(lvalue_data, output);
            return { lvalue_data, output };
        }
        case IC_TOK_AMPERSAND:
        {
            assert(lvalue_data);
            ic_value output;
            output.type = { value.type.basic_type, value.type.indirection_level + 1 };
            output.pointer = lvalue_data;
            return { nullptr, output };
        }
        case IC_TOK_STAR:
        {
            assert(value.type.indirection_level);
            assert(value.type.basic_type != IC_TYPE_NULLPTR);
            ic_value output;
            output.type = { value.type.basic_type, value.type.indirection_level - 1 };

            // the exact amount of data needs to be read to not trigger segmentation fault, without any conversion
            if (value.type.indirection_level > 1 || value.type.basic_type == IC_TYPE_F64)
            {
                assert(value.pointer); // runtime error, not type error
                // pointer to pointer or pointer to double; both pointer and double are 8 bytes
                output.pointer = *(void**)value.pointer;
            }
            else
            {
                switch (value.type.basic_type)
                {
                case IC_TYPE_BOOL:
                case IC_TYPE_S8:
                case IC_TYPE_U8:
                    output.s8 = *(char*)value.pointer;
                    break;
                case IC_TYPE_S32:
                case IC_TYPE_U32:
                case IC_TYPE_F32:
                    output.s32 = *(int*)value.pointer;
                    break;
                default:
                    assert(false);
                }
            }

            return { value.pointer, output };
        }
        } // switch

        assert(false);
    }
    case IC_EXPR_PARENTHESES:
    {
        return ic_evaluate_expr(expr->_parentheses.expr, runtime);
    }
    case IC_EXPR_FUNCTION_CALL:
    {
        ic_token token = expr->token;
        ic_function* function = runtime.get_function(token.string);
        assert(function);

        int argc = 0;
        ic_value argv[IC_MAX_ARGC];
        ic_expr* expr_arg = expr->_function_call.arg;

        while (expr_arg)
        {
            assert(argc < IC_MAX_ARGC);
            ic_expr_result result = ic_evaluate_expr(expr_arg, runtime);
            expr_arg = expr_arg->next;
            argv[argc] = result.value;
            argc += 1;
        }

        if (function->type == IC_FUN_SOURCE || function->host.type_check)
        {
            assert(argc == function->param_count);

            for (int i = 0; i < argc; ++i)
            {
                assert(argv[i].type == function->params[i].type);
                assert(argv[i].type.indirection_level || argv[i].type.basic_type != IC_TYPE_VOID);
            }
        }

        if (function->type == IC_FUN_HOST)
        {
            assert(function->host.callback);
            ic_value value = function->host.callback(argc, argv);
            // execution will fail because of a host fault
            assert(value.type == function->return_type);
            // function can never return an lvalue
            return { nullptr, value };
        }
        else
        {
            assert(function->type == IC_FUN_SOURCE);
            // all arguments must be retrieved before upper scopes are hidden
            runtime.push_scope(true);

            for(int i = 0; i < argc; ++i)
                runtime.add_var(function->params[i].name, argv[i]);

            ic_expr_result output;
            output.lvalue_data = nullptr;
            ic_stmt_result fun_result = ic_execute_stmt(function->body, runtime);

            if (fun_result.type == IC_STMT_RESULT_RETURN)
            {
                assert(fun_result.value.type == function->return_type);
                output.lvalue_data = nullptr;
                output.value = fun_result.value;
            }
            else
            {
                // function without return statement
                assert(fun_result.type == IC_STMT_RESULT_NOP);
                assert(function->return_type.basic_type == IC_TYPE_VOID && !function->return_type.indirection_level);
                // this value should never be used (type pass should terminate)
                // this is only for bug detection, e.g. failing in to_boolean()
                output.value.type = { IC_TYPE_VOID, 0 };
            }

            runtime.pop_scope();
            return output;
        }
    }
    case IC_EXPR_PRIMARY:
    {
        ic_token token = expr->token;

        // todo; string literal
        switch (token.type)
        {
        case IC_TOK_INT_NUMBER:
        {
            // todo; decide which type to use (signed or unsigned) based on the number
            ic_expr_result output;
            output.lvalue_data = nullptr;
            output.value.type = { IC_TYPE_S32, 0 };
            output.value.s32 = token.number;
            return output;
        }
        case IC_TOK_FLOAT_NUMBER:
        {
            ic_expr_result output;
            output.lvalue_data = nullptr;
            output.value.type = { IC_TYPE_F64, 0 };
            output.value.f64 = token.number;
            return output;
        }
        case IC_TOK_IDENTIFIER:
        {
            ic_value* value = runtime.get_var(token.string);
            assert(value);
            // all union members start at the same address
            return { &value->u8, *value };
        }
        case IC_TOK_TRUE:
        case IC_TOK_FALSE:
        {
            ic_expr_result output;
            output.lvalue_data = nullptr;
            output.value.type = { IC_TYPE_BOOL, 0 };
            output.value.s8 = token.type == IC_TOK_TRUE ? 1 : 0;
            return output;
        }
        case IC_TOK_NULLPTR:
        {
            ic_expr_result output;
            output.lvalue_data = nullptr;
            output.value.type = { IC_TYPE_NULLPTR, 1 };
            output.value.pointer = nullptr;
            return output;
        }
        } // switch

        assert(false);
    }
    } // switch

    assert(false);
    return {};
}

double get_numeric_data(ic_value value)
{
    assert(!value.type.indirection_level);

    switch (value.type.basic_type)
    {
    case IC_TYPE_BOOL:
    case IC_TYPE_S8:
        return value.s8;
    case IC_TYPE_U8:
        return value.u8;
    case IC_TYPE_S32:
        return value.s32;
    case IC_TYPE_U32:
        return value.u32;
    case IC_TYPE_F32:
        return value.f32;
    case IC_TYPE_F64:
        return value.f64;
    }

    assert(false);
    return {};
}

void set_numeric_data(ic_value& value, double number)
{
    assert(!value.type.indirection_level);

    // cast operators are used to silence warnings
    switch (value.type.basic_type)
    {
    case IC_TYPE_BOOL:
        value.s8 = (char)number ? true : false;
        return;
    case IC_TYPE_S8:
        value.s8 = (char)number;
        return;
    case IC_TYPE_U8:
        value.u8 = (unsigned char)number;
        return;
    case IC_TYPE_S32:
        value.s32 = (int)number;
        return;
    case IC_TYPE_U32:
        value.u32 = (unsigned int)number;
        return;
    case IC_TYPE_F32:
        value.f32 = (float)number;
        return;
    case IC_TYPE_F64:
        value.f64 = (double)number;
        return;
    }

    assert(false);
}

ic_basic_type get_numeric_expr_result_type(ic_value left, ic_value right)
{
    assert(!left.type.indirection_level);
    assert(!right.type.indirection_level);
    ic_basic_type lbtype = left.type.basic_type, rbtype = right.type.basic_type;
    return lbtype > rbtype ? lbtype : rbtype;
}

ic_value produce_numeric_value(ic_basic_type btype, double number)
{
    ic_value value;
    value.type = { btype, 0 };
    set_numeric_data(value, number);
    return value;
}

ic_value implicit_convert_type(ic_value_type target_type, ic_value value)
{
    assert(target_type.indirection_level == value.type.indirection_level);

    if (target_type.basic_type == value.type.basic_type)
        return value;

    ic_value output;
    output.type = target_type;

    if (target_type.indirection_level)
    {
        if (value.type.basic_type == IC_TYPE_NULLPTR || target_type.basic_type == IC_TYPE_VOID)
        {
            output.pointer = value.pointer;
            return output;
        }

        assert(false);
    }

    // else - numeric conversion
    set_numeric_data(output, get_numeric_data(value));
    return output;
}

// the main point of this function is to write the right amount of data (to not trigger segmentation fault)
// without any conversion
void set_lvalue_data(void* lvalue_data, ic_value value)
{
    assert(lvalue_data);

    if (value.type.indirection_level || value.type.basic_type == IC_TYPE_F64) // pointer and double are both 8 bytes
    {
        *(double*)lvalue_data = value.f64;
        return;
    }

    switch (value.type.basic_type)
    {
    case IC_TYPE_BOOL:
    case IC_TYPE_S8:
    case IC_TYPE_U8:
        *(char*)lvalue_data = value.s8;
        return;
    case IC_TYPE_S32:
    case IC_TYPE_U32:
    case IC_TYPE_F32:
        *(int*)lvalue_data = value.s32;
        return;
    }
    
    assert(false);
}

ic_value evaluate_add(ic_value left, ic_value right, bool subtract)
{
    if (left.type.indirection_level)
    {
        ic_basic_type rbtype = right.type.basic_type;
        // only integer types can be added to a pointer
        assert(rbtype == IC_TYPE_U8 || rbtype == IC_TYPE_S8 || rbtype == IC_TYPE_U32 || rbtype == IC_TYPE_S32); 
        ptrdiff_t offset = subtract ? get_numeric_data(right) : -get_numeric_data(right);

        if (left.type.indirection_level > 1 || left.type.basic_type == IC_TYPE_F64) // pointer to pointer or pointer to double
            left.pointer = (void**)left.pointer + offset;
        else
        {
            switch (left.type.basic_type)
            {
            case IC_TYPE_BOOL:
            case IC_TYPE_S8:
            case IC_TYPE_U8:
                left.pointer = (char*)left.pointer + offset;
            case IC_TYPE_S32:
            case IC_TYPE_U32:
            case IC_TYPE_F32:
                left.pointer = (int*)left.pointer + offset;
            default:
                assert(false);
            }
        }

        return left;
    }

    double lhs_number = get_numeric_data(left);
    double rhs_number = subtract ? -get_numeric_data(right) : get_numeric_data(right);
    return produce_numeric_value(get_numeric_expr_result_type(left, right), lhs_number + rhs_number);
}

bool compare_equal(ic_value left, ic_value right)
{
    if (left.type.indirection_level)
    {
        assert(right.type.indirection_level);
        bool is_nullptr = left.type.basic_type == IC_TYPE_NULLPTR || right.type.basic_type == IC_TYPE_NULLPTR;
        bool is_void = left.type.basic_type == IC_TYPE_VOID || right.type.basic_type == IC_TYPE_VOID;

        if (!is_nullptr && !is_void) // void* and nullptr can compare with any other type
            assert(left.type.basic_type == right.type.basic_type);

        return left.pointer == right.pointer;
    }

    return get_numeric_data(left) == get_numeric_data(right);
}

bool compare_greater(ic_value left, ic_value right)
{
    if (left.type.indirection_level)
    {
        assert(right.type.indirection_level);
        bool is_nullptr = left.type.basic_type == IC_TYPE_NULLPTR || right.type.basic_type == IC_TYPE_NULLPTR;
        bool is_void = left.type.basic_type == IC_TYPE_VOID || right.type.basic_type == IC_TYPE_VOID;

        if (!is_nullptr && !is_void) // void* and nullptr can compare with any other type
            assert(left.type.basic_type == right.type.basic_type);

        return left.pointer > right.pointer;
    }

    return get_numeric_data(left) > get_numeric_data(right);
}

struct ic_lexer
{
    std::vector<ic_token>& _tokens;
    int _line = 1;
    int _col = 0;
    int _token_col;
    int _token_line;
    const char* _source_it;

    void add_token_impl(ic_token_type type, ic_string string = { nullptr, 0 }, double number = 0.0)
    {
        ic_token token;
        token.type = type;
        token.col = _token_col;
        token.line = _token_line;

        if (string.data)
            token.string = string;
        else
            token.number = number;

        _tokens.push_back(token);
    }

    void add_token(ic_token_type type) { add_token_impl(type); }
    void add_token(ic_token_type type, ic_string string) { add_token_impl(type, string); }
    void add_token(ic_token_type type, double number) { add_token_impl(type, { nullptr, 0 }, number); }
    bool end() { return *_source_it == '\0'; }
    char peek() { return *_source_it; }
    char peek_second() { return *(_source_it + 1); }
    const char* prev_it() { return _source_it - 1; }

    char advance()
    {
        ++_source_it;
        const char c = *(_source_it - 1);

        if (c == '\n')
        {
            ++_line;
            _col = 0;
        }
        else
            ++_col;

        return c;
    }

    bool consume(char c)
    {
        if (end())
            return false;

        if (*_source_it != c)
            return false;

        advance();
        return true;
    }
};

bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

bool is_alphanumeric(char c)
{
    return is_digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_');
}

bool ic_tokenize(std::vector<ic_token>& tokens, const char* source_code)
{
    ic_lexer lexer{ tokens };
    lexer._source_it = source_code;
    bool success = true;

    while (!lexer.end())
    {
        const char c = lexer.advance();
        lexer._token_col = lexer._col;
        lexer._token_line = lexer._line;
        const char* const begin_it = lexer.prev_it();

        switch (c)
        {
        case ' ':
        case '\t':
        case '\n':
            break;
        case '(':
            lexer.add_token(IC_TOK_LEFT_PAREN);
            break;
        case ')':
            lexer.add_token(IC_TOK_RIGHT_PAREN);
            break;
        case '{':
            lexer.add_token(IC_TOK_LEFT_BRACE);
            break;
        case '}':
            lexer.add_token(IC_TOK_RIGHT_BRACE);
            break;
        case ';':
            lexer.add_token(IC_TOK_SEMICOLON);
            break;
        case ',':
            lexer.add_token(IC_TOK_COMMA);
            break;
        case '!':
            lexer.add_token(lexer.consume('=') ? IC_TOK_BANG_EQUAL : IC_TOK_BANG);
            break;
        case '=':
            lexer.add_token(lexer.consume('=') ? IC_TOK_EQUAL_EQUAL : IC_TOK_EQUAL);
            break;
        case '>':
            lexer.add_token(lexer.consume('=') ? IC_TOK_GREATER_EQUAL : IC_TOK_GREATER);
            break;
        case '<':
            lexer.add_token(lexer.consume('=') ? IC_TOK_LESS_EQUAL : IC_TOK_LESS);
            break;
        case '&':
            lexer.add_token(lexer.consume('&') ? IC_TOK_AMPERSAND_AMPERSAND : IC_TOK_AMPERSAND);
            break;
        case '*':
        {
            if (lexer.consume('='))
                lexer.add_token(IC_TOK_STAR_EQUAL);
            else
                lexer.add_token(IC_TOK_STAR);

            break;
        }
        case '-':
        {
            if (lexer.consume('='))
                lexer.add_token(IC_TOK_MINUS_EQUAL);
            else if (lexer.consume('-'))
                lexer.add_token(IC_TOK_MINUS_MINUS);
            else
                lexer.add_token(IC_TOK_MINUS);

            break;
        }
        case '+':
        {
            if (lexer.consume('='))
                lexer.add_token(IC_TOK_PLUS_EQUAL);
            else if (lexer.consume('+'))
                lexer.add_token(IC_TOK_PLUS_PLUS);
            else
                lexer.add_token(IC_TOK_PLUS);

            break;
        }
        case '|':
        {
            if (lexer.consume('|'))
                lexer.add_token(IC_TOK_VBAR_VBAR);
            break;
        }
        case '/':
        {
            if (lexer.consume('='))
                lexer.add_token(IC_TOK_SLASH_EQUAL);
            else if (lexer.consume('/'))
            {
                while (!lexer.end() && lexer.advance() != '\n')
                    ;
            }
            else
                lexer.add_token(IC_TOK_SLASH);

            break;
        }
        case '"':
        {
            while (!lexer.end() && lexer.advance() != '"')
                ;

            if (lexer.end() && lexer.advance() != '"')
            {
                success = false;
                ic_print_error(IC_ERR_LEXING, lexer._token_line, lexer._token_col, "unterminated string literal");
            }
            else
            {
                const char* data = begin_it + 1;
                lexer.add_token(IC_TOK_STRING, { data, int(lexer.prev_it() - data) });
            }
            break;
        }
        default:
        {
            if (is_digit(c))
            {
                while (!lexer.end() && is_digit(lexer.peek()))
                    lexer.advance();

                ic_token_type token_type = IC_TOK_INT_NUMBER;

                if (lexer.peek() == '.' && is_digit(lexer.peek_second()))
                {
                    token_type = IC_TOK_FLOAT_NUMBER;
                    lexer.advance();
                }

                while (!lexer.end() && is_digit(lexer.peek()))
                    lexer.advance();

                const int len = lexer.prev_it() - begin_it + 1;
                char buf[1024];
                assert(len < sizeof(buf));
                memcpy(buf, begin_it, len);
                buf[len] = '\0';
                lexer.add_token(token_type, atof(buf));
            }
            else if (is_alphanumeric(c))
            {
                while (!lexer.end() && is_alphanumeric(lexer.peek()))
                    lexer.advance();

                const int len = lexer.prev_it() - begin_it + 1;
                bool is_keyword = false;
                ic_token_type token_type;

                for (const ic_keyword& keyword : _keywords)
                {
                    if (ic_string_compare({ begin_it, len }, { keyword.str, int(strlen(keyword.str)) }))
                    {
                        is_keyword = true;
                        token_type = keyword.token_type;
                        break;
                    }
                }

                if (!is_keyword)
                    lexer.add_token(IC_TOK_IDENTIFIER, { begin_it, int(lexer.prev_it() - begin_it + 1) });
                else
                    lexer.add_token(token_type);
            }
            else
            {
                success = false;
                ic_print_error(IC_ERR_LEXING, lexer._line, lexer._col, "unexpected character '%c'", c);
            }
        }
        } // switch
    }

    assert(lexer._tokens.size());
    ic_token token;
    token.type = IC_TOK_EOF;
    token.col = lexer._tokens.back().col + 1;
    token.line = lexer._tokens.back().line;
    lexer._tokens.push_back(token);
    return success;
}

enum ic_op_precedence
{
    IC_PRECEDENCE_LOGICAL_OR,
    IC_PRECEDENCE_LOGICAL_AND,
    IC_PRECEDENCE_COMPARE_EQUAL,
    IC_PRECEDENCE_COMPARE_GREATER,
    IC_PRECEDENCE_ADD,
    IC_PRECEDENCE_MULTIPLY,
    IC_PRECEDENCE_UNARY,
};

// grammar, production rules hierarchy

ic_global produce_global(const ic_token** it, ic_runtime& runtime);
ic_stmt* produce_stmt(const ic_token** it, ic_runtime& runtime);
ic_stmt* produce_stmt_var_declaration(const ic_token** it, ic_runtime& runtime);
ic_expr* produce_expr_stmt(const ic_token** it, ic_runtime& runtime);
ic_expr* produce_expr(const ic_token** it, ic_runtime& runtime);
ic_expr* produce_expr_assignment(const ic_token** it, ic_runtime& runtime);
ic_expr* produce_expr_binary(const ic_token** it, ic_op_precedence precedence, ic_runtime& runtime);
ic_expr* produce_expr_unary(const ic_token** it, ic_runtime& runtime);
ic_expr* produce_expr_primary(const ic_token** it, ic_runtime& runtime);

void token_advance(const ic_token** it) { ++(*it); }

bool token_consume(const ic_token** it, ic_token_type type, const char* err_msg = nullptr)
{
    if ((**it).type == type)
    {
        token_advance(it);
        return true;
    }

    if (err_msg)
    {
        ic_print_error(IC_ERR_PARSING, (**it).line, (**it).col, err_msg);
        throw ic_exception_parsing{};
    }

    return false;
}

void exit_parsing(const ic_token** it, const char* err_msg, ...)
{
    va_list list;
    va_start(list, err_msg);
    ic_print_error(IC_ERR_PARSING, (**it).line, (**it).col, err_msg, list);
    va_end(list);
    throw ic_exception_parsing{};
}

bool produce_type(const ic_token** it, ic_runtime& runtime, ic_value_type& type)
{
    switch ((**it).type)
    {
    case IC_TOK_BOOL:
        type.basic_type = IC_TYPE_BOOL;
    case IC_TOK_S8:
        type.basic_type = IC_TYPE_S8;
        break;
    case IC_TOK_U8:
        type.basic_type = IC_TYPE_U8;
        break;
    case IC_TOK_S32:
        type.basic_type = IC_TYPE_S32;
        break;
    case IC_TOK_U32:
        type.basic_type = IC_TYPE_U32;
        break;
    case IC_TOK_F32:
        type.basic_type = IC_TYPE_F32;
        break;
    case IC_TOK_F64:
        type.basic_type = IC_TYPE_F64;
        break;
    case IC_TOK_VOID:
        type.basic_type = IC_TYPE_VOID;
        break;
    default:
        return false;
    }

    token_advance(it);
    type.indirection_level = 0;

    while (token_consume(it, IC_TOK_STAR))
        type.indirection_level += 1;

    return true;
}

// todo; struct, enum, etc.
ic_global produce_global(const ic_token** it, ic_runtime& runtime)
{
    ic_value_type return_type;

    if (!produce_type(it, runtime, return_type))
        exit_parsing(it, "expected return type");

    ic_global global;
    global.type = IC_GLOBAL_FUNCTION;
    ic_function& function = global.function;
    function.type = IC_FUN_SOURCE;
    function.token = **it;
    token_consume(it, IC_TOK_IDENTIFIER, "expected function name");
    function.return_type = return_type;
    function.param_count = 0;
    token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after function name");

    if (!token_consume(it, IC_TOK_RIGHT_PAREN))
    {
        for (;;)
        {
            if (function.param_count >= IC_MAX_ARGC)
                exit_parsing(it, "exceeded maximal number of parameters (%d)", IC_MAX_ARGC);

            ic_value_type param_type;

            if (!produce_type(it, runtime, param_type))
                exit_parsing(it, "expected parameter type");

            function.params[function.param_count].type = param_type;
            function.params[function.param_count].name = (**it).string;
            token_consume(it, IC_TOK_IDENTIFIER, "expected parameter name");
            function.param_count += 1;

            if (token_consume(it, IC_TOK_RIGHT_PAREN))
                break;

            token_consume(it, IC_TOK_COMMA, "expected ',' or ')' after a function argument");
        }
    }

    function.body = produce_stmt(it, runtime);

    if (function.body->type != IC_STMT_COMPOUND)
        exit_parsing(it, "expected compound stmt after function parameter list");

    function.body->_compound.push_scope = false; // do not allow shadowing of arguments
    return global;
}

ic_stmt* produce_stmt(const ic_token** it, ic_runtime& runtime)
{
    switch ((**it).type)
    {
    case IC_TOK_LEFT_BRACE:
    {
        token_advance(it);
        ic_stmt* stmt = runtime.allocate_stmt(IC_STMT_COMPOUND);
        stmt->_compound.push_scope = true;
        ic_stmt** body_tail  = &(stmt->_compound.body);

        while ((**it).type != IC_TOK_RIGHT_BRACE && (**it).type != IC_TOK_EOF)
        {
            *body_tail = produce_stmt(it, runtime);
            body_tail = &((*body_tail)->next);
        }

        token_consume(it, IC_TOK_RIGHT_BRACE, "expected '}' to close a compound statement");
        return stmt;
    }
    case IC_TOK_WHILE:
    {
        token_advance(it);
        ic_stmt* stmt = runtime.allocate_stmt(IC_STMT_FOR);
        token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after while keyword");
        stmt->_for.header2 = produce_expr(it, runtime);
        token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after while condition");
        stmt->_for.body = produce_stmt(it, runtime);
        return stmt;
    }
    case IC_TOK_FOR:
    {
        token_advance(it);
        ic_stmt* stmt = runtime.allocate_stmt(IC_STMT_FOR);
        token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after for keyword");
        stmt->_for.header1 = produce_stmt_var_declaration(it, runtime); // only in header1 var declaration is allowed
        stmt->_for.header2 = produce_expr_stmt(it, runtime);

        if ((**it).type != IC_TOK_RIGHT_PAREN)
            stmt->_for.header3 = produce_expr(it, runtime); // this one should not end with ';', that's why we don't use produce_stmt_expr()

        token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after for header");
        stmt->_for.body = produce_stmt(it, runtime);

        // prevent shadowing of header variable
        if (stmt->_for.body->type == IC_STMT_COMPOUND)
            stmt->_for.body->_compound.push_scope = false;

        return stmt;
    }
    case IC_TOK_IF:
    {
        token_advance(it);
        ic_stmt* stmt = runtime.allocate_stmt(IC_STMT_IF);
        token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after if keyword");
        stmt->_if.header = produce_expr(it, runtime);

        if (stmt->_if.header->token.type == IC_TOK_EQUAL)
            exit_parsing(it, "assignment expression can't be used directly in if header, encolse it with ()");

        token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after if condition");
        stmt->_if.body_if = produce_stmt(it, runtime);

        if (token_consume(it, IC_TOK_ELSE))
            stmt->_if.body_else = produce_stmt(it, runtime);

        return stmt;
    }
    case IC_TOK_RETURN:
    {
        token_advance(it);
        ic_stmt* stmt = runtime.allocate_stmt(IC_STMT_RETURN);
        stmt->_return.expr = produce_expr_stmt(it, runtime);
        return stmt;
    }
    case IC_TOK_BREAK:
    {
        token_advance(it);
        token_consume(it, IC_TOK_SEMICOLON, "expected ';' after break keyword");
        return runtime.allocate_stmt(IC_STMT_BREAK);
    }
    case IC_TOK_CONTINUE:
    {
        token_advance(it);
        token_consume(it, IC_TOK_SEMICOLON, "expected ';' after continue keyword");
        return runtime.allocate_stmt(IC_STMT_CONTINUE);
    }
    } // switch

    return produce_stmt_var_declaration(it, runtime);
}

// this function is seperate from produce_expr_assignment so we don't allow(): var x = var y = 5;
// and: while(var x = 6);
ic_stmt* produce_stmt_var_declaration(const ic_token** it, ic_runtime& runtime)
{
    ic_value_type type;

    if (produce_type(it, runtime, type))
    {
        if (type.basic_type == IC_TYPE_VOID && !type.indirection_level)
            exit_parsing(it, "variables can't be of type void");

        ic_stmt* stmt = runtime.allocate_stmt(IC_STMT_VAR_DECLARATION);
        stmt->_var_declaration.type = type;
        stmt->_var_declaration.token = **it;
        token_consume(it, IC_TOK_IDENTIFIER, "expected variable name");

        if (token_consume(it, IC_TOK_EQUAL))
        {
            stmt->_var_declaration.expr = produce_expr_stmt(it, runtime);
            return stmt;
        }

        token_consume(it, IC_TOK_SEMICOLON, "expected ';' or '=' after variable name");
        return stmt;
    }
    else
    {
        ic_stmt* stmt = runtime.allocate_stmt(IC_STMT_EXPR);
        stmt->_expr = produce_expr_stmt(it, runtime);
        return stmt;
    }
}

ic_expr* produce_expr_stmt(const ic_token** it, ic_runtime& runtime)
{
    if (token_consume(it, IC_TOK_SEMICOLON))
        return nullptr;

    ic_expr* expr = produce_expr(it, runtime);
    token_consume(it, IC_TOK_SEMICOLON, "expected ';' after expression");
    return expr;
}

ic_expr* produce_expr(const ic_token** it, ic_runtime& runtime)
{
    return produce_expr_assignment(it, runtime);
}

// produce_expr_assignment() - grows down and right
// produce_expr_binary() - grows up and right (given operators with the same precedence)

ic_expr* produce_expr_assignment(const ic_token** it, ic_runtime& runtime)
{
    ic_expr* expr_left = produce_expr_binary(it, IC_PRECEDENCE_LOGICAL_OR, runtime);

    switch ((**it).type)
    {
    case IC_TOK_EQUAL:
    case IC_TOK_PLUS_EQUAL:
    case IC_TOK_MINUS_EQUAL:
    case IC_TOK_STAR_EQUAL:
    case IC_TOK_SLASH_EQUAL:
        ic_expr* expr = runtime.allocate_expr(IC_EXPR_BINARY, **it);
        token_advance(it);
        expr->_binary.right = produce_expr(it, runtime);
        expr->_binary.left = expr_left;
        return expr;
    }

    return expr_left;
}

ic_expr* produce_expr_binary(const ic_token** it, ic_op_precedence precedence, ic_runtime& runtime)
{
    ic_token_type target_token_types[5] = {}; // this is important, initialize all elements to IC_TOK_EOF

    switch (precedence)
    {
    case IC_PRECEDENCE_LOGICAL_OR:
    {
        target_token_types[0] = IC_TOK_VBAR_VBAR;
        break;
    }
    case IC_PRECEDENCE_LOGICAL_AND:
    {
        target_token_types[0] = IC_TOK_AMPERSAND_AMPERSAND;
        break;
    }
    case IC_PRECEDENCE_COMPARE_EQUAL:
    {
        target_token_types[0] = IC_TOK_EQUAL_EQUAL;
        target_token_types[1] = IC_TOK_BANG_EQUAL;
        break;
    }
    case IC_PRECEDENCE_COMPARE_GREATER:
    {
        target_token_types[0] = IC_TOK_GREATER;
        target_token_types[1] = IC_TOK_GREATER_EQUAL;
        target_token_types[2] = IC_TOK_LESS;
        target_token_types[3] = IC_TOK_LESS_EQUAL;
        break;
    }
    case IC_PRECEDENCE_ADD:
    {
        target_token_types[0] = IC_TOK_PLUS;
        target_token_types[1] = IC_TOK_MINUS;
        break;
    }
    case IC_PRECEDENCE_MULTIPLY:
    {
        target_token_types[0] = IC_TOK_STAR;
        target_token_types[1] = IC_TOK_SLASH;
        break;
    }
    case IC_PRECEDENCE_UNARY:
        return produce_expr_unary(it, runtime);
    }

    ic_expr* expr = produce_expr_binary(it, ic_op_precedence(int(precedence) + 1), runtime);

    for (;;)
    {
        const ic_token_type* target_token_type  = target_token_types;
        bool match = false;

        while (*target_token_type != IC_TOK_EOF)
        {
            if (*target_token_type == (**it).type)
            {
                match = true;
                break;
            }
            ++target_token_type;
        }

        if (!match)
            break;

        // operator matches given precedence
        ic_expr* expr_parent = runtime.allocate_expr(IC_EXPR_BINARY, **it);
        token_advance(it);
        expr_parent->_binary.right = produce_expr_binary(it, ic_op_precedence(int(precedence) + 1), runtime);
        expr_parent->_binary.left = expr;
        expr = expr_parent;
    }

    return expr;
}

ic_expr* produce_expr_unary(const ic_token** it, ic_runtime& runtime)
{
    switch ((**it).type)
    {
    case IC_TOK_BANG:
    case IC_TOK_MINUS:
    case IC_TOK_PLUS_PLUS:
    case IC_TOK_MINUS_MINUS:
    case IC_TOK_AMPERSAND:
    case IC_TOK_STAR:
    {
        ic_expr* expr = runtime.allocate_expr(IC_EXPR_UNARY, **it);
        token_advance(it);
        expr->_unary.expr = produce_expr_unary(it, runtime);
        return expr;
    }
    } // switch

    return produce_expr_primary(it, runtime);
}

ic_expr* produce_expr_primary(const ic_token** it, ic_runtime& runtime)
{
    switch ((**it).type)
    {
    case IC_TOK_INT_NUMBER:
    case IC_TOK_FLOAT_NUMBER:
    case IC_TOK_STRING:
    case IC_TOK_IDENTIFIER:
    case IC_TOK_TRUE:
    case IC_TOK_FALSE:
    case IC_TOK_NULLPTR:
    {
        ic_token token_id = **it;
        token_advance(it);

        if (token_consume(it, IC_TOK_LEFT_PAREN))
        {
            ic_expr* expr = runtime.allocate_expr(IC_EXPR_FUNCTION_CALL, token_id);

            if (token_consume(it, IC_TOK_RIGHT_PAREN))
                return expr;

            ic_expr** arg_tail = &expr->_function_call.arg;
            int argc = 0;

            for(;;)
            {
                *arg_tail = produce_expr(it, runtime);
                arg_tail = &((*arg_tail)->next);
                ++argc;

                if(argc > IC_MAX_ARGC)
                    exit_parsing(it, "exceeded maximal number of arguments (%d)", IC_MAX_ARGC);

                if (token_consume(it, IC_TOK_RIGHT_PAREN))
                    break;

                token_consume(it, IC_TOK_COMMA, "expected ',' or ')' after a function argument");
            }

            return expr;
        }

        return runtime.allocate_expr(IC_EXPR_PRIMARY, token_id);
    }
    case IC_TOK_LEFT_PAREN:
    {
        ic_expr* expr = runtime.allocate_expr(IC_EXPR_PARENTHESES, **it);
        token_advance(it);
        expr->_parentheses.expr = produce_expr(it, runtime);
        token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after expression");
        return expr;
    }
    } // switch

    exit_parsing(it, "expected literal / indentifier / parentheses / function call");
    return {};
}
