#include "ic_impl.h"

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

void ic_program_free(ic_program& program)
{
    free(program.data);
    free(program.functions);
}

void ic_buf_free(unsigned char* buf)
{
    free(buf);
}

ic_type non_pointer_type(ic_basic_type type)
{
    return { type, 0, 0 };
}

ic_type const_pointer1_type(ic_basic_type type)
{
    return { type, 1, 1 << 2 };
}

ic_type pointer1_type(ic_basic_type type)
{
    return { type, 1, 0 };
}

bool is_struct(ic_type type)
{
    return !type.indirection_level && type.basic_type == IC_TYPE_STRUCT;
}

bool is_void(ic_type type)
{
    return !type.indirection_level && type.basic_type == IC_TYPE_VOID;
}

void ic_print_error(int line, int col, const char* fmt, ...)
{
    printf("error; line: %d; col: %d; ", line, col);
    va_list list;
    va_start(list, fmt);
    vprintf(fmt, list);
    va_end(list);
    printf("\n");
}

ic_data ic_host_prints(ic_data* argv, void*)
{
    printf("prints: %s\n", (const char*)argv->pointer);
    return {};
}

ic_data ic_host_printf(ic_data* argv, void*)
{
    printf("printf: %f\n", argv->f64);
    return {};
}

ic_data ic_host_printp(ic_data* argv, void*)
{
    printf("printp: %p\n", argv->pointer);
    return {};
}

ic_data ic_host_malloc(ic_data* argv, void*)
{
    void* ptr = malloc(argv->s32);
    ic_data data;
    data.pointer = ptr;
    return data;
}

ic_data ic_host_tan(ic_data* argv, void*)
{
    ic_data data;
    data.f64 = tan(argv->f64);
    return data;
}

ic_data ic_host_sqrt(ic_data* argv, void*)
{
    ic_data data;
    data.f64 = sqrt(argv->f64);
    return data;
}

ic_data ic_host_pow(ic_data* argv, void*)
{
    ic_data data;
    data.f64 = pow(argv[0].f64, argv[1].f64);
    return data;
}

ic_data ic_host_exit(ic_data*, void*)
{
    exit(0);
}

bool ic_string_compare(ic_string str1, ic_string str2)
{
    if (str1.len != str2.len)
        return false;

    return (strncmp(str1.data, str2.data, str1.len) == 0);
}

static ic_host_function core_lib[] =
{
    {"void prints(const s8*)", ic_host_prints},
    {"void printf(f64)", ic_host_printf},
    {"void printp(const void*)", ic_host_printp},
    {"void* malloc(s32)", ic_host_malloc},
    {"f64 tan(f64)", ic_host_tan},
    {"f64 sqrt(f64)", ic_host_sqrt},
    {"f64 pow(f64, f64)", ic_host_pow},
    {"void exit()", ic_host_exit},
    nullptr
};

 unsigned int hash_string (const char* str)
{
    unsigned int hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c;

    return hash;
}

 void resolve_function(ic_vm_function& vmfun, ic_host_function* it)
 {
     assert(it);
     while (it->prototype_str)
     {
         if (vmfun.hash == hash_string(it->prototype_str))
         {
             vmfun.callback = it->callback;
             vmfun.host_data = it->host_data;
             return;
         }
         ++it;
     }
     assert(false);
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
        ic_vm_function& vmfun = program.functions[i];
        read_bytes(&vmfun, &buf_it, sizeof(ic_vm_function));

        if (!vmfun.host_impl)
            continue;

        if (vmfun.lib == IC_LIB_CORE)
            resolve_function(vmfun, core_lib);
        else if (vmfun.lib == IC_USER_FUNCTION)
            resolve_function(vmfun, host_functions);
        else
            assert(false);
    }
}

bool ic_tokenize(const char* source, std::vector<ic_token>& _tokens, std::vector<unsigned char>& _string_data);
// create new function produce_host_decl, and don't use bool flag
ic_global produce_global(const ic_token** it, ic_parser& parser, bool host_declaration);

 // todo, better name
// + it would be better if it run over array of functions
ic_function produce_function_from_host(ic_host_function& host_function, ic_parser& parser)
{
    std::vector<ic_token> tokens;
    std::vector<unsigned char> string_data;
    bool success = ic_tokenize(host_function.prototype_str, tokens, string_data);
    assert(success);
    assert(string_data.size() == 0);

    const ic_token* tok_ptr = tokens.data();
    ic_global global = produce_global(&tok_ptr, parser, true);
    assert(global.type == IC_GLOBAL_FUNCTION);
    global.function.callback = host_function.callback;
    global.function.hash = hash_string(host_function.prototype_str);
    global.function.host_data = host_function.host_data;
    return global.function;
}

// todo cleanup on early return
bool ic_program_init_compile(ic_program& program, const char* source, int libs, ic_host_function* host_functions)
{
    assert(source);

    std::vector<ic_token> tokens;
    std::vector<unsigned char> program_data;

    if (!ic_tokenize(source, tokens, program_data))
        return false;

    program.strings_byte_size = program_data.size();
    program.global_data_size = program_data.size() / sizeof(ic_data) + 1;
    ic_parser parser;
    parser.init();

    if (libs & IC_LIB_CORE)
    {
        ic_host_function* it = core_lib;
        while (it->prototype_str)
        {
            ic_function fun = produce_function_from_host(*it, parser);
            fun.lib = IC_LIB_CORE;
            parser.functions.push_back(fun);
            ++it;
        }
    }
    if (host_functions)
    {
        ic_host_function* it = host_functions;
        while (it->prototype_str)
        {
            ic_function fun = produce_function_from_host(*it, parser);
            fun.lib = IC_USER_FUNCTION;
            parser.functions.push_back(fun);
            ++it;
        }
    }

    const ic_token* token_it = tokens.data();

    while (token_it->type != IC_TOK_EOF)
    {
        ic_global global;

        try
        {
            global = produce_global(&token_it, parser, false);
        }
        catch (ic_exception_parsing)
        {
            return false;
        }

        switch (global.type)
        {
        case IC_GLOBAL_FUNCTION:
        {
            ic_token token = global.function.token;
            ic_function* prev_function = parser.get_function(token.string);

            if (prev_function)
            {
                if(prev_function->type == IC_FUN_HOST)
                    ic_print_error(token.line, token.col, "function (provided by host) with such name already exists");
                else
                    ic_print_error(token.line, token.col, "function with such name already exists");

                return false;
            }
            parser.functions.push_back(global.function);
            break;
        }
        case IC_GLOBAL_STRUCT:
        {
            ic_token token = global._struct.token;

            if (parser.get_struct(token.string))
            {
                ic_print_error(token.line, token.col, "struct with such name already exists");
                return false;
            }
            parser.structs.push_back(global._struct);
            break;
        }
        case IC_GLOBAL_VAR_DECLARATION:
        {
            ic_token token = global.var.token;

            if (parser.get_global_var(token.string))
            {
                ic_print_error(token.line, token.col, "global variable with such name already exists");
                return false;
            }
            ic_var var;
            var.idx = program.global_data_size * sizeof(ic_data);
            var.type = global.var.type;
            var.name = token.string;

            if (is_struct(var.type))
            {
                ic_struct* _struct = parser.get_struct(var.type.struct_name);
                assert(_struct);
                program.global_data_size += _struct->num_data;
            }
            else
                program.global_data_size += 1;

            parser.global_vars.push_back(var);
            break;
        }
        default:
            assert(false);
        }
    }

    std::vector<ic_function*> active_functions;

    for (ic_function& function : parser.functions)
    {
        if (function.type == IC_FUN_SOURCE)
        {
            if (ic_string_compare(function.token.string, { "main", 4 }))
            {
                assert(function.param_count == 0);
                assert(is_void(function.return_type));
                active_functions.push_back(&function);
            }
            function.data_idx = -1;
        }
    }

    assert(active_functions.size());
    // important, active_functions.size() changes during loop execution
    for (int i = 0; i < active_functions.size(); ++i)
    {
        if (active_functions[i]->type == IC_FUN_SOURCE)
            compile_function(*active_functions[i], &parser, &active_functions, &program_data);
    }

    program.data_size = program_data.size();
    program.data = (unsigned char*)malloc(program.data_size);
    memcpy(program.data, program_data.data(), program.data_size);

    // compile inactive function for code corectness, these will not be included in returned program
    for (ic_function& function : parser.functions)
    {
        if (function.type == IC_FUN_HOST || function.data_idx != -1)
            continue;
        printf("warning: function '%.*s' not used but compiled\n", function.token.string.len, function.token.string.data);
        compile_function(function, &parser, nullptr, nullptr);
    }

    std::vector<ic_vm_function> vm_functions;

    for (ic_function* _ptr : active_functions)
    {
        ic_function& function = *_ptr;
        ic_vm_function vmfun;
        vmfun.param_size = 0;

        for (int j = 0; j < function.param_count; ++j)
        {
            if (is_struct(function.params[j].type)) // use function for this
            {
                ic_struct* _struct = parser.get_struct(function.params[j].type.struct_name);
                assert(_struct);
                vmfun.param_size += _struct->num_data;
            }
            else
                vmfun.param_size += 1;
        }

        if (function.type == IC_FUN_HOST)
        {
            vmfun.host_impl = true;
            vmfun.callback = function.callback;
            vmfun.host_data = function.host_data;
            vmfun.hash = function.hash;
            vmfun.lib = function.lib;
            vmfun.returns_value = is_void(function.return_type) ? 0 : 1;
        }
        else
        {
            vmfun.host_impl = false;
            vmfun.data_idx = function.data_idx;
            vmfun.stack_size = function.stack_size;
        }

        vm_functions.push_back(vmfun);
    }
    program.functions = (ic_vm_function*)malloc(vm_functions.size() * sizeof(ic_vm_function));
    memcpy(program.functions, vm_functions.data(), vm_functions.size() * sizeof(ic_vm_function));
    program.functions_size = vm_functions.size();
    return true;
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

bool ic_tokenize(const char* source, std::vector<ic_token>& _tokens, std::vector<unsigned char>& _string_data)
{
    ic_lexer lexer{ _tokens };
    lexer._source_it = source;

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
                ic_print_error(lexer._line, lexer._col, "unexpected character '%c'", c);
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
                ic_print_error(lexer._token_line, lexer._token_col, "unterminated string literal");
                return false;
            }

            // todo, if the same string literal already exists, reuse it
            int idx_begin = _string_data.size();
            lexer.add_token_number(IC_TOK_STRING_LITERAL, idx_begin);
            const char* string_begin = token_begin + 1; // skip first "
            int len = lexer.pos() - string_begin; // this doesn't count last "
            _string_data.resize(_string_data.size() + len + 1);
            memcpy(&_string_data[idx_begin], string_begin, len);
            _string_data.back() = '\0';
            break;
        }
        case '\'':
        {
            while (!lexer.end() && lexer.advance() != '\'')
                ;

            if (lexer.end() && *lexer.pos() != '\'')
            {
                ic_print_error(lexer._token_line, lexer._token_col, "unterminated character literal");
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
                ic_print_error(lexer._token_line, lexer._token_col,
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
                ic_print_error(lexer._line, lexer._col, "unexpected character '%c'", c);
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

ic_global produce_global(const ic_token** it, ic_parser& parser, bool host_declaration);
ic_stmt* produce_stmt(const ic_token** it, ic_parser& parser);
ic_stmt* produce_stmt_var_declaration(const ic_token** it, ic_parser& parser);
ic_expr* produce_expr_stmt(const ic_token** it, ic_parser& parser);
ic_expr* produce_expr(const ic_token** it, ic_parser& parser);
ic_expr* produce_expr_assignment(const ic_token** it, ic_parser& parser);
ic_expr* produce_expr_binary(const ic_token** it, ic_op_precedence precedence, ic_parser& parser);
ic_expr* produce_expr_unary(const ic_token** it, ic_parser& parser);
ic_expr* produce_expr_subscript(const ic_token** it, ic_parser& parser);
ic_expr* produce_expr_primary(const ic_token** it, ic_parser& parser);

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
        ic_print_error((**it).line, (**it).col, err_msg);
        throw ic_exception_parsing{};
    }

    return false;
}

void exit_parsing(const ic_token** it, const char* err_msg, ...)
{
    va_list list;
    va_start(list, err_msg);
    ic_print_error((**it).line, (**it).col, err_msg, list);
    va_end(list);
    throw ic_exception_parsing{};
}

bool produce_type(const ic_token** it, ic_type& type, ic_parser& parser)
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
        if ( parser.get_struct((**it).string))
        {
            type.basic_type = IC_TYPE_STRUCT;
            type.struct_name = (**it).string;
            break;
        }
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

#define IC_MAX_MEMBERS 50

ic_global produce_global(const ic_token** it, ic_parser& parser, bool host_declaration)
{
    if (token_consume(it, IC_TOK_STRUCT))
    {
        ic_global global;
        global.type = IC_GLOBAL_STRUCT;
        ic_struct& _struct = global._struct;
        _struct.token = **it;
        token_consume(it, IC_TOK_IDENTIFIER, "expected struct name");
        token_consume(it, IC_TOK_LEFT_BRACE, "expected '{'");
        ic_param members[IC_MAX_MEMBERS]; // todo; this is only temp solution
        _struct.num_members = 0;
        _struct.num_data = 0;
        ic_type type;

        while (produce_type(it, type, parser))
        {
            assert(_struct.num_members < IC_MAX_MEMBERS);

            // todo; support const members
            if (type.const_mask & 1)
                exit_parsing(it, "struct member can't be const");

            if (is_void(type))
                exit_parsing(it, "struct member can't be of type void");

            if (is_struct(type))
            {
                ic_struct* sub_struct = parser.get_struct(type.struct_name);
                assert(sub_struct); //  todo, print nice error here
                _struct.num_data += sub_struct->num_data;
            }
            else
                _struct.num_data += 1;

            ic_param& member = members[_struct.num_members];
            member.type = type;
            member.name = (**it).string;
            _struct.num_members += 1;
            token_consume(it, IC_TOK_IDENTIFIER, "expected member name");
            token_consume(it, IC_TOK_SEMICOLON, "expected ';' after struct member name");
        }

        int bytes = sizeof(ic_param) * _struct.num_members;
        _struct.members = (ic_param*)malloc(bytes);
        memcpy(_struct.members, members, bytes);
        token_consume(it, IC_TOK_RIGHT_BRACE, "expected '}'");
        token_consume(it, IC_TOK_SEMICOLON, "expected ';'");
        return global;
    }

    // else produce a function or a global variable declaration
    ic_type type;

    if (!produce_type(it, type, parser))
        exit_parsing(it, "expected: a type of a global variable / return type of a function / 'struct' keyword");

    ic_token token_id = **it;
    token_consume(it, IC_TOK_IDENTIFIER, "expected identifier");

    // function
    if (token_consume(it, IC_TOK_LEFT_PAREN))
    {
        ic_global global;
        global.type = IC_GLOBAL_FUNCTION;
        ic_function& function = global.function;
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

                if (!produce_type(it, param_type, parser))
                    exit_parsing(it, "expected parameter type");

                if (is_void(param_type))
                    exit_parsing(it, "parameter can't be of type void");

                function.params[function.param_count].type = param_type;
                const ic_token* id_token = *it;

                if(token_consume(it, IC_TOK_IDENTIFIER))
                    function.params[function.param_count].name = id_token->string;
                else
                    function.params[function.param_count].name = { nullptr };


                function.param_count += 1;

                if (token_consume(it, IC_TOK_RIGHT_PAREN))
                    break;

                token_consume(it, IC_TOK_COMMA, "expected ',' or ')' after a function argument");
            }
        }

        if (host_declaration)
        {
            function.type = IC_FUN_HOST;
            assert((**it).type == IC_TOK_EOF);
            return global;
        }

        function.type = IC_FUN_SOURCE;
        function.body = produce_stmt(it, parser);

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

ic_stmt* produce_stmt(const ic_token** it, ic_parser& parser)
{
    switch ((**it).type)
    {
    case IC_TOK_LEFT_BRACE:
    {
        token_advance(it);
        ic_stmt* stmt = parser.allocate_stmt(IC_STMT_COMPOUND);
        stmt->_compound.push_scope = true;
        ic_stmt** body_tail  = &(stmt->_compound.body);

        while ((**it).type != IC_TOK_RIGHT_BRACE && (**it).type != IC_TOK_EOF)
        {
            *body_tail = produce_stmt(it, parser);
            body_tail = &((*body_tail)->next);
        }

        token_consume(it, IC_TOK_RIGHT_BRACE, "expected '}' to close a compound statement");
        return stmt;
    }
    case IC_TOK_WHILE:
    {
        token_advance(it);
        ic_stmt* stmt = parser.allocate_stmt(IC_STMT_FOR);
        token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after while keyword");
        stmt->_for.header2 = produce_expr(it, parser);
        token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after while condition");
        stmt->_for.body = produce_stmt(it, parser);

        // be consistent with IC_TOK_FOR
        if (stmt->_for.body->type == IC_STMT_COMPOUND)
            stmt->_for.body->_compound.push_scope = false;

        return stmt;
    }
    case IC_TOK_FOR:
    {
        token_advance(it);
        ic_stmt* stmt = parser.allocate_stmt(IC_STMT_FOR);
        token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after for keyword");
        stmt->_for.header1 = produce_stmt_var_declaration(it, parser); // only in header1 var declaration is allowed
        stmt->_for.header2 = produce_expr_stmt(it, parser);

        if ((**it).type != IC_TOK_RIGHT_PAREN)
            stmt->_for.header3 = produce_expr(it, parser); // this one should not end with ';', that's why we don't use produce_stmt_expr()

        token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after for header");
        stmt->_for.body = produce_stmt(it, parser);

        // prevent shadowing of header variable
        if (stmt->_for.body->type == IC_STMT_COMPOUND)
            stmt->_for.body->_compound.push_scope = false;

        return stmt;
    }
    case IC_TOK_IF:
    {
        token_advance(it);
        ic_stmt* stmt = parser.allocate_stmt(IC_STMT_IF);
        token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after if keyword");
        stmt->_if.header = produce_expr(it, parser);

        if (stmt->_if.header->token.type == IC_TOK_EQUAL)
            exit_parsing(it, "assignment expression can't be used directly in if header, encolse it with ()");

        token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after if condition");
        stmt->_if.body_if = produce_stmt(it, parser);

        if (token_consume(it, IC_TOK_ELSE))
            stmt->_if.body_else = produce_stmt(it, parser);

        return stmt;
    }
    case IC_TOK_RETURN:
    {
        token_advance(it);
        ic_stmt* stmt = parser.allocate_stmt(IC_STMT_RETURN);
        stmt->_return.expr = produce_expr_stmt(it, parser);
        return stmt;
    }
    case IC_TOK_BREAK:
    {
        token_advance(it);
        token_consume(it, IC_TOK_SEMICOLON, "expected ';' after break keyword");
        return parser.allocate_stmt(IC_STMT_BREAK);
    }
    case IC_TOK_CONTINUE:
    {
        token_advance(it);
        token_consume(it, IC_TOK_SEMICOLON, "expected ';' after continue keyword");
        return parser.allocate_stmt(IC_STMT_CONTINUE);
    }
    } // switch

    return produce_stmt_var_declaration(it, parser);
}

// this function is seperate from produce_expr_assignment so we don't allow(): var x = var y = 5;
// and: while(var x = 6);
ic_stmt* produce_stmt_var_declaration(const ic_token** it, ic_parser& parser)
{
    ic_type type;

    if (produce_type(it, type, parser))
    {
        if (is_void(type))
            exit_parsing(it, "variables can't be of type void");

        ic_stmt* stmt = parser.allocate_stmt(IC_STMT_VAR_DECLARATION);
        stmt->_var_declaration.type = type;
        stmt->_var_declaration.token = **it;
        token_consume(it, IC_TOK_IDENTIFIER, "expected variable name");

        if (token_consume(it, IC_TOK_EQUAL))
        {
            stmt->_var_declaration.expr = produce_expr_stmt(it, parser);
            return stmt;
        }

        token_consume(it, IC_TOK_SEMICOLON, "expected ';' or '=' after variable name");
        return stmt;
    }
    else
    {
        ic_stmt* stmt = parser.allocate_stmt(IC_STMT_EXPR);
        stmt->_expr = produce_expr_stmt(it, parser);
        return stmt;
    }
}

ic_expr* produce_expr_stmt(const ic_token** it, ic_parser& parser)
{
    if (token_consume(it, IC_TOK_SEMICOLON))
        return nullptr;

    ic_expr* expr = produce_expr(it, parser);
    token_consume(it, IC_TOK_SEMICOLON, "expected ';' after expression");
    return expr;
}

ic_expr* produce_expr(const ic_token** it, ic_parser& parser)
{
    return produce_expr_assignment(it, parser);
}

// produce_expr_assignment() - grows down and right
// produce_expr_binary() - grows up and right (given operators with the same precedence)

ic_expr* produce_expr_assignment(const ic_token** it, ic_parser& parser)
{
    ic_expr* expr_lhs = produce_expr_binary(it, IC_PRECEDENCE_LOGICAL_OR, parser);

    switch ((**it).type)
    {
    case IC_TOK_EQUAL:
    case IC_TOK_PLUS_EQUAL:
    case IC_TOK_MINUS_EQUAL:
    case IC_TOK_STAR_EQUAL:
    case IC_TOK_SLASH_EQUAL:
        ic_expr* expr = parser.allocate_expr(IC_EXPR_BINARY, **it);
        token_advance(it);
        expr->_binary.rhs = produce_expr(it, parser);
        expr->_binary.lhs = expr_lhs;
        return expr;
    }

    return expr_lhs;
}

ic_expr* produce_expr_binary(const ic_token** it, ic_op_precedence precedence, ic_parser& parser)
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
        return produce_expr_unary(it, parser);
    }

    ic_expr* expr = produce_expr_binary(it, ic_op_precedence(int(precedence) + 1), parser);

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
        ic_expr* expr_parent = parser.allocate_expr(IC_EXPR_BINARY, **it);
        token_advance(it);
        expr_parent->_binary.rhs = produce_expr_binary(it, ic_op_precedence(int(precedence) + 1), parser);
        expr_parent->_binary.lhs = expr;
        expr = expr_parent;
    }

    return expr;
}

ic_expr* produce_expr_unary(const ic_token** it, ic_parser& parser)
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
        ic_expr* expr = parser.allocate_expr(IC_EXPR_UNARY, **it);
        token_advance(it);
        expr->_unary.expr = produce_expr_unary(it, parser);
        return expr;
    }
    case IC_TOK_LEFT_PAREN:
    {
        token_advance(it);
        ic_type type;

        if (produce_type(it, type, parser))
        {
            token_consume(it, IC_TOK_RIGHT_PAREN, "expected ) at the end of a cast operator");
            ic_expr* expr = parser.allocate_expr(IC_EXPR_CAST_OPERATOR, **it);
            expr->_cast_operator.type = type;
            expr->_cast_operator.expr = produce_expr_unary(it, parser);
            return expr;
        }

        *it -= 1; // go back by one
        break;
    }
    case IC_TOK_SIZEOF:
    {
        ic_expr* expr = parser.allocate_expr(IC_EXPR_SIZEOF, **it);
        token_advance(it);
        token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after sizeof operator");
        
        if (!produce_type(it, expr->_sizeof.type, parser))
            exit_parsing(it, "expected type");

        token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after type");
        return expr;
    }
    } // switch

    return produce_expr_subscript(it, parser);
}

ic_expr* produce_expr_subscript(const ic_token** it, ic_parser& parser)
{
    ic_expr* lhs = produce_expr_primary(it, parser);

    for (;;)
    {
        if ((**it).type == IC_TOK_LEFT_BRACKET)
        {
            ic_expr* expr = parser.allocate_expr(IC_EXPR_SUBSCRIPT, **it);
            token_advance(it);
            expr->_subscript.lhs = lhs;
            expr->_subscript.rhs = produce_expr(it, parser);
            token_consume(it, IC_TOK_RIGHT_BRACKET, "expected a closing bracket for a subscript operator");
            lhs = expr;
        }
        else if ((**it).type == IC_TOK_DOT || (**it).type == IC_TOK_ARROW)
        {
            ic_expr* expr = parser.allocate_expr(IC_EXPR_MEMBER_ACCESS, **it);
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

ic_expr* produce_expr_primary(const ic_token** it, ic_parser& parser)
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
        ic_expr* expr = parser.allocate_expr(IC_EXPR_PRIMARY, **it);
        token_advance(it);
        return expr;
    }

    case IC_TOK_IDENTIFIER:
    {
        ic_token token_id = **it;
        token_advance(it);

        if (token_consume(it, IC_TOK_LEFT_PAREN))
        {
            ic_expr* expr = parser.allocate_expr(IC_EXPR_FUNCTION_CALL, token_id);

            if (token_consume(it, IC_TOK_RIGHT_PAREN))
                return expr;

            ic_expr** arg_tail = &expr->_function_call.arg;
            int argc = 0;

            for(;;)
            {
                *arg_tail = produce_expr(it, parser);
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

        return parser.allocate_expr(IC_EXPR_PRIMARY, token_id);
    }
    case IC_TOK_LEFT_PAREN:
    {
        ic_expr* expr = parser.allocate_expr(IC_EXPR_PARENTHESES, **it);
        token_advance(it);
        expr->_parentheses.expr = produce_expr(it, parser);
        token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after expression");
        return expr;
    }
    } // switch

    exit_parsing(it, "expected literal / indentifier / parentheses / function call");
    return {};
}
