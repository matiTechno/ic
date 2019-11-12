#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <string>
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

#define IC_POOL_SIZE 2048
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
        ic_string address;
    };
};

struct ic_var
{
    ic_string name;
    ic_value value;
};

struct ic_scope
{
    std::vector<ic_var> vars;
    bool new_call_frame; // so variables do not leak to lower functions
};

struct ic_ast_node_pool
{
    ic_ast_node data[IC_POOL_SIZE];
    int size;
};

struct ic_mem
{
    std::vector<ic_ast_node_pool*> pools;
    int current_pool = 0;

    ic_ast_node* allocate_node(ic_ast_node_type);
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
        // todo; host function signature for type checking
        ic_host_function host;
        struct
        {
            int param_count;
            ic_string params[IC_MAX_ARGC];
            ic_ast_node* node_body;
            // todo return type
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
    ic_string lvalue_name; // lvalue if lvalue_name.len != 0
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
    // interface
    void init();
    void clear_host_functions();
    void clear_global_vars(); // use this before next run()
    void add_var_global(const char* name, ic_value value);
    const ic_value* get_var_global(const char* name);
    void add_fun(const char* name, ic_host_function function);
    bool run(const char* source); // after run() variables can be extracted

    // implementation
    ic_mem _mem;
    std::vector<ic_token> _tokens;
    // todo; this is inefficient (vector in vector)
    std::vector<ic_scope> _scopes;
    std::vector<ic_function> _functions;

    void push_scope(bool new_call_frame = false);
    void pop_scope();
    bool add_var(ic_string name, ic_value value);
    const ic_value* get_var(ic_string name);
    bool set_var(ic_string, ic_value value);
    ic_function* get_function(ic_string name);
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

// todo; check argc and argv in compile time
ic_value ic_host_print(int argc, ic_value* argv)
{
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
    // todo; use add_host_function()
    _functions.push_back(function);
}

// todo
void ic_runtime::clear_host_functions()
{
}

void ic_runtime::clear_global_vars()
{
    assert(_scopes.size() == 1);
    _scopes[0].vars.clear();
}

// todo; this should also overwrite existing variable
void ic_runtime::add_var_global(const char* name, ic_value value)
{
}

// todo
const ic_value* ic_runtime::get_var_global(const char* name)
{
    return {};
}

// todo; overwrite existing function
void ic_runtime::add_fun(const char* name, ic_host_function function)
{
}

bool ic_string_compare(ic_string str1, ic_string str2)
{
    if (str1.len != str2.len)
        return false;

    return (strncmp(str1.data, str2.data, str1.len) == 0);
}

bool ic_tokenize(std::vector<ic_token>& tokens, const char* source_code);
ic_global produce_global(const ic_token** it, ic_mem& _mem);
ic_exe_result ic_ast_execute(const ic_ast_node* node, ic_runtime& runtime);

bool ic_runtime::run(const char* source_code)
{
    assert(source_code);
    // free tokens from previous run
    _tokens.clear();
    // free ast from previous run
    _mem.current_pool = 0;
    for (ic_ast_node_pool* pool : _mem.pools)
        pool->size = 0;

    assert(_scopes.size() == 1);

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
            global = produce_global(&token_it, _mem);
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
    ic_ast_node* node = _mem.allocate_node(IC_AST_FUNCTION_CALL);
    node->_function_call.node = _mem.allocate_node(IC_AST_PRIMARY);
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

ic_ast_node* ic_mem::allocate_node(ic_ast_node_type type)
{
    bool allocate_new_pool = false;

    if (pools.empty())
    {
        allocate_new_pool = true;
    }
    else if (pools[current_pool]->size == IC_POOL_SIZE)
    {
        current_pool += 1;

        if (current_pool >= pools.size())
            allocate_new_pool = true;
    }

    if (allocate_new_pool)
    {
        ic_ast_node_pool* new_pool = (ic_ast_node_pool*)malloc(sizeof(ic_ast_node_pool));
        new_pool->size = 0;
        pools.push_back(new_pool);
    }

    ic_ast_node_pool* pool = pools[current_pool];
    ic_ast_node* node = &pool->data[pool->size];
    memset(node, 0, sizeof(ic_ast_node)); // this is important
    node->type = type;
    pool->size += 1;
    return node;
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
    _scopes.push_back({});
    _scopes.back().new_call_frame = new_call_frame;
}

void ic_runtime::pop_scope()
{
    _scopes.pop_back();
}

bool ic_runtime::add_var(ic_string name, ic_value value)
{
    bool present = false;
    for (ic_var& var : _scopes.back().vars)
    {
        if (ic_string_compare(var.name, name))
        {
            present = true;
            break;
        }
    }

    if (present)
        return false;

    _scopes.back().vars.push_back({ name, value });
    return true;
}

const ic_value* ic_runtime::get_var(ic_string name)
{
    for(int i = _scopes.size() - 1; i >= 0; --i)
    {
        for (ic_var& var : _scopes[i].vars)
        {
            if (ic_string_compare(var.name, name))
                return &var.value;
        }

        // go to the global scope, so no variables leak from the parent function
        if (_scopes[i].new_call_frame)
            i = 1;
    }
    return nullptr;
}

bool ic_runtime::set_var(ic_string name, ic_value value)
{
    for(int i = _scopes.size() - 1; i >= 0; --i)
    {
        for (ic_var& var : _scopes[i].vars)
        {
            if (ic_string_compare(var.name, name))
            {
                var.value = value;
                return true;
            }
        }

        // go to the global scope, so no variables leak from the parent function
        if (_scopes[i].new_call_frame)
            i = 1;
    }
    return false;
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
    if (result.lvalue_name.len == 0)
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

        if (in_left_result.value.indirection_count != in_right_result.value.indirection_count)
            exit_execution(token, "operands must be on the same level of indirection");

        // todo; separate cases into functions and remove this helper code block
        // separate by operator or operand type?
        ic_exe_result out_result;
        out_result.lvalue_name.len = 0;
        out_result.value.indirection_count = 0;
        out_result.value.type = IC_VAL_NUMBER;

        switch (token.type)
        {
        case IC_TOK_AND:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            out_result.value.number = in_left_result.value.number && in_right_result.value.number;
            return out_result;
        }
        case IC_TOK_OR:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            out_result.value.number = in_left_result.value.number || in_right_result.value.number;
            return out_result;
        }
        case IC_TOK_EQUAL:
        {
            assert_lvalue(in_left_result, token);
            in_left_result.value = in_right_result.value;
            assert(runtime.set_var(in_left_result.lvalue_name, in_left_result.value));
            return in_left_result;
        }
        case IC_TOK_PLUS_EQUAL:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            assert_lvalue(in_left_result, token);
            in_left_result.value.number += in_right_result.value.number;
            runtime.set_var(in_left_result.lvalue_name, in_left_result.value);
            return in_left_result;
        }
        case IC_TOK_MINUS_EQUAL:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            assert_lvalue(in_left_result, token);
            in_left_result.value.number -= in_right_result.value.number;
            runtime.set_var(in_left_result.lvalue_name, in_left_result.value);
            return in_left_result;
        }
        case IC_TOK_STAR_EQUAL:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            assert_lvalue(in_left_result, token);
            in_left_result.value.number *= in_right_result.value.number;
            runtime.set_var(in_left_result.lvalue_name, in_left_result.value);
            return in_left_result;
        }
        case IC_TOK_SLASH_EQUAL:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            assert_lvalue(in_left_result, token);
            in_left_result.value.number /= in_right_result.value.number;
            runtime.set_var(in_left_result.lvalue_name, in_left_result.value);
            return in_left_result;
        }
        case IC_TOK_BANG_EQUAL:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            out_result.value.number = in_left_result.value.number != in_right_result.value.number;
            return out_result;
        }
        case IC_TOK_EQUAL_EQUAL:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            out_result.value.number = in_left_result.value.number == in_right_result.value.number;
            return out_result;
        }
        case IC_TOK_MINUS:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            out_result.value.number = in_left_result.value.number - in_right_result.value.number;
            return out_result;
        }
        case IC_TOK_PLUS:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            out_result.value.number = in_left_result.value.number + in_right_result.value.number;
            return out_result;
        }
        case IC_TOK_STAR:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            out_result.value.number = in_left_result.value.number * in_right_result.value.number;
            return out_result;
        }
        case IC_TOK_SLASH:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            out_result.value.number = in_left_result.value.number / in_right_result.value.number;
            return out_result;
        }
        case IC_TOK_GREATER:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            out_result.value.number = in_left_result.value.number > in_right_result.value.number;
            return out_result;
        }
        case IC_TOK_GREATER_EQUAL:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            out_result.value.number = in_left_result.value.number >= in_right_result.value.number;
            return out_result;
        }
        case IC_TOK_LESS:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
            out_result.value.number = in_left_result.value.number < in_right_result.value.number;
            return out_result;
        }
        case IC_TOK_LESS_EQUAL:
        {
            if (in_left_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_left_result, token);
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
            in_result.lvalue_name.len = 0;
            return in_result;
        }
        case IC_TOK_BANG:
        {
            if (in_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_non_pointer(in_result, token);
            in_result.value.number = !in_result.value.number;
            in_result.lvalue_name.len = 0;
            return in_result;
        }
        case IC_TOK_MINUS_MINUS:
        {
            if (in_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_lvalue(in_result, token);
            assert_non_pointer(in_result, token);
            in_result.value.number -= 1;
            runtime.set_var(in_result.lvalue_name, in_result.value);
            return in_result;
        }
        case IC_TOK_PLUS_PLUS:
        {
            if (in_result.value.type != IC_VAL_NUMBER)
                exit_execution(token, "expected number type");

            assert_lvalue(in_result, token);
            assert_non_pointer(in_result, token);
            in_result.value.number += 1;
            runtime.set_var(in_result.lvalue_name, in_result.value);
            return in_result;
        }
        case IC_TOK_AMPERSAND:
        {
            assert_lvalue(in_result, token);
            ic_exe_result out_result;
            out_result.lvalue_name.len = 0;
            out_result.value.type = in_result.value.type;
            out_result.value.indirection_count = in_result.value.indirection_count + 1;
            out_result.value.address = in_result.lvalue_name;
            return out_result;
        }
        case IC_TOK_STAR:
        {
            if (in_result.value.indirection_count == 0)
                exit_execution(token, "cannot dereference non-pointer");

            const ic_value* value = runtime.get_var(in_result.value.address);
            // todo; it is triggered when pointer is passed to a function
            // operating on real pointers instead of ic_string will fix this
            assert(value);
            ic_exe_result out_result;
            out_result.lvalue_name = in_result.value.address;
            out_result.value = *value;
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
            out_result.lvalue_name.len = 0;
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

            for(int i = 0; i < argc; ++i)
                runtime.add_var(function->source.params[i], argv[i]);

            ic_exe_result out_result = {}; // if not initialized visual studio thorws an exception
            out_result.lvalue_name.len = 0; // function can never return an lvalue

            try
            {
                ic_ast_execute(function->source.node_body, runtime);
            }
            catch (ic_exception_return& e)
            {
                out_result.value = e.value;
                // restore correct number of scopes
                runtime._scopes.erase(runtime._scopes.begin() + num_scopes, runtime._scopes.end());
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
            out_result.lvalue_name.len = 0;
            out_result.value.type = IC_VAL_NUMBER;
            out_result.value.indirection_count = 0;
            out_result.value.number = token.number;
            return out_result;
        }
        case IC_TOK_IDENTIFIER:
        {
            const ic_value* value = runtime.get_var(token.string);

            if (!value)
                exit_execution(token, "variable with such name does not exist");

            ic_exe_result out_result;
            out_result.lvalue_name = token.string;
            out_result.value = *value;
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

ic_global produce_global(const ic_token** it, ic_mem& _mem);
ic_ast_node* produce_stmt(const ic_token** it, ic_mem& _mem);
ic_ast_node* produce_stmt_var_definition(const ic_token** it, ic_mem& _mem);
ic_ast_node* produce_stmt_expr(const ic_token** it, ic_mem& _mem);
ic_ast_node* produce_expr(const ic_token** it, ic_mem& _mem);
ic_ast_node* produce_expr_assignment(const ic_token** it, ic_mem& _mem);
ic_ast_node* produce_expr_binary(const ic_token** it, ic_op_precedence precedence, ic_mem& _mem);
ic_ast_node* produce_expr_unary(const ic_token** it, ic_mem& _mem);
ic_ast_node* produce_expr_primary(const ic_token** it, ic_mem& _mem);

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

ic_global produce_global(const ic_token** it, ic_mem& _mem)
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

                ic_ast_node* node_param = produce_expr_primary(it, _mem);

                if (node_param->type != IC_AST_PRIMARY || node_param->_primary.token.type != IC_TOK_IDENTIFIER)
                    exit_parsing(it, "expected parameter name");

                function.source.params[function.source.param_count] = node_param->_primary.token.string;
                function.source.param_count += 1;
                if (token_consume(it, IC_TOK_RIGHT_PAREN)) break;
                token_consume(it, IC_TOK_COMMA, "expected ',' or ')' after a function argument");
            }
        }

        function.source.node_body = produce_stmt(it, _mem);

        if (function.source.node_body->type != IC_AST_BLOCK)
            exit_parsing(it, "expected block stmt after function parameter list");

        function.source.node_body->_block.push_scope = false; // do not allow shadowing of arguments
        return global;
    }
    // todo; struct, var
    assert(false);
    return {};
}

ic_ast_node* produce_stmt(const ic_token** it, ic_mem& _mem)
{
    switch ((**it).type)
    {
    case IC_TOK_LEFT_BRACE:
    {
        token_advance(it);
        ic_ast_node* node = _mem.allocate_node(IC_AST_BLOCK);
        node->_block.push_scope = true;
        ic_ast_node* block_it = node;

        while (!token_match_type(it, IC_TOK_RIGHT_BRACE) && !token_match_type(it, IC_TOK_EOF))
        {
            block_it->_block.node = produce_stmt(it, _mem);
            block_it->_block.next = _mem.allocate_node(IC_AST_BLOCK);
            block_it = block_it->_block.next;
        }

        token_consume(it, IC_TOK_RIGHT_BRACE, "expected '}' to close a block statement");
        return node;
    }
    case IC_TOK_WHILE:
    {
        // implemented as a IC_AST_FOR
        token_advance(it);
        ic_ast_node* node = _mem.allocate_node(IC_AST_FOR);
        token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after while keyword");
        node->_for.token_header2 = **it;
        node->_for.header2 = produce_expr(it, _mem);
        token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after while condition");
        node->_for.body = produce_stmt(it, _mem);
        return node;
    }
    case IC_TOK_FOR:
    {
        token_advance(it);
        ic_ast_node* node = _mem.allocate_node(IC_AST_FOR);
        token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after for keyword");
        node->_for.header1 = produce_stmt_var_definition(it, _mem); // only in header1 var definition is allowed
        node->_for.token_header2 = **it;
        node->_for.header2 = produce_stmt_expr(it, _mem);

        if (!token_match_type(it, IC_TOK_RIGHT_PAREN))
            node->_for.header3 = produce_expr(it, _mem); // this one should not end with ';', that's why we don't use produce_stmt_expr()

        token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after for header");
        node->_for.body = produce_stmt(it, _mem);
        return node;
    }
    case IC_TOK_IF:
    {
        token_advance(it);
        ic_ast_node* node = _mem.allocate_node(IC_AST_IF);
        token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after if keyword");
        node->_if.token_header = **it;
        node->_if.header = produce_expr(it, _mem);

        if (node->_if.header->type == IC_AST_BINARY && node->_if.header->_binary.token_operator.type == IC_TOK_EQUAL)
            exit_parsing(it, "assignment expression can't be used directly in if header, encolse it with ()");

        token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after if condition");
        node->_if.body_if = produce_stmt(it, _mem);

        if (token_consume(it, IC_TOK_ELSE))
            node->_if.body_else = produce_stmt(it, _mem);

        return node;
    }
    case IC_TOK_RETURN:
    {
        token_advance(it);
        ic_ast_node* node = _mem.allocate_node(IC_AST_RETURN);
        node->_return.node = produce_stmt_expr(it, _mem);
        return node;
    }
    case IC_TOK_BREAK:
    {
        token_advance(it);
        token_consume(it, IC_TOK_SEMICOLON, "expected ';' after break keyword");
        return _mem.allocate_node(IC_AST_BREAK);
    }
    case IC_TOK_CONTINUE:
    {
        token_advance(it);
        token_consume(it, IC_TOK_SEMICOLON, "expected ';' after continue keyword");
        return _mem.allocate_node(IC_AST_CONTINUE);
    }
    } // switch

    return produce_stmt_var_definition(it, _mem);
}

// this function is seperate from produce_expr_assignment so we don't allow(): var x = var y = 5;
// and: while(var x = 6);
ic_ast_node* produce_stmt_var_definition(const ic_token** it, ic_mem& _mem)
{
    if (token_consume(it, IC_TOK_VAR))
    {
        ic_ast_node* node = _mem.allocate_node(IC_AST_VAR_DEFINITION);

        while (token_consume(it, IC_TOK_STAR))
            node->_var_definition.indirection_count += 1;

        node->_var_definition.token_id = **it;
        token_consume(it, IC_TOK_IDENTIFIER, "expected variable name");

        if (token_consume(it, IC_TOK_EQUAL))
        {
            node->_var_definition.node = produce_stmt_expr(it, _mem);
            return node;
        }

        token_consume(it, IC_TOK_SEMICOLON, "expected ';' or '=' after variable name");
        // uninitialized variable
        node->_var_definition.node = _mem.allocate_node(IC_AST_PRIMARY);
        node->_var_definition.node->_primary.token.type = IC_TOK_NUMBER;
        return node;
    }
    return produce_stmt_expr(it, _mem);
}

ic_ast_node* produce_stmt_expr(const ic_token** it, ic_mem& _mem)
{
    if (token_consume(it, IC_TOK_SEMICOLON))
        return nullptr;

    ic_ast_node* node = produce_expr(it, _mem);
    token_consume(it, IC_TOK_SEMICOLON, "expected ';' after expression");
    return node;
}

ic_ast_node* produce_expr(const ic_token** it, ic_mem& _mem)
{
    return produce_expr_assignment(it, _mem);
}

// produce_expr_assignment() - grows down and right
// produce_expr_binary() - grows up and right (given operators with the same precedence)

ic_ast_node* produce_expr_assignment(const ic_token** it, ic_mem& _mem)
{
    ic_ast_node* node_left = produce_expr_binary(it, IC_PRECEDENCE_OR, _mem);

    if (token_match_type(it, IC_TOK_EQUAL) || token_match_type(it, IC_TOK_PLUS_EQUAL) || token_match_type(it, IC_TOK_MINUS_EQUAL) ||
        token_match_type(it, IC_TOK_STAR_EQUAL) || token_match_type(it, IC_TOK_SLASH_EQUAL))
    {
        ic_ast_node* node = _mem.allocate_node(IC_AST_BINARY);
        node->_binary.token_operator = **it;
        token_advance(it);
        node->_binary.right = produce_expr(it, _mem);
        node->_binary.left = node_left;
        return node;
    }

    return node_left;
}

ic_ast_node* produce_expr_binary(const ic_token** it, ic_op_precedence precedence, ic_mem& _mem)
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
        return produce_expr_unary(it, _mem);
    }

    ic_ast_node* node = produce_expr_binary(it, ic_op_precedence(int(precedence) + 1), _mem);

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
        ic_ast_node* node_parent = _mem.allocate_node(IC_AST_BINARY);
        node_parent->_binary.token_operator = **it;
        token_advance(it);
        node_parent->_binary.right = produce_expr_binary(it, ic_op_precedence(int(precedence) + 1), _mem);
        node_parent->_binary.left = node;
        node = node_parent;
    }

    return node;
}

ic_ast_node* produce_expr_unary(const ic_token** it, ic_mem& _mem)
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
        ic_ast_node* node = _mem.allocate_node(IC_AST_UNARY);
        node->_unary.token_operator = **it;
        token_advance(it);
        node->_unary.node = produce_expr_unary(it, _mem);
        return node;
    }
    } // switch

    return produce_expr_primary(it, _mem);
}

ic_ast_node* produce_expr_primary(const ic_token** it, ic_mem& _mem)
{
    switch ((**it).type)
    {
    case IC_TOK_NUMBER:
    case IC_TOK_STRING:
    case IC_TOK_IDENTIFIER:
    {
        ic_ast_node* node_id = _mem.allocate_node(IC_AST_PRIMARY);
        node_id->_primary.token = **it;
        token_advance(it);

        if (token_consume(it, IC_TOK_LEFT_PAREN))
        {
            ic_ast_node* node = _mem.allocate_node(IC_AST_FUNCTION_CALL);
            node->_function_call.node = node_id;
            ic_ast_node* arg_it = node;

            if (token_consume(it, IC_TOK_RIGHT_PAREN))
                return node;

            int argc = 0;

            for(;;)
            {
                ic_ast_node* cnode = _mem.allocate_node(IC_AST_FUNCTION_CALL);
                cnode->_function_call.node = produce_expr(it, _mem);
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
        ic_ast_node* node = _mem.allocate_node(IC_AST_GROUPING);
        node->_grouping.node = produce_expr(it, _mem);
        token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after expression");
        return node;
    }
    } // switch

    exit_parsing(it, "expected primary, grouping, or function call expression");
    return {};
}
