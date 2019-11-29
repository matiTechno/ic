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
    ic_runtime runtime;
    runtime.init();

    // I hate chrono api, but it is easy to use and there is no portable C version for high resolution timers
    auto t1 = std::chrono::high_resolution_clock::now();
    runtime.run(source_code.data());
    auto t2 = std::chrono::high_resolution_clock::now();
    printf("execution time: %d ms\n", (int)std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());
    return 0;
}

// these type functions exist to avoid bugs in value.type initialization

ic_value_type non_pointer_type(ic_basic_type type)
{
    return { type, 0, 0 };
}

// todo; I don't like bool arguments like this one, it is not obvious what it does from a function call
ic_value_type pointer1_type(ic_basic_type type, bool at_const = false)
{
    return { type, 1, (unsigned)(at_const ? 2 : 0) };
}

ic_value void_value()
{
    ic_value value;
    value.type = non_pointer_type(IC_TYPE_VOID);
    return value;
}

bool is_non_pointer_struct(ic_value_type& type)
{
    return !type.indirection_level && type.basic_type == IC_TYPE_STRUCT;
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

double get_numeric_data(ic_value value);

ic_data ic_host_print_s(ic_data* argv)
{
    printf("print: %s\n", (const char*)argv->pointer);
    return {};
}

ic_data ic_host_print_f(ic_data* argv)
{
    printf("print: %f\n", argv->f64);
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

void ic_runtime::init()
{
    assert(_scopes.size() == 0);
    push_scope(); // global scope

    // todo; use add_host_function()
    {
        const char* str = "print_s";
        ic_string name = { str, strlen(str) };
        ic_function function;
        function.type = IC_FUN_HOST;
        function.token.string = name;
        function.return_type = non_pointer_type(IC_TYPE_VOID);
        function.param_count = 1;
        function.params[0].type = pointer1_type(IC_TYPE_S8, true);
        function.callback = ic_host_print_s;
        _functions.push_back(function);
    }
    {
        const char* str = "print_f";
        ic_string name = { str, strlen(str) };
        ic_function function;
        function.type = IC_FUN_HOST;
        function.token.string = name;
        function.return_type = non_pointer_type(IC_TYPE_VOID);
        function.param_count = 1;
        function.params[0].type = pointer1_type(IC_TYPE_F64);
        function.callback = ic_host_print_f;
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
}

void ic_runtime::free()
{
    _stmt_deque.free();
    _expr_deque.free();
    _var_deque.free();
    _struct_data_deque.free();

    for (char* str : _string_literals)
        ::free(str);

    _string_literals.clear();

    for (ic_struct& _struct : _structs)
        ::free(_struct.members);
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

bool ic_tokenize(ic_runtime& runtime, const char* source_code);
ic_global produce_global(const ic_token** it, ic_runtime& runtime);
ic_expr_result ic_evaluate_expr(const ic_expr* expr, ic_runtime& runtime);
ic_stmt_result ic_execute_stmt(const ic_stmt* stmt, ic_runtime& runtime);
void run_bytecode(ic_runtime& runtime);


bool ic_runtime::run(const char* source_code)
{
    assert(_scopes.size() == 1); // assert ic_runtime isnitialized
    assert(source_code);
    _tokens.clear();
    _stmt_deque.size = 0;
    _expr_deque.size = 0;
    _struct_data_deque.size = 0;

    // clear allocated strings from previous run
    for (char* str : _string_literals)
        ::free(str);

    _string_literals.clear();

    // remove source functions from previous run; it must be here and not at the end of this function because it can return early
    {
        auto it = _functions.begin();
        for (; it != _functions.end(); ++it)
        {
            if (it->type == IC_FUN_SOURCE)
                break;
        }
        _functions.erase(it, _functions.end()); // source function are always stored after host ones
    }
    for (ic_struct& _struct : _structs)
        ::free(_struct.members);

    _structs.clear();

    _global_vars.clear();
    _global_size = 0;

    if (!ic_tokenize(*this, source_code))
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

        switch (global.type)
        {
        case IC_GLOBAL_FUNCTION:
        {
            ic_token token = global.function.token;

            if (get_function(token.string))
            {
                ic_print_error(IC_ERR_PARSING, token.line, token.col, "function with such name already exists");
                return false;
            }

            _functions.push_back(global.function);
            break;
        }
        case IC_GLOBAL_STRUCT:
        {
            ic_token token = global._struct.token;

            if (get_struct(token.string))
            {
                ic_print_error(IC_ERR_PARSING, token.line, token.col, "struct with such name already exists");
                return false;
            }

            _structs.push_back(global._struct);
            break;
        }
        case IC_GLOBAL_VAR_DECLARATION:
        {
            ic_execute_stmt(&global.stmt, *this);

            // set to 0
            // memset 0 will work for all data types, e.g. if all bits of double are 0 its value is also 0 (at least that's what I hope for)
            ic_string name = global.stmt._var_declaration.token.string;
            ic_value* value = get_var(name);
            assert(value);

            if (is_non_pointer_struct(value->type))
            {
                ic_struct* _struct = get_struct(value->type.struct_name);
                assert(_struct);
                memset(value->pointer, 0, _struct->num_data * sizeof(ic_data));
            }
            else
                memset(&value->f64, 0, sizeof(double));

            // code for bytecode
            {
                ic_vm_global_var gv;
                gv.idx = _global_size;
                gv.type = value->type;
                gv.name = name;
                _global_vars.push_back(gv);

                if (is_non_pointer_struct(value->type))
                {
                    ic_struct* _struct = get_struct(value->type.struct_name);
                    assert(_struct);
                    _global_size += _struct->num_data;
                }
                else
                    _global_size += 1;
            }

            break;
        }
        default:
            assert(false);
        }
    }

    if (true)
    {
        // execute main funtion
        const char* str = "main";
        ic_string name = { str, strlen(str) };
        ic_token token;
        token.type = IC_TOK_IDENTIFIER;
        token.string = name;
        ic_expr* expr = allocate_expr(IC_EXPR_FUNCTION_CALL, token);
        ic_evaluate_expr(expr, *this);
        assert(_scopes.size() == 1); // all non global scopes are gone
        // todo return value from main()?
    }
    else
    {
        for (ic_function& function : _functions)
            compile(function, *this);

        ic_vm vm; // todo, vm cleanup

        {
            int bytes = _global_size * sizeof(ic_data);
            vm.global_data = (ic_data*)malloc(bytes);
            memset(vm.global_data, 0, bytes);
        }
        vm.functions = (ic_vm_function*)malloc(_functions.size() * sizeof(ic_vm_function));
        vm.call_stack = (ic_data*)malloc(IC_CALL_STACK_SIZE * sizeof(ic_data));
        vm.operand_stack = (ic_data*)malloc(IC_OPERAND_STACK_SIZE * sizeof(ic_data));
        vm.call_stack_size = 0;
        vm.operand_stack_size = 0;

        // map parser functions to vm functions
        for (int i = 0; i < _functions.size(); ++i)
        {
            ic_function& fun = _functions[i];
            ic_vm_function vm_fun = vm.functions[i];

            vm_fun.is_host = fun.type == IC_FUN_HOST;
            vm_fun.param_size = 0;

            for (int j = 0; j < fun.param_count; ++j)
            {
                if (is_non_pointer_struct(fun.params[j].type))
                {
                    ic_struct* _struct = get_struct(fun.params[j].type.struct_name);
                    assert(_struct);
                    vm_fun.param_size += _struct->num_data;
                }
                else
                    vm_fun.param_size += 1;
            }

            if (is_non_pointer_struct(fun.return_type))
            {
                ic_struct* _struct = get_struct(fun.return_type.struct_name);
                assert(_struct);
                vm_fun.return_size = _struct->num_data;
            }
            else if (fun.return_type.indirection_level || fun.return_type.basic_type != IC_TYPE_VOID)
                vm_fun.return_size = 1;
            else
                vm_fun.return_size = 0;

            if (vm_fun.is_host)
                vm_fun.host_callback = fun.callback;
            else
            {
                vm_fun.source.bytecode = fun.bytecode;
                vm_fun.source.stack_size = fun.stack_size;
            }
        }

        for (ic_function& function : _functions)
        {
            if (ic_string_compare(function.token.string, { "main", 4 }))
            {
                assert(function.type == IC_FUN_SOURCE);
                assert(function.param_count == 0);
                assert(function.return_type.basic_type == IC_TYPE_VOID && !function.return_type.indirection_level);
                vm.push_stack_frame(function.bytecode, function.stack_size, 0);
                break;
            }
        }

        assert(vm.stack_frames.size() == 1);
        run_bytecode(vm);
    }

    return true;
}

void ic_runtime::push_scope(bool new_stack_frame)
{
    ic_scope scope;
    scope.prev_var_count = _var_deque.size;
    scope.prev_struct_data_count = _struct_data_deque.size;
    scope.new_stack_frame = new_stack_frame;
    _scopes.push_back(scope);
}

void ic_runtime::pop_scope()
{
    assert(_scopes.size());
    _var_deque.size = _scopes.back().prev_var_count;
    _struct_data_deque.size = _scopes.back().prev_struct_data_count;
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
        if (scope.new_stack_frame)
        {
            assert(!_scopes[0].new_stack_frame); // avoid infinite loop
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
            return &function;
    }
    return nullptr;
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

// interpreter does calculations only in double type and then converts value to expression result type
// this is to avoid boilerplate code + double can contain all other supported types
// why not just store everything as f64? in short - host data is supported
// why can't every numeric expression result in f64? only integer types can be added to pointers; ptr + (5 + 6); would fail then

bool to_boolean(ic_value value);
double get_numeric_data(ic_value value);
void set_numeric_data(ic_value& value, double number);
ic_basic_type get_numeric_expr_result_type(ic_value lhs, ic_value rhs);
ic_value produce_numeric_value(ic_basic_type btype, double number);
// use before set_lvalue_data(); for only numeric expressions use set_numeric_data() instead
ic_value implicit_convert_type(ic_value_type target_type, ic_value value);
void set_lvalue_data(void* lvalue_data, unsigned int const_mask, ic_value value, ic_runtime& runtime);
ic_value evaluate_add(ic_value lhs, ic_value rhs, ic_runtime& runtime, bool subtract = false);
bool compare_equal(ic_value lhs, ic_value rhs);
bool compare_greater(ic_value lhs, ic_value rhs);
void assert_integer_type(ic_value_type type);
ic_expr_result evaluate_dereference(ic_value value);

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

        if (stmt->_for.header1)
            ic_execute_stmt(stmt->_for.header1, runtime);

        // todo?
        int var_count = runtime._var_deque.size;
        int struct_data_count = runtime._struct_data_deque.size;
        ic_stmt_result output = { IC_STMT_RESULT_NOP };

        for (;;)
        {
            if (stmt->_for.header2)
            {
                ic_expr_result header2_result = ic_evaluate_expr(stmt->_for.header2, runtime);

                if (!to_boolean(header2_result.value))
                    break;
            }

            // free variables and struct data from previous loop iteration
            runtime._var_deque.size = var_count;
            runtime._struct_data_deque.size = struct_data_count;
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
        {
            ic_value return_value = ic_evaluate_expr(stmt->_return.expr, runtime).value;

            // this is very important (copy struct data to the function call scope)
            // todo; this is not a very robust code...
            if (is_non_pointer_struct(return_value.type))
            {
                ic_struct* _struct = runtime.get_struct(return_value.type.struct_name);
                assert(_struct);

                ic_scope* function_scope = nullptr;
                for (int i = runtime._scopes.size() - 1; i >= 0; --i)
                {
                    if (runtime._scopes[i].new_stack_frame)
                    {
                        function_scope = &runtime._scopes[i];
                        break;
                    }
                }

                assert(function_scope);
                runtime._struct_data_deque.size = function_scope->prev_struct_data_count;
                ic_data* dst = runtime._struct_data_deque.allocate_continuous(_struct->num_data);
                // return_value.pointer is still valid even if _struct_data_deque.size has changed; memmove, not memcpy - memory might overlap
                memmove(dst, return_value.pointer, _struct->num_data * sizeof(ic_data));
                return_value.pointer = dst;
                function_scope->prev_struct_data_count = runtime._struct_data_deque.size;
            }

            // this must be after if, return_value might be modified
            output.value = return_value;
        }
        else
        {
            // this value should never be used (type pass should terminate)
            // this is only for bug detection, e.g. failing in to_boolean()
            output.value.type = non_pointer_type(IC_TYPE_VOID);
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
            ic_value rhs;
            bool rhs_lvalue;
            {
                ic_expr_result result = ic_evaluate_expr(stmt->_var_declaration.expr, runtime);
                rhs = implicit_convert_type(value.type, result.value);
                rhs_lvalue = result.lvalue_data;
            }

            if (is_non_pointer_struct(value.type))
            {
                if (rhs_lvalue) // allocate struct and copy data
                {
                    ic_struct* _struct = runtime.get_struct(value.type.struct_name);
                    assert(_struct);
                    value.pointer = runtime._struct_data_deque.allocate_continuous(_struct->num_data);
                    memcpy(value.pointer, rhs.pointer, _struct->num_data * sizeof(ic_data));
                }
                else // move data (as in C++); rhs can be reused
                    value.pointer = rhs.pointer;
            }
            else
                value = rhs;
        }
        else
        {
            assert((value.type.const_mask & 1) == 0); // const variable must be initialized

            if (is_non_pointer_struct(value.type)) // allocate struct
            {
                ic_struct* _struct = runtime.get_struct(value.type.struct_name);
                assert(_struct);
                value.pointer = runtime._struct_data_deque.allocate_continuous(_struct->num_data);
            }
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

ic_expr_result ic_evaluate_expr(const ic_expr* expr, ic_runtime& runtime)
{
    assert(expr);

    switch(expr->type)
    {
    case IC_EXPR_BINARY:
    {
        ic_value lhs;
        void* lhs_lvalue_data;
        ic_value rhs = ic_evaluate_expr(expr->_binary.rhs, runtime).value;
        {
            ic_expr_result result = ic_evaluate_expr(expr->_binary.lhs, runtime);
            lhs = result.value;
            lhs_lvalue_data = result.lvalue_data;
        }

        switch (expr->token.type)
        {
        case IC_TOK_EQUAL:
        {
            rhs = implicit_convert_type(lhs.type, rhs);

            if (is_non_pointer_struct(lhs.type)) // special case for structs
            {
                assert(lhs_lvalue_data);
                assert(!lhs.type.const_mask);
                ic_struct* _struct = runtime.get_struct(lhs.type.struct_name);
                assert(_struct);
                memcpy(lhs.pointer, rhs.pointer, _struct->num_data * sizeof(ic_data));
                return { lhs_lvalue_data, lhs };
            }

            set_lvalue_data(lhs_lvalue_data, lhs.type.const_mask, rhs, runtime);
            return { lhs_lvalue_data, rhs };
        }
        case IC_TOK_PLUS_EQUAL:
        {
            ic_value output = evaluate_add(lhs, rhs, runtime);
            output = implicit_convert_type(lhs.type, output);
            set_lvalue_data(lhs_lvalue_data, lhs.type.const_mask, output, runtime);
            return { lhs_lvalue_data, output };
        }
        case IC_TOK_MINUS_EQUAL:
        {
            ic_value output = evaluate_add(lhs, rhs, runtime, true);
            output = implicit_convert_type(lhs.type, output);
            set_lvalue_data(lhs_lvalue_data, lhs.type.const_mask, output, runtime);
            return { lhs_lvalue_data, output };
        }
        case IC_TOK_STAR_EQUAL:
        {
            double number = get_numeric_data(lhs) * get_numeric_data(rhs);
            set_numeric_data(lhs, number);
            set_lvalue_data(lhs_lvalue_data, lhs.type.const_mask, lhs, runtime);
            return { lhs_lvalue_data, lhs };
        }
        case IC_TOK_SLASH_EQUAL:
        {
            double number = get_numeric_data(lhs) / get_numeric_data(rhs);
            set_numeric_data(lhs, number);
            set_lvalue_data(lhs_lvalue_data, lhs.type.const_mask, lhs, runtime);
            return { lhs_lvalue_data, lhs };
        }
        case IC_TOK_VBAR_VBAR:
        {
            // todo; bug, don't evaluate rhs if not necessary
            ic_value output;
            output.type = non_pointer_type(IC_TYPE_BOOL);
            output.s8 = to_boolean(lhs) || to_boolean(rhs);
            return { nullptr, output };
        }
        case IC_TOK_AMPERSAND_AMPERSAND:
        {
            // todo; bug, don't evaluate rhs if not necessary
            ic_value output;
            output.type = non_pointer_type(IC_TYPE_BOOL);
            output.s8 = to_boolean(lhs) && to_boolean(rhs);
            return { nullptr, output };
        }
        case IC_TOK_EQUAL_EQUAL:
        {
            ic_value output;
            output.type = non_pointer_type(IC_TYPE_BOOL);
            output.s8 = compare_equal(lhs, rhs);
            return { nullptr, output};
        }
        case IC_TOK_BANG_EQUAL:
        {
            ic_value output;
            output.type = non_pointer_type(IC_TYPE_BOOL);
            output.s8 = !compare_equal(lhs, rhs);
            return { nullptr, output};
        }
        case IC_TOK_GREATER:
        {
            ic_value output;
            output.type = non_pointer_type(IC_TYPE_BOOL);
            output.s8 = compare_greater(lhs, rhs);
            return { nullptr, output};
        }
        case IC_TOK_GREATER_EQUAL:
        {
            ic_value output;
            output.type = non_pointer_type(IC_TYPE_BOOL);
            output.s8 = compare_greater(lhs, rhs) || compare_equal(lhs, rhs);
            return { nullptr, output};
        }
        case IC_TOK_LESS:
        {
            ic_value output;
            output.type = non_pointer_type(IC_TYPE_BOOL);
            output.s8 = !compare_greater(lhs, rhs) && !compare_equal(lhs, rhs);
            return { nullptr, output};
        }
        case IC_TOK_LESS_EQUAL:
        {
            ic_value output;
            output.type = non_pointer_type(IC_TYPE_BOOL);
            output.s8 = !compare_greater(lhs, rhs) || compare_equal(lhs, rhs);
            return { nullptr, output};
        }
        case IC_TOK_PLUS:
        {
            return { nullptr, evaluate_add(lhs, rhs, runtime) };
        }
        case IC_TOK_MINUS:
        {
            return { nullptr, evaluate_add(lhs, rhs, runtime, true) };
        }
        case IC_TOK_STAR:
        {
            double number = get_numeric_data(lhs) * get_numeric_data(rhs);
            return { nullptr, produce_numeric_value(get_numeric_expr_result_type(lhs, rhs), number) };
        }
        case IC_TOK_SLASH:
        {
            double number = get_numeric_data(lhs) / get_numeric_data(rhs);
            return { nullptr, produce_numeric_value(get_numeric_expr_result_type(lhs, rhs), number) };
        }
        case IC_TOK_PERCENT:
        {
            ic_value output;
            output.type = non_pointer_type(get_numeric_expr_result_type(lhs, rhs));
            assert_integer_type(output.type);
            // int64_t can contain all supported integer types; todo; this code feels bad, too many conversions
            // the entire code for operators feels bad; I hope the situation will resolve after implementing bytecode
            int64_t number = (int64_t)get_numeric_data(lhs) % (int64_t)get_numeric_data(rhs);
            set_numeric_data(output, number);
            return { nullptr, output };
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
            // todo; don't allow -true expression; but it is not a big deal
            set_numeric_data(value, -get_numeric_data(value));
            return { nullptr, value };
        }
        case IC_TOK_BANG:
        {
            ic_value output;
            output.type = non_pointer_type(IC_TYPE_BOOL);
            output.s8 = !to_boolean(value);
            return { nullptr, output };
        }
        case IC_TOK_MINUS_MINUS:
        {
            ic_value output = evaluate_add(value, produce_numeric_value(IC_TYPE_S8, -1), runtime);
            output = implicit_convert_type(value.type, output);
            set_lvalue_data(lvalue_data, value.type.const_mask, output, runtime);
            return { lvalue_data, output };
        }
        case IC_TOK_PLUS_PLUS:
        {
            ic_value output = evaluate_add(value, produce_numeric_value(IC_TYPE_S8, 1), runtime);
            output = implicit_convert_type(value.type, output);
            set_lvalue_data(lvalue_data, value.type.const_mask, output, runtime);
            return { lvalue_data, output };
        }
        case IC_TOK_AMPERSAND:
        {
            assert(lvalue_data);
            ic_value output;
            output.type = { value.type.basic_type, value.type.indirection_level + 1, value.type.const_mask << 1, value.type.struct_name };
            output.pointer = lvalue_data;
            return { nullptr, output };
        }
        case IC_TOK_STAR:
        {
            return evaluate_dereference(value);
        }
        } // switch

        assert(false);
    }
    case IC_EXPR_SIZEOF:
    {
        ic_value_type type = expr->_sizeof.type;
        int size;

        if (type.indirection_level || type.basic_type == IC_TYPE_F64)
            size = sizeof(void*);
        else
        {
            switch (type.basic_type)
            {
            case IC_TYPE_BOOL:
            case IC_TYPE_S8:
            case IC_TYPE_U8:
                size = sizeof(char);
                break;
            case IC_TYPE_S32:
            case IC_TYPE_U32:
            case IC_TYPE_F32:
                size = sizeof(int);
                break;
            case IC_TYPE_STRUCT:
            {
                ic_struct* _struct = runtime.get_struct(type.struct_name);
                assert(_struct);
                size = _struct->num_data * sizeof(ic_data);
                break;
            }
            default:
                assert(false);
            }
        }

        return { nullptr, produce_numeric_value(IC_TYPE_S32, size) };
    }
    case IC_EXPR_CAST_OPERATOR:
    {
        // allows only non-pointer to non-pointer and pointer to pointer conversions
        // creates temporary value, that's why lvalue_data is set to nullptr

        ic_value value = ic_evaluate_expr(expr->_cast_operator.expr, runtime).value;
        ic_value_type target_type = expr->_cast_operator.type;

        if (target_type.indirection_level)
        {
            assert(value.type.indirection_level);
            value.type = target_type;
            return { nullptr, value };
        }

        return { nullptr, produce_numeric_value(target_type.basic_type, get_numeric_data(value)) };
    }
    case IC_EXPR_SUBSCRIPT:
    {
        ic_value lhs = ic_evaluate_expr(expr->_subscript.lhs, runtime).value;
        ic_value rhs = ic_evaluate_expr(expr->_subscript.rhs, runtime).value;
        ic_value output = evaluate_add(lhs, rhs, runtime);
        return evaluate_dereference(output);
    }
    case IC_EXPR_MEMBER_ACCESS:
    {
        ic_value lhs;
        void* lvalue_data;
        {
            ic_expr_result result = ic_evaluate_expr(expr->_member_access.lhs, runtime);
            lhs = result.value;
            lvalue_data = result.lvalue_data;
        }

        assert(lhs.type.basic_type == IC_TYPE_STRUCT);

        if (lhs.type.indirection_level)
        {
            assert(lhs.type.indirection_level == 1);
            assert(expr->token.type == IC_TOK_ARROW);
            ic_expr_result result = evaluate_dereference(lhs);
            lhs = result.value;
            lvalue_data = result.lvalue_data;
        }
        else
            assert(expr->token.type == IC_TOK_DOT);

        ic_struct* _struct = runtime.get_struct(lhs.type.struct_name);
        assert(_struct);
        bool match = false;
        ic_value_type member_type;
        int data_offset = 0;

        for (int i = 0; i < _struct->num_members; ++i)
        {
            ic_struct_member& member = _struct->members[i];

            if (ic_string_compare(member.name, expr->_member_access.rhs_token.string))
            {
                match = true;
                member_type = member.type;
                break;
            }

            if (is_non_pointer_struct(member.type))
            {
                ic_struct* sub_struct = runtime.get_struct(member.type.struct_name);
                data_offset += sub_struct->num_data;
            }
            else
                data_offset += 1;
        }

        assert(match);
        ic_value output;
        output.type = member_type;
        assert(lhs.type.const_mask == 0 || lhs.type.const_mask == 1); // non-pointer value should use only one bit of a const mask
        output.type.const_mask |= lhs.type.const_mask; // propagate constness to a member
        ic_data* data = (ic_data*)lhs.pointer + data_offset;

        if (is_non_pointer_struct(output.type))
            output.pointer = data;
        else
            output.f64 = data->f64; // ub? what if f64 is non-active union member?

        // each member of a union has the same address + a union itself has the same address as its members,
        // i.e. lvalue_data is set correctly here for both struct and non-struct values
        // for more information see IC_TOK_IDENTIFIER code
        return { lvalue_data ? data : nullptr, output };
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
        bool lvalue_arg[IC_MAX_ARGC];
        ic_expr* expr_arg = expr->_function_call.arg;

        while (expr_arg)
        {
            assert(argc < IC_MAX_ARGC);
            ic_expr_result result = ic_evaluate_expr(expr_arg, runtime);

            if (is_non_pointer_struct(result.value.type) && function->type == IC_FUN_HOST)
                assert(false); // struct arguments are not supported for host functions

            expr_arg = expr_arg->next;
            argv[argc] = result.value;
            lvalue_arg[argc] = result.lvalue_data;
            argc += 1;
        }

        assert(argc == function->param_count);

        for (int i = 0; i < argc; ++i)
            argv[i] = implicit_convert_type(function->params[i].type, argv[i]);

        if (function->type == IC_FUN_HOST)
        {
            assert(function->callback);

            // todo, hack
            ic_data dargv[IC_MAX_ARGC];

            for (int i = 0; i < argc; ++i)
                dargv[i].f64 = argv[i].f64;

            ic_value return_value;
            return_value.type = function->return_type;
            return_value.f64 = function->callback(dargv).f64;
            // function can never return an lvalue
            return { nullptr, return_value };
        }
        // else
        assert(function->type == IC_FUN_SOURCE);
        // all arguments must be retrieved before upper scopes are hidden
        runtime.push_scope(true);
        // only now, when new scope is pushed, allocate data for struct arguments and add variables

        for (int i = 0; i < argc; ++i)
        {
            // only data of the rvalue struct args can be reused
            if (lvalue_arg[i] && is_non_pointer_struct(argv[i].type))
            {
                ic_struct* _struct = runtime.get_struct(argv[i].type.struct_name);
                assert(_struct);
                void* new_data = runtime._struct_data_deque.allocate_continuous(_struct->num_data);
                memcpy(new_data, argv[i].pointer, _struct->num_data * sizeof(ic_data));
                argv[i].pointer = new_data;
            }

            runtime.add_var(function->params[i].name, argv[i]);
        }

        ic_expr_result output;
        output.lvalue_data = nullptr;
        ic_stmt_result fun_result = ic_execute_stmt(function->body, runtime);

        if (fun_result.type == IC_STMT_RESULT_RETURN)
            output.value = implicit_convert_type(function->return_type, fun_result.value);
        else
        {
            // function without return statement
            assert(fun_result.type == IC_STMT_RESULT_NOP);
            assert(!function->return_type.indirection_level && function->return_type.basic_type == IC_TYPE_VOID);
            // this value should never be used (type pass should terminate)
            // this is only for bug detection, e.g. failing in to_boolean()
            output.value.type = non_pointer_type(IC_TYPE_VOID);
        }

        runtime.pop_scope();
        return output;
    }
    case IC_EXPR_PRIMARY:
    {
        ic_token token = expr->token;

        switch (token.type)
        {
        case IC_TOK_INT_NUMBER_LITERAL:
        {
            // todo; decide which type to use (signed or unsigned) based on the number
            ic_value value;
            value.type = non_pointer_type(IC_TYPE_S32);
            value.s32 = token.number;
            return { nullptr, value };
        }
        case IC_TOK_FLOAT_NUMBER_LITERAL:
        {
            ic_value value;
            value.type = non_pointer_type(IC_TYPE_F64);
            value.f64 = token.number;
            return { nullptr, value };
        }
        case IC_TOK_IDENTIFIER:
        {
            ic_value* value = runtime.get_var(token.string);
            assert(value);
            // the only reason to set lvalue_data for struct is to mark if it is an lvalue or an rvalue
            // (from a language perspective), in both cases data can be read and set through value->pointer
            return { is_non_pointer_struct(value->type) ? value->pointer : &value->s8, *value };
        }
        case IC_TOK_TRUE:
        case IC_TOK_FALSE:
        {
            ic_value value;
            value.type = non_pointer_type(IC_TYPE_BOOL);
            value.s8 = token.type == IC_TOK_TRUE ? 1 : 0;
            return { nullptr, value };
        }
        case IC_TOK_NULLPTR:
        {
            ic_value value;
            value.type = pointer1_type(IC_TYPE_NULLPTR);
            value.pointer = nullptr;
            return { nullptr, value };
        }
        case IC_TOK_STRING_LITERAL:
        {
            ic_value value;
            value.type = pointer1_type(IC_TYPE_S8);
            value.type.const_mask = 1 << 1;
            value.pointer = (void*)token.string.data; // todo; casting const char* to void* - ub?
            return { nullptr, value };
        }
        case IC_TOK_CHARACTER_LITERAL:
        {
            ic_value value;
            value.type = non_pointer_type(IC_TYPE_S8);
            value.s8 = token.number;
            return { nullptr, value };

        }
        } // switch

        assert(false);
    }
    } // switch

    assert(false);
    return {};
}

bool to_boolean(ic_value value)
{
    if (value.type.indirection_level)
        return value.pointer;

    return get_numeric_data(value);
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
        value.s8 = number ? true : false;
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

ic_basic_type get_numeric_expr_result_type(ic_value lhs, ic_value rhs)
{
    assert(!lhs.type.indirection_level);
    assert(!rhs.type.indirection_level);
    ic_basic_type lbtype = lhs.type.basic_type;
    ic_basic_type rbtype = rhs.type.basic_type;
    ic_basic_type output = lbtype > rbtype ? lbtype : rbtype;
    return output > IC_TYPE_BOOL ? output : IC_TYPE_S8; // true + true should return 2 not 1, this is a fix
}

ic_value produce_numeric_value(ic_basic_type btype, double number)
{
    ic_value value;
    value.type = non_pointer_type(btype);
    set_numeric_data(value, number);
    return value;
}

ic_value implicit_convert_type(ic_value_type target_type, ic_value value)
{
    assert(target_type.indirection_level == value.type.indirection_level);

    if (target_type.basic_type == IC_TYPE_STRUCT && value.type.basic_type == IC_TYPE_STRUCT)
        assert(ic_string_compare(target_type.struct_name, value.type.struct_name));

    if (target_type.indirection_level)
    {
        // target_type must be 'more' const than value.type
        for (int i = 0; i <= target_type.indirection_level; ++i)
        {
            int const_lhs = target_type.const_mask & (1 << i);
            int const_rhs = value.type.const_mask & (1 << i);
            assert(const_lhs >= const_rhs);
        }

        if (value.type.basic_type == IC_TYPE_NULLPTR || target_type.basic_type == IC_TYPE_VOID ||
            (target_type.basic_type == value.type.basic_type))
        {
            value.type = target_type;
            return value;
        }

        assert(false);
    }

    if (target_type.basic_type == value.type.basic_type)
    {
        value.type.const_mask = target_type.const_mask; // this is important for var declarations and assignments
        return value;
    }

    ic_value output;
    output.type = target_type;
    set_numeric_data(output, get_numeric_data(value));
    return output;
}

// the main point of this function is to write the right amount of data (to not trigger segmentation fault)
// without any conversion
void set_lvalue_data(void* lvalue_data, unsigned int const_mask, ic_value value, ic_runtime& runtime)
{
    assert(lvalue_data); // todo type pass - this is a type error
    // can't assign new data to a const variable
    assert((const_mask & 1) == 0); // watch out for operator precendence

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

ic_value evaluate_add(ic_value lhs, ic_value rhs, ic_runtime& runtime, bool subtract)
{
    if (lhs.type.indirection_level)
    {
        assert_integer_type(rhs.type);
        int64_t offset = subtract ? -get_numeric_data(rhs) : get_numeric_data(rhs);

        if (lhs.type.indirection_level > 1 || lhs.type.basic_type == IC_TYPE_F64) // pointer to pointer or pointer to double
            lhs.pointer = (void**)lhs.pointer + offset;
        else
        {
            switch (lhs.type.basic_type)
            {
            case IC_TYPE_BOOL:
            case IC_TYPE_S8:
            case IC_TYPE_U8:
                lhs.pointer = (char*)lhs.pointer + offset;
                break;
            case IC_TYPE_S32:
            case IC_TYPE_U32:
            case IC_TYPE_F32:
                lhs.pointer = (int*)lhs.pointer + offset;
                break;
            case IC_TYPE_STRUCT:
            {
                ic_struct * _struct = runtime.get_struct(lhs.type.struct_name);
                assert(_struct);
                lhs.pointer = (ic_data*)lhs.pointer + offset * _struct->num_data;
                break;
            }
            default:
                assert(false);
            }
        }

        return lhs;
    }

    double lhs_number = get_numeric_data(lhs);
    double rhs_number = subtract ? -get_numeric_data(rhs) : get_numeric_data(rhs);
    return produce_numeric_value(get_numeric_expr_result_type(lhs, rhs), lhs_number + rhs_number);
}

bool compare_equal(ic_value lhs, ic_value rhs)
{
    if (lhs.type.indirection_level)
    {
        assert(rhs.type.indirection_level);
        bool is_nullptr = lhs.type.basic_type == IC_TYPE_NULLPTR || rhs.type.basic_type == IC_TYPE_NULLPTR;
        bool is_void = lhs.type.basic_type == IC_TYPE_VOID || rhs.type.basic_type == IC_TYPE_VOID;

        if (!is_nullptr && !is_void) // void* and nullptr can compare with any other type
        {
            assert(lhs.type.basic_type == rhs.type.basic_type);

            if (lhs.type.basic_type == IC_TYPE_STRUCT)
                assert(ic_string_compare(lhs.type.struct_name, rhs.type.struct_name));
        }

        return lhs.pointer == rhs.pointer;
    }

    // no compare_equal for structs
    return get_numeric_data(lhs) == get_numeric_data(rhs);
}

bool compare_greater(ic_value lhs, ic_value rhs)
{
    if (lhs.type.indirection_level)
    {
        assert(rhs.type.indirection_level);
        bool is_nullptr = lhs.type.basic_type == IC_TYPE_NULLPTR || rhs.type.basic_type == IC_TYPE_NULLPTR;
        bool is_void = lhs.type.basic_type == IC_TYPE_VOID || rhs.type.basic_type == IC_TYPE_VOID;

        if (!is_nullptr && !is_void) // void* and nullptr can compare with any other type
        {
            assert(lhs.type.basic_type == rhs.type.basic_type);

            if (lhs.type.basic_type == IC_TYPE_STRUCT)
                assert(ic_string_compare(lhs.type.struct_name, rhs.type.struct_name));
        }

        return lhs.pointer > rhs.pointer;
    }

    return get_numeric_data(lhs) > get_numeric_data(rhs);
}

void assert_integer_type(ic_value_type type)
{
    assert(!type.indirection_level);

    switch (type.basic_type)
    {
    case IC_TYPE_BOOL:
    case IC_TYPE_S8:
    case IC_TYPE_U8:
    case IC_TYPE_S32:
    case IC_TYPE_U32:
        return;
    }

    assert(false);
}

ic_expr_result evaluate_dereference(ic_value value)
{
    assert(value.type.indirection_level);
    assert(value.type.basic_type != IC_TYPE_NULLPTR);

    ic_value output;
    output.type = { value.type.basic_type, value.type.indirection_level - 1, value.type.const_mask >> 1, value.type.struct_name };

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
        case IC_TYPE_STRUCT:
            // this is somehow a special case, both struct values and pointers point to struct data
            // for more information see IC_TOK_IDENTIFIER code
            output.pointer = value.pointer;
            break;
        default:
            assert(false);
        }
    }

    return { value.pointer, output };
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

            const char* string_begin = token_begin + 1; // skip first "
            int len = lexer.pos() - string_begin; // this doesn't count last "
            char* data = (char*)malloc(len + 1); // one more for \0
            memcpy(data, string_begin, len);
            data[len] = '\0';
            lexer.add_token_string(IC_TOK_STRING_LITERAL, { data }); // len doesn't need to be initialized, string will never be compared
            runtime._string_literals.push_back(data);
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

bool produce_type(const ic_token** it, ic_runtime& runtime, ic_value_type& type)
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
        ic_value_type type;

        while (produce_type(it, runtime, type))
        {
            assert(_struct.num_members < IC_MAX_MEMBERS);

            // todo; support const members
            if (type.const_mask & 1)
                exit_parsing(it, "struct member can't be const");

            if (is_non_pointer_struct(type))
            {
                ic_struct* sub_struct = runtime.get_struct(type.struct_name);
                assert(sub_struct);
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
    ic_value_type type;

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
        function.type = IC_FUN_SOURCE;
        function.token = token_id;
        function.return_type = type;
        function.param_count = 0;

        if (!token_consume(it, IC_TOK_RIGHT_PAREN))
        {
            for (;;)
            {
                if (function.param_count >= IC_MAX_ARGC)
                    exit_parsing(it, "exceeded maximal number of parameters (%d)", IC_MAX_ARGC);

                ic_value_type param_type;

                if (!produce_type(it, runtime, param_type))
                    exit_parsing(it, "expected parameter type");

                // constness should not be compared here and that's why == non_pointer(IC_TYPE_VOID) is not used
                if (!param_type.indirection_level && param_type.basic_type == IC_TYPE_VOID)
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

        function.body = produce_stmt(it, runtime);

        if (function.body->type != IC_STMT_COMPOUND)
            exit_parsing(it, "expected compound stmt after function parameter list");

        function.body->_compound.push_scope = false; // do not allow shadowing of arguments
        return global;
    }

    // variable
    // todo; redundant with produce_stmt_var_declaration()
    if (type.basic_type == IC_TYPE_VOID && !type.indirection_level)
        exit_parsing(it, "variables can't be of type void");

    ic_global global;
    global.type = IC_GLOBAL_VAR_DECLARATION;
    ic_stmt& stmt = global.stmt;
    stmt.type = IC_STMT_VAR_DECLARATION;
    stmt._var_declaration.type = type;
    stmt._var_declaration.token = token_id;

    if ((**it).type == IC_TOK_EQUAL)
        exit_parsing(it, "global variables can't be initialized by an expression, they are set to 0 by default");

    token_consume(it, IC_TOK_SEMICOLON, "expected ';' or '=' after variable name");
    stmt._var_declaration.expr = nullptr;
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
    ic_value_type type;

    if (produce_type(it, runtime, type))
    {
        // constness should not be compared here and that's why == non_pointer(IC_TYPE_VOID) is not used
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
        ic_value_type type;

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
