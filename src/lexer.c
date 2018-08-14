int after_leading_space(FILE *l2file) {
	int c;
	do {
		c = getc(l2file);
	} while(isspace(c));
	
	return c;
}

list build_expr_list(FILE *l2file);

list build_sigil(char *sigil, FILE *l2file) {
	char d = getc(l2file);
	ungetc(d, l2file);
	if(d == EOF || isspace(d) || d == ')' || d == '}' || d == ']' || d == '(' || d == '{' || d =='[') {
		return build_symbol_sexpr(sigil);
	} else {
		list sexprs = nil();
		append(build_symbol_sexpr(sigil), &sexprs);
		append(build_expr_list(l2file), &sexprs);
		return sexprs;
	}
}

list build_list(char *primitive, char delimiter, FILE *l2file) {
	char c;
	list sexprs = nil();
	append(build_symbol_sexpr(primitive), &sexprs);
	
	while((c = after_leading_space(l2file)) != delimiter) {
		ungetc(c, l2file);
		append(build_expr_list(l2file), &sexprs);
	}
	return sexprs;
}

jmp_buf *build_expr_list_handler;

list build_expr_list(FILE *l2file) {
	int c = getc(l2file);
	
	if(c == EOF || isspace(c) || c == ')' || c == '}' || c == ']') {
		thelongjmp(*build_expr_list_handler, make_unexpected_character(c, ftell(l2file)));
	} else if(c == '(') {
		return rst(build_list("()", ')', l2file));
	} else if(c == '{') {
		return build_list("jump", '}', l2file);
	} else if(c == '[') {
		return build_list("invoke", ']', l2file);
	} else if(c == '$') {
		return build_sigil("$", l2file);
	} else if(c == '&') {
		return build_sigil("&", l2file);
	} else if(c == ',') {
		return build_sigil(",", l2file);
	} else if(c == '`') {
		return build_sigil("`", l2file);
	} else {
		list l = nil();
		do {
			append(build_character_sexpr(c), &l);
			c = getc(l2file);
		} while(c != EOF && !isspace(c) && c != '(' && c != ')' && c != '{' && c != '}' && c != '[' && c != ']');
		
		ungetc(c, l2file);
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
