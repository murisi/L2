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

list copy_sexpr_list(list l, region r) {
	list c = nil;
	union sexpr *s;
	foreach(s, l) {
		if(is_lst(s)) {
			append(copy_sexpr_list((list) s, r), &c, r);
		} else {
			union sexpr *t = region_alloc(r, sizeof(union sexpr));
			t->character = s->character;
			append(t, &c, r);
		}
	}
	return c;
}

bool char_equals(struct character *a, struct character *b) {
	return a->character == b->character ? true : false;
}

#define char_sexpr(ch) {{ .character = { .list_flag = NULL, .character = ch }}}

union sexpr character_sexprs[][1] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, char_sexpr('!'),
	char_sexpr('"'), char_sexpr('#'), char_sexpr('$'), char_sexpr('%'), char_sexpr('&'), char_sexpr('\''), 0, 0, char_sexpr('*'),
	char_sexpr('+'), char_sexpr(','), char_sexpr('-'), char_sexpr('.'), char_sexpr('/'), char_sexpr('0'), char_sexpr('1'),
	char_sexpr('2'), char_sexpr('3'), char_sexpr('4'), char_sexpr('5'), char_sexpr('6'), char_sexpr('7'), char_sexpr('8'),
	char_sexpr('9'), char_sexpr(':'), char_sexpr(';'), char_sexpr('<'), char_sexpr('='), char_sexpr('>'), char_sexpr('?'),
	char_sexpr('@'), char_sexpr('A'), char_sexpr('B'), char_sexpr('C'), char_sexpr('D'), char_sexpr('E'), char_sexpr('F'),
	char_sexpr('G'), char_sexpr('H'), char_sexpr('I'), char_sexpr('J'), char_sexpr('K'), char_sexpr('L'), char_sexpr('M'),
	char_sexpr('N'), char_sexpr('O'), char_sexpr('P'), char_sexpr('Q'), char_sexpr('R'), char_sexpr('S'), char_sexpr('T'),
	char_sexpr('U'), char_sexpr('V'), char_sexpr('W'), char_sexpr('X'), char_sexpr('Y'), char_sexpr('Z'), 0, char_sexpr('\\'),
	0, char_sexpr('^'), char_sexpr('_'), char_sexpr('`'), char_sexpr('a'), char_sexpr('b'), char_sexpr('c'), char_sexpr('d'),
	char_sexpr('e'), char_sexpr('f'), char_sexpr('g'), char_sexpr('h'), char_sexpr('i'), char_sexpr('j'), char_sexpr('k'),
	char_sexpr('l'), char_sexpr('m'), char_sexpr('n'), char_sexpr('o'), char_sexpr('p'), char_sexpr('q'), char_sexpr('r'),
	char_sexpr('s'), char_sexpr('t'), char_sexpr('u'), char_sexpr('v'), char_sexpr('w'), char_sexpr('x'), char_sexpr('y'),
	char_sexpr('z'), 0, char_sexpr('|'), 0, char_sexpr('~'), 0
};

union sexpr *build_character_sexpr(char d) {
	return character_sexprs[d];
}

list build_symbol_sexpr(char *str, region r) {
	list sexprs = nil;
	for(; *str; str++) {
		append(build_character_sexpr(*str), &sexprs, r);
	}
	return sexprs;
}

int after_leading_space(char *l2src, int l2src_sz, int *pos) {
	for(; *pos < l2src_sz && isspace(l2src[*pos]); (*pos)++);
	return l2src_sz - *pos;
}

list build_expr_list(char *l2src, int l2src_sz, int *pos, region r, jumpbuf *handler);

list build_sigil(char *sigil, char *l2src, int l2src_sz, int *pos, region r, jumpbuf *handler) {
	if(l2src_sz == *pos) {
		return build_symbol_sexpr(sigil, r);
	}
	char d = l2src[*pos];
	if(isspace(d) || d == ')' || d == '}' || d == ']' || d == '(' || d == '{' || d =='[') {
		return build_symbol_sexpr(sigil, r);
	} else {
		list sexprs = nil;
		append(build_symbol_sexpr(sigil, r), &sexprs, r);
		append(build_expr_list(l2src, l2src_sz, pos, r, handler), &sexprs, r);
		return sexprs;
	}
}

list build_list(char *primitive, char delimiter, char *l2src, int l2src_sz, int *pos, region r, jumpbuf *handler) {
	list sexprs = nil;
	append(build_symbol_sexpr(primitive, r), &sexprs, r);
	
	while(1) {
		int rem = after_leading_space(l2src, l2src_sz, pos);
		if(rem && l2src[*pos] == delimiter) {
			(*pos) ++;
			break;
		}
		append(build_expr_list(l2src, l2src_sz, pos, r, handler), &sexprs, r);
	}
	return sexprs;
}

list build_expr_list(char *l2src, int l2src_sz, int *pos, region r, jumpbuf *handler) {
	if(l2src_sz == *pos) {
		throw_unexpected_character(0, *pos, handler);
	}
	char c = l2src[*pos];
	if(isspace(c) || c == ')' || c == '}' || c == ']') {
		throw_unexpected_character(c, *pos, handler);
	}
	(*pos)++;
	if(c == '(') {
		return build_list("expression", ')', l2src, l2src_sz, pos, r, handler)->rst;
	} else if(c == '{') {
		return build_list("jump", '}', l2src, l2src_sz, pos, r, handler);
	} else if(c == '[') {
		return build_list("invoke", ']', l2src, l2src_sz, pos, r, handler);
	} else if(c == '$') {
		return build_sigil("$", l2src, l2src_sz, pos, r, handler);
	} else if(c == '&') {
		return build_sigil("&", l2src, l2src_sz, pos, r, handler);
	} else if(c == ',') {
		return build_sigil(",", l2src, l2src_sz, pos, r, handler);
	} else if(c == '`') {
		return build_sigil("`", l2src, l2src_sz, pos, r, handler);
	} else {
		list l = nil;
		do {
			append(build_character_sexpr(c), &l, r);
			if(*pos == l2src_sz) return l;
			c = l2src[(*pos)++];
		} while(!isspace(c) && c != '(' && c != ')' && c != '{' && c != '}' && c != '[' && c != ']');
		(*pos)--;
		return l;
	}
}

bool is_string(list d) {
	union sexpr *t;
	foreach(t, d) {
		if(is_lst(t)) {
			return false;
		}
	}
	return true;
}

char *to_string(list d, region r) {
	char *str = region_alloc(r, (length(d) + 1) * sizeof(char));
	int i = 0;
	
	union sexpr *t;
	foreach(t, d) {
		str[i++] = t->character.character;
	}
	str[i] = '\0';
	return str;
}

void print_expr_list(list d) {
	write_str(STDOUT, "(");
	if(!is_nil(d)) {
		union sexpr *_fst = d->fst;
		if(is_lst(_fst)) {
			print_expr_list((list) _fst);
		} else {
			write_char(STDOUT, _fst->character.character);
		}
		write_str(STDOUT, " . ");
		print_expr_list(d->rst);
	}
	write_str(STDOUT, ")");	
}
