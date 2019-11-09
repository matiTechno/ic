#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <stdarg.h>

enum ic_value_type
{
	IC_VAL_BOOLEAN,
	IC_VAL_NUMBER,
	IC_VAL_STRING,
	IC_VAL_NIL,
	IC_VAL_ARRAY,
	IC_OBJECT,
};

enum ic_token_type
{
	IC_TOK_EOF,
	// single character tokens
	IC_TOK_LEFT_PAREN,
	IC_TOK_RIGHT_PAREN,
	IC_TOK_LEFT_BRACE,
	IC_TOK_RIGHT_BRACE,
	IC_TOK_COMMA,
	IC_TOK_DOT,
	IC_TOK_MINUS,
	IC_TOK_PLUS,
	IC_TOK_SEMICOLON,
	IC_TOK_SLASH,
	IC_TOK_STAR,
	// one or two character tokens
	IC_TOK_BANG,
	IC_TOK_BANG_EQUAL,
	IC_TOK_EQUAL,
	IC_TOK_EQUAL_EQUAL,
	IC_TOK_GREATER,
	IC_TOK_GREATER_EQUAL,
	IC_TOK_LESS,
	IC_TOK_LESS_EQUAL,
	// literals
	IC_TOK_IDENTIFIER,
	IC_TOK_STRING,
	IC_TOK_NUMBER,
	// keywords
	IC_TOK_AND,
	IC_TOK_ELSE,
	IC_TOK_FALSE,
	IC_TOK_FOR,
	IC_TOK_IF,
	IC_TOK_NIL,
	IC_TOK_OR,
	IC_TOK_RETURN,
	IC_TOK_TRUE,
	IC_TOK_WHILE,
	IC_TOK_VAR,
};

enum ic_ast_type
{
	IC_AST_ERROR,
	IC_AST_STMT_VAR_DECLARATION,
	IC_AST_STMT_BLOCK,
	IC_AST_STMT_WHILE,
	IC_AST_STMT_IF,
	IC_AST_STMT_FOR,
	IC_AST_STMT_RETURN,
	IC_AST_STMT_SEMICOLON,
	IC_AST_EXPR_BINARY,
	IC_AST_EXPR_UNARY,
	IC_AST_EXPR_FUNCTION_CALL,
	IC_AST_EXPR_GROUPING,
	IC_AST_EXPR_PRIMARY,
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
	{"false", IC_TOK_FALSE},
	{"for", IC_TOK_FOR},
	{"if", IC_TOK_IF},
	{"return", IC_TOK_RETURN},
	{"nil", IC_TOK_NIL},
	{"or", IC_TOK_OR},
	{"var", IC_TOK_VAR},
	{"while", IC_TOK_WHILE},
	{"true", IC_TOK_TRUE},
};

// @TODO
// custom allocator for ast nodes - array of pools
// now we are leaking memory on error
// this is important, if want to execute more than once

struct ic_value
{
	ic_value_type type;

	// this should be union
	bool boolean;
	double number;
	std::string string;
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
	ic_ast_type type = IC_AST_ERROR;
	ic_token token;
	ic_ast_node* next_left = nullptr;
	ic_ast_node* next_right = nullptr;
};

struct ic_var
{
	ic_value value;
	ic_string name;
};

struct ic_scope
{
	std::vector<ic_var> vars;
};

struct ic_line
{
	const char* data;
	int len;
};

struct ic_function
{
};

struct ic_structure
{
};

struct ic_runtime
{
	ic_ast_node _ast;
	std::vector<ic_function> _functions;
	std::vector<ic_structure> _structures;
	std::vector<ic_scope> _scopes;
	std::vector<char> _source_code;
	std::vector<ic_line> _lines;
	std::vector<ic_token> _tokens;

	bool tokenize();
	bool parse();
	bool execute();
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

	if (!runtime.execute())
		return 0;

	return 0;
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
	int line_len = 0;
	const char* line_data = _source_code.data();

	for (int i = 0; i < _source_code.size(); ++i)
	{
		if (_source_code[i] == '\n')
		{
			_lines.push_back({ line_data, line_len });
			line_len = 0;
			line_data = _source_code.data() + i + 1;
		}
		else
			++line_len;
	}

	_lines.push_back({ line_data, line_len });

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

		// single character
		case '(': lexer.add_token(IC_TOK_LEFT_PAREN); break;
		case ')': lexer.add_token(IC_TOK_RIGHT_PAREN); break;
		case '{': lexer.add_token(IC_TOK_LEFT_BRACE); break;
		case '}': lexer.add_token(IC_TOK_RIGHT_BRACE); break;
		case ',': lexer.add_token(IC_TOK_COMMA); break;
		case '.': lexer.add_token(IC_TOK_DOT); break;
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

		// other

		case '/':
		{
			if (!lexer.match('/'))
				lexer.add_token(IC_TOK_SLASH);
			else
			{
				while (!lexer.end() && lexer.advance() != '\n')
					;
			}
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
					if (strlen(keyword.str) != len)
						continue;

					if (strncmp(begin_it, keyword.str, len) == 0)
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

		// end of switch stmt
		}
	}

	lexer.add_token(IC_TOK_EOF);
	return success;
}

enum ic_op_precedence
{
	IC_PRECEDENCE_OR,
	IC_PRECEDENCE_AND,
	IC_PRECDENCE_EQUAL,
	IC_PRECEDENCE_GREATER,
	IC_PRECEDENCE_ADDITION,
	IC_PRECEDENCE_MULTIPLICATION,
	IC_PRECEDENCE_UNARY,
};

// grammar, production rules hierarchy

ic_ast_node produce_stmt(const ic_token** it);
ic_ast_node produce_stmt_expr(const ic_token** it);
ic_ast_node produce_expr(const ic_token** it); // in current design variable declaration is an expression
ic_ast_node produce_expr_assignment(const ic_token** it);
ic_ast_node produce_expr_binary(const ic_token** it, ic_op_precedence precedence);
ic_ast_node produce_expr_unary(const ic_token** it);
ic_ast_node produce_expr_function_call(const ic_token** it);
ic_ast_node produce_expr_primary(const ic_token** it);

bool ic_runtime::parse()
{
	// temporary hack
	{
		ic_token token;

		token.type = IC_TOK_LEFT_BRACE;
		_tokens.insert(_tokens.begin(), token);

		token.type = IC_TOK_RIGHT_BRACE;
		_tokens.insert(_tokens.end() - 1, token);
	}

	const ic_token* it = _tokens.data();
	_ast = produce_stmt(&it);
	return _ast.type != 0;
}

bool ic_runtime::execute()
{
	bool success = true;
	return success;
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
		ic_print_error(IC_ERR_PARSING, (**it).line, (**it).col, err_msg);

	return false;
}

ic_ast_node produce_stmt(const ic_token** it)
{
	switch ((**it).type)
	{
	case IC_TOK_LEFT_BRACE:
	{
		token_advance(it);

		ic_ast_node node;
		node.type = IC_AST_STMT_BLOCK;

		ic_ast_node** next = &node.next_right;

		while (!token_match_type(it, IC_TOK_RIGHT_BRACE) && !token_match_type(it, IC_TOK_EOF))
		{
			ic_ast_node cnode = produce_stmt(it);
			
			if (!cnode.type)
				return {};

			*next = new ic_ast_node(cnode);
			next = &(**next).next_right;
		}

		if (!token_consume(it, IC_TOK_RIGHT_BRACE, "expected '}' to close a block statement"))
			return {};

		return node;
	}
	case IC_TOK_IF:
	{
		token_advance(it);

		if (!token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after if keyword"))
			return {};

		ic_ast_node node_condition = produce_expr(it);

		if (!node_condition.type)
			return {};

		if (!token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after if condition"))
			return {};

		ic_ast_node node_if_body = produce_stmt(it);

		if (!node_if_body.type)
			return {};

		ic_ast_node node_else_body;

		if (token_consume(it, IC_TOK_ELSE))
		{
			node_else_body = produce_stmt(it);

			if (!node_else_body.type)
				return {};
		}

		node_condition.next_right = new ic_ast_node(node_if_body);
		
		ic_ast_node node;
		node.type = IC_AST_STMT_IF;
		node.next_right = new ic_ast_node(node_if_body);
		node.next_left = new ic_ast_node(node_else_body);
		return node;
	}
	case IC_TOK_WHILE:
	{
		token_advance(it);

		if (!token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after while keyword"))
			return {};

		ic_ast_node node_condition = produce_expr(it);

		if (!node_condition.type)
			return {};

		if (!token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after while condition"))
			return {};

		ic_ast_node node_body = produce_stmt(it);

		if (!node_body.type)
			return {};

		ic_ast_node node;
		node.type = IC_AST_STMT_WHILE;
		node.next_left = new ic_ast_node(node_condition);
		node.next_right = new ic_ast_node(node_body);
		return node;
	}
	case IC_TOK_FOR:
	{
		token_advance(it);

		if (!token_consume(it, IC_TOK_LEFT_PAREN, "expected '(' after for keyword"))
			return {};

		ic_ast_node node_header1 = produce_stmt_expr(it);
		if (!node_header1.type)
			return {};

		ic_ast_node node_header2 = produce_stmt_expr(it);
		if (!node_header2.type)
			return {};

		ic_ast_node node_header3 = produce_expr(it); // this one should not end with ;
		if (!node_header3.type)
			return {};

		if (!token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after for header"))
			return {};
		
		ic_ast_node node_body = produce_stmt(it);
		if (!node_body.type)
			return {};

		node_header1.next_left = new ic_ast_node(node_header2);
		node_header1.next_right = new ic_ast_node(node_header3);

		ic_ast_node node;
		node.type = IC_AST_STMT_FOR;
		node.next_left = new ic_ast_node(node_header1);
		node.next_right = new ic_ast_node(node_body);
		return node;
	}
	case IC_TOK_RETURN:
	{
		token_advance(it);

		ic_ast_node cnode;
		cnode.type = IC_AST_EXPR_PRIMARY;
		cnode.token.type = IC_TOK_NIL;

		if (!token_match_type(it, IC_TOK_SEMICOLON))
		{
			cnode = produce_expr(it);
			if (!cnode.type)
				return {};
		}

		if (!token_consume(it, IC_TOK_SEMICOLON, "expected ';' after return statement"))
			return {};

		ic_ast_node node;
		node.type = IC_AST_STMT_RETURN;
		node.next_right = new ic_ast_node(cnode);
		return node;
	}
	// end of switch stmt
	}

	return produce_stmt_expr(it);
}

ic_ast_node produce_stmt_expr(const ic_token** it)
{
	if (token_consume(it, IC_TOK_SEMICOLON))
	{
		ic_ast_node node;
		node.type = IC_AST_STMT_SEMICOLON;
		return node;
	}

	ic_ast_node node = produce_expr(it);

	if (!node.type)
		return {};

	if (!token_consume(it, IC_TOK_SEMICOLON, "expected ';' after expression"))
		return {};

	if (node.type == IC_AST_EXPR_PRIMARY)
		ic_print_error(IC_ERR_PARSING, node.token.line, node.token.col, "warning, statement has no effect");

	return node;
}

ic_ast_node produce_expr(const ic_token** it)
{
	if (token_consume(it, IC_TOK_VAR))
	{
		ic_ast_node node;
		node.type = IC_AST_STMT_VAR_DECLARATION;
		node.token = **it;

		if (!token_consume(it, IC_TOK_IDENTIFIER, "expected variable name"))
			return {};

		ic_ast_node cnode;

		if (token_consume(it, IC_TOK_EQUAL))
		{
			cnode = produce_expr_assignment(it);

			if (!cnode.type)
				return {};
		}
		// init to nil
		else
		{
			cnode.type = IC_AST_EXPR_PRIMARY;
			cnode.token.type = IC_TOK_NIL;
		}

		node.next_right = new ic_ast_node(cnode);
		return node;
	}

	return produce_expr_assignment(it);
}

// assignment is right-associative, that's why this operator is not handled in produce_expr_binary()
ic_ast_node produce_expr_assignment(const ic_token** it)
{
	ic_ast_node node_left = produce_expr_binary(it, IC_PRECDENCE_EQUAL);

	if (!node_left.type)
		return {};

	if (token_match_type(it, IC_TOK_EQUAL))
	{
		if (node_left.type != IC_AST_EXPR_PRIMARY && node_left.token.type != IC_TOK_IDENTIFIER)
		{
			ic_print_error(IC_ERR_PARSING, (**it).line, (**it).col, "left side of the assignment must be an lvalue");
			return {};
		}

		ic_ast_node node;
		node.token = **it;
		token_advance(it);

		ic_ast_node right_node = produce_expr_assignment(it);

		if (!right_node.type)
			return {};

		node.next_left = new ic_ast_node(node_left);
		node.next_right = new ic_ast_node(right_node);
		return node;
	}

	return node_left;
}

ic_ast_node produce_expr_binary(const ic_token** it, ic_op_precedence precedence)
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
	case IC_PRECDENCE_EQUAL:
	{
		target_operators[0] = IC_TOK_EQUAL;
		target_operators[1] = IC_TOK_BANG_EQUAL;
		break;
	}
	case IC_PRECEDENCE_GREATER:
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
		return produce_expr_unary(it);
	}

	ic_ast_node node = produce_expr_binary(it, ic_op_precedence(int(precedence) + 1));

	if (!node.type)
		return {};

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

		ic_ast_node new_node;
		new_node.token = **it;
		token_advance(it);

		ic_ast_node node_right = produce_expr_binary(it, precedence);

		if (!node_right.type)
			return {};

		new_node.type = IC_AST_EXPR_BINARY;
		new_node.next_left = new ic_ast_node(node);
		new_node.next_right = new ic_ast_node(node_right);
		node = new_node;
	}

	return node;
}

ic_ast_node produce_expr_unary(const ic_token** it)
{
	ic_ast_node node;
	node.token = **it;

	if (token_consume(it, IC_TOK_BANG) || token_consume(it, IC_TOK_MINUS))
	{
		ic_ast_node cnode = produce_expr(it);

		if (!cnode.type)
			return {};

		node.type = IC_AST_EXPR_UNARY;
		node.next_right = new ic_ast_node(cnode);
		return node;
	}

	return produce_expr_function_call(it);
}

ic_ast_node produce_expr_function_call(const ic_token** it)
{
	// @TODO
	return produce_expr_primary(it);
}

ic_ast_node produce_expr_primary(const ic_token** it)
{
	ic_ast_node node;

	switch ((**it).type)
	{
	case IC_TOK_FALSE:
	case IC_TOK_TRUE:
	case IC_TOK_NIL:
	case IC_TOK_NUMBER:
	case IC_TOK_STRING:
	case IC_TOK_IDENTIFIER:
	{
		node.type = IC_AST_EXPR_PRIMARY;
		node.token = **it;
		token_advance(it);
		return node;
	}

	case IC_TOK_LEFT_PAREN:
	{
		token_advance(it);
		ic_ast_node cnode = produce_expr(it);

		if (!cnode.type)
			return {};

		if (!token_consume(it, IC_TOK_RIGHT_PAREN, "expected ')' after expression"))
			return {};

		node.type = IC_AST_EXPR_GROUPING;
		node.next_right = new ic_ast_node(cnode);
		return node;
	}
	// end of switch stmt
	}

	ic_print_error(IC_ERR_PARSING, (**it).line, (**it).col, "expected expression");
	return {};
}