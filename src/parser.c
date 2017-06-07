list with_continuation_primitive(list arg) { return lst(build_symbol_sexpr("with-continuation"), arg); }
list begin_primitive(list arg) { return lst(build_symbol_sexpr("begin"), arg); }
list if_primitive(list arg) { return lst(build_symbol_sexpr("if"), arg); }
list function_primitive(list arg) { return lst(build_symbol_sexpr("function"), arg); }
list make_continuation_primitive(list arg) { return lst(build_symbol_sexpr("make-continuation"), arg); }
list b_primitive(list arg) { return lst(build_symbol_sexpr("b"), arg); }
list invoke_primitive(list arg) { return lst(build_symbol_sexpr("invoke"), arg); }
list continue_primitive(list arg) { return lst(build_symbol_sexpr("continue"), arg); }

jmp_buf *build_syntax_tree_handler;

union expression *build_syntax_tree(list d, union expression *parent) {
	union expression *s = calloc(1, sizeof(union expression));
	s->base.parent = parent;
	
	if(is_string(d)) {
		char *str = to_string(d);
		s->reference.type = reference;
		s->reference.name = str;
	} else if(!strcmp(to_string(fst(d)), "with-continuation")) {
		if(length(d) != 3) {
			longjmp(*build_syntax_tree_handler, (int) make_special_form(d, NULL));
		} else if(!is_string(frst(d))) {
			longjmp(*build_syntax_tree_handler, (int) make_special_form(d, frst(d)));
		}
	
		s->withc.type = withc;
		s->withc.reference = build_syntax_tree(frst(d), s);
		s->withc.expression = build_syntax_tree(frrst(d), s);
		s->withc.parameter = make_list(1, NULL);
	} else if(!strcmp(to_string(fst(d)), "begin")) {
		s->begin.type = begin;
		s->begin.expressions = nil();
	
		list t = rst(d);
		s_expression v;
		foreach(v, t) {
			append(build_syntax_tree((list) v, s), &(s->begin.expressions));
		}
	} else if(!strcmp(to_string(fst(d)), "if")) {
		if(length(d) != 4) {
			longjmp(*build_syntax_tree_handler, (int) make_special_form(d, NULL));
		}
	
		s->_if.type = _if;
		s->_if.condition = build_syntax_tree(frst(d), s);
		s->_if.consequent = build_syntax_tree(frrst(d), s);
		s->_if.alternate = build_syntax_tree(frrrst(d), s);
	} else if(!strcmp(to_string(fst(d)), "function") || !strcmp(to_string(fst(d)), "make-continuation")) {
		if(length(d) != 4) {
			longjmp(*build_syntax_tree_handler, (int) make_special_form(d, NULL));
		} else if(!is_string(frst(d))) {
			longjmp(*build_syntax_tree_handler, (int) make_special_form(d, frst(d)));
		} else if(!is_nil(frrst(d)) && is_string(frrst(d))) {
			longjmp(*build_syntax_tree_handler, (int) make_special_form(d, frrst(d)));
		}
		
		s->function.type = !strcmp(to_string(fst(d)), "function") ? function : makec;
		put(s, function.reference, build_syntax_tree(frst(d), s));
		
		if(s->function.type == function) {
			s->function.locals = nil();
		}
		s->function.parameters = nil();
		s_expression v;
		foreach(v, frrst(d)) {
			if(!is_string((list) v)) {
				longjmp(*build_syntax_tree_handler, (int) make_special_form(d, (list) v));
			}
			append(build_syntax_tree((list) v, s), &(s->function.parameters));
		}
	
		s->function.expression = build_syntax_tree(frrrst(d), s);
	} else if(!strcmp(to_string(fst(d)), "b")) {
		char *str;
		if(length(d) != 2) {
			longjmp(*build_syntax_tree_handler, (int) make_special_form(d, NULL));
		} else if(!is_string(frst(d)) || strlen(str = to_string(frst(d))) != 32) {
			longjmp(*build_syntax_tree_handler, (int) make_special_form(d, frst(d)));
		}
	
		s->constant.type = constant;
		s->constant.value = 0;
		int i;
		for(i = 0; i < strlen(str); i++) {
			if(str[strlen(str) - i - 1] == '1') {
				s->constant.value |= (1 << i);
			} else if(str[strlen(str) - i - 1] != '0') {
				longjmp(*build_syntax_tree_handler, (int) make_special_form(d, frst(d)));
			}
		}
	} else if(!strcmp(to_string(fst(d)), "invoke") || !strcmp(to_string(fst(d)), "continue")) {
		if(length(d) == 1) {
			longjmp(*build_syntax_tree_handler, (int) make_special_form(d, NULL));
		}
	
		s->invoke.type = !strcmp(to_string(fst(d)), "invoke") ? invoke : _continue;
		s->invoke.reference = build_syntax_tree(frst(d), s);
	
		s_expression v;
		s->invoke.arguments = nil();
		foreach(v, rrst(d)) {
			append(build_syntax_tree((list) v, s), &s->invoke.arguments);
		}
	} else {
		union expression *expander_container = make_function("");
		char *expander_container_name = expander_container->function.reference->reference.name;
		union expression *expander = build_syntax_tree(fst(d), expander_container);
		put(expander_container, function.expression, expander);
		char *sofn = dynamic_load(make_list(1, expander_container), build_syntax_tree_handler);
		void *handle = dlopen(sofn, RTLD_NOW | RTLD_LOCAL);
		if(!handle) {
			remove(sofn);
			longjmp(*build_syntax_tree_handler, (int) make_environment(cprintf("%s", dlerror())));
		}
		list (*(*macro_container)())(list) = dlsym(handle, expander_container_name);
		list (*macro)(list) = macro_container();
		list transformed = macro(rst(d));
		dlclose(handle);
		remove(sofn);
		return build_syntax_tree(transformed, parent);
	}
	return s;
}