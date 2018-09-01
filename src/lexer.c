int after_leading_space(char *l2src, int l2src_sz, int *pos) {
	for(; *pos < l2src_sz && isspace(l2src[*pos]); (*pos)++);
	return l2src_sz - *pos;
}

list build_expr_list(char *l2src, int l2src_sz, int *pos);

list build_sigil(char *sigil, char *l2src, int l2src_sz, int *pos) {
	if(l2src_sz == *pos) {
		return build_symbol_sexpr(sigil);
	}
	char d = l2src[*pos];
	if(isspace(d) || d == ')' || d == '}' || d == ']' || d == '(' || d == '{' || d =='[') {
		return build_symbol_sexpr(sigil);
	} else {
		list sexprs = nil();
		append(build_symbol_sexpr(sigil), &sexprs);
		append(build_expr_list(l2src, l2src_sz, pos), &sexprs);
		return sexprs;
	}
}

list build_list(char *primitive, char delimiter, char *l2src, int l2src_sz, int *pos) {
	char c;
	list sexprs = nil();
	append(build_symbol_sexpr(primitive), &sexprs);
	
	while(1) {
		int rem = after_leading_space(l2src, l2src_sz, pos);
		if(rem && l2src[*pos] == delimiter) {
			(*pos) ++;
			break;
		}
		append(build_expr_list(l2src, l2src_sz, pos), &sexprs);
	}
	return sexprs;
}

jmp_buf *build_expr_list_handler;

list build_expr_list(char *l2src, int l2src_sz, int *pos) {
	if(l2src_sz == *pos) {
		thelongjmp(*build_expr_list_handler, make_unexpected_character(0, *pos));
	}
	char c = l2src[*pos];
	if(isspace(c) || c == ')' || c == '}' || c == ']') {
		thelongjmp(*build_expr_list_handler, make_unexpected_character(c, *pos));
	}
	(*pos)++;
	if(c == '(') {
		return rst(build_list("()", ')', l2src, l2src_sz, pos));
	} else if(c == '{') {
		return build_list("jump", '}', l2src, l2src_sz, pos);
	} else if(c == '[') {
		return build_list("invoke", ']', l2src, l2src_sz, pos);
	} else if(c == '$') {
		return build_sigil("$", l2src, l2src_sz, pos);
	} else if(c == '&') {
		return build_sigil("&", l2src, l2src_sz, pos);
	} else if(c == ',') {
		return build_sigil(",", l2src, l2src_sz, pos);
	} else if(c == '`') {
		return build_sigil("`", l2src, l2src_sz, pos);
	} else {
		list l = nil();
		do {
			append(build_character_sexpr(c), &l);
			if(*pos == l2src_sz) break;
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

char *to_string(list d) {
	char *str = calloc(length(d) + 1, sizeof(char));
	int i = 0;
	
	union sexpr *t;
	foreach(t, d) {
		str[i++] = t->character.character;
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
		union sexpr *_fst = fst(d);
		if(is_lst(_fst)) {
			print_expr_list((list) _fst);
		} else {
			printf("%c", _fst->character.character);
		}
		printf(" . ");
		print_expr_list(rst(d));
		printf(")");
	}	
}
