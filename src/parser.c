list with_continuation_primitive(list arg) { return lst(build_symbol_sexpr("with-continuation"), arg); }
list begin_primitive(list arg) { return lst(build_symbol_sexpr("begin"), arg); }
list if_primitive(list arg) { return lst(build_symbol_sexpr("if"), arg); }
list function_primitive(list arg) { return lst(build_symbol_sexpr("function"), arg); }
list make_continuation_primitive(list arg) { return lst(build_symbol_sexpr("make-continuation"), arg); }
list b_primitive(list arg) { return lst(build_symbol_sexpr("b"), arg); }
list invoke_primitive(list arg) { return lst(build_symbol_sexpr("invoke"), arg); }
list continue_primitive(list arg) { return lst(build_symbol_sexpr("continue"), arg); }

struct expansion {
	union expression *function;
	list argument;
	union expression **dest;
};

#define build_syntax_tree_under(x, y, z) { \
	union expression *_build_syntax_tree_under_u = z; \
	union expression **_build_syntax_tree_under_v = y; \
	build_syntax_tree(x, _build_syntax_tree_under_v); \
	(*_build_syntax_tree_under_v)->base.parent = _build_syntax_tree_under_u; \
}

list *list_at(int index, list *l) {
	while(!is_nil(*l) && index > 0) {
		l = &(*l)->rst;
		index--;
	}
	if(is_nil(*l)) {
		while(true) {
			*l = lst(nil(), nil());
			if(index == 0) break;
			l = &(*l)->rst;
			index--;
		}
	}
	return (list *) &(*l)->fst;
}

jmp_buf *build_syntax_tree_handler;
list build_syntax_tree_expansion_lists;

void build_syntax_tree(list d, union expression **s) {
	static int expansion_depth = 0;
	*s = calloc(1, sizeof(union expression));
	
	if(is_string(d)) {
		char *str = to_string(d);
		(*s)->reference.type = reference;
		(*s)->reference.name = str;
	} else if(!strcmp(to_string(fst(d)), "with-continuation")) {
		if(length(d) != 3) {
			longjmp(*build_syntax_tree_handler, (int) make_special_form(d, NULL));
		} else if(!is_string(frst(d))) {
			longjmp(*build_syntax_tree_handler, (int) make_special_form(d, frst(d)));
		}
	
		(*s)->withc.type = withc;
		build_syntax_tree_under(frst(d), &(*s)->withc.reference, *s);
		build_syntax_tree_under(frrst(d), &(*s)->withc.expression, *s);
		(*s)->withc.parameter = make_list(1, NULL);
	} else if(!strcmp(to_string(fst(d)), "begin")) {
		(*s)->begin.type = begin;
		(*s)->begin.expressions = nil();
	
		list t = rst(d);
		s_expression v;
		foreach(v, t) {
			build_syntax_tree_under((list) v, append(NULL, &(*s)->begin.expressions), *s);
		}
	} else if(!strcmp(to_string(fst(d)), "if")) {
		if(length(d) != 4) {
			longjmp(*build_syntax_tree_handler, (int) make_special_form(d, NULL));
		}
	
		(*s)->_if.type = _if;
		build_syntax_tree_under(frst(d), &(*s)->_if.condition, *s);
		build_syntax_tree_under(frrst(d), &(*s)->_if.consequent, *s);
		build_syntax_tree_under(frrrst(d), &(*s)->_if.alternate, *s);
	} else if(!strcmp(to_string(fst(d)), "function") || !strcmp(to_string(fst(d)), "make-continuation")) {
		if(length(d) != 4) {
			longjmp(*build_syntax_tree_handler, (int) make_special_form(d, NULL));
		} else if(!is_string(frst(d))) {
			longjmp(*build_syntax_tree_handler, (int) make_special_form(d, frst(d)));
		} else if(!is_nil(frrst(d)) && is_string(frrst(d))) {
			longjmp(*build_syntax_tree_handler, (int) make_special_form(d, frrst(d)));
		}
		
		(*s)->function.type = !strcmp(to_string(fst(d)), "function") ? function : makec;
		build_syntax_tree_under(frst(d), &(*s)->function.reference, *s);
		
		if((*s)->function.type == function) {
			(*s)->function.locals = nil();
		}
		(*s)->function.parameters = nil();
		s_expression v;
		foreach(v, frrst(d)) {
			if(!is_string((list) v)) {
				longjmp(*build_syntax_tree_handler, (int) make_special_form(d, (list) v));
			}
			build_syntax_tree_under((list) v, append(NULL, &(*s)->function.parameters), *s);
		}
	
		build_syntax_tree_under(frrrst(d), &(*s)->function.expression, *s);
	} else if(!strcmp(to_string(fst(d)), "b")) {
		char *str;
		if(length(d) != 2) {
			longjmp(*build_syntax_tree_handler, (int) make_special_form(d, NULL));
		} else if(!is_string(frst(d)) || strlen(str = to_string(frst(d))) != 32) {
			longjmp(*build_syntax_tree_handler, (int) make_special_form(d, frst(d)));
		}
	
		(*s)->constant.type = constant;
		(*s)->constant.value = 0;
		int i;
		for(i = 0; i < strlen(str); i++) {
			if(str[strlen(str) - i - 1] == '1') {
				(*s)->constant.value |= (1 << i);
			} else if(str[strlen(str) - i - 1] != '0') {
				longjmp(*build_syntax_tree_handler, (int) make_special_form(d, frst(d)));
			}
		}
	} else if(!strcmp(to_string(fst(d)), "invoke") || !strcmp(to_string(fst(d)), "continue")) {
		if(length(d) == 1) {
			longjmp(*build_syntax_tree_handler, (int) make_special_form(d, NULL));
		}
	
		(*s)->invoke.type = !strcmp(to_string(fst(d)), "invoke") ? invoke : _continue;
		build_syntax_tree_under(frst(d), &(*s)->invoke.reference, *s);
	
		s_expression v;
		(*s)->invoke.arguments = nil();
		foreach(v, rrst(d)) {
			build_syntax_tree_under((list) v, append(NULL, &(*s)->invoke.arguments), *s);
		}
	} else {
		struct expansion *e = malloc(sizeof(struct expansion));
		e->argument = rst(d);
		e->dest = s;
		expansion_depth++;
		build_syntax_tree_under(fst(d), &e->function, *s);
		expansion_depth--;
		prepend(e, list_at(expansion_depth, &build_syntax_tree_expansion_lists));
	}
}

void merge_onto(list src, list *dest) {
	while(!is_nil(src) && !is_nil(*dest)) {
		append_list((list *) &(*dest)->fst, fst(src));
		src = rst(src);
		dest = &(*dest)->rst;
	}
	if(!is_nil(src)) {
		append_list(dest, src);
	}
}

void expand_expressions(list expansion_lists) {
	expansion_lists = reverse(expansion_lists);
	list expansions, *remaining_expansion_lists;
	foreachlist(remaining_expansion_lists, expansions, expansion_lists) {
		struct expansion *expansion;
		list expander_containers = nil();
		list expander_container_names = nil();
		foreach(expansion, expansions) {
			union expression *expander_container = make_function("");
			put(expander_container, function.expression, expansion->function);
			append(expander_container, &expander_containers);
			append(expander_container->function.reference->reference.name, &expander_container_names);
		}
		
		char *sofn = dynamic_load(expander_containers, build_syntax_tree_handler);
		void *handle = dlopen(sofn, RTLD_NOW | RTLD_LOCAL);
		if(!handle) {
			remove(sofn);
			longjmp(*build_syntax_tree_handler, (int) make_environment(cprintf("%s", dlerror())));
		}
		
		char *expander_container_name;
		foreachzipped(expansion, expander_container_name, expansions, expander_container_names) {
			list (*(*macro_container)())(list) = dlsym(handle, expander_container_name);
			list (*macro)(list) = macro_container();
			list transformed = macro(expansion->argument);
			
			build_syntax_tree_expansion_lists = nil();
			build_syntax_tree_under(transformed, expansion->dest, (*expansion->dest)->base.parent);
			merge_onto(reverse(build_syntax_tree_expansion_lists), &(*remaining_expansion_lists)->rst);
		}
		dlclose(handle);
		remove(sofn);
	}
}