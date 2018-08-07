#include "stdlib.h"
#include "stdarg.h"

#define true (~((int) 0))
#define false ((int) 0)
typedef int bool;
#include "list.c"

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

bool sexpr_equals(s_expression a, s_expression b) {
	if(a->type != b->type) {
		return false;
	} else if(a->type == character) {
		return a->character == b->character ? true : false;
	} else if(is_nil((list) a) || is_nil((list) b)) {
		return is_nil((list) a) & is_nil((list) b);
	} else {
		return sexpr_equals(fst((list) a), fst((list) b)) &&
			sexpr_equals((s_expression) rst((list) a), (s_expression) rst((list) b));
	}
}

#define char_sexpr(str, ch) \
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

list with_primitive(list arg) { return lst(build_symbol_sexpr("with"), arg); }
list begin_primitive(list arg) { return lst(build_symbol_sexpr("begin"), arg); }
list if_primitive(list arg) { return lst(build_symbol_sexpr("if"), arg); }
list function_primitive(list arg) { return lst(build_symbol_sexpr("function"), arg); }
list continuation_primitive(list arg) { return lst(build_symbol_sexpr("continuation"), arg); }
list b_primitive(list arg) { return lst(build_symbol_sexpr("b"), arg); }
list invoke_primitive(list arg) { return lst(build_symbol_sexpr("invoke"), arg); }
list jump_primitive(list arg) { return lst(build_symbol_sexpr("jump"), arg); }
