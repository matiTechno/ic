#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <stdarg.h>

// ic - interpreted c
// todo
// all api functions should be named ic_
// all implementation functions / enums / etc. should be named ic_p_
// comma operator
// C types; type checking pass
// print source code line on error
// execution should always succeed
// type casting operator
// somehow support multithreading? run function ast on separate thread?

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

#define IC_MAX_ARGC 10

enum ic_token_type
{
    IC_TOK_EOF,
    IC_TOK_LEFT_PAREN,
    IC_TOK_RIGHT_PAREN,
    IC_TOK_LEFT_BRACE,
    IC_TOK_RIGHT_BRACE,
    IC_TOK_MINUS,
    IC_TOK_PLUS,
    IC_TOK_SEMICOLON,
    IC_TOK_SLASH,
    IC_TOK_STAR,
    IC_TOK_BANG,
    IC_TOK_BANG_EQUAL,
    IC_TOK_EQUAL,
    IC_TOK_EQUAL_EQUAL,
    IC_TOK_GREATER,
    IC_TOK_GREATER_EQUAL,
    IC_TOK_LESS,
    IC_TOK_LESS_EQUAL,
    IC_TOK_IDENTIFIER,
    IC_TOK_STRING,
    IC_TOK_NUMBER,
    IC_TOK_AND,
    IC_TOK_ELSE,
    IC_TOK_FOR,
    IC_TOK_IF,
    IC_TOK_OR,
    IC_TOK_RETURN,
    IC_TOK_WHILE,
    IC_TOK_VAR,
    IC_TOK_PLUS_EQUAL,
    IC_TOK_MINUS_EQUAL,
    IC_TOK_STAR_EQUAL,
    IC_TOK_SLASH_EQUAL,
    IC_TOK_PLUS_PLUS,
    IC_TOK_MINUS_MINUS,
    IC_TOK_BREAK,
    IC_TOK_CONTINUE,
    IC_TOK_COMMA,
    IC_TOK_AMPERSAND,
};

enum ic_ast_node_type
{
    IC_AST_BLOCK,
    IC_AST_FOR, // while is implemented as a for statement
    IC_AST_IF,
    IC_AST_RETURN,
    IC_AST_BREAK,
    IC_AST_CONTINUE,
    IC_AST_VAR_DEFINITION,
    IC_AST_BINARY,
    IC_AST_UNARY,
    IC_AST_FUNCTION_CALL,
    // grouping exists so if(x=3) doesn't compile
    // and if((x=3)) compiles
    IC_AST_GROUPING,
    IC_AST_PRIMARY,
};

enum ic_value_type
{
    IC_VAL_NUMBER,
};

enum ic_error_type
{
    IC_ERR_LEXING,
    IC_ERR_PARSING,
    IC_ERR_EXECUTION,
};

struct ic_keyword
{
    const char* str;
    ic_token_type token_type;
};

static ic_keyword _keywords[] = {
    {"and", IC_TOK_AND},
    {"else", IC_TOK_ELSE},
    {"for", IC_TOK_FOR},
    {"if", IC_TOK_IF},
    {"return", IC_TOK_RETURN},
    {"or", IC_TOK_OR},
    {"var", IC_TOK_VAR},
    {"while", IC_TOK_WHILE},
    {"break", IC_TOK_BREAK},
    {"continue", IC_TOK_CONTINUE},
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

struct ic_ast_node
{
    ic_ast_node_type type;

    union
    {
        struct
        {
            ic_ast_node* node;
            ic_ast_node* next;
            // todo; better design?
            bool push_scope;
        } _block;

        struct
        {
            ic_ast_node* header1;
            ic_ast_node* header2;
            ic_ast_node* header3;
            ic_ast_node* body;
            ic_token token_header2; // for error reporting
        } _for;

        struct
        {
            ic_ast_node* header;
            ic_ast_node* body_if;
            ic_ast_node* body_else;
            ic_token token_header; // for error reporting
        } _if;

        struct
        {
            ic_ast_node* node;
        } _return;

        struct
        {
            ic_ast_node* node;
            ic_token token_id;
            int indirection_count;
        } _var_definition;

        struct
        {
            ic_ast_node* left;
            ic_ast_node* right;
            ic_token token_operator;
        } _binary;

        struct
        {
            ic_ast_node* node;
            ic_token token_operator;
        } _unary;

        struct
        {
            ic_ast_node* node;
            ic_ast_node* next;
        } _function_call;

        struct
        {
            ic_ast_node* node;
        } _grouping;

        struct
        {
            ic_token token;
        } _primary;
    };
};

struct ic_value
{
    ic_value_type type;
    int indirection_count;

    union
    {
        double number;
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

using ic_host_function = ic_value(*)(int argc, ic_value* argv);

enum ic_function_type
{
    IC_FUN_HOST,
    IC_FUN_SOURCE,
};

struct ic_function
{
    ic_function_type type;
    ic_string name;

    union
    {
        // todo; function signature for type checking
        ic_host_function host;
        struct
        {
            int param_count;
            ic_string params[IC_MAX_ARGC];
            ic_ast_node* node_body;
        } source;
    };
};

// todo; union?
enum ic_global_type
{
    IC_GLOBAL_FUNCTION_DEFINITION,
    IC_GLOBAL_FUNCTION_DECLARATION,
    IC_GLOBAL_STRUCTURE_DEFINITION,
    IC_GLOBAL_STRUCTURE_DECLARATION,
    IC_GLOBAL_VAR_DEFINITION,
    IC_GLOBAL_VAR_DECLARATION,
    IC_GLOBAL_ENUM,
};

struct ic_global
{
    ic_global_type type;

    union
    {
        ic_function function;
    };
};

struct ic_exe_result
{
    // todo; redundant?
    void* lvalue_data_address;
    ic_value value;
};

struct ic_exception_parsing {};
struct ic_exception_execution {};
struct ic_exception_break {};
struct ic_exception_continue {};
struct ic_exception_return
{
    ic_value value;
};

struct ic_runtime
{
    void init();
    void add_global_var(const char* name, ic_value value);
    void add_host_function(const char* name, ic_host_function function);
    bool run(const char* source); // after run() variables can be extracted
    ic_value* get_global_var(const char* name);
    void clear_global_vars(); // use this before next run()
    void clear_host_functions(); // useful when e.g. running different script
    void free();

    // implementation
    ic_deque<ic_ast_node, 1000> _ast_deque;
    ic_deque<ic_var, 1000> _var_deque; // deque is used because variables must not be invalidated (pointers are supported)
    std::vector<ic_token> _tokens;
    std::vector<ic_scope> _scopes;
    std::vector<ic_function> _functions;

    void push_scope(bool new_call_frame = false);
    void pop_scope();
    bool add_var(ic_string name, ic_value value);
    ic_value* get_var(ic_string name);
    ic_function* get_function(ic_string name);
    ic_ast_node* allocate_node(ic_ast_node_type type);
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

    // test
    int width = 1920;
    int height = 1080;
    double* image_buf = (double*)malloc(sizeof(double) * width * height);
    const char* var_name_buf = "image_buf";
    const char* var_name_w = "width";
    const char* var_name_h = "height";

    ic_value value;
    value.type = IC_VAL_NUMBER;
    value.indirection_count = 1;
    value.pointer = image_buf;
    runtime.add_var({ var_name_buf, int(strlen(var_name_buf)) }, value);

    value.indirection_count = 0;
    value.number = width;
    runtime.add_var({ var_name_w, int(strlen(var_name_w)) }, value);

    value.number = height;
    runtime.add_var({ var_name_h, int(strlen(var_name_h)) }, value);

    runtime.run(source_code.data());

    {
        FILE* file = fopen("render.ppm", "w");

        char buf[1024];
        snprintf(buf, sizeof(buf), "P6 %d %d 255 ", width, height);
        int len = strlen(buf);
        fwrite(buf, 1, len, file);

        // currently char type is not supported in compiler
        unsigned char* out_buf = (unsigned char*)malloc(width * height * 3);
        unsigned char* it = out_buf;

        for (int i = 0; i < height * width; ++i)
        {
            *it = image_buf[i];
            ++it;
            *it = image_buf[i];
            ++it;
            *it = image_buf[i];
            ++it;
        }

        fwrite(out_buf, 1, width * height * 3, file);
        fclose(file);
    }

    return 0;
}

ic_ast_node* ic_runtime::allocate_node(ic_ast_node_type type)
{
    ic_ast_node* node = _ast_deque.allocate();
    memset(node, 0, sizeof(ic_ast_node));
    node->type = type;
    return node;
}

ic_value ic_host_print(int argc, ic_value* argv)
{
    // todo; check argc and argv at compile time
    if (argc != 1 || argv[0].type != IC_VAL_NUMBER || argv[0].indirection_count)
    {
        printf("expected number argument\n");
        return {};
    }
    printf("print: %f\n", argv[0].number);
    return {};
}

void ic_runtime::init()
{
    push_scope(); // global scope

    static const char* str = "print";
    ic_string name = { str, strlen(str) };
    ic_function function;
    function.type = IC_FUN_HOST;
    function.name = name;
    function.host = ic_host_print;
    // todo; use add_host_function("print")
    _functions.push_back(function);
}

void ic_runtime::free()
{
    _ast_deque.free();
    _var_deque.free();
}

// todo; these functions should manage host strings

void ic_runtime::clear_host_functions()
{
    assert(false);
}

void ic_runtime::clear_global_vars()
{
    assert(_scopes.size() == 1);
    _var_deque.size = 0;
}

// todo; overwrite if exists
void ic_runtime::add_global_var(const char* name, ic_value value)
{
    assert(false);
}

ic_value* ic_runtime::get_global_var(const char* name)
{
    assert(false);
    return {};
}

// todo; overwrite if exists
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
ic_exe_result ic_ast_execute(const ic_ast_node* node, ic_runtime& runtime);

bool ic_runtime::run(const char* source_code)
{
    assert(_scopes.size() == 1);
    assert(source_code);
    // free tokens from previous run
    _tokens.clear();
    // free ast from previous run
    _ast_deque.size = 0;
    // remove source functions from previous run
    {
        auto it = _functions.begin();
        for (; it != _functions.end(); ++it)
        {
            if (it->type == IC_FUN_SOURCE)
                break;
        }
        _functions.erase(it, _functions.end());
    }


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

        assert(global.type == IC_GLOBAL_FUNCTION_DEFINITION);
        ic_string name = global.function.name;

        if (get_function(name))
        {
            printf("execution error; function already exists: '%.*s'\n", name.len, name.data);
            return false;
        }

        _functions.push_back(global.function);
    }

    // execute main funtion
    const char* str = "main";
    ic_string name = { str, int(strlen(str)) };
    ic_token token;
    token.type = IC_TOK_IDENTIFIER;
    token.string = name;
    ic_ast_node* node = allocate_node(IC_AST_FUNCTION_CALL);
    node->_function_call.node = allocate_node(IC_AST_PRIMARY);
    node->_function_call.node->_primary.token = token;

    // todo; excecution should never fail
    try
    {
        ic_ast_execute(node, *this);
    }
    catch(ic_exception_execution)
    {
        return false;
    }
    catch (ic_exception_return)
    {
        assert(false);
    }

    return true;
}

void ic_print_error(ic_error_type error_type, int line, int col, const char* fmt, ...)
{
    const char* str_err_type;

    if (error_type == IC_ERR_LEXING)
        str_err_type = "lexing";
    else if (error_type == IC_ERR_PARSING)
        str_err_type = "parsing";
    else
        str_err_type = "execution";

    printf("%s error; line: %d; col: %d; ", str_err_type, line, col);
    va_list list;
    va_start(list, fmt);
    vprintf(fmt, list);
    va_end(list);
    printf("\n");
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
        if (ic_string_compare(function.name, name))
            return& function;
    }
    return nullptr;
}

void exit_execution(const ic_token& token, const char* err_msg, ...)
{
    va_list list;
    va_start(list, err_msg);
    ic_print_error(IC_ERR_EXECUTION, token.line, token.col, err_msg, list);
    va_end(list);
    throw ic_exception_execution{};
}

void assert_lvalue(const ic_exe_result& result, const ic_token& token)
{
    if (!result.lvalue_data_address)
        exit_execution(token, "expected lvalue");
}

// todo; remove
void assert_non_pointer(const ic_exe_result& result, const ic_token& token)
{
    if (result.value.indirection_count > 0)
        exit_execution(token, "expected non-poninter");
}

ic_exe_result ic_ast_execute(const ic_ast_node* node, ic_runtime& runtime)
{
    assert(node);

    switch (node->type)
    {
    case IC_AST_BLOCK:
    {
        bool push = node->_block.push_scope;

        if(push)
            runtime.push_scope();

        while (node->_block.node)
        {
            // skip empty statements
            if(node->_block.node)
                ic_ast_execute(node->_block.node, runtime);

            node = node->_block.next;
        }

        if(push)
            runtime.pop_scope();

        return {};
    }
    case IC_AST_FOR:
    {
        runtime.push_scope();
        int num_scopes = runtime._scopes.size();
        int num_variables = runtime._scopes.back().prev_var_count;

        if(node->_for.header1)
            ic_ast_execute(node->_for.header1, runtime);

        for (;;)
        {
            if (node->_for.header2)
            {
                ic_exe_result in_result = ic_ast_execute(node->_for.header2, runtime);

                // todo; support pointers
                assert_non_pointer(in_result, node->_for.token_header2);

                // todo; support other types
                if (in_result.value.type != IC_VAL_NUMBER)
                    exit_execution(node->_for.token_header2, "expression must evaluate to number");

                if(!in_result.value.number)
                    break;
            }

            try
            {
                ic_ast_execute(node->_for.body, runtime);
            }
            catch (ic_exception_break)
            {
                // restore correct number of scopes
                runtime._scopes.erase(runtime._scopes.begin() + num_scopes, runtime._scopes.end());
                runtime._var_deque.size = num_variables;
                break;
            }
            catch (ic_exception_continue)
            {
                runtime._scopes.erase(runtime._scopes.begin() + num_scopes, runtime._scopes.end());
            }

            if(node->_for.header3)
                ic_ast_execute(node->_for.header3, runtime);
        }

        runtime.pop_scope();
        return {};
    }
    case IC_AST_IF:
    {
        runtime.push_scope();
        ic_exe_result in_result = ic_ast_execute(node->_if.header, runtime);

        // todo; support pointers
        assert_non_pointer(in_result, node->_if.token_header);

        // todo; support other types
        if (in_result.value.type != IC_VAL_NUMBER)
            exit_execution(node->_if.token_header, "expression must evaluate to number");

        if (in_result.value.number)
            ic_ast_execute(node->_if.body_if, runtime);

        else if(node->_if.body_else)
            ic_ast_execute(node->_if.body_else, runtime);

        runtime.pop_scope();
        return {};
    }
    case IC_AST_RETURN:
    {
        if (node->_return.node)
        {
            ic_exe_result in_result = ic_ast_execute(node->_return.node, runtime);
            throw ic_exception_return{ in_result.value };
        }

        throw ic_exception_return{};
    }
    case IC_AST_BREAK:
    {
        throw ic_exception_break{};
    }
    case IC_AST_CONTINUE:
    {
        throw ic_exception_continue{};
    }
    case IC_AST_VAR_DEFINITION:
    {
        ic_exe_result in_result = ic_ast_execute(node->_var_definition.node, runtime);
        ic_token token = node->_var_definition.token_id;
        // todo; compare types

        if(node->_var_definition.indirection_count != in_result.value.indirection_count)
            exit_execution(token, "level of indirection between type and expression does not match");

        if (!runtime.add_var(token.string, in_result.value))
            exit_execution(token, "variable with such name already exists in the current scope");

        return {};
    }
    case IC_AST_BINARY:
    {
        ic_exe_result in_left_result = ic_ast_execute(node->_binary.left, runtime);
        ic_exe_result in_right_result = ic_ast_execute(node->_binary.right, runtime);
        ic_token token = node->_binary.token_operator;

        if (in_left_result.value.type != in_right_result.value.type)
            exit_execution(token, "operands must be of the same type");

        // todo; separate cases into functions and remove this helper code block
        // separate by operator or operand type?
        ic_exe_result out_result;
        out_result.lvalue_data_address = nullptr;
        out_result.value.indirection_count = 0;
        out_result.value.type = IC_VAL_NUMBER;

        switch (token.type)
        {
        case IC_TOK_AND:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            assert_non_pointer(in_right_result, token);
            out_result.value.number = in_left_result.value.number && in_right_result.value.number;
            return out_result;
        }
        case IC_TOK_OR:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            assert_non_pointer(in_right_result, token);
            out_result.value.number = in_left_result.value.number || in_right_result.value.number;
            return out_result;
        }
        case IC_TOK_EQUAL:
        {
            assert_lvalue(in_left_result, token);

            if (in_left_result.value.indirection_count != in_right_result.value.indirection_count)
                exit_execution(token, "operands must be on the same level of indirection");

            in_left_result.value = in_right_result.value;
            assert(in_left_result.value.type == IC_VAL_NUMBER);
            // todo: this is kind of redundant
            *(double*)in_left_result.lvalue_data_address = in_left_result.value.number;
            return in_left_result;
        }
        case IC_TOK_PLUS_EQUAL:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            assert_non_pointer(in_right_result, token);
            assert_lvalue(in_left_result, token);
            in_left_result.value.number += in_right_result.value.number;
            assert(in_left_result.value.type == IC_VAL_NUMBER);
            *(double*)in_left_result.lvalue_data_address = in_left_result.value.number;
            return in_left_result;
        }
        case IC_TOK_MINUS_EQUAL:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            assert_non_pointer(in_right_result, token);
            assert_lvalue(in_left_result, token);
            in_left_result.value.number += in_right_result.value.number;
            assert(in_left_result.value.type == IC_VAL_NUMBER);
            *(double*)in_left_result.lvalue_data_address = in_left_result.value.number;
            return in_left_result;
        }
        case IC_TOK_STAR_EQUAL:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            assert_non_pointer(in_right_result, token);
            assert_lvalue(in_left_result, token);
            in_left_result.value.number *= in_right_result.value.number;
            assert(in_left_result.value.type == IC_VAL_NUMBER);
            *(double*)in_left_result.lvalue_data_address = in_left_result.value.number;
            return in_left_result;
        }
        case IC_TOK_SLASH_EQUAL:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            assert_non_pointer(in_right_result, token);
            assert_lvalue(in_left_result, token);
            in_left_result.value.number /= in_right_result.value.number;
            assert(in_left_result.value.type == IC_VAL_NUMBER);
            *(double*)in_left_result.lvalue_data_address = in_left_result.value.number;
            return in_left_result;
        }
        case IC_TOK_BANG_EQUAL:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            assert_non_pointer(in_right_result, token);
            out_result.value.number = in_left_result.value.number != in_right_result.value.number;
            return out_result;
        }
        case IC_TOK_EQUAL_EQUAL:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            assert_non_pointer(in_right_result, token);
            out_result.value.number = in_left_result.value.number == in_right_result.value.number;
            return out_result;
        }
        case IC_TOK_MINUS:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            assert_non_pointer(in_right_result, token);
            out_result.value.number = in_left_result.value.number - in_right_result.value.number;
            return out_result;
        }
        case IC_TOK_PLUS:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_right_result, token);

            if (in_left_result.value.indirection_count)
            {
                // todo; only int types can be added to a pointer
                out_result.value.pointer = (double*)in_left_result.value.pointer + (int)in_right_result.value.number;
                out_result.value.indirection_count = in_left_result.value.indirection_count;
            }
            else
                out_result.value.number = in_left_result.value.number + in_right_result.value.number;

            return out_result;
        }
        case IC_TOK_STAR:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            assert_non_pointer(in_right_result, token);
            out_result.value.number = in_left_result.value.number * in_right_result.value.number;
            return out_result;
        }
        case IC_TOK_SLASH:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            assert_non_pointer(in_right_result, token);
            out_result.value.number = in_left_result.value.number / in_right_result.value.number;
            return out_result;
        }
        case IC_TOK_GREATER:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            assert_non_pointer(in_right_result, token);
            out_result.value.number = in_left_result.value.number > in_right_result.value.number;
            return out_result;
        }
        case IC_TOK_GREATER_EQUAL:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            assert_non_pointer(in_right_result, token);
            out_result.value.number = in_left_result.value.number >= in_right_result.value.number;
            return out_result;
        }
        case IC_TOK_LESS:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            assert_non_pointer(in_right_result, token);
            out_result.value.number = in_left_result.value.number < in_right_result.value.number;
            return out_result;
        }
        case IC_TOK_LESS_EQUAL:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            assert_non_pointer(in_right_result, token);
            out_result.value.number = in_left_result.value.number <= in_right_result.value.number;
            return out_result;
        }
        } // switch

        assert(false);
    }
    case IC_AST_UNARY:
    {
        ic_exe_result in_result = ic_ast_execute(node->_unary.node, runtime);
        ic_token token = node->_unary.token_operator;

        switch (token.type)
        {
        case IC_TOK_MINUS:
        {
            if (in_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_result, token);
            in_result.value.number *= -1.0;
            in_result.lvalue_data_address = nullptr;
            return in_result;
        }
        case IC_TOK_BANG:
        {
            if (in_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_result, token);
            in_result.value.number = !in_result.value.number;
            in_result.lvalue_data_address = nullptr;
            return in_result;
        }
        case IC_TOK_MINUS_MINUS:
        {
            if (in_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_lvalue(in_result, token);
            assert_non_pointer(in_result, token);
            in_result.value.number -= 1;
            assert(in_result.value.type == IC_VAL_NUMBER);
            *(double*)in_result.lvalue_data_address = in_result.value.number;
            return in_result;
        }
        case IC_TOK_PLUS_PLUS:
        {
            if (in_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_lvalue(in_result, token);
            assert_non_pointer(in_result, token);
            in_result.value.number += 1;
            assert(in_result.value.type == IC_VAL_NUMBER);
            *(double*)in_result.lvalue_data_address = in_result.value.number;
            return in_result;
        }
        case IC_TOK_AMPERSAND:
        {
            assert_lvalue(in_result, token);
            ic_exe_result out_result;
            out_result.lvalue_data_address = nullptr;
            out_result.value.type = in_result.value.type;
            out_result.value.indirection_count = in_result.value.indirection_count + 1;
            out_result.value.pointer = in_result.lvalue_data_address;
            return out_result;
        }
        case IC_TOK_STAR:
        {
            int indirection_count = in_result.value.indirection_count;

            if(!indirection_count)
                exit_execution(token, "cannot dereference non-pointer");
            
            ic_exe_result out_result;
            out_result.value.indirection_count = indirection_count - 1;
            out_result.value.type = in_result.value.type;
            out_result.lvalue_data_address = in_result.value.pointer;

            if (indirection_count == 1)
            {
                assert(in_result.value.type == IC_VAL_NUMBER);
                out_result.value.number = *(double*)in_result.value.pointer;
            }
            else
                out_result.value.pointer = (void*)*(char**)in_result.value.pointer;

            return out_result;
        }
        } // switch

        assert(false);
    }
    case IC_AST_FUNCTION_CALL:
    {
        assert(node->_function_call.node->type == IC_AST_PRIMARY);
        ic_token token = node->_function_call.node->_primary.token;
        ic_function* function = runtime.get_function(token.string);

        if (!function)
            exit_execution(token, "function with such name does not exist");

        int argc = 0;
        ic_value argv[IC_MAX_ARGC];
        ic_ast_node* arg_it = node->_function_call.next;

        while (arg_it)
        {
            assert(argc < IC_MAX_ARGC);
            ic_exe_result in_result = ic_ast_execute(arg_it->_function_call.node, runtime);
            arg_it = arg_it->_function_call.next;
            argv[argc] = in_result.value;
            argc += 1;
        }

        if (function->type == IC_FUN_HOST)
        {
            assert(function->host);
            ic_value value = function->host(argc, argv);
            ic_exe_result out_result;
            // function can never return an lvalue
            out_result.lvalue_data_address = nullptr;
            out_result.value = value;
            return out_result;
        }
        else
        {
            assert(function->type == IC_FUN_SOURCE);

            if (argc != function->source.param_count)
                exit_execution(token, "number of arguments does not match");

            // all arguments must be retrieved before upper scopes are hidden
            runtime.push_scope(true);
            int num_scopes = runtime._scopes.size();
            int num_variables = runtime._scopes.back().prev_var_count;

            for(int i = 0; i < argc; ++i)
                runtime.add_var(function->source.params[i], argv[i]);

            ic_exe_result out_result;
            out_result.lvalue_data_address = nullptr;

            try
            {
                ic_ast_execute(function->source.node_body, runtime);
            }
            catch (ic_exception_return& e)
            {
                out_result.value = e.value;
                // restore correct number of scopes and variables
                runtime._scopes.erase(runtime._scopes.begin() + num_scopes, runtime._scopes.end());
                runtime._var_deque.size = num_variables;
            }

            runtime.pop_scope();
            return out_result;
        }
    }
    case IC_AST_GROUPING:
    {
        return ic_ast_execute(node->_grouping.node, runtime);
    }
    case IC_AST_PRIMARY:
    {
        ic_token token = node->_primary.token;

        switch (token.type)
        {
        case IC_TOK_NUMBER:
        {
            ic_exe_result out_result;
            out_result.lvalue_data_address = nullptr;
            out_result.value.type = IC_VAL_NUMBER;
            out_result.value.indirection_count = 0;
            out_result.value.number = token.number;
            return out_result;
        }
        case IC_TOK_IDENTIFIER:
        {
            ic_value* value = runtime.get_var(token.string);

            if (!value)
                exit_execution(token, "variable with such name does not exist");

            ic_exe_result out_result;
            out_result.value = *value;

            if (value->indirection_count)
                out_result.lvalue_data_address = &value->pointer;
            else
            {
                assert(value->type == IC_VAL_NUMBER);
                out_result.lvalue_data_address = &value->number;
            }

            return out_result;
        }
        } // switch
        assert(false);
    }
    } // switch
    assert(false);
    return {};
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
            lexer.add_token(lexer.consume('&') ? IC_TOK_AND : IC_TOK_AMPERSAND);
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
                lexer.add_token(IC_TOK_OR);
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

                if (lexer.peek() == '.' && is_digit(lexer.peek_second()))
                    lexer.advance();

                while (!lexer.end() && is_digit(lexer.peek()))
                    lexer.advance();

                const int len = lexer.prev_it() - begin_it + 1;
                char buf[1024];
                assert(len < sizeof(buf));
                memcpy(buf, begin_it, len);
                buf[len] = '\0';
                lexer.add_token(IC_TOK_NUMBER, atof(buf));
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

    lexer.add_token(IC_TOK_EOF);
    return success;
}

enum ic_op_precedence
{
    IC_PRECEDENCE_OR,
    IC_PRECEDENCE_AND,
    IC_PRECEDENCE_COMPARE_EQUAL,
    IC_PRECEDENCE_COMPARE_GREATER,
    IC_PRECEDENCE_ADDITION,
    IC_PRECEDENCE_MULTIPLICATION,
    IC_PRECEDENCE_UNARY,
};

// grammar, production rules hierarchy

ic_global produce_global(const ic_token** it, ic_runtime& runtime);
ic_ast_node* produce_stmt(const ic_token** it, ic_runtime& runtime);
ic_ast_node* produce_stmt_var_definition(const ic_token** it, ic_runtime& runtime);
ic_ast_node* produce_stmt_expr(const ic_token** it, ic_runtime& runtime);
ic_ast_node* produce_expr(const ic_token** it, ic_runtime& runtime);
ic_ast_node* produce_expr_assignment(const ic_token** it, ic_runtime& runtime);
ic_ast_node* produce_expr_binary(const ic_token** it, ic_op_precedence precedence, ic_runtime& runtime);
ic_ast_node* produce_expr_unary(const ic_token** it, ic_runtime& runtime);
ic_ast_node* produce_expr_primary(const ic_token** it, ic_runtime& runtime);

void token_advance(const ic_token** it) { ++(*it); }
bool token_match_type(const ic_token** it, ic_token_type type) { return (**it).type == type; }

bool token_consume(const ic_token** it, ic_token_type type, const char* err_msg = nullptr)
{
    if (token_match_type(it, type))
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

ic_global produce_global(const ic_token** it, ic_runtime& runtime)
{
    if (token_match_type(it, IC_TOK_IDENTIFIER))
    {
        ic_global global;
        global.type = IC_GLOBAL_FUNCTION_DEFINITION;
        ic_function& function = global.function;
        function.type = IC_FUN_SOURCE;
        function.name = (**it).string;
        function.source.param_count = 0;
        token_advance(it);
        token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after function name");

        if (!token_consume(it, IC_TOK_RIGHT_PAREN))
        {
            for(;;)
            {
                if (function.source.param_count > IC_MAX_ARGC)
                    exit_parsing(it, "exceeded maximal number of parameters (%d)", IC_MAX_ARGC);

                ic_ast_node* node_param = produce_expr_primary(it, runtime);

                if (node_param->type != IC_AST_PRIMARY || node_param->_primary.token.type != IC_TOK_IDENTIFIER)
                    exit_parsing(it, "expected parameter name");

                function.source.params[function.source.param_count] = node_param->_primary.token.string;
                function.source.param_count += 1;
                if (token_consume(it, IC_TOK_RIGHT_PAREN)) break;
                token_consume(it, IC_TOK_COMMA, "expected ',' or ')' after a function argument");
            }
        }

        function.source.node_body = produce_stmt(it, runtime);

        if (function.source.node_body->type != IC_AST_BLOCK)
            exit_parsing(it, "expected block stmt after function parameter list");

        function.source.node_body->_block.push_scope = false; // do not allow shadowing of arguments
        return global;
    }
    // todo; struct, var
    assert(false);
    return {};
}

ic_ast_node* produce_stmt(const ic_token** it, ic_runtime& runtime)
{
    switch ((**it).type)
    {
    case IC_TOK_LEFT_BRACE:
    {
        token_advance(it);
        ic_ast_node* node = runtime.allocate_node(IC_AST_BLOCK);
        node->_block.push_scope = true;
        ic_ast_node* block_it = node;

        while (!token_match_type(it, IC_TOK_RIGHT_BRACE) && !token_match_type(it, IC_TOK_EOF))
        {
            block_it->_block.node = produce_stmt(it, runtime);
            block_it->_block.next = runtime.allocate_node(IC_AST_BLOCK);
            block_it = block_it->_block.next;
        }

        token_consume(it, IC_TOK_RIGHT_BRACE, "expected '}' to close a block statement");
        return node;
    }
    case IC_TOK_WHILE:
    {
        // implemented as a IC_AST_FOR
        token_advance(it);
        ic_ast_node* node = runtime.allocate_node(IC_AST_FOR);
        token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after while keyword");
        node->_for.token_header2 = **it;
        node->_for.header2 = produce_expr(it, runtime);
        token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after while condition");
        node->_for.body = produce_stmt(it, runtime);
        return node;
    }
    case IC_TOK_FOR:
    {
        token_advance(it);
        ic_ast_node* node = runtime.allocate_node(IC_AST_FOR);
        token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after for keyword");
        node->_for.header1 = produce_stmt_var_definition(it, runtime); // only in header1 var definition is allowed
        node->_for.token_header2 = **it;
        node->_for.header2 = produce_stmt_expr(it, runtime);

        if (!token_match_type(it, IC_TOK_RIGHT_PAREN))
            node->_for.header3 = produce_expr(it, runtime); // this one should not end with ';', that's why we don't use produce_stmt_expr()

        token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after for header");
        node->_for.body = produce_stmt(it, runtime);
        return node;
    }
    case IC_TOK_IF:
    {
        token_advance(it);
        ic_ast_node* node = runtime.allocate_node(IC_AST_IF);
        token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after if keyword");
        node->_if.token_header = **it;
        node->_if.header = produce_expr(it, runtime);

        if (node->_if.header->type == IC_AST_BINARY && node->_if.header->_binary.token_operator.type == IC_TOK_EQUAL)
            exit_parsing(it, "assignment expression can't be used directly in if header, encolse it with ()");

        token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after if condition");
        node->_if.body_if = produce_stmt(it, runtime);

        if (token_consume(it, IC_TOK_ELSE))
            node->_if.body_else = produce_stmt(it, runtime);

        return node;
    }
    case IC_TOK_RETURN:
    {
        token_advance(it);
        ic_ast_node* node = runtime.allocate_node(IC_AST_RETURN);
        node->_return.node = produce_stmt_expr(it, runtime);
        return node;
    }
    case IC_TOK_BREAK:
    {
        token_advance(it);
        token_consume(it, IC_TOK_SEMICOLON, "expected ';' after break keyword");
        return runtime.allocate_node(IC_AST_BREAK);
    }
    case IC_TOK_CONTINUE:
    {
        token_advance(it);
        token_consume(it, IC_TOK_SEMICOLON, "expected ';' after continue keyword");
        return runtime.allocate_node(IC_AST_CONTINUE);
    }
    } // switch

    return produce_stmt_var_definition(it, runtime);
}

// this function is seperate from produce_expr_assignment so we don't allow(): var x = var y = 5;
// and: while(var x = 6);
ic_ast_node* produce_stmt_var_definition(const ic_token** it, ic_runtime& runtime)
{
    if (token_consume(it, IC_TOK_VAR))
    {
        ic_ast_node* node = runtime.allocate_node(IC_AST_VAR_DEFINITION);

        while (token_consume(it, IC_TOK_STAR))
            node->_var_definition.indirection_count += 1;

        node->_var_definition.token_id = **it;
        token_consume(it, IC_TOK_IDENTIFIER, "expected variable name");

        if (token_consume(it, IC_TOK_EQUAL))
        {
            node->_var_definition.node = produce_stmt_expr(it, runtime);
            return node;
        }

        token_consume(it, IC_TOK_SEMICOLON, "expected ';' or '=' after variable name");
        // uninitialized variable
        node->_var_definition.node = runtime.allocate_node(IC_AST_PRIMARY);
        node->_var_definition.node->_primary.token.type = IC_TOK_NUMBER;
        return node;
    }
    return produce_stmt_expr(it, runtime);
}

ic_ast_node* produce_stmt_expr(const ic_token** it, ic_runtime& runtime)
{
    if (token_consume(it, IC_TOK_SEMICOLON))
        return nullptr;

    ic_ast_node* node = produce_expr(it, runtime);
    token_consume(it, IC_TOK_SEMICOLON, "expected ';' after expression");
    return node;
}

ic_ast_node* produce_expr(const ic_token** it, ic_runtime& runtime)
{
    return produce_expr_assignment(it, runtime);
}

// produce_expr_assignment() - grows down and right
// produce_expr_binary() - grows up and right (given operators with the same precedence)

ic_ast_node* produce_expr_assignment(const ic_token** it, ic_runtime& runtime)
{
    ic_ast_node* node_left = produce_expr_binary(it, IC_PRECEDENCE_OR, runtime);

    if (token_match_type(it, IC_TOK_EQUAL) || token_match_type(it, IC_TOK_PLUS_EQUAL) || token_match_type(it, IC_TOK_MINUS_EQUAL) ||
        token_match_type(it, IC_TOK_STAR_EQUAL) || token_match_type(it, IC_TOK_SLASH_EQUAL))
    {
        ic_ast_node* node = runtime.allocate_node(IC_AST_BINARY);
        node->_binary.token_operator = **it;
        token_advance(it);
        node->_binary.right = produce_expr(it, runtime);
        node->_binary.left = node_left;
        return node;
    }

    return node_left;
}

ic_ast_node* produce_expr_binary(const ic_token** it, ic_op_precedence precedence, ic_runtime& runtime)
{
    ic_token_type target_operators[5] = {};

    switch (precedence)
    {
    case IC_PRECEDENCE_OR:
    {
        target_operators[0] = IC_TOK_OR;
        break;
    }
    case IC_PRECEDENCE_AND:
    {
        target_operators[0] = IC_TOK_AND;
        break;
    }
    case IC_PRECEDENCE_COMPARE_EQUAL:
    {
        target_operators[0] = IC_TOK_EQUAL_EQUAL;
        target_operators[1] = IC_TOK_BANG_EQUAL;
        break;
    }
    case IC_PRECEDENCE_COMPARE_GREATER:
    {
        target_operators[0] = IC_TOK_GREATER;
        target_operators[1] = IC_TOK_GREATER_EQUAL;
        target_operators[2] = IC_TOK_LESS;
        target_operators[3] = IC_TOK_LESS_EQUAL;
        break;
    }
    case IC_PRECEDENCE_ADDITION:
    {
        target_operators[0] = IC_TOK_PLUS;
        target_operators[1] = IC_TOK_MINUS;
        break;
    }
    case IC_PRECEDENCE_MULTIPLICATION:
    {
        target_operators[0] = IC_TOK_STAR;
        target_operators[1] = IC_TOK_SLASH;
        break;
    }
    case IC_PRECEDENCE_UNARY:
        return produce_expr_unary(it, runtime);
    }

    ic_ast_node* node = produce_expr_binary(it, ic_op_precedence(int(precedence) + 1), runtime);

    for (;;)
    {
        const ic_token_type* op = target_operators;
        bool match = false;

        while (*op != IC_TOK_EOF)
        {
            if (*op == (**it).type)
            {
                match = true;
                break;
            }
            ++op;
        }

        if (!match)
            break;

        // operator matches given precedence
        ic_ast_node* node_parent = runtime.allocate_node(IC_AST_BINARY);
        node_parent->_binary.token_operator = **it;
        token_advance(it);
        node_parent->_binary.right = produce_expr_binary(it, ic_op_precedence(int(precedence) + 1), runtime);
        node_parent->_binary.left = node;
        node = node_parent;
    }

    return node;
}

ic_ast_node* produce_expr_unary(const ic_token** it, ic_runtime& runtime)
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
        ic_ast_node* node = runtime.allocate_node(IC_AST_UNARY);
        node->_unary.token_operator = **it;
        token_advance(it);
        node->_unary.node = produce_expr_unary(it, runtime);
        return node;
    }
    } // switch

    return produce_expr_primary(it, runtime);
}

ic_ast_node* produce_expr_primary(const ic_token** it, ic_runtime& runtime)
{
    switch ((**it).type)
    {
    case IC_TOK_NUMBER:
    case IC_TOK_STRING:
    case IC_TOK_IDENTIFIER:
    {
        ic_ast_node* node_id = runtime.allocate_node(IC_AST_PRIMARY);
        node_id->_primary.token = **it;
        token_advance(it);

        if (token_consume(it, IC_TOK_LEFT_PAREN))
        {
            ic_ast_node* node = runtime.allocate_node(IC_AST_FUNCTION_CALL);
            node->_function_call.node = node_id;
            ic_ast_node* arg_it = node;

            if (token_consume(it, IC_TOK_RIGHT_PAREN))
                return node;

            int argc = 0;

            for(;;)
            {
                ic_ast_node* cnode = runtime.allocate_node(IC_AST_FUNCTION_CALL);
                cnode->_function_call.node = produce_expr(it, runtime);
                arg_it->_function_call.next = cnode;
                arg_it = cnode;
                ++argc;

                if(argc > IC_MAX_ARGC)
                    exit_parsing(it, "exceeded maximal number of arguments (%d)", IC_MAX_ARGC);

                if (token_consume(it, IC_TOK_RIGHT_PAREN))
                    break;

                token_consume(it, IC_TOK_COMMA, "expected ',' or ')' after a function argument");
            }

            return node;
        }

        return node_id;
    }
    case IC_TOK_LEFT_PAREN:
    {
        token_advance(it);
        ic_ast_node* node = runtime.allocate_node(IC_AST_GROUPING);
        node->_grouping.node = produce_expr(it, runtime);
        token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after expression");
        return node;
    }
    } // switch

    exit_parsing(it, "expected primary, grouping, or function call expression");
    return {};
}
