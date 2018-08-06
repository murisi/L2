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
		thelongjmp(*build_expr_list_handler, make_unexpected_character(c, ftell(l2file)));
	} else if(c == '(') {
		list sexprs = nil();
		
		while((c = after_leading_space(l2file)) != ')') {
			ungetc(c, l2file);
			append(sexpr(build_expr_list(l2file)), &sexprs);
		}
		return sexprs;
	} else if(c == '{') {
		list sexprs = nil();
		append(build_symbol_sexpr("jump"), &sexprs);
		
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
