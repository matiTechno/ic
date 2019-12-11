#include "ic_impl.h"

bool string_compare(ic_string str1, ic_string str2)
{
    if (str1.len != str2.len)
        return false;

    return (strncmp(str1.data, str2.data, str1.len) == 0);
}

bool is_struct(ic_type type)
{
    return !type.indirection_level && type.basic_type == IC_TYPE_STRUCT;
}

bool is_void(ic_type type)
{
    return !type.indirection_level && type.basic_type == IC_TYPE_VOID;
}

ic_type non_pointer_type(ic_basic_type btype)
{
    return { btype, 0 };
}

ic_type const_pointer1_type(ic_basic_type btype)
{
    return { btype, 1, 1 << 1 };
}

ic_type pointer1_type(ic_basic_type btype)
{
    return { btype, 1};
}

int read_int(unsigned char** buf_it)
{
    int v;
    memcpy(&v, *buf_it, sizeof(int));
    *buf_it += sizeof(int);
    return v;
}

float read_float(unsigned char** buf_it)
{
    float v;
    memcpy(&v, *buf_it, sizeof(float));
    *buf_it += sizeof(float);
    return v;
}

double read_double(unsigned char** buf_it)
{
    double v;
    memcpy(&v, *buf_it, sizeof(double));
    *buf_it += sizeof(double);
    return v;
}

void write_bytes(unsigned char** buf_it, void* src, int bytes)
{
    memcpy(*buf_it, src, bytes);
    *buf_it += bytes;
}

void read_bytes(void* dst, unsigned char** buf_it, int bytes)
{
    memcpy(dst, *buf_it, bytes);
    *buf_it += bytes;
}

void ic_program_serialize(ic_program& program, unsigned char*& buf, int& size)
{
    size = sizeof(ic_program) + program.data_size + program.functions_size * sizeof(ic_vm_function);
    buf = (unsigned char*)malloc(size);
    unsigned char* buf_it = buf;
    write_bytes(&buf_it, &program, sizeof(ic_program));
    write_bytes(&buf_it, program.data, program.data_size);
    for(int i = 0; i < program.functions_size; ++i)
        write_bytes(&buf_it, program.functions + i, sizeof(ic_vm_function));
}

void ic_buf_free(unsigned char* buf)
{
    free(buf);
}

unsigned int hash_string(const char* str)
{
    unsigned int hash = 5381;
    int c;
    while (c = *str)
    {
        hash = ((hash << 5) + hash) + c;
        ++str;
    }
    return hash;
}

 void resolve_vm_function(ic_vm_function& fun, ic_host_function* it)
 {
     assert(it);
     while (it->prototype_str)
     {
         if (fun.hash == hash_string(it->prototype_str))
         {
             fun.callback = it->callback;
             fun.host_data = it->host_data;
             return;
         }
         ++it;
     }
     assert(false);
 }

ic_data host_prints(ic_data* argv, void*)
{
    printf("prints: %s\n", (const char*)argv->pointer);
    return {};
}

ic_data host_printf(ic_data* argv, void*)
{
    printf("printf: %f\n", argv->f64);
    return {};
}

ic_data host_printp(ic_data* argv, void*)
{
    printf("printp: %p\n", argv->pointer);
    return {};
}

ic_data host_malloc(ic_data* argv, void*)
{
    void* ptr = malloc(argv->s32);
    ic_data data;
    data.pointer = ptr;
    return data;
}

ic_data host_tan(ic_data* argv, void*)
{
    ic_data data;
    data.f64 = tan(argv->f64);
    return data;
}

ic_data host_sqrt(ic_data* argv, void*)
{
    ic_data data;
    data.f64 = sqrt(argv->f64);
    return data;
}

ic_data host_pow(ic_data* argv, void*)
{
    ic_data data;
    data.f64 = pow(argv[0].f64, argv[1].f64);
    return data;
}

ic_data host_exit(ic_data*, void*)
{
    exit(0);
}

static ic_host_function _core_lib[] =
{
    {"void prints(const s8*)", host_prints},
    {"void printf(f64)", host_printf},
    {"void printp(const void*)", host_printp},
    {"void* malloc(s32)", host_malloc},
    {"f64 tan(f64)", host_tan},
    {"f64 sqrt(f64)", host_sqrt},
    {"f64 pow(f64, f64)", host_pow},
    {"void exit()", host_exit},
    nullptr
};

#define IC_USER_FUNCTION -1

void ic_program_init_load(ic_program& program, unsigned char* buf, int libs, ic_host_function* host_functions)
{
    assert(buf);
    unsigned char* buf_it = buf;
    read_bytes(&program, &buf_it, sizeof(program));
    program.data = (unsigned char*)malloc(program.data_size);
    read_bytes(program.data, &buf_it, program.data_size);
    program.functions = (ic_vm_function*)malloc(program.functions_size * sizeof(ic_vm_function));

    for (int i = 0; i < program.functions_size; ++i)
    {
        ic_vm_function& fun = program.functions[i];
        read_bytes(&fun, &buf_it, sizeof(ic_vm_function));

        if (!fun.host_impl)
            continue;

        if (fun.origin == IC_LIB_CORE)
            resolve_vm_function(fun, _core_lib);
        else if (fun.origin == IC_USER_FUNCTION)
            resolve_vm_function(fun, host_functions);
        else
            assert(false);
    }
}

void ic_program_free(ic_program& program)
{
    free(program.data);
    free(program.functions);
}

// neded for e.g. statements and expressions to not invalidate pointers and allocate from a pool
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

void print(ic_print_type type, int line, int col, ic_string* source_lines, const char* err_msg)
{
    assert(source_lines);
    ic_string source_line = source_lines[line];
    printf("%s (line: %d, col: %d): %s\n%.*s\n", type == IC_PERROR ? "error" : "warning",
        line, col, err_msg, source_line.len, source_line.data);

    for (int i = 1; i < col; ++i)
        printf("-");
    printf("^\n");
}

struct ic_parser
{
    ic_deque<ic_expr, 1000> expressions;
    ic_deque<ic_stmt, 1000> statements;
    ic_global_scope* gscope;
    bool error;
    ic_token* token_it;
    ic_string* source_lines;

    void init(ic_global_scope& global_scope)
    {
        expressions.init();
        statements.init();
        gscope = &global_scope;
        error = false;
    }

    void free()
    {
        expressions.free();
        statements.free();
    }

    ic_stmt* allocate_stmt(ic_stmt_type type, ic_token token)
    {
        ic_stmt* stmt = statements.allocate();
        memset(stmt, 0, sizeof(ic_stmt));
        stmt->type = type;
        stmt->token = token;
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

    void advance()
    {
        if (token_it->type != IC_TOK_EOF)
            ++token_it;
    }

    void consume(ic_token_type type, const char* err_msg)
    {
        if (token_it->type == type)
        {
            advance();
            return;
        }
        exit(err_msg);
    }
    
    bool try_consume(ic_token_type type)
    {
        if (token_it->type == type)
        {
            advance();
            return true;
        }
        return false;
    }

    void exit(const char* err_msg)
    {
        if (error)
            return;

        print(IC_PERROR, token_it->line, token_it->col, source_lines, err_msg);
        error = true;

        while (token_it->type != IC_TOK_EOF)
            ++token_it;
    }

    ic_token& get_token()
    {
        return *token_it;
    }
};

bool lex(const char* source, std::vector<ic_token>& _tokens, std::vector<unsigned char>& _string_data,
    std::vector<ic_string>& source_lines);
ic_type produce_type(ic_parser& parser);
void produce_parameter_list(ic_parser& parser, ic_function& function);
ic_decl produce_decl(ic_parser& parser);

bool load_host_functions(ic_host_function* it, int origin, ic_parser& parser)
{
    assert(it);
    std::vector<ic_token> tokens;
    std::vector<unsigned char> string_data;
    std::vector<ic_string> source_lines;

    while (it->prototype_str)
    {
        tokens.clear();
        source_lines.clear();

        if (!lex(it->prototype_str, tokens, string_data, source_lines))
            return false;
        parser.token_it = tokens.data();
        parser.source_lines = source_lines.data();
        ic_function function;
        function.type = IC_FUN_HOST;
        function.return_type = produce_type(parser);
        function.token = parser.get_token();
        parser.consume(IC_TOK_IDENTIFIER, "expected function name");
        // proudce_parameter_list assumes that ( is consumed this is to avoid redundancy in produce_decl()
        parser.consume(IC_TOK_LEFT_PAREN, "expected '('");
        produce_parameter_list(parser, function);

        if (parser.error)
            return false;
        function.host_function = it;
        function.origin = origin;
        parser.gscope->functions.push_back(function);
        ++it;
    }
    return true;
}

void print_error(ic_token token, std::vector<ic_string>& lines, const char* msg)
{
    print(IC_PERROR, token.line, token.col, lines.data(), msg);
}

bool program_init_compile_impl(ic_program& program, const char* source, int libs, ic_host_function* host_functions)
{
    assert(source);
    ic_global_scope gscope;
    ic_parser parser;
    parser.init(gscope);
    bool success = true;

    if (libs & IC_LIB_CORE)
        success = load_host_functions(_core_lib, IC_LIB_CORE, parser);
    if (host_functions)
        success = success && load_host_functions(host_functions, IC_USER_FUNCTION, parser);
    if (!success)
        return false;

    std::vector<ic_token> tokens;
    std::vector<unsigned char> program_data;
    std::vector<ic_string> source_lines;
    success = lex(source, tokens, program_data, source_lines);

    if (!success)
        return false;

    const int strings_byte_size = program_data.size();
    gscope.global_data_size = strings_byte_size / sizeof(ic_data) + 1;
    parser.token_it = tokens.data();
    parser.source_lines = source_lines.data();

    while (parser.get_token().type != IC_TOK_EOF)
    {
        ic_decl decl = produce_decl(parser);

        if (parser.error)
            return false;

        switch (decl.type)
        {
        case IC_DECL_FUNCTION:
        {
            ic_token token = decl.function.token;
            ic_function* prev_function = gscope.get_function(token.string);

            if (prev_function)
            {
                if (prev_function->type == IC_FUN_HOST)
                    print_error(token, source_lines, "function with such name (provided by host) already exists");
                else
                    print_error(token, source_lines, "function with such name already exists");
                return false;
            }
            gscope.functions.push_back(decl.function);
            break;
        }
        case IC_DECL_STRUCT:
        {
            ic_token token = decl._struct.token;

            if (gscope.get_struct(token.string))
            {
                print_error(token, source_lines, "struct with such name already exists");
                return false;
            }
            gscope.structs.push_back(decl._struct);
            break;
        }
        case IC_DECL_VAR:
        {
            ic_token token = decl.var.token;

            if (gscope.get_var(token.string))
            {
                print_error(token, source_lines, "global variable with such name already exists");
                return false;
            }
            ic_var var;
            var.idx = gscope.global_data_size * sizeof(ic_data);
            var.type = decl.var.type;
            var.name = token.string;

            if (is_struct(var.type))
            {
                ic_struct* _struct = gscope.get_struct(var.type.struct_name);
                assert(_struct);
                gscope.global_data_size += _struct->num_data;
            }
            else
                gscope.global_data_size += 1;

            gscope.vars.push_back(var);
            break;
        }
        default:
            assert(false);
        }
    } // while

    std::vector<ic_function*> active_functions;

    for (ic_function& function : gscope.functions)
    {
        if (function.type == IC_FUN_SOURCE)
        {
            if (string_compare(function.token.string, { "main", 4 }))
            {
                success = function.param_count == 0;
                success = success && is_void(function.return_type);
                active_functions.push_back(&function);
            }
            function.data_idx = -1; // this is important
        }
    }

    if (!success || !active_functions.size())
        return false;

    // important, active_functions.size() changes during loop execution
    for (int i = 0; i < active_functions.size(); ++i)
    {
        if (active_functions[i]->type == IC_FUN_SOURCE)
        {
            success = compile_function(*active_functions[i], gscope, &active_functions, &program_data, source_lines.data());
            if (!success)
                return false;
        }
    }

    // compile inactive functions for code corectness, these will not be included in returned program
    for (ic_function& function : gscope.functions)
    {
        if (function.type == IC_FUN_HOST || function.data_idx != -1)
            continue;
        print(IC_PWARN, function.token.line, function.token.col, source_lines.data(), "function defined but not used");
        success = compile_function(function, gscope, nullptr, nullptr, source_lines.data());
        if (!success)
            return false;
    }
    program.strings_byte_size = strings_byte_size;
    program.data_size = program_data.size();
    program.global_data_size = gscope.global_data_size;
    program.data = (unsigned char*)malloc(program.data_size);
    memcpy(program.data, program_data.data(), program.data_size);
    program.functions_size = active_functions.size();
    program.functions = (ic_vm_function*)malloc(program.functions_size * sizeof(ic_vm_function));

    for(int i = 0; i < active_functions.size(); ++i)
    {
        ic_function& fun = *active_functions[i];
        ic_vm_function& vmfun = program.functions[i];
        vmfun.host_impl = fun.type == IC_FUN_HOST;
        vmfun.param_size = 0;

        for (int j = 0; j < fun.param_count; ++j)
        {
            if (is_struct(fun.params[j].type))
            {
                ic_struct* _struct = gscope.get_struct(fun.params[j].type.struct_name);
                assert(_struct);
                vmfun.param_size += _struct->num_data;
            }
            else
                vmfun.param_size += 1;
        }

        if (vmfun.host_impl)
        {
            vmfun.callback = fun.host_function->callback;
            vmfun.host_data = fun.host_function->host_data;
            vmfun.hash = hash_string(fun.host_function->prototype_str);
            vmfun.origin = fun.origin;
            vmfun.returns_value = is_void(fun.return_type) ? 0 : 1;
            // make sure there is no hash collision with previously initialized vm functions
            for(int j = 0; j < i; ++j)
                assert(vmfun.hash != program.functions[j].hash);
        }
        else
        {
            vmfun.data_idx = fun.data_idx;
            vmfun.stack_size = fun.stack_size;
        }
    }
    return true;
}

bool ic_program_init_compile(ic_program& program, const char* source, int libs, ic_host_function* host_functions)
{
    // keep all the memory resources here and pass down to implementation, deallocate on exit
    // remove all std::vector usage and replace with ic_array
    // but first implement error line printing
    return program_init_compile_impl(program, source, libs, host_functions);
}

struct ic_lexer
{
    std::vector<ic_token>* tokens;
    int line;
    int col;
    int token_line;
    int token_col;
    const char* source_it;

    // when string.data is nullptr number is used
    void add_token_impl(ic_token_type type, ic_string string, double number)
    {
        ic_token token;
        token.type = type;
        token.line = token_line;
        token.col = token_col;

        if(string.data)
            token.string = string;
        else
            token.number = number;
        tokens->push_back(token);
    }

    void add_token(ic_token_type type) { add_token_impl(type, {nullptr}, {}); }
    void add_token_string(ic_token_type type, ic_string string) { add_token_impl(type, string, {}); }
    void add_token_number(ic_token_type type, double number) { add_token_impl(type, {nullptr}, number); }
    bool end() { return *source_it == '\0'; }
    char peek() { return *source_it; }
    const char* pos() { return source_it - 1; } // this function name makes sense from the interface perspective

    char advance()
    {
        assert(!end());
        const char c = *source_it;
        ++source_it;

        if (c == '\n')
        {
            ++line;
            col = 1;
        }
        else
            ++col;
        return c;
    }

    bool consume(char c)
    {
        if (end())
            return false;

        if (*source_it != c)
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
    {"f32", IC_TOK_F32},
    {"f64", IC_TOK_F64},
    {"void", IC_TOK_VOID},
    {"nullptr", IC_TOK_NULLPTR},
    {"const", IC_TOK_CONST},
    {"struct", IC_TOK_STRUCT},
    {"sizeof", IC_TOK_SIZEOF},
};

bool lex(const char* source, std::vector<ic_token>& _tokens, std::vector<unsigned char>& string_data,
    std::vector<ic_string>& source_lines)
{
    assert(source);
    {
        source_lines.push_back({}); // dummy line for index 0
        const char* it = source;
        const char* line_begin = it;
        int len = 0;

        while (*it)
        {
            if (*it == '\n')
            {
                source_lines.push_back({ line_begin, len });
                len = 0;
                line_begin = it + 1;
            }
            else
                len += 1;
            it += 1;
        }
    }

    ic_lexer lexer;
    lexer.tokens = &_tokens;
    lexer.line = 1;
    lexer.col = 1;
    lexer.source_it = source;

    while (!lexer.end())
    {
        lexer.token_line = lexer.line;
        lexer.token_col = lexer.col;
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
                print(IC_PERROR, lexer.line, lexer.col, source_lines.data(), "unexpected character");
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
                print(IC_PERROR, lexer.token_line, lexer.token_col, source_lines.data(), "unterminated string literal");
                return false;
            }

            // todo, if the same string literal already exists, reuse it
            int idx_begin = string_data.size();
            lexer.add_token_number(IC_TOK_STRING_LITERAL, idx_begin);
            const char* string_begin = token_begin + 1; // skip first "
            int len = lexer.pos() - string_begin; // this doesn't count last "
            string_data.resize(string_data.size() + len + 1);
            memcpy(&string_data[idx_begin], string_begin, len);
            string_data.back() = '\0';
            break;
        }
        case '\'':
        {
            while (!lexer.end() && lexer.advance() != '\'')
                ;

            if (lexer.end() && *lexer.pos() != '\'')
            {
                print(IC_PERROR, lexer.token_line, lexer.token_col, source_lines.data(), "unterminated character literal");
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
                print(IC_PERROR, lexer.token_line, lexer.token_col, source_lines.data(),
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
                    if (string_compare(string, { keyword.str, int(strlen(keyword.str)) }))
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
                print(IC_PERROR, lexer.line, lexer.col, source_lines.data(), "unexpected character");
                return false;
            }
        }
        } // switch
    } // while
    ic_token token;
    token.type = IC_TOK_EOF;
    token.line = lexer.line;
    token.col = lexer.col;
    lexer.tokens->push_back(token);
    return true;
}

#define IC_CMASK_MSB 7

bool try_produce_type(ic_parser& parser, ic_type& type)
{
    bool init = false;
    type.indirection_level = 0;
    type.const_mask = 0;

    if (parser.try_consume(IC_TOK_CONST))
    {
        type.const_mask = 1 << IC_CMASK_MSB;
        init = true;
    }

    switch (parser.get_token().type)
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
    {
        ic_string name = parser.get_token().string;

        if (parser.gscope->get_struct(name))
        {
            type.basic_type = IC_TYPE_STRUCT;
            type.struct_name = name;
            break;
        }
        // fall through, not a type
    }
    default:
        if (init)
            parser.exit("expected type name after const keyword");
        return false;
    }
    parser.advance();

    while (parser.try_consume(IC_TOK_STAR))
    {
        if (type.indirection_level == IC_CMASK_MSB)
            parser.exit("exceeded maximal level of indirection");

        type.indirection_level += 1;

        if (parser.try_consume(IC_TOK_CONST))
            type.const_mask += 1 << (IC_CMASK_MSB - type.indirection_level);
    }
    type.const_mask = type.const_mask >> (IC_CMASK_MSB - type.indirection_level);
    return true;
}

ic_type produce_type(ic_parser& parser)
{
    ic_type type;
    if (!try_produce_type(parser, type))
        parser.exit("expected type");
    return type;
}

void produce_parameter_list(ic_parser& parser, ic_function& function)
{
    function.param_count = 0;

    if(parser.try_consume(IC_TOK_RIGHT_PAREN))
        return;

    // this is important; because exceptions are not used all loops must terminate on EOF token
    // either by condition fail or break that is guaranteed to execute
    while(parser.get_token().type != IC_TOK_EOF)
    {
        if (function.param_count >= IC_MAX_ARGC)
            parser.exit("exceeded maxial number of parameters");

        ic_type param_type = produce_type(parser);

        if (is_void(param_type))
            parser.exit("parameter can't be of type void");

        function.params[function.param_count].type = param_type;
        const ic_token* id_token = parser.token_it;

        if(parser.try_consume(IC_TOK_IDENTIFIER))
            function.params[function.param_count].name = id_token->string;
        else
            function.params[function.param_count].name = { nullptr };

        function.param_count += 1;

        if (parser.try_consume(IC_TOK_RIGHT_PAREN))
            break;

        parser.consume(IC_TOK_COMMA, "expected ',' or ')' after a function argument");
    }
}

#define IC_MAX_MEMBERS 50

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

// production rules hierarchy
ic_stmt* produce_stmt(ic_parser& parser);
ic_stmt* produce_stmt_var_decl(ic_parser& parser);
ic_expr* produce_expr_stmt(ic_parser& parser);
ic_expr* produce_expr(ic_parser& parser);
ic_expr* produce_expr_assignment(ic_parser& parser);
ic_expr* produce_expr_binary(ic_parser& parser, ic_op_precedence precedence);
ic_expr* produce_expr_unary(ic_parser& parser);
ic_expr* produce_expr_subscript(ic_parser& parser);
ic_expr* produce_expr_primary(ic_parser& parser);

ic_decl produce_decl(ic_parser& parser)
{
    if (parser.try_consume(IC_TOK_STRUCT))
    {
        ic_decl decl;
        decl.type = IC_DECL_STRUCT;
        ic_struct& _struct = decl._struct;
        _struct.token = parser.get_token();
        parser.consume(IC_TOK_IDENTIFIER, "expected struct name");
        parser.consume(IC_TOK_LEFT_BRACE, "expected '{'");
        ic_param members[IC_MAX_MEMBERS]; // todo
        _struct.num_members = 0;
        _struct.num_data = 0;
        ic_type type;

        while (try_produce_type(parser, type))
        {
            assert(_struct.num_members < IC_MAX_MEMBERS); // todo

            // todo, support const members?
            if (type.const_mask & 1)
                parser.exit("struct member can't be const");

            if (is_void(type))
                parser.exit("struct member can't be of type void");

            if (is_struct(type))
            {
                ic_struct* sub_struct = parser.gscope->get_struct(type.struct_name);
                assert(sub_struct); //  todo, print nice error here
                _struct.num_data += sub_struct->num_data;
            }
            else
                _struct.num_data += 1;

            ic_param& member = members[_struct.num_members];
            member.type = type;
            member.name = parser.get_token().string;
            _struct.num_members += 1;
            parser.consume(IC_TOK_IDENTIFIER, "expected member name");
            parser.consume(IC_TOK_SEMICOLON, "expected ';' after struct member name");
        }

        int bytes = sizeof(ic_param) * _struct.num_members;
        _struct.members = (ic_param*)malloc(bytes); // todo TODO
        memcpy(_struct.members, members, bytes);
        parser.consume(IC_TOK_RIGHT_BRACE, "expected '}'");
        parser.consume(IC_TOK_SEMICOLON, "expected ';'");
        return decl;
    }

    // else produce function or variable declaration
    ic_type type;

    if (!try_produce_type(parser, type)) // try_produce_type() is used to print nondefault error message
        parser.exit("expected: a type of a global variable / return type of a function / 'struct' keyword");

    ic_token token_id = parser.get_token();
    parser.consume(IC_TOK_IDENTIFIER, "expected identifier");

    if (parser.try_consume(IC_TOK_LEFT_PAREN))
    {
        ic_decl decl;
        decl.type = IC_DECL_FUNCTION;
        ic_function& function = decl.function;
        function.type = IC_FUN_SOURCE;
        function.token = token_id;
        function.return_type = type;
        produce_parameter_list(parser, function);
        function.body = produce_stmt(parser);

        if (function.body->type != IC_STMT_COMPOUND)
            parser.exit("expected compound stmt after function parameter list");

        function.body->compound.push_scope = false; // do not allow shadowing of arguments
        return decl;
    }

    // variable
    if (is_void(type))
        parser.exit("variables can't be of type void");

    ic_decl decl;
    decl.type = IC_DECL_VAR;
    decl.var.type = type;
    decl.var.token = token_id;

    if (parser.get_token().type == IC_TOK_EQUAL)
        parser.exit("global variables can't be initialized by an expression, they are set to 0");

    parser.consume(IC_TOK_SEMICOLON, "expected ';'");
    return decl;
}

ic_stmt* produce_stmt(ic_parser& parser)
{
    switch (parser.get_token().type)
    {
    case IC_TOK_LEFT_BRACE:
    {
        ic_stmt* stmt = parser.allocate_stmt(IC_STMT_COMPOUND, parser.get_token());
        parser.advance();
        stmt->compound.push_scope = true;
        ic_stmt** body_tail  = &(stmt->compound.body);

        while (parser.get_token().type != IC_TOK_RIGHT_BRACE && parser.get_token().type != IC_TOK_EOF)
        {
            *body_tail = produce_stmt(parser);
            body_tail = &((*body_tail)->next);
        }
        parser.consume(IC_TOK_RIGHT_BRACE, "expected '}' to close a compound statement");
        return stmt;
    }
    case IC_TOK_WHILE:
    {
        ic_stmt* stmt = parser.allocate_stmt(IC_STMT_FOR, parser.get_token());
        parser.advance();
        parser.consume(IC_TOK_LEFT_PAREN, "expected '(' after while keyword");
        stmt->_for.header2 = produce_expr(parser);
        parser.consume(IC_TOK_RIGHT_PAREN, "expected ')' after while condition");
        stmt->_for.body = produce_stmt(parser);

        // be consistent with IC_TOK_FOR
        if (stmt->_for.body->type == IC_STMT_COMPOUND)
            stmt->_for.body->compound.push_scope = false;
        return stmt;
    }
    case IC_TOK_FOR:
    {
        ic_stmt* stmt = parser.allocate_stmt(IC_STMT_FOR, parser.get_token());
        parser.advance();
        parser.consume(IC_TOK_LEFT_PAREN, "expected '(' after for keyword");
        stmt->_for.header1 = produce_stmt_var_decl(parser); // only in header1 var declaration is allowed
        stmt->_for.header2 = produce_expr_stmt(parser);

        if (parser.get_token().type != IC_TOK_RIGHT_PAREN)
            stmt->_for.header3 = produce_expr(parser); // this one should not end with ';', that's why we don't use produce_stmt_expr()

        parser.consume(IC_TOK_RIGHT_PAREN, "expected ')' after for header");
        stmt->_for.body = produce_stmt(parser);

        // prevent shadowing of header variable
        if (stmt->_for.body->type == IC_STMT_COMPOUND)
            stmt->_for.body->compound.push_scope = false;
        return stmt;
    }
    case IC_TOK_IF:
    {
        ic_stmt* stmt = parser.allocate_stmt(IC_STMT_IF, parser.get_token());
        parser.advance();
        parser.consume(IC_TOK_LEFT_PAREN, "expected '(' after if keyword");
        stmt->_if.header = produce_expr(parser);

        if (stmt->_if.header->token.type == IC_TOK_EQUAL)
            parser.exit("assignment expression can't be used directly in if header, encolse it with ()");

        parser.consume(IC_TOK_RIGHT_PAREN, "expected ')' after if condition");
        stmt->_if.body_if = produce_stmt(parser);

        if (parser.try_consume(IC_TOK_ELSE))
            stmt->_if.body_else = produce_stmt(parser);
        return stmt;
    }
    case IC_TOK_RETURN:
    {
        ic_stmt* stmt = parser.allocate_stmt(IC_STMT_RETURN, parser.get_token());
        parser.advance();
        stmt->_return.expr = produce_expr_stmt(parser);
        return stmt;
    }
    case IC_TOK_BREAK:
    {
        ic_stmt* stmt = parser.allocate_stmt(IC_STMT_BREAK, parser.get_token());
        parser.advance();
        parser.consume(IC_TOK_SEMICOLON, "expected ';' after break keyword");
        return stmt;
    }
    case IC_TOK_CONTINUE:
    {
        ic_stmt* stmt = parser.allocate_stmt(IC_STMT_CONTINUE, parser.get_token());
        parser.advance();
        parser.consume(IC_TOK_SEMICOLON, "expected ';' after continue keyword");
        return stmt;
    }
    } // switch
    return produce_stmt_var_decl(parser);
}

// this function is seperate from produce_expr_assignment so we don't allow(): var x = var y = 5;
// and: while(var x = 6);
ic_stmt* produce_stmt_var_decl(ic_parser& parser)
{
    ic_type type;
    ic_token init_token = parser.get_token();

    if (try_produce_type(parser, type))
    {
        if (is_void(type))
            parser.exit("variables can't be of type void");

        ic_stmt* stmt = parser.allocate_stmt(IC_STMT_VAR_DECL, init_token);
        stmt->var_decl.type = type;
        stmt->var_decl.token = parser.get_token();
        parser.consume(IC_TOK_IDENTIFIER, "expected variable name");

        if (parser.try_consume(IC_TOK_EQUAL))
        {
            stmt->var_decl.expr = produce_expr_stmt(parser);
            return stmt;
        }
        parser.consume(IC_TOK_SEMICOLON, "expected ';' or '=' after variable name");
        return stmt;
    }
    else
    {
        ic_stmt* stmt = parser.allocate_stmt(IC_STMT_EXPR, init_token);
        stmt->expr = produce_expr_stmt(parser);
        return stmt;
    }
}

ic_expr* produce_expr_stmt(ic_parser& parser)
{
    if (parser.try_consume(IC_TOK_SEMICOLON))
        return nullptr;
    ic_expr* expr = produce_expr(parser);
    parser.consume(IC_TOK_SEMICOLON, "expected ';' after expression");
    return expr;
}

ic_expr* produce_expr(ic_parser& parser)
{
    return produce_expr_assignment(parser);
}

// produce_expr_assignment() - grows down and right
// produce_expr_binary() - grows up and right (given operators with the same precedence)

ic_expr* produce_expr_assignment(ic_parser& parser)
{
    ic_expr* expr_lhs = produce_expr_binary(parser, IC_PRECEDENCE_LOGICAL_OR);

    switch (parser.get_token().type)
    {
    case IC_TOK_EQUAL:
    case IC_TOK_PLUS_EQUAL:
    case IC_TOK_MINUS_EQUAL:
    case IC_TOK_STAR_EQUAL:
    case IC_TOK_SLASH_EQUAL:
        ic_expr* expr = parser.allocate_expr(IC_EXPR_BINARY, parser.get_token());
        parser.advance();
        expr->binary.rhs = produce_expr(parser);
        expr->binary.lhs = expr_lhs;
        return expr;
    }
    return expr_lhs;
}

ic_expr* produce_expr_binary(ic_parser& parser, ic_op_precedence precedence)
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
        return produce_expr_unary(parser);
    }

    ic_expr* expr = produce_expr_binary(parser, ic_op_precedence(int(precedence) + 1));

    for (;;)
    {
        const ic_token_type* target_token_type  = target_token_types;
        bool match = false;

        while (*target_token_type != IC_TOK_EOF)
        {
            if (*target_token_type == parser.get_token().type)
            {
                match = true;
                break;
            }
            ++target_token_type;
        }

        if (!match)
            break; // important, avoid infinite loop

        // operator matches given precedence
        ic_expr* expr_parent = parser.allocate_expr(IC_EXPR_BINARY, parser.get_token());
        parser.advance();
        expr_parent->binary.rhs = produce_expr_binary(parser, ic_op_precedence(int(precedence) + 1));
        expr_parent->binary.lhs = expr;
        expr = expr_parent;
    }
    return expr;
}

ic_expr* produce_expr_unary(ic_parser& parser)
{
    switch (parser.get_token().type)
    {
    case IC_TOK_BANG:
    case IC_TOK_MINUS:
    case IC_TOK_PLUS_PLUS:
    case IC_TOK_MINUS_MINUS:
    case IC_TOK_AMPERSAND:
    case IC_TOK_STAR:
    {
        ic_expr* expr = parser.allocate_expr(IC_EXPR_UNARY, parser.get_token());
        parser.advance();
        expr->unary.expr = produce_expr_unary(parser);
        return expr;
    }
    case IC_TOK_LEFT_PAREN:
    {
        parser.advance();
        ic_type type;

        if (try_produce_type(parser, type))
        {
            parser.consume(IC_TOK_RIGHT_PAREN, "expected ) at the end of a cast operator");
            ic_expr* expr = parser.allocate_expr(IC_EXPR_CAST_OPERATOR, parser.get_token());
            expr->cast_operator.type = type;
            expr->cast_operator.expr = produce_expr_unary(parser);
            return expr;
        }
        parser.token_it -= 1; // go back by one, parentheses expression also starts with '('
        break;
    }
    case IC_TOK_SIZEOF:
    {
        ic_expr* expr = parser.allocate_expr(IC_EXPR_SIZEOF, parser.get_token());
        parser.advance();
        parser.consume(IC_TOK_LEFT_PAREN, "expected '(' after sizeof operator");
        expr->_sizeof.type = produce_type(parser);
        parser.consume(IC_TOK_RIGHT_PAREN, "expected ')' after type");
        return expr;
    }
    } // switch
    return produce_expr_subscript(parser);
}

ic_expr* produce_expr_subscript(ic_parser& parser)
{
    ic_expr* lhs = produce_expr_primary(parser);

    for (;;)
    {
        if (parser.get_token().type == IC_TOK_LEFT_BRACKET)
        {
            ic_expr* expr = parser.allocate_expr(IC_EXPR_SUBSCRIPT, parser.get_token());
            parser.advance();
            expr->subscript.lhs = lhs;
            expr->subscript.rhs = produce_expr(parser);
            parser.consume(IC_TOK_RIGHT_BRACKET, "expected a closing bracket for a subscript operator");
            lhs = expr;
        }
        else if (parser.get_token().type == IC_TOK_DOT || parser.get_token().type == IC_TOK_ARROW)
        {
            ic_expr* expr = parser.allocate_expr(IC_EXPR_MEMBER_ACCESS, parser.get_token());
            parser.advance();
            expr->member_access.lhs = lhs;
            expr->member_access.rhs_token = parser.get_token();
            parser.consume(IC_TOK_IDENTIFIER, "expected member name after '.' operator");
            lhs = expr;
        }
        else
            break; // important, avoid infinite loop
    }
    return lhs;
}

ic_expr* produce_expr_primary(ic_parser& parser)
{
    switch (parser.get_token().type)
    {
    case IC_TOK_INT_NUMBER_LITERAL:
    case IC_TOK_FLOAT_NUMBER_LITERAL:
    case IC_TOK_STRING_LITERAL:
    case IC_TOK_CHARACTER_LITERAL:
    case IC_TOK_TRUE:
    case IC_TOK_FALSE:
    case IC_TOK_NULLPTR:
    {
        ic_expr* expr = parser.allocate_expr(IC_EXPR_PRIMARY, parser.get_token());
        parser.advance();
        return expr;
    }
    case IC_TOK_IDENTIFIER:
    {
        ic_token token_id = parser.get_token();
        parser.advance();

        if (parser.try_consume(IC_TOK_LEFT_PAREN))
        {
            ic_expr* expr = parser.allocate_expr(IC_EXPR_FUNCTION_CALL, token_id);

            if (parser.try_consume(IC_TOK_RIGHT_PAREN))
                return expr;

            ic_expr** arg_tail = &expr->function_call.arg;
            int argc = 0;

            while(parser.get_token().type != IC_TOK_EOF) // important, avoid infinite loop
            {
                *arg_tail = produce_expr(parser);
                arg_tail = &((*arg_tail)->next);
                ++argc;

                if(argc > IC_MAX_ARGC)
                    parser.exit("exceeded maximal number of arguments");

                if (parser.try_consume(IC_TOK_RIGHT_PAREN))
                    break;

                parser.consume(IC_TOK_COMMA, "expected ',' or ')' after a function argument");
            }
            return expr;
        }
        return parser.allocate_expr(IC_EXPR_PRIMARY, token_id);
    }
    case IC_TOK_LEFT_PAREN:
    {
        ic_expr* expr = parser.allocate_expr(IC_EXPR_PARENTHESES, parser.get_token());
        parser.advance();
        expr->parentheses.expr = produce_expr(parser);
        parser.consume(IC_TOK_RIGHT_PAREN, "expected ')' after expression");
        return expr;
    }
    } // switch
    parser.exit("expected literal / indentifier / parentheses / function call");
    return parser.allocate_expr({}, {}); // don't return nullptr, may be dereferenced
}
