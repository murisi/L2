int after_leading_space(int l2file, int *pos, char *c) {
	do {
		int rd = myread(l2file, c, 1);
		(*pos) += rd;
		if(!rd) return 0;
	} while(isspace(*c));
	(*pos)--;
	return 1;
}

list build_expr_list(int l2file, int *pos);

list build_sigil(char *sigil, int l2file, int *pos) {
	char d;
	int rd = myread(l2file, &d, 1);
	if(rd) {
		myseek(l2file, -1, SEEK_CUR);
	}
	if(rd == 0 || isspace(d) || d == ')' || d == '}' || d == ']' || d == '(' || d == '{' || d =='[') {
		return build_symbol_sexpr(sigil);
	} else {
		list sexprs = nil();
		append(build_symbol_sexpr(sigil), &sexprs);
		append(build_expr_list(l2file, pos), &sexprs);
		return sexprs;
	}
}

list build_list(char *primitive, char delimiter, int l2file, int *pos) {
	char c;
	list sexprs = nil();
	append(build_symbol_sexpr(primitive), &sexprs);
	
	while(1) {
		int rd = after_leading_space(l2file, pos, &c);
		if(rd && c == delimiter) {
			(*pos) ++;
			break;
		} else if(rd) {
			myseek(l2file, -1, SEEK_CUR);
			(*pos)--;
		}
		append(build_expr_list(l2file, pos), &sexprs);
	}
	return sexprs;
}

jmp_buf *build_expr_list_handler;

list build_expr_list(int l2file, int *pos) {
	char c;
	int rd = myread(l2file, &c, 1);
	(*pos) += rd;
	
	if(rd == 0 || isspace(c) || c == ')' || c == '}' || c == ']') {
		thelongjmp(*build_expr_list_handler, make_unexpected_character(c, *pos));
	} else if(c == '(') {
		return rst(build_list("()", ')', l2file, pos));
	} else if(c == '{') {
		return build_list("jump", '}', l2file, pos);
	} else if(c == '[') {
		return build_list("invoke", ']', l2file, pos);
	} else if(c == '$') {
		return build_sigil("$", l2file, pos);
	} else if(c == '&') {
		return build_sigil("&", l2file, pos);
	} else if(c == ',') {
		return build_sigil(",", l2file, pos);
	} else if(c == '`') {
		return build_sigil("`", l2file, pos);
	} else {
		list l = nil();
		do {
			append(build_character_sexpr(c), &l);
			rd = myread(l2file, &c, 1);
			(*pos) += rd;
		} while(rd && !isspace(c) && c != '(' && c != ')' && c != '{' && c != '}' && c != '[' && c != ']');
		
		myseek(l2file, -1, SEEK_CUR);
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
