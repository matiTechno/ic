#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <stdarg.h>

enum ic_value_type
{
	IC_VAL_ERROR,
	IC_VAL_VOID,
	IC_VAL_NUMBER,
};

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
};

enum ic_ast_node_type
{
	IC_AST_BLOCK,
	IC_AST_WHILE,
	IC_AST_FOR,
	IC_AST_IF,
	IC_AST_RETURN,
	IC_AST_SEMICOLON,
	IC_AST_VAR_DECLARATION,
	IC_AST_BINARY,
	IC_AST_UNARY,
	IC_AST_FUNCTION_CALL,
	IC_AST_GROUPING,
	IC_AST_PRIMARY,
};

enum ic_error_type
{
	IC_ERR_LEXING,
	IC_ERR_PARSING,
	IC_ERR_EXECUCTION,
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
};

struct ic_value
{
	ic_value_type type;
	double number;
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
		} _block;

		struct
		{
			ic_ast_node* header;
			ic_ast_node* body;
		} _while;

		struct
		{
			ic_ast_node* header1;
			ic_ast_node* header2;
			ic_ast_node* header3;
			ic_ast_node* body;
		} _for;

		struct
		{
			ic_ast_node* header;
			ic_ast_node* body_if;
			ic_ast_node* body_else;
		} _if;

		struct
		{
			ic_ast_node* node;
		} _return;

		struct
		{
			ic_ast_node* node;
			ic_token token_id;
		} _var_declaration;

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

struct ic_var
{
	ic_string name;
	ic_value value;
};

struct ic_scope
{
	std::vector<ic_var> vars;
};

#define IC_POOL_SIZE 2048

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

struct ic_runtime
{
	ic_ast_node* _ast;
	ic_mem _mem;
	std::vector<ic_scope> _scopes;
	std::vector<char> _source_code;
	std::vector<ic_token> _tokens;

	bool tokenize();
	bool parse();
	void execute();
	void clean();

	void push_scope();
	void pop_scope();

	// this one should have an additional api interface, so if the host program adds new variable we allocate it
	bool add_var(ic_string name, ic_value value);

	ic_value* get_var(ic_string name);
	bool set_var(ic_string, ic_value value);
	
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

	ic_runtime runtime;

	runtime._source_code.resize(size);
	fread(runtime._source_code.data(), 1, size, file);
	fclose(file);
	runtime._source_code.push_back('\0');

	if (!runtime.tokenize())
		return 0;

	if (!runtime.parse())
		return 0;

	runtime.push_scope(); // global scope
	const char* var_name = "dupa";
	ic_string string = { var_name, strlen(var_name) };
	ic_value value;
	value.type = IC_VAL_NUMBER;
	value.number = 666;
	runtime.add_var(string, value);

	runtime.execute();

	printf("dupa = %f\n", runtime.get_var(string)->number);

	return 0;
}

void ic_runtime::clean()
{
	_mem.current_pool = 0;

	for (ic_ast_node_pool* pool : _mem.pools)
		// it is important to zero out not only size but also nodes
		memset(pool, 0, sizeof(ic_ast_node_pool));
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
		// it is important to zero out not only size but also nodes
		memset(new_pool, 0, sizeof(ic_ast_node_pool));
		pools.push_back(new_pool);
	}

	ic_ast_node_pool* pool = pools[current_pool];
	ic_ast_node* node = &pool->data[pool->size];
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

bool ic_string_compare(ic_string str1, ic_string str2)
{
	if (str1.len != str2.len)
		return false;

	return (strncmp(str1.data, str2.data, str1.len) == 0);
}

void ic_runtime::push_scope()
{
	_scopes.push_back({});
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

ic_value* ic_runtime::get_var(ic_string name)
{
	for (ic_scope& scope : _scopes)
	{
		for (ic_var& var : scope.vars)
		{
			if (ic_string_compare(var.name, name))
				return &var.value;
		}
	}

	return nullptr;
}

bool ic_runtime::set_var(ic_string name, ic_value value)
{
	for (ic_scope& scope : _scopes)
	{
		for (ic_var& var : scope.vars)
		{
			if (ic_string_compare(var.name, name))
			{
				var.value = value;
				return true;
			}
		}
	}

	return false;
}

ic_value ic_ast_execute(ic_ast_node* node, ic_runtime& runtime)
{
	assert(node);

	switch (node->type)
	{
	case IC_AST_BLOCK:
	{
		runtime.push_scope();
		while (node->_block.node)
		{
			ic_value value = ic_ast_execute(node->_block.node, runtime);
			if (!value.type) return {};
			node = node->_block.next;
		}
		runtime.pop_scope();
		return { IC_VAL_VOID };
	}
	case IC_AST_WHILE:
	{
		runtime.push_scope();
		for (;;)
		{
			ic_value value = ic_ast_execute(node->_while.header, runtime);
			if (!value.type) return {};
			if (!value.number) break;
			value = ic_ast_execute(node->_while.body, runtime);
			if (!value.type) return {};
		}
		runtime.pop_scope();
		return { IC_VAL_VOID };
	}
	case IC_AST_FOR:
	{
		runtime.push_scope();
		ic_value value;
		value = ic_ast_execute(node->_for.header1, runtime);
		if (!value.type) return {};
		for (;;)
		{
			value = ic_ast_execute(node->_for.header2, runtime);
			if (!value.type) return {};
			if (value.type == IC_VAL_NUMBER && !value.number) break;
			value = ic_ast_execute(node->_for.body, runtime);
			if (!value.type) return {};
			value = ic_ast_execute(node->_for.header3, runtime);
			if (!value.type) return {};
		}
		runtime.pop_scope();
		return { IC_VAL_VOID };
	}
	case IC_AST_IF:
	{
		runtime.push_scope();
		ic_value value;
		value = ic_ast_execute(node->_if.header, runtime);
		if (!value.type) return {};
		assert(value.type == IC_VAL_NUMBER);

		if (value.number)
		{
			value = ic_ast_execute(node->_if.body_if, runtime);
			if (!value.type) return {};
		}
		else
		{
			value = ic_ast_execute(node->_if.body_else, runtime);
			if (!value.type) return {};
		}

		runtime.pop_scope();
		return { IC_VAL_VOID };
	}
	case IC_AST_RETURN:
	{
		assert(false);
	}
	case IC_AST_SEMICOLON:
	{
		return { IC_VAL_VOID };
	}
	case IC_AST_VAR_DECLARATION:
	{
		ic_value value = ic_ast_execute(node->_var_declaration.node, runtime);
		if (!value.type) return {};
		assert(value.type == IC_VAL_NUMBER);
		ic_token token = node->_var_declaration.token_id;
		if (!runtime.add_var(token.string, value))
		{
			ic_print_error(IC_ERR_EXECUCTION, token.line, token.col, "variable with such name already exists in the current scope");
			return {};
		}
		return value;
	}
	case IC_AST_BINARY:
	{
		ic_value result = { IC_VAL_NUMBER };
		ic_value lvalue = ic_ast_execute(node->_binary.left, runtime);
		if (!lvalue.type) return {};
		ic_value rvalue = ic_ast_execute(node->_binary.right, runtime);
		if (!rvalue.type) return {};
		assert(lvalue.type == IC_VAL_NUMBER);
		assert(rvalue.type == IC_VAL_NUMBER);

		switch (node->_binary.token_operator.type)
		{
		case IC_TOK_AND:
			result.number = lvalue.number && rvalue.number;
			break;
		case IC_TOK_OR:
			result.number = lvalue.number || rvalue.number;
			break;
		case IC_TOK_EQUAL:
		{
			assert(node->_binary.left->type == IC_AST_PRIMARY);
			ic_token token = node->_binary.left->_primary.token;
			assert(token.type == IC_TOK_IDENTIFIER);
			runtime.set_var(token.string, rvalue);
			result = rvalue;
			break;
		}
		case IC_TOK_BANG_EQUAL:
			result.number = lvalue.number != rvalue.number;
			break;
		case IC_TOK_EQUAL_EQUAL:
			result.number = lvalue.number == rvalue.number;
			break;
		case IC_TOK_MINUS:
			result.number = lvalue.number - rvalue.number;
			break;
		case IC_TOK_PLUS:
			result.number = lvalue.number + rvalue.number;
			break;
		case IC_TOK_STAR:
			result.number = lvalue.number * rvalue.number;
			break;
		case IC_TOK_SLASH:
			result.number = lvalue.number / rvalue.number;
			break;
		case IC_TOK_GREATER:
			result.number = lvalue.number > rvalue.number;
			break;
		case IC_TOK_GREATER_EQUAL:
			result.number = lvalue.number >= rvalue.number;
			break;
		case IC_TOK_LESS:
			result.number = lvalue.number < rvalue.number;
			break;
		case IC_TOK_LESS_EQUAL:
			result.number = lvalue.number <= rvalue.number;
			break;
		default:
			assert(false);
		}

		return result;
	}
	case IC_AST_UNARY:
	{
		ic_value value = ic_ast_execute(node->_unary.node, runtime);
		if (!value.type) return {};
		assert(value.type == IC_VAL_NUMBER);

		switch (node->_unary.token_operator.type)
		{
		case IC_TOK_MINUS:
			value.number *= -1.0;
			break;
		case IC_TOK_BANG:
			value.number = !value.number;
			break;
		default:
			assert(false);
		}

		return value;
	}
	case IC_AST_FUNCTION_CALL:
	{
		assert(false);
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
			ic_value value = { IC_VAL_NUMBER };
			value.number = token.number;
			return value;
		}
		case IC_TOK_IDENTIFIER:
		{
			ic_value* value = runtime.get_var(token.string);
			if (!value)
			{
				ic_print_error(IC_ERR_EXECUCTION, token.line, token.col, "variable with such name does not exist");
				return {};
			}
			return *value;
		}
		} // end of switch stmt
		assert(false);
	}
	} // end of switch stmt
	assert(false);
}

void ic_runtime::execute()
{
	ic_ast_execute(_ast, *this);
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

	bool match(char c)
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
	return is_digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool ic_runtime::tokenize()
{
	ic_lexer lexer{ _tokens };
	lexer._source_it = _source_code.data();
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
		case '\n': break;

		case '(': lexer.add_token(IC_TOK_LEFT_PAREN); break;
		case ')': lexer.add_token(IC_TOK_RIGHT_PAREN); break;
		case '{': lexer.add_token(IC_TOK_LEFT_BRACE); break;
		case '}': lexer.add_token(IC_TOK_RIGHT_BRACE); break;
		case '-': lexer.add_token(IC_TOK_MINUS); break;
		case '+': lexer.add_token(IC_TOK_PLUS); break;
		case ';': lexer.add_token(IC_TOK_SEMICOLON); break;
		case '*': lexer.add_token(IC_TOK_STAR); break;

		case '!':
		{
			lexer.add_token(lexer.match('=') ? IC_TOK_BANG_EQUAL : IC_TOK_BANG);
			break;
		}
		case '=':
		{
			lexer.add_token(lexer.match('=') ? IC_TOK_EQUAL_EQUAL : IC_TOK_EQUAL);
			break;
		}
		case '>':
		{
			lexer.add_token(lexer.match('=') ? IC_TOK_GREATER_EQUAL : IC_TOK_GREATER);
			break;
		}
		case '<':
		{
			lexer.add_token(lexer.match('=') ? IC_TOK_LESS_EQUAL : IC_TOK_LESS);
			break;
		}
		case '/':
		{
			if (!lexer.match('/'))
				lexer.add_token(IC_TOK_SLASH);
			else
			{
				while (!lexer.end() && lexer.advance() != '\n')
					;
			}
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
		} // end of switch stmt
	}

	lexer.add_token(IC_TOK_EOF);
	return success;
}

enum ic_op_precedence
{
	IC_PRECEDENCE_OR,
	IC_PRECEDENCE_AND,
	IC_PRECDENCE_COMPARE_EQUAL,
	IC_PRECEDENCE_COMPARE_GREATER,
	IC_PRECEDENCE_ADDITION,
	IC_PRECEDENCE_MULTIPLICATION,
	IC_PRECEDENCE_UNARY,
};

// grammar, production rules hierarchy

ic_ast_node* produce_stmt(const ic_token** it, ic_mem& _mem);
ic_ast_node* produce_stmt_expr(const ic_token** it, ic_mem& _mem);
ic_ast_node* produce_expr(const ic_token** it, ic_mem& _mem); // in current design variable declaration is an expression
ic_ast_node* produce_expr_assignment(const ic_token** it, ic_mem& _mem);
ic_ast_node* produce_expr_binary(const ic_token** it, ic_op_precedence precedence, ic_mem& _mem);
ic_ast_node* produce_expr_unary(const ic_token** it, ic_mem& _mem);
ic_ast_node* produce_expr_function_call(const ic_token** it, ic_mem& _mem);
ic_ast_node* produce_expr_primary(const ic_token** it, ic_mem& _mem);

bool ic_runtime::parse()
{
	// @TODO temporary hack
	{
		ic_token token;

		token.type = IC_TOK_LEFT_BRACE;
		_tokens.insert(_tokens.begin(), token);

		token.type = IC_TOK_RIGHT_BRACE;
		_tokens.insert(_tokens.end() - 1, token);
	}

	const ic_token* it = _tokens.data();
	_ast = produce_stmt(&it, _mem);
	return _ast != nullptr;
}

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
	}

	return false;
}

ic_ast_node* produce_stmt(const ic_token** it, ic_mem& _mem)
{
	switch ((**it).type)
	{
	case IC_TOK_LEFT_BRACE:
	{
		token_advance(it);
		ic_ast_node* node = _mem.allocate_node(IC_AST_BLOCK);
		ic_ast_node* block_it = node;

		while (!token_match_type(it, IC_TOK_RIGHT_BRACE) && !token_match_type(it, IC_TOK_EOF))
		{
			ic_ast_node* cnode = produce_stmt(it, _mem);
			if (!cnode) return {};
			block_it->_block.node = cnode;
			block_it->_block.next = _mem.allocate_node(IC_AST_BLOCK);
			block_it = block_it->_block.next;
		}

		if (!token_consume(it, IC_TOK_RIGHT_BRACE, "expected '}' to close a block statement")) return {};
		return node;
	}
	case IC_TOK_WHILE:
	{
		token_advance(it);
		ic_ast_node* node = _mem.allocate_node(IC_AST_WHILE);
		if (!token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after while keyword")) return {};
		node->_while.header = produce_expr(it, _mem);
		if (!node->_while.header) return {};
		if (!token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after while condition")) return {};
		node->_while.body = produce_stmt(it, _mem);
		if (!node->_while.body) return {};
		return node;
	}
	case IC_TOK_FOR:
	{
		token_advance(it);

		ic_ast_node* node = _mem.allocate_node(IC_AST_FOR);
		if (!token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after for keyword")) return {};
		node->_for.header1 = produce_stmt_expr(it, _mem);
		if (!node->_for.header1) return {};
		node->_for.header2 = produce_stmt_expr(it, _mem);
		if (!node->_for.header2) return {};
		node->_for.header3 = produce_expr(it, _mem); // this one should not end with ';', that's why we use don't use produce_stmt_expr()
		if (!node->_for.header3) return {};
		if (!token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after for header")) return {};
		node->_for.body = produce_stmt(it, _mem);
		if (!node->_for.body) return {};
		return node;
	}
	case IC_TOK_IF:
	{
		token_advance(it);
		ic_ast_node* node = _mem.allocate_node(IC_AST_IF);
		if (!token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after if keyword")) return {};
		node->_if.header = produce_expr(it, _mem);
		if (!node->_if.header) return {};
		if (!token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after if condition")) return {};
		node->_if.body_if = produce_stmt(it, _mem);
		if (!node->_if.body_if) return {};
		if (token_consume(it, IC_TOK_ELSE))
		{
			node->_if.body_else = produce_stmt(it, _mem);
			if (!node->_if.body_else) return {};
		}
		return node;
	}
	case IC_TOK_RETURN:
	{
		token_advance(it);
		ic_ast_node* node = _mem.allocate_node(IC_AST_RETURN);
		if (!token_match_type(it, IC_TOK_SEMICOLON))
		{
			node->_return.node = produce_expr(it, _mem);
			if (!node->_return.node) return {};
		}
		if (!token_consume(it, IC_TOK_SEMICOLON, "expected ';' after return statement")) return {};
		return node;
	}
	} // end of switch stmt

	return produce_stmt_expr(it, _mem);
}

ic_ast_node* produce_stmt_expr(const ic_token** it, ic_mem& _mem)
{
	if (token_consume(it, IC_TOK_SEMICOLON))
		return _mem.allocate_node(IC_AST_SEMICOLON);

	ic_ast_node* node = produce_expr(it, _mem);
	if (!node) return {};
	if (!token_consume(it, IC_TOK_SEMICOLON, "expected ';' after expression")) return {};
	return node;
}

ic_ast_node* produce_expr(const ic_token** it, ic_mem& _mem)
{
	if (token_consume(it, IC_TOK_VAR))
	{
		ic_ast_node* node = _mem.allocate_node(IC_AST_VAR_DECLARATION);
		node->_var_declaration.token_id = **it;
		if (!token_consume(it, IC_TOK_IDENTIFIER, "expected variable name")) return {};

		if (token_consume(it, IC_TOK_EQUAL))
		{
			node->_var_declaration.node = produce_expr_assignment(it, _mem);
			if (!node->_var_declaration.node) return {};
		}
		// uninitialized variable
		else
		{
			node->_var_declaration.node = _mem.allocate_node(IC_AST_PRIMARY);
			node->_var_declaration.node->_primary.token.type = IC_TOK_NUMBER;
		}

		return node;
	}

	return produce_expr_assignment(it, _mem);
}

// assignment is right-associative, that's why this operator is not handled in produce_expr_binary()
ic_ast_node* produce_expr_assignment(const ic_token** it, ic_mem& _mem)
{
	ic_ast_node* node_left = produce_expr_binary(it, IC_PRECDENCE_COMPARE_EQUAL, _mem);
	if (!node_left) return {};

	if (token_match_type(it, IC_TOK_EQUAL))
	{
		if (node_left->type != IC_AST_PRIMARY || node_left->_primary.token.type != IC_TOK_IDENTIFIER)
		{
			ic_print_error(IC_ERR_PARSING, (**it).line, (**it).col, "left side of the assignment must be an lvalue");
			return {};
		}
		
		ic_ast_node* node = _mem.allocate_node(IC_AST_BINARY);
		node->_binary.token_operator = **it;
		token_advance(it);
		node->_binary.right = produce_expr_assignment(it, _mem);
		if (!node->_binary.right) return {};
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
	case IC_PRECDENCE_COMPARE_EQUAL:
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
	if (!node) return {};

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

		ic_ast_node* new_node = _mem.allocate_node(IC_AST_BINARY);
		new_node->_binary.token_operator = **it;
		token_advance(it);
		new_node->_binary.right = produce_expr_binary(it, precedence, _mem);
		if (!new_node->_binary.right) return {};
		new_node->_binary.left = node;
		node = new_node;
	}

	return node;
}

ic_ast_node* produce_expr_unary(const ic_token** it, ic_mem& _mem)
{
	ic_ast_node* node = _mem.allocate_node(IC_AST_UNARY);
	node->_unary.token_operator = **it;

	if (token_consume(it, IC_TOK_BANG) || token_consume(it, IC_TOK_MINUS))
	{
		node->_unary.node = produce_expr(it, _mem);
		if (!node->_unary.node) return {};
		return node;
	}

	return produce_expr_function_call(it, _mem);
}

ic_ast_node* produce_expr_function_call(const ic_token** it, ic_mem& _mem)
{
	// @TODO
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
		ic_ast_node* node = _mem.allocate_node(IC_AST_PRIMARY);
		node->_primary.token = **it;
		token_advance(it);
		return node;
	}
	case IC_TOK_LEFT_PAREN:
	{
		token_advance(it);
		ic_ast_node* node = _mem.allocate_node(IC_AST_GROUPING);
		node->_grouping.node = produce_expr(it, _mem);
		if (!node->_grouping.node) return {};
		if (!token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after expression")) return {};
		return node;
	}
	} // end of switch stmt

	ic_print_error(IC_ERR_PARSING, (**it).line, (**it).col, "expected expression");
	return {};
}