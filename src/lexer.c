enum s_expression_type {
	_list,
	character
};

struct _s_expression_ {
	struct _list_ _list; //void * = s_expression 
	enum s_expression_type type;
	char character;
};

typedef struct _s_expression_* s_expression;

bool is_lst(s_expression s) {
	return s->type == _list ? true : false;
}

s_expression sexpr(list l) {
	s_expression d = malloc(sizeof(struct _s_expression_));
	d->type = _list;
	d->_list = *l;
	return d;
}

#define char_sexpr(str, ch) \
\
bool is_ ## str(s_expression s) { \
	return (s->type == character && s->character == ch) ? true : false; \
} \
\
s_expression _ ## str ## _() { \
	s_expression sexpr = calloc(1, sizeof(struct _s_expression_)); \
	sexpr->type = character; \
	sexpr->character = ch; \
	return sexpr; \
}

#define an_sexpr(ch) char_sexpr(ch, #ch [0])

char_sexpr(exclamation_mark, '!'); char_sexpr(double_quotation, '"'); char_sexpr(dollar_sign, '$'); char_sexpr(percent, '%');
char_sexpr(ampersand, '&'); char_sexpr(apostrophe, '\''); char_sexpr(asterisk, '*'); char_sexpr(plus_sign, '+');
char_sexpr(comma, ','); char_sexpr(hyphen, '-'); char_sexpr(period, '.'); char_sexpr(slash, '/'); an_sexpr(0); an_sexpr(1);
an_sexpr(2); an_sexpr(3); an_sexpr(4); an_sexpr(5); an_sexpr(6); an_sexpr(7); an_sexpr(8); an_sexpr(9); char_sexpr(colon, ':');
char_sexpr(semicolon, ';'); char_sexpr(less_than_sign, '<'); char_sexpr(equal_sign, '='); char_sexpr(greater_than_sign, '>');
char_sexpr(question_mark, '?'); an_sexpr(A); an_sexpr(B); an_sexpr(C); an_sexpr(D); an_sexpr(E); an_sexpr(F); an_sexpr(G);
an_sexpr(H); an_sexpr(I); an_sexpr(J); an_sexpr(K); an_sexpr(L); an_sexpr(M); an_sexpr(N); an_sexpr(O); an_sexpr(P); an_sexpr(Q);
an_sexpr(R); an_sexpr(S); an_sexpr(T); an_sexpr(U); an_sexpr(V); an_sexpr(W); an_sexpr(X); an_sexpr(Y); an_sexpr(Z);
char_sexpr(backslash, '\\'); char_sexpr(caret, '^'); char_sexpr(underscore, '_'); char_sexpr(backquote, '`'); an_sexpr(a);
an_sexpr(b); an_sexpr(c); an_sexpr(d); an_sexpr(e); an_sexpr(f); an_sexpr(g); an_sexpr(h); an_sexpr(i); an_sexpr(j); an_sexpr(k);
an_sexpr(l); an_sexpr(m); an_sexpr(n); an_sexpr(o); an_sexpr(p); an_sexpr(q); an_sexpr(r); an_sexpr(s); an_sexpr(t); an_sexpr(u);
an_sexpr(v); an_sexpr(w); an_sexpr(x); an_sexpr(y); an_sexpr(z); char_sexpr(vertical_bar, '|'); char_sexpr(tilde, '~');

int after_leading_space(FILE *l2file) {
	int c;
	do {
		c = getc(l2file);
	} while(isspace(c));
	
	return c;
}

s_expression build_symbol_sexpr(char *str) {
	list sexprs = calloc(1, sizeof(struct _list_));
	
	for(; *str; str++) {
		s_expression _character = malloc(sizeof(struct _s_expression_));
		_character->type = character;
		_character->character = *str;
		append(_character, &sexprs);
	}
	return sexpr(sexprs);
}

jmp_buf *build_expr_list_handler;

list build_expr_list(FILE *l2file) {
	int c = getc(l2file);
	
	if(c == EOF || isspace(c) || c == ')' || c == '}' || c == ']') {
		longjmp(*build_expr_list_handler, (int) make_unexpected_character(c, ftell(l2file)));
	} else if(c == '(') {
		list sexprs = nil();
		
		while((c = after_leading_space(l2file)) != ')') {
			ungetc(c, l2file);
			append(sexpr(build_expr_list(l2file)), &sexprs);
		}
		return sexprs;
	} else if(c == '{') {
		list sexprs = nil();
		append(build_symbol_sexpr("continue"), &sexprs);
		
		while((c = after_leading_space(l2file)) != '}') {
			ungetc(c, l2file);
			append(sexpr(build_expr_list(l2file)), &sexprs);
		}
		return sexprs;
	} else if(c == '[') {
		list sexprs = nil();
		append(build_symbol_sexpr("invoke"), &sexprs);
		
		while((c = after_leading_space(l2file)) != ']') {
			ungetc(c, l2file);
			append(sexpr(build_expr_list(l2file)), &sexprs);
		}
		return sexprs;
	} else {
		list l = nil();
		do {
			s_expression _character = malloc(sizeof(struct _s_expression_));
			_character->type = character;
			_character->character = c;
			append(_character, &l);
			
			c = getc(l2file);
		} while(c != EOF && !isspace(c) && c != '(' && c != ')' && c != '{' && c != '}' && c != '[' && c != ']');
		
		ungetc(c, l2file);
		return l;
	}
}

bool is_string(list d) {
	s_expression t;
	foreach(t, d) {
		if(t->type != character) {
			return false;
		}
	}
	return true;
}

int string_length(list d) {
	s_expression t;
	int length = 0;
	
	foreach(t, d) {
		length++;
	}
	return length;
}

char *to_string(list d) {
	char *str = calloc(string_length(d) + 1, sizeof(char));
	int i = 0;
	
	s_expression t;
	foreach(t, d) {
		str[i++] = t->character;
	}
	str[i] = '\0';
	return str;
}

void print_expr_list(list d) {
	printf("(");
	if(is_nil(d)) {
		printf(")");
		return;
	} else {
		s_expression _fst = fst(d);
		if(_fst->type == character) {
			printf("%c", _fst->character);
		} else {
			print_expr_list((list) _fst);
		}
		printf(" . ");
		print_expr_list(rst(d));
		printf(")");
	}	
}
