struct character {
	void *list_flag;
	char character;
};

bool char_equals(struct character *a, struct character *b) {
	return a->character == b->character;
}

#define char_init(ch) {{ .list_flag = NULL, .character = ch }}

struct character chars[][1] = {
	{0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
	{0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, char_init('!'),
	char_init('"'), char_init('#'), char_init('$'), char_init('%'), char_init('&'), char_init('\''), {0}, {0}, char_init('*'),
	char_init('+'), char_init(','), char_init('-'), char_init('.'), char_init('/'), char_init('0'), char_init('1'),
	char_init('2'), char_init('3'), char_init('4'), char_init('5'), char_init('6'), char_init('7'), char_init('8'),
	char_init('9'), char_init(':'), char_init(';'), char_init('<'), char_init('='), char_init('>'), char_init('?'),
	char_init('@'), char_init('A'), char_init('B'), char_init('C'), char_init('D'), char_init('E'), char_init('F'),
	char_init('G'), char_init('H'), char_init('I'), char_init('J'), char_init('K'), char_init('L'), char_init('M'),
	char_init('N'), char_init('O'), char_init('P'), char_init('Q'), char_init('R'), char_init('S'), char_init('T'),
	char_init('U'), char_init('V'), char_init('W'), char_init('X'), char_init('Y'), char_init('Z'), {0}, char_init('\\'),
	{0}, char_init('^'), char_init('_'), char_init('`'), char_init('a'), char_init('b'), char_init('c'), char_init('d'),
	char_init('e'), char_init('f'), char_init('g'), char_init('h'), char_init('i'), char_init('j'), char_init('k'),
	char_init('l'), char_init('m'), char_init('n'), char_init('o'), char_init('p'), char_init('q'), char_init('r'),
	char_init('s'), char_init('t'), char_init('u'), char_init('v'), char_init('w'), char_init('x'), char_init('y'),
	char_init('z'), {0}, char_init('|'), {0}, char_init('~'), {0}
};

#define _char(d) (chars[d])

list build_symbol(char *str, region r) {
	list sexprs = nil;
	for(; *str; str++) {
		append(_char(*str), &sexprs, r);
	}
	return sexprs;
}

int after_leading_space(char *l2src, int l2src_sz, int *pos) {
	for(; *pos < l2src_sz && isspace(l2src[*pos]); (*pos)++);
	return l2src_sz - *pos;
}

list build_fragment(char *l2src, int l2src_sz, int *pos, region r, jumpbuf *handler);

list build_sigilled_symbol(char *sigil, char *l2src, int l2src_sz, int *pos, region r, jumpbuf *handler) {
	if(l2src_sz == *pos) {
		return build_symbol(sigil, r);
	}
	char d = l2src[*pos];
	if(isspace(d) || d == ')' || d == '}' || d == ']' || d == '(' || d == '{' || d =='[') {
		return build_symbol(sigil, r);
	} else {
		list sexprs = nil;
		append(build_symbol(sigil, r), &sexprs, r);
		append(build_fragment(l2src, l2src_sz, pos, r, handler), &sexprs, r);
		return sexprs;
	}
}

list build_fragment_list(char *primitive, char delimiter, char *l2src, int l2src_sz, int *pos, region r, jumpbuf *handler) {
	list sexprs = nil;
	append(build_symbol(primitive, r), &sexprs, r);
	
	while(1) {
		int rem = after_leading_space(l2src, l2src_sz, pos);
		if(rem && l2src[*pos] == delimiter) {
			(*pos) ++;
			break;
		}
		append(build_fragment(l2src, l2src_sz, pos, r, handler), &sexprs, r);
	}
	return sexprs;
}

list build_fragment(char *l2src, int l2src_sz, int *pos, region r, jumpbuf *handler) {
	if(l2src_sz == *pos) {
		throw_unexpected_character(0, *pos, handler);
	}
	char c = l2src[*pos];
	if(isspace(c) || c == ')' || c == '}' || c == ']') {
		throw_unexpected_character(c, *pos, handler);
	}
	(*pos)++;
	if(c == '(') {
		return build_fragment_list("expression", ')', l2src, l2src_sz, pos, r, handler)->rst;
	} else if(c == '{') {
		return build_fragment_list("jump", '}', l2src, l2src_sz, pos, r, handler);
	} else if(c == '[') {
		return build_fragment_list("invoke", ']', l2src, l2src_sz, pos, r, handler);
	} else if(c == '$') {
		return build_sigilled_symbol("$", l2src, l2src_sz, pos, r, handler);
	} else if(c == '#') {
		return build_sigilled_symbol("#", l2src, l2src_sz, pos, r, handler);
	} else if(c == ',') {
		return build_sigilled_symbol(",", l2src, l2src_sz, pos, r, handler);
	} else if(c == '`') {
		return build_sigilled_symbol("`", l2src, l2src_sz, pos, r, handler);
	} else {
		list l = nil;
		do {
			append(_char(c), &l, r);
			if(*pos == l2src_sz) return l;
			c = l2src[(*pos)++];
		} while(!isspace(c) && c != '(' && c != ')' && c != '{' && c != '}' && c != '[' && c != ']');
		(*pos)--;
		return l;
	}
}

bool is_symbol(list d) {
	return length(d) && !((struct character *) d->fst)->list_flag;
}

char *to_string(list d, region r) {
	char *str = buffer_alloc(r, (length(d) + 1) * sizeof(char));
	int i = 0;
	
	struct character *t;
	foreach(t, d) {
		str[i++] = t->character;
	}
	str[i] = '\0';
	return str;
}

list copy_fragment(list l, region r) {
	list c = nil;
	if(is_symbol(l)) {
		struct character *s;
		foreach(s, l) {
			append(_char(s->character), &c, r);
		}
	} else {
		list s;
		foreach(s, l) {
			append(copy_fragment(s, r), &c, r);
		}
	}
	return c;
}

void print_fragment(list d) {
	write_str(STDOUT, "(");
	if(!is_nil(d)) {
		struct character *_fst = d->fst;
		if(_fst->list_flag) {
			print_fragment((list) _fst);
		} else {
			write_char(STDOUT, _fst->character);
		}
		write_str(STDOUT, " . ");
		print_fragment(d->rst);
	}
	write_str(STDOUT, ")");	
}
