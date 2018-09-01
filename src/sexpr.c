#include "stdlib.h"
#include "stdarg.h"
#include "stdint.h"
typedef int64_t bool;
#define true (~((bool) 0))
#define false ((bool) 0)
#include "list.c"

struct character {
	void *list_flag;
	char character;
};

union sexpr {
	void *list_flag;
	struct _list_ _list;
	struct character character;
};

bool is_lst(union sexpr *s) {
	return s->list_flag ? true : false;
}

bool sexpr_equals(union sexpr *a, union sexpr *b) {
	if(!a->list_flag != !b->list_flag) {
		return false;
	} else if(!a->list_flag) {
		return a->character.character == b->character.character ? true : false;
	} else if(is_nil((list) a) || is_nil((list) b)) {
		return is_nil((list) a) & is_nil((list) b);
	} else {
		return sexpr_equals(fst((list) a), fst((list) b)) &
			sexpr_equals((union sexpr *) rst((list) a), (union sexpr *) rst((list) b));
	}
}

union sexpr *build_character_sexpr(char d) {
	union sexpr *c = malloc(sizeof(union sexpr));
	c->character.list_flag = NULL;
	c->character.character = d;
	return c;
}

#define char_sexpr(str, ch) \
union sexpr * _ ## str ## _() { \
	return build_character_sexpr(ch); \
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

list build_symbol_sexpr(char *str) {
	list sexprs = nil();
	for(; *str; str++) {
		append(build_character_sexpr(*str), &sexprs);
	}
	return sexprs;
}

list with_primitive(list arg) { return lst(build_symbol_sexpr("with"), arg); }
list begin_primitive(list arg) { return lst(build_symbol_sexpr("begin"), arg); }
list if_primitive(list arg) { return lst(build_symbol_sexpr("if"), arg); }
list function_primitive(list arg) { return lst(build_symbol_sexpr("function"), arg); }
list continuation_primitive(list arg) { return lst(build_symbol_sexpr("continuation"), arg); }
list literal_primitive(list arg) { return lst(build_symbol_sexpr("literal"), arg); }
list invoke_primitive(list arg) { return lst(build_symbol_sexpr("invoke"), arg); }
list jump_primitive(list arg) { return lst(build_symbol_sexpr("jump"), arg); }
