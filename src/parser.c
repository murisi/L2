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
	union expression **target;
};

#define build_syntax_tree_under(x, y, z) { \
	union expression *_build_syntax_tree_under_u = z; \
	union expression **_build_syntax_tree_under_v = y; \
	build_syntax_tree(x, _build_syntax_tree_under_v); \
	(*_build_syntax_tree_under_v)->base.parent = _build_syntax_tree_under_u; \
}

/*
 * Argument l is a pointer to a list of lists. An index of -1 refers to the last
 * element of l, an index of -2 refers to the second last element in l, and so
 * on. The following function returns the element at index index of l. If the
 * element doesn't exist, just enough empty lists are inserted at the beginning
 * of the list of lists to make it exist.
 */

list *list_at(int index, list *l) {
	int len = length(*l);
	while(-len < index) {
		l = &(*l)->rst;
		len--;
	}
	while(index < -len) {
		*l = lst(nil(), *l);
		len++;
	}
	return (list *) &(*l)->fst;
}

/*
 * Builds a syntax tree at s from the list of s-expressions d. Expansion
 * expressions are not immediately compiled and executed as this is inefficient.
 * Rather, the list of lists, build_syntax_tree_expansion_lists, is used to
 * store all the expansions that need to happen. More specifically, the first
 * list in build_syntax_tree_expansion_lists stores all the expansions that need
 * to happen first, the second list stores all the expansions that need to
 * happen second, and so on.
 */

jmp_buf *build_syntax_tree_handler;
list build_syntax_tree_expansion_lists;

void build_syntax_tree(list d, union expression **s) {
	static int expansion_depth = -1;
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
		e->target = s;
		expansion_depth--;
		build_syntax_tree_under(fst(d), &e->function, *s);
		expansion_depth++;
		prepend(e, list_at(expansion_depth, &build_syntax_tree_expansion_lists));
	}
}

/*
 * Argument src is a list of lists and dest is a pointer to a list of lists. The
 * function appends each list in src to the corresponding list in dest. If the
 * corresponding list does not exist, it is implicitly assummed to be the empty
 * list.
 */

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

/*
 * Does the expansions in the expansions lists. For the purposes of efficiency,
 * the first expansion list is compiled and sent to the assembler together.
 * With the expanders compiled, the expansions are done, and the resulting list
 * of s-expressions is parsed using build_syntax_tree. This action may create
 * more expansion lists - this is merged onto the remaing expansion lists that
 * this function has to deal with. This process is done for the second expansion
 * list and so on.
 */

jmp_buf *expand_expressions_handler;

void expand_expressions(list expansion_lists) {
	list expansions, *remaining_expansion_lists;
	foreachlist(remaining_expansion_lists, expansions, expansion_lists) {
		list urgent_expansion_lists = nil();
		
		struct expansion *expansion;
		list expander_containers = nil();
		list expander_container_names = nil();
		foreach(expansion, expansions) {
			union expression *expander_container = make_function("");
			put(expander_container, function.expression, expansion->function);
			append(expander_container, &expander_containers);
			append(expander_container->function.reference->reference.name, &expander_container_names);
		}
		
		void *handle = load(compile(expander_containers, true, build_syntax_tree_handler), build_syntax_tree_handler);
		if(!handle) {
			longjmp(*expand_expressions_handler, (int) make_environment(cprintf("%s", dlerror())));
		}
		
		char *expander_container_name;
		foreachzipped(expansion, expander_container_name, expansions, expander_container_names) {
			list (*(*macro_container)())(list) = dlsym(handle, expander_container_name);
			list (*macro)(list) = macro_container();
			list transformed = macro(expansion->argument);
			
			build_syntax_tree_handler = expand_expressions_handler;
			build_syntax_tree_expansion_lists = nil();
			build_syntax_tree_under(transformed, expansion->target, (*expansion->target)->base.parent);
			merge_onto(build_syntax_tree_expansion_lists, &urgent_expansion_lists);
		}
		unload(handle);
		
		append_list(&urgent_expansion_lists, (*remaining_expansion_lists)->rst);
		(*remaining_expansion_lists)->rst = urgent_expansion_lists;
	}
}
