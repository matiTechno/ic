#include "ic.h"

int main(int argc, const char** argv)
{
    assert(argc == 2);
    FILE* file = fopen(argv[1], "rb"); // oh my dear Windows, you have to make life harder
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
    source_code.back() = '\0';
    fclose(file);
    ic_runtime runtime; // todo, runtime structure is not needed at all right now
    runtime.init();
    ic_program program;
    ic_vm vm;
    // I hate chrono api, but it is easy to use and there is no portable C version for high resolution timers
    auto t1 = std::chrono::high_resolution_clock::now();
    {
        auto t1 = std::chrono::high_resolution_clock::now();
        bool success = runtime.compile_to_bytecode(source_code.data(), &program, IC_LIB_CORE, nullptr, 0);
        assert(success);
        auto t2 = std::chrono::high_resolution_clock::now();
        printf("compilation time: %d ms\n", (int)std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());
    }
    {
        auto t1 = std::chrono::high_resolution_clock::now();
        vm_init(vm, program, nullptr, 0);
        vm_run(vm, program);
        auto t2 = std::chrono::high_resolution_clock::now();
        printf("execution time: %d ms\n", (int)std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());
    }
    auto t2 = std::chrono::high_resolution_clock::now();
    printf("total time: %d ms\n", (int)std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());
    return 0;
}

// these type functions exist to avoid bugs in value.type initialization

ic_type non_pointer_type(ic_basic_type type)
{
    return { type, 0, 0 };
}

// todo; I don't like bool arguments like this one, it is not obvious what it does from a function call
ic_type pointer1_type(ic_basic_type type, bool at_const)
{
    return { type, 1, (unsigned)(at_const ? 2 : 0) };
}

bool is_struct(ic_type type)
{
    return !type.indirection_level && type.basic_type == IC_TYPE_STRUCT;
}

bool is_void(ic_type type)
{
    return !type.indirection_level && type.basic_type == IC_TYPE_VOID;
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

ic_data ic_host_prints(ic_data* argv)
{
    printf("prints: %s\n", (const char*)argv->pointer);
    return {};
}

ic_data ic_host_printf(ic_data* argv)
{
    printf("printf: %f\n", argv->f64);
    return {};
}

ic_data ic_host_printp(ic_data* argv)
{
    printf("printp: %p\n", argv->pointer);
    return {};
}

ic_data ic_host_malloc(ic_data* argv)
{
    void* ptr = malloc(argv->s32);
    ic_data data;
    data.pointer = ptr;
    return data;
}

ic_data ic_host_write_ppm6(ic_data* argv)
{
    FILE* file = fopen((char*)argv[0].pointer, "wb");
    char buf[1024];
    int width = argv[1].s32;
    int height = argv[2].s32;
    snprintf(buf, sizeof(buf), "P6 %d %d 255 ", width, height);
    int len = strlen(buf);
    fwrite(buf, 1, len, file);
    fwrite(argv[3].pointer, 1, width * height * 3, file);
    fclose(file);
    return {};
}

ic_data ic_host_tan(ic_data* argv)
{
    ic_data data;
    data.f64 = tan(argv->f64);
    return data;
}

ic_data ic_host_sqrt(ic_data* argv)
{
    ic_data data;
    data.f64 = sqrt(argv->f64);
    return data;
}

ic_data ic_host_pow(ic_data* argv)
{
    ic_data data;
    data.f64 = pow(argv[0].f64, argv[1].f64);
    return data;
}

ic_data ic_host_random01(ic_data*)
{
    ic_data data;
    data.f64 = (double)rand() / RAND_MAX; // todo; the distribution of numbers is probably not that good
    return data;
}

ic_data ic_host_exit(ic_data*)
{
    exit(0);
}

bool ic_string_compare(ic_string str1, ic_string str2)
{
    if (str1.len != str2.len)
        return false;

    return (strncmp(str1.data, str2.data, str1.len) == 0);
}

bool ic_tokenize(ic_runtime& runtime, const char* source_code);
ic_global produce_global(const ic_token** it, ic_runtime& runtime);

void resolve_function(ic_function& function, ic_runtime& runtime)
{
}

void ic_runtime::init()
{
    // todo, this sucks ass, after replacing vectors initialize all fucking members,
    // but anyway, it will be massive redesign
    _stmt_deque.size = 0;
    _expr_deque.size = 0;
}

void ic_runtime::load_core_functions()
{
    {
        const char* str = "prints";
        ic_string name = { str, strlen(str) };
        ic_function function;
        function.type = IC_FUN_HOST;
        function.token.string = name;
        function.return_type = non_pointer_type(IC_TYPE_VOID);
        function.param_count = 1;
        function.params[0].type = pointer1_type(IC_TYPE_S8, true);
        function.callback = ic_host_prints;
        _functions.push_back(function);
    }
    {
        const char* str = "printf";
        ic_string name = { str, strlen(str) };
        ic_function function;
        function.type = IC_FUN_HOST;
        function.token.string = name;
        function.return_type = non_pointer_type(IC_TYPE_VOID);
        function.param_count = 1;
        function.params[0].type = non_pointer_type(IC_TYPE_F64);
        function.callback = ic_host_printf;
        _functions.push_back(function);
    }
    {
        const char* str = "printp";
        ic_string name = { str, strlen(str) };
        ic_function function;
        function.type = IC_FUN_HOST;
        function.token.string = name;
        function.return_type = non_pointer_type(IC_TYPE_VOID);
        function.param_count = 1;
        function.params[0].type = pointer1_type(IC_TYPE_VOID, true);
        function.callback = ic_host_printp;
        _functions.push_back(function);
    }
    {
        const char* str = "malloc";
        ic_string name = { str, strlen(str) };
        ic_function function;
        function.type = IC_FUN_HOST;
        function.token.string = name;
        function.return_type = pointer1_type(IC_TYPE_VOID);
        function.param_count = 1;
        function.params[0].type = non_pointer_type(IC_TYPE_S32);
        function.callback = ic_host_malloc;
        _functions.push_back(function);
    }
    {
        const char* str = "write_ppm6";
        ic_string name = { str, strlen(str) };
        ic_function function;
        function.type = IC_FUN_HOST;
        function.token.string = name;
        function.return_type = non_pointer_type(IC_TYPE_VOID);
        function.param_count = 4;
        function.params[0].type = pointer1_type(IC_TYPE_S8, true);
        function.params[1].type = non_pointer_type(IC_TYPE_S32);
        function.params[2].type = non_pointer_type(IC_TYPE_S32);
        function.params[3].type = pointer1_type(IC_TYPE_U8);
        function.callback = ic_host_write_ppm6;
        _functions.push_back(function);
    }
    {
        const char* str = "tan";
        ic_string name = { str, strlen(str) };
        ic_function function;
        function.type = IC_FUN_HOST;
        function.token.string = name;
        function.return_type = non_pointer_type(IC_TYPE_F64);
        function.param_count = 1;
        function.params[0].type = non_pointer_type(IC_TYPE_F64);
        function.callback = ic_host_tan;
        _functions.push_back(function);
    }
    {
        const char* str = "sqrt";
        ic_string name = { str, strlen(str) };
        ic_function function;
        function.type = IC_FUN_HOST;
        function.token.string = name;
        function.return_type = non_pointer_type(IC_TYPE_F64);
        function.param_count = 1;
        function.params[0].type = non_pointer_type(IC_TYPE_F64);
        function.callback = ic_host_sqrt;
        _functions.push_back(function);
    }
    {
        const char* str = "pow";
        ic_string name = { str, strlen(str) };
        ic_function function;
        function.type = IC_FUN_HOST;
        function.token.string = name;
        function.return_type = non_pointer_type(IC_TYPE_F64);
        function.param_count = 2;
        function.params[0].type = non_pointer_type(IC_TYPE_F64);
        function.params[1].type = non_pointer_type(IC_TYPE_F64);
        function.callback = ic_host_pow;
        _functions.push_back(function);
    }
    {
        const char* str = "random01";
        ic_string name = { str, strlen(str) };
        ic_function function;
        function.type = IC_FUN_HOST;
        function.token.string = name;
        function.return_type = non_pointer_type(IC_TYPE_F64);
        function.param_count = 0;
        function.callback = ic_host_random01;
        _functions.push_back(function);
    }
    {
        const char* str = "exit";
        ic_string name = { str, strlen(str) };
        ic_function function;
        function.type = IC_FUN_HOST;
        function.token.string = name;
        function.return_type = non_pointer_type(IC_TYPE_VOID);
        function.param_count = 0;
        function.callback = ic_host_exit;
        _functions.push_back(function);
    }
}

void compile_cleanup(ic_runtime& runtime)
{
    runtime._tokens.clear();
    runtime._string_literals.clear();
    runtime._string_literals.clear();
    runtime._global_size = 0;
    runtime._stmt_deque.size = 0;
    runtime._expr_deque.size = 0;
    runtime._functions.clear();

    for (ic_struct& _struct : runtime._structs)
        ::free(_struct.members);

    runtime._structs.clear();
    runtime._global_vars.clear();
}

bool compare_types_drop_level_0_const(ic_type l, ic_type r)
{
    if (l.indirection_level != r.indirection_level)
        return false;

    if ((l.const_mask >> 1) != (r.const_mask >> 1))
        return false;

    if (l.basic_type != r.basic_type)
        return false;

    if (l.basic_type == IC_TYPE_STRUCT)
    {
        if (!ic_string_compare(l.struct_name, r.struct_name))
            return false;
    }

    return true;
}

bool ic_runtime::compile_to_bytecode(const char* source, ic_program* program, int libs, ic_host_function* host_functions, int host_functions_size)
{
    assert(source);
    bool success = true;
    const ic_token* token_it;

    if (!ic_tokenize(*this, source))
    {
        compile_cleanup(*this);
        return false;
    }

    // add lib and host functions to _functions
    load_core_functions(); // this is temporary

    _global_size = _string_literals.size() / sizeof(ic_data) + 1;
    token_it = _tokens.data();

    // parsing...
    while (token_it->type != IC_TOK_EOF)
    {
        ic_global global;

        try
        {
            global = produce_global(&token_it, *this);
        }
        catch (ic_exception_parsing)
        {
            compile_cleanup(*this);
            return false;
        }

        switch (global.type)
        {
        case IC_GLOBAL_FUNCTION:
        {
            ic_token token = global.function.token;

            ic_function* existing_fun = get_function(token.string, nullptr);
            if (existing_fun)
            {
                bool sig_match = true;
                sig_match = sig_match && compare_types_drop_level_0_const(existing_fun->return_type, global.function.return_type);
                sig_match = sig_match && existing_fun->param_count == global.function.param_count;
                for (int i = 0; i < existing_fun->param_count; ++i)
                    sig_match = sig_match && compare_types_drop_level_0_const(existing_fun->params[i].type, global.function.params[i].type);

                if (!sig_match)
                {
                    ic_print_error(IC_ERR_PARSING, token.line, token.col, "functions with the same name must have matching types");
                    if (existing_fun->type == IC_FUN_HOST)
                        printf("previous function was declared by host, not visible in a source file\n");
                    return false;
                }

                if (global.function.type == IC_FUN_SOURCE_FORWARD_DECL)
                    break;

                if (existing_fun->type == IC_FUN_SOURCE_FORWARD_DECL)
                {
                    *existing_fun = global.function;
                    break;
                }

                // function can be only defined once

                if(existing_fun->type == IC_FUN_HOST)
                    ic_print_error(IC_ERR_PARSING, token.line, token.col, "function (provided by host) with such name already exists");
                else
                    ic_print_error(IC_ERR_PARSING, token.line, token.col, "function with such name already exists");

                compile_cleanup(*this);
                return false;
            }
            assert(!existing_fun);
            _functions.push_back(global.function);
            break;
        }
        case IC_GLOBAL_STRUCT:
        {
            ic_token token = global._struct.token;

            if (get_struct(token.string))
            {
                ic_print_error(IC_ERR_PARSING, token.line, token.col, "struct with such name already exists");
                compile_cleanup(*this);
                return false;
            }

            _structs.push_back(global._struct);
            break;
        }
        case IC_GLOBAL_VAR_DECLARATION:
        {
            for (ic_var& var : _global_vars)
            {
                if (ic_string_compare(var.name, global.var.token.string))
                    assert(false); // todo print nice error
            }

            ic_var var;
            var.idx = _global_size * sizeof(ic_data);
            var.type = global.var.type;
            var.name = global.var.token.string;

            if (is_struct(var.type))
            {
                ic_struct* _struct = get_struct(var.type.struct_name);
                assert(_struct); //  todo
                _global_size += _struct->num_data;
            }
            else
                _global_size += 1;

            _global_vars.push_back(var);
            break;
        }
        default:
            assert(false);
        }
    }
    // compiling ...

    for (ic_function& function : _functions)
    {
        if (function.type == IC_FUN_SOURCE)
        {
            if (ic_string_compare(function.token.string, { "main", 4 }))
            {
                assert(function.param_count == 0);
                assert(is_void(function.return_type));
                _active_functions.push_back(&function);
                function.active = true;
                break;
            }
        }
    }

    assert(_active_functions.size());
    _add_to_active = true;

    for (int i = 0; i < _active_functions.size(); ++i) // size will increase as function currently compiled will pull more functions
    {
        // be careful because active_functions will be invalidated as new functions come in
        // non source functions are added to
        if (_active_functions[i]->type == IC_FUN_SOURCE)
            compile(*_active_functions[i], *this);
    }

    // now compile rest of the functions - to check if program is correct but these will not be included in final program structure
    // host functions are not checked for activness
    _add_to_active = false;

    for (ic_function& function : _functions)
    {
        if (function.type == IC_FUN_HOST || function.active)
            continue;

        if (function.type == IC_FUN_SOURCE)
        {
            printf("warning: function '%.*s' not used but compiled\n", function.token.string.len, function.token.string.data);
            compile(function, *this);
        }
        else if (function.type == IC_FUN_SOURCE_FORWARD_DECL)
            printf("warning: function '%.*s' not used but forward declared\n", function.token.string.len, function.token.string.data);
        else
            assert(false);
    }

    // set program structure
    program->strings_byte_size = _string_literals.size();
    program->strings = (char*)malloc(program->strings_byte_size);
    memcpy(program->strings, _string_literals.data(), program->strings_byte_size);
    program->global_data_size = _global_size;
    std::vector<ic_vm_function> vm_functions;

    for (ic_function* fun_ptr : _active_functions)
    {
        ic_function& function = *fun_ptr;

        ic_vm_function vmfun;
        vmfun.param_size = 0;

        for (int j = 0; j < function.param_count; ++j)
        {
            if (is_struct(function.params[j].type))
            {
                ic_struct* _struct = get_struct(function.params[j].type.struct_name);
                assert(_struct); // todo; nice error
                vmfun.param_size += _struct->num_data;
            }
            else
                vmfun.param_size += 1;
        }

        if (is_struct(function.return_type))
        {
            ic_struct* _struct = get_struct(function.return_type.struct_name);
            assert(_struct);
            vmfun.return_size = _struct->num_data; // todo, have a function that returns a data size of a type
        }
        else if(!is_void(function.return_type))
            vmfun.return_size = 1;
        else
            vmfun.return_size = 0;

        if (function.type == IC_FUN_HOST)
        {
            vmfun.host_impl = true;
            vmfun.callback = function.callback;
            // todo, create hash
        }
        else if (function.type == IC_FUN_SOURCE_FORWARD_DECL)
        {
            vmfun.host_impl = true;
            vmfun.callback = nullptr;
            // create hash
        }
        else
        {
            assert(function.type == IC_FUN_SOURCE);
            vmfun.host_impl = false;
            vmfun.bytecode = function.bytecode;
            vmfun.stack_size = function.stack_size;
        }

        vm_functions.push_back(vmfun);
    }

    program->functions = (ic_vm_function*)malloc(vm_functions.size() * sizeof(ic_vm_function));
    memcpy(program->functions, vm_functions.data(), vm_functions.size() * sizeof(ic_vm_function));
    program->functions_size = vm_functions.size();

    compile_cleanup(*this);
    return true;
}

void ic_runtime::free()
{
    _functions.clear();
    _stmt_deque.free();
    _expr_deque.free();
}

// todo, this is very temporary
// this must be more readable... this idx stuff is lolz
ic_function* ic_runtime::get_function(ic_string name, int* idx)
{
    ic_function* target_fun = nullptr;
    for (ic_function& function : _functions)
    {
        if (ic_string_compare(function.token.string, name))
            target_fun = &function;
    }

    if (!idx)
        return target_fun;



    // part used during compilation

    assert(target_fun);

    if (!_add_to_active)
        return target_fun;

    for (int i = 0; i < _active_functions.size(); ++i)
    {
        if (target_fun == _active_functions[i])
        {
            *idx = i;
            return target_fun;
        }
    }

    if (target_fun->type != IC_FUN_HOST)
        target_fun->active = true;

    _active_functions.push_back(target_fun);
    *idx = _active_functions.size() - 1;
    return target_fun;
}

ic_struct* ic_runtime::get_struct(ic_string name)
{
    for (ic_struct& _struct : _structs)
    {
        if (ic_string_compare(_struct.token.string, name))
            return &_struct;
    }
    return nullptr;
}

struct ic_lexer
{
    std::vector<ic_token>& _tokens;
    int _line = 1;
    int _col = 1;
    int _token_line;
    int _token_col;
    const char* _source_it;

    // when string.data is nullptr number is used
    void add_token_impl(ic_token_type type, ic_string string, double number)
    {
        ic_token token;
        token.type = type;
        token.line = _token_line;
        token.col = _token_col;

        if(string.data)
            token.string = string;
        else
            token.number = number;

        _tokens.push_back(token);
    }

    void add_token(ic_token_type type) { add_token_impl(type, {nullptr}, {}); }
    void add_token_string(ic_token_type type, ic_string string) { add_token_impl(type, string, {}); }
    void add_token_number(ic_token_type type, double number) { add_token_impl(type, {nullptr}, number); }
    bool end() { return *_source_it == '\0'; }
    char peek() { return *_source_it; }
    const char* pos() { return _source_it - 1; } // this function name makes sense from the interface perspective

    char advance()
    {
        assert(!end());
        const char c = *_source_it;
        ++_source_it;

        if (c == '\n')
        {
            ++_line;
            _col = 1;
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

bool is_identifier_char(char c)
{
    return is_digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_');
}

bool ic_tokenize(ic_runtime& runtime, const char* source_code)
{
    ic_lexer lexer{ runtime._tokens };
    lexer._source_it = source_code;

    while (!lexer.end())
    {
        lexer._token_line = lexer._line;
        lexer._token_col = lexer._col;
        const char c = lexer.advance();
        const char* const token_begin = lexer.pos();

        switch (c)
        {
        case ' ':
        case '\t':
        case '\n':
        case '\r':
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
        case '%':
            lexer.add_token(IC_TOK_PERCENT);
            break;
        case '[':
            lexer.add_token(IC_TOK_LEFT_BRACKET);
            break;
        case ']':
            lexer.add_token(IC_TOK_RIGHT_BRACKET);
            break;
        case '.':
            lexer.add_token(IC_TOK_DOT);
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
            lexer.add_token(lexer.consume('=') ? IC_TOK_STAR_EQUAL : IC_TOK_STAR);
            break;
        case '-':
            lexer.add_token(lexer.consume('=') ? IC_TOK_MINUS_EQUAL : lexer.consume('-') ? IC_TOK_MINUS_MINUS :
                lexer.consume('>') ? IC_TOK_ARROW : IC_TOK_MINUS);
            break;
        case '+':
            lexer.add_token(lexer.consume('=') ? IC_TOK_PLUS_EQUAL : lexer.consume('+') ? IC_TOK_PLUS_PLUS : IC_TOK_PLUS);
            break;
        case '|':
        {
            if (lexer.consume('|'))
                lexer.add_token(IC_TOK_VBAR_VBAR);
            else // single | is not allowed in the source code
            {
                ic_print_error(IC_ERR_LEXING, lexer._line, lexer._col, "unexpected character '%c'", c);
                return false;
            }

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

            if (lexer.end() && *lexer.pos() != '"')
            {
                ic_print_error(IC_ERR_LEXING, lexer._token_line, lexer._token_col, "unterminated string literal");
                return false;
            }

            // todo, if the same string literal already exists, reuse it
            int idx_begin = runtime._string_literals.size();
            lexer.add_token_number(IC_TOK_STRING_LITERAL, idx_begin);
            const char* string_begin = token_begin + 1; // skip first "
            int len = lexer.pos() - string_begin; // this doesn't count last "
            runtime._string_literals.resize(runtime._string_literals.size() + len + 1);
            memcpy(&runtime._string_literals[idx_begin], string_begin, len);
            runtime._string_literals.back() = '\0';
            break;
        }
        case '\'':
        {
            while (!lexer.end() && lexer.advance() != '\'')
                ;

            if (lexer.end() && *lexer.pos() != '\'')
            {
                ic_print_error(IC_ERR_LEXING, lexer._token_line, lexer._token_col, "unterminated character literal");
                return false;
            }

            const char* string_begin = token_begin + 1; // skip first '
            int len = lexer.pos() - string_begin; // this doesn't count last '
            char code = -1;

            if (len && string_begin[0] == '\\') // this is to not allow '\' as a valid literal; '\\' is valid
            {
                if (len == 2)
                {
                    if (string_begin[1] == '\\')
                        code = '\\';
                    else if (string_begin[1] == 'n')
                        code = '\n';
                    else if (string_begin[1] == '0')
                        code = '\0';
                    }
            }
            else if (len == 1)
                code = *string_begin;

            if (code == -1)
            {
                ic_print_error(IC_ERR_LEXING, lexer._token_line, lexer._token_col,
                    "invalid character literal; only printable, \\n and \\0 characters are supported");
                return false;
            }

            lexer.add_token_number(IC_TOK_CHARACTER_LITERAL, code);
            break;
        }
        default:
        {
            if (is_digit(c))
            {
                while (!lexer.end() && is_digit(lexer.peek()))
                    lexer.advance();

                ic_token_type token_type = IC_TOK_INT_NUMBER_LITERAL;

                if (lexer.peek() == '.')
                {
                    token_type = IC_TOK_FLOAT_NUMBER_LITERAL;
                    lexer.advance();
                }

                while (!lexer.end() && is_digit(lexer.peek()))
                    lexer.advance();

                const int len = (lexer.pos() + 1) - token_begin;
                char buf[1024];
                assert(len < sizeof(buf));
                memcpy(buf, token_begin, len);
                buf[len] = '\0';
                lexer.add_token_number(token_type, atof(buf));
            }
            else if (is_identifier_char(c))
            {
                while (!lexer.end() && is_identifier_char(lexer.peek()))
                    lexer.advance();

                ic_string string = { token_begin, (lexer.pos() + 1) - token_begin };
                bool is_keyword = false;
                ic_token_type token_type;

                for (const ic_keyword& keyword : _keywords)
                {
                    if (ic_string_compare(string, { keyword.str, int(strlen(keyword.str)) }))
                    {
                        is_keyword = true;
                        token_type = keyword.token_type;
                        break;
                    }
                }

                if (!is_keyword)
                    lexer.add_token_string(IC_TOK_IDENTIFIER, string);
                else
                    lexer.add_token(token_type);
            }
            else
            {
                ic_print_error(IC_ERR_LEXING, lexer._line, lexer._col, "unexpected character '%c'", c);
                return false;
            }
        }
        } // switch
    }

    ic_token token;
    token.type = IC_TOK_EOF;
    token.line = lexer._line;
    token.col = lexer._col;
    lexer._tokens.push_back(token);
    return true;
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
ic_expr* produce_expr_subscript(const ic_token** it, ic_runtime& runtime);
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

bool produce_type(const ic_token** it, ic_runtime& runtime, ic_type& type)
{
    bool init = false;
    type.indirection_level = 0;
    type.const_mask = 0;

    if (token_consume(it, IC_TOK_CONST))
    {
        type.const_mask = 1 << 31;
        init = true;
    }

    switch ((**it).type)
    {
    case IC_TOK_BOOL:
        type.basic_type = IC_TYPE_BOOL;
        break;
    case IC_TOK_S8:
        type.basic_type = IC_TYPE_S8;
        break;
    case IC_TOK_U8:
        type.basic_type = IC_TYPE_U8;
        break;
    case IC_TOK_S32:
        type.basic_type = IC_TYPE_S32;
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
    case IC_TOK_IDENTIFIER:
        if (runtime.get_struct((**it).string))
        {
            type.basic_type = IC_TYPE_STRUCT;
            type.struct_name = (**it).string;
            break;
        }
        // todo; warning that a type name is possibly misspelled
        // fall through
    default:
        if (init)
            exit_parsing(it, "expected type name after const keyword");
        return false;
    }

    token_advance(it);

    while (token_consume(it, IC_TOK_STAR))
    {
        if (type.indirection_level == 31)
            exit_parsing(it, "exceeded maximal level of indirection");

        type.indirection_level += 1;

        if (token_consume(it, IC_TOK_CONST))
            type.const_mask += 1 << (31 - type.indirection_level);
    }

    type.const_mask = type.const_mask >> (31 - type.indirection_level);
    return true;
}

#define IC_MAX_MEMBERS 20

ic_global produce_global(const ic_token** it, ic_runtime& runtime)
{
    if (token_consume(it, IC_TOK_STRUCT))
    {
        ic_global global;
        global.type = IC_GLOBAL_STRUCT;
        ic_struct& _struct = global._struct;
        _struct.token = **it;
        token_consume(it, IC_TOK_IDENTIFIER, "expected struct name");
        token_consume(it, IC_TOK_LEFT_BRACE, "expected '{'");
        ic_struct_member members[IC_MAX_MEMBERS]; // todo; this is only temp solution
        _struct.num_members = 0;
        _struct.num_data = 0;
        ic_type type;

        while (produce_type(it, runtime, type))
        {
            assert(_struct.num_members < IC_MAX_MEMBERS);

            // todo; support const members
            if (type.const_mask & 1)
                exit_parsing(it, "struct member can't be const");

            if (is_void(type))
                exit_parsing(it, "struct member can't be of type void");

            if (is_struct(type))
            {
                ic_struct* sub_struct = runtime.get_struct(type.struct_name);
                assert(sub_struct); //  todo, print nice error here
                _struct.num_data += sub_struct->num_data;
            }
            else
                _struct.num_data += 1;

            ic_struct_member& member = members[_struct.num_members];
            member.type = type;
            member.name = (**it).string;
            _struct.num_members += 1;
            token_consume(it, IC_TOK_IDENTIFIER, "expected member name");
            token_consume(it, IC_TOK_SEMICOLON, "expected ';' after struct member name");
        }

        int bytes = sizeof(ic_struct_member) * _struct.num_members;
        _struct.members = (ic_struct_member*)malloc(bytes);
        memcpy(_struct.members, members, bytes);
        token_consume(it, IC_TOK_RIGHT_BRACE, "expected '}'");
        token_consume(it, IC_TOK_SEMICOLON, "expected ';'");
        return global;
    }

    // else produce a function or a global variable declaration
    ic_type type;

    if (!produce_type(it, runtime, type))
        exit_parsing(it, "expected: a type of a global variable / return type of a function / 'struct' keyword");

    ic_token token_id = **it;
    token_consume(it, IC_TOK_IDENTIFIER, "expected identifier");

    // function
    if (token_consume(it, IC_TOK_LEFT_PAREN))
    {
        ic_global global;
        global.type = IC_GLOBAL_FUNCTION;
        ic_function& function = global.function;
        function.active = false;
        function.token = token_id;
        function.return_type = type;
        function.param_count = 0;

        if (!token_consume(it, IC_TOK_RIGHT_PAREN))
        {
            for (;;)
            {
                if (function.param_count >= IC_MAX_ARGC)
                    exit_parsing(it, "exceeded maximal number of parameters (%d)", IC_MAX_ARGC);

                ic_type param_type;

                if (!produce_type(it, runtime, param_type))
                    exit_parsing(it, "expected parameter type");

                if (is_void(param_type))
                    exit_parsing(it, "parameter can't be of type void");

                function.params[function.param_count].type = param_type;
                function.params[function.param_count].name = (**it).string;
                token_consume(it, IC_TOK_IDENTIFIER, "expected parameter name");
                function.param_count += 1;

                if (token_consume(it, IC_TOK_RIGHT_PAREN))
                    break;

                token_consume(it, IC_TOK_COMMA, "expected ',' or ')' after a function argument");
            }
        }

        if (token_consume(it, IC_TOK_SEMICOLON))
        {
            function.type = IC_FUN_SOURCE_FORWARD_DECL;
            return global;
        }

        function.type = IC_FUN_SOURCE;
        function.body = produce_stmt(it, runtime);

        if (function.body->type != IC_STMT_COMPOUND)
            exit_parsing(it, "expected compound stmt after function parameter list");

        function.body->_compound.push_scope = false; // do not allow shadowing of arguments
        return global;
    }

    // variable
    // todo; redundant with produce_stmt_var_declaration()
    if (is_void(type))
        exit_parsing(it, "variables can't be of type void");

    ic_global global;
    global.type = IC_GLOBAL_VAR_DECLARATION;
    global.var.type = type;
    global.var.token = token_id;

    if ((**it).type == IC_TOK_EQUAL)
        exit_parsing(it, "global variables can't be initialized by an expression, they are set to 0 by default");

    token_consume(it, IC_TOK_SEMICOLON, "expected ';' or '=' after variable name");
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

        // be consistent with IC_TOK_FOR
        if (stmt->_for.body->type == IC_STMT_COMPOUND)
            stmt->_for.body->_compound.push_scope = false;

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
    ic_type type;

    if (produce_type(it, runtime, type))
    {
        if (is_void(type))
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
    ic_expr* expr_lhs = produce_expr_binary(it, IC_PRECEDENCE_LOGICAL_OR, runtime);

    switch ((**it).type)
    {
    case IC_TOK_EQUAL:
    case IC_TOK_PLUS_EQUAL:
    case IC_TOK_MINUS_EQUAL:
    case IC_TOK_STAR_EQUAL:
    case IC_TOK_SLASH_EQUAL:
        ic_expr* expr = runtime.allocate_expr(IC_EXPR_BINARY, **it);
        token_advance(it);
        expr->_binary.rhs = produce_expr(it, runtime);
        expr->_binary.lhs = expr_lhs;
        return expr;
    }

    return expr_lhs;
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
        target_token_types[2] = IC_TOK_PERCENT;
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
        expr_parent->_binary.rhs = produce_expr_binary(it, ic_op_precedence(int(precedence) + 1), runtime);
        expr_parent->_binary.lhs = expr;
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
    case IC_TOK_LEFT_PAREN:
    {
        token_advance(it);
        ic_type type;

        if (produce_type(it, runtime, type))
        {
            token_consume(it, IC_TOK_RIGHT_PAREN, "expected ) at the end of a cast operator");
            ic_expr* expr = runtime.allocate_expr(IC_EXPR_CAST_OPERATOR, **it);
            expr->_cast_operator.type = type;
            expr->_cast_operator.expr = produce_expr_unary(it, runtime);
            return expr;
        }

        *it -= 1; // go back by one
        break;
    }
    case IC_TOK_SIZEOF:
    {
        ic_expr* expr = runtime.allocate_expr(IC_EXPR_SIZEOF, **it);
        token_advance(it);
        token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after sizeof operator");
        
        if (!produce_type(it, runtime, expr->_sizeof.type))
            exit_parsing(it, "expected type");

        token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after type");
        return expr;
    }
    } // switch

    return produce_expr_subscript(it, runtime);
}

ic_expr* produce_expr_subscript(const ic_token** it, ic_runtime& runtime)
{
    ic_expr* lhs = produce_expr_primary(it, runtime);

    for (;;)
    {
        if ((**it).type == IC_TOK_LEFT_BRACKET)
        {
            ic_expr* expr = runtime.allocate_expr(IC_EXPR_SUBSCRIPT, **it);
            token_advance(it);
            expr->_subscript.lhs = lhs;
            expr->_subscript.rhs = produce_expr(it, runtime);
            token_consume(it, IC_TOK_RIGHT_BRACKET, "expected a closing bracket for a subscript operator");
            lhs = expr;
        }
        else if ((**it).type == IC_TOK_DOT || (**it).type == IC_TOK_ARROW)
        {
            ic_expr* expr = runtime.allocate_expr(IC_EXPR_MEMBER_ACCESS, **it);
            token_advance(it);
            expr->_member_access.lhs = lhs;
            expr->_member_access.rhs_token = **it;
            token_consume(it, IC_TOK_IDENTIFIER, "expected member name after '.' operator");
            lhs = expr;
        }
        else
            break;
    }

    return lhs;
}

ic_expr* produce_expr_primary(const ic_token** it, ic_runtime& runtime)
{
    switch ((**it).type)
    {
    case IC_TOK_INT_NUMBER_LITERAL:
    case IC_TOK_FLOAT_NUMBER_LITERAL:
    case IC_TOK_STRING_LITERAL:
    case IC_TOK_CHARACTER_LITERAL:
    case IC_TOK_TRUE:
    case IC_TOK_FALSE:
    case IC_TOK_NULLPTR:
    {
        ic_expr* expr = runtime.allocate_expr(IC_EXPR_PRIMARY, **it);
        token_advance(it);
        return expr;
    }

    case IC_TOK_IDENTIFIER:
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
