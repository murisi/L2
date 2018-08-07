int after_leading_space(FILE *l2file) {
	int c;
	do {
		c = getc(l2file);
	} while(isspace(c));
	
	return c;
}

jmp_buf *build_expr_list_handler;

list build_expr_list(FILE *l2file) {
	int c = getc(l2file);
	
	if(c == EOF || isspace(c) || c == ')' || c == '}' || c == ']') {
		thelongjmp(*build_expr_list_handler, make_unexpected_character(c, ftell(l2file)));
	} else if(c == '(') {
		list sexprs = nil();
		
		while((c = after_leading_space(l2file)) != ')') {
			ungetc(c, l2file);
			append(build_expr_list(l2file), &sexprs);
		}
		return sexprs;
	} else if(c == '{') {
		list sexprs = nil();
		append(build_symbol_sexpr("jump"), &sexprs);
		
		while((c = after_leading_space(l2file)) != '}') {
			ungetc(c, l2file);
			append(build_expr_list(l2file), &sexprs);
		}
		return sexprs;
	} else if(c == '[') {
		list sexprs = nil();
		append(build_symbol_sexpr("invoke"), &sexprs);
		
		while((c = after_leading_space(l2file)) != ']') {
			ungetc(c, l2file);
			append(build_expr_list(l2file), &sexprs);
		}
		return sexprs;
	} else if(c == '$') {
		char d = getc(l2file);
		if(d == EOF || isspace(d) || d == ')' || d == '}' || d == ']' || d == '(' || d == '{' || d =='[') {
			ungetc(d, l2file);
			return build_symbol_sexpr("$");
		} else {
			ungetc(d, l2file);
			list sexprs = nil();
			append(build_symbol_sexpr("$"), &sexprs);
			append(build_expr_list(l2file), &sexprs);
			return sexprs;
		}
	} else if(c == '&') {
		char d = getc(l2file);
		if(d == EOF || isspace(d) || d == ')' || d == '}' || d == ']' || d == '(' || d == '{' || d =='[') {
			ungetc(d, l2file);
			return build_symbol_sexpr("&");
		} else {
			ungetc(d, l2file);
			list sexprs = nil();
			append(build_symbol_sexpr("&"), &sexprs);
			append(build_expr_list(l2file), &sexprs);
			return sexprs;
		}
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
			printf("%c", _fst->character);
		}
		printf(" . ");
		print_expr_list(rst(d));
		printf(")");
	}	
}
