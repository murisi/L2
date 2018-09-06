unsigned long execute_macro(list (*expander)(list), list arg, list bindings);

Symbol *make_symbol(char *nm, void *addr, region r) {
	Symbol *sym = region_malloc(r, sizeof(Symbol));
	sym->name = nm;
	sym->address = addr;
	return sym;
}

#define WORD_BIN_LEN 64

/*
 * Builds a syntax tree at s from the list of s-expressions d. Expansion
 * expressions are not immediately compiled and executed as this is inefficient.
 * Rather, the list of lists, build_syntax_tree_expansion_lists, is used to
 * store all the expansions that need to happen. More specifically, the first
 * list in build_syntax_tree_expansion_lists stores all the expansions that need
 * to happen first, the second list stores all the expansions that need to
 * happen second, and so on.
 */

myjmp_buf *build_syntax_tree_handler;

union expression *build_syntax_tree(list d, union expression *parent, list static_bindings, list dynamic_bindings, region reg) {
	union expression *s = region_malloc(reg, sizeof(union expression));
	s->base.parent = parent;
	list *bindings = get_zeroth_function(s) ? &dynamic_bindings : &static_bindings;
	
	if(is_string(d)) {
		char *str = to_string(d, reg);
		s->reference.type = reference;
		s->reference.name = str;
	} else if(!strcmp(to_string(d->fst, reg), "with")) {
		if(length(d) != 3) {
			throw_special_form(d, NULL, build_syntax_tree_handler);
		} else if(!is_string(d->frst)) {
			throw_special_form(d, d->frst, build_syntax_tree_handler);
		}
	
		s->with.type = with;
		s->with.reference = build_syntax_tree(d->frst, s, NULL, NULL, reg);
		prepend(s->with.reference, bindings, reg);
		s->with.expression = build_syntax_tree(d->frrst, s, static_bindings, dynamic_bindings, reg);
		s->with.parameter = lst(NULL, nil(reg), reg);
	} else if(!strcmp(to_string(d->fst, reg), "begin")) {
		s->begin.type = begin;
		s->begin.expressions = nil(reg);
	
		list t = d->rst;
		list v;
		foreach(v, t) {
			append(build_syntax_tree(v, s, static_bindings, dynamic_bindings, reg), &s->begin.expressions, reg);
		}
	} else if(!strcmp(to_string(d->fst, reg), "if")) {
		if(length(d) != 4) {
			throw_special_form(d, NULL, build_syntax_tree_handler);
		}
	
		s->_if.type = _if;
		s->_if.condition = build_syntax_tree(d->frst, s, static_bindings, dynamic_bindings, reg);
		s->_if.consequent = build_syntax_tree(d->frrst, s, static_bindings, dynamic_bindings, reg);
		s->_if.alternate = build_syntax_tree(d->frrrst, s, static_bindings, dynamic_bindings, reg);
	} else if(!strcmp(to_string(d->fst, reg), "function") || !strcmp(to_string(d->fst, reg), "continuation")) {
		if(length(d) != 4) {
			throw_special_form(d, NULL, build_syntax_tree_handler);
		} else if(!is_string(d->frst)) {
			throw_special_form(d, d->frst, build_syntax_tree_handler);
		} else if(!is_nil(d->frrst) && is_string(d->frrst)) {
			throw_special_form(d, d->frrst, build_syntax_tree_handler);
		}
		
		s->function.type = !strcmp(to_string(d->fst, reg), "function") ? function : continuation;
		if(s->function.type == function) {
			bindings = &dynamic_bindings;
			*bindings = nil(reg);
		}
		s->function.reference = build_syntax_tree(d->frst, s, static_bindings, dynamic_bindings, reg);
		prepend(s->function.reference, bindings, reg);
		
		if(s->function.type == function) {
			s->function.locals = nil(reg);
		}
		s->function.parameters = nil(reg);
		list v;
		foreach(v, d->frrst) {
			if(!is_string(v)) {
				throw_special_form(d, (list) v, build_syntax_tree_handler);
			}
			union expression *param = build_syntax_tree((list) v, s, static_bindings, dynamic_bindings, reg);
			append(param, &s->function.parameters, reg);
			prepend(param, bindings, reg);
		}
	
		s->function.expression = build_syntax_tree(d->frrrst, s, static_bindings, dynamic_bindings, reg);
	} else if(!strcmp(to_string(d->fst, reg), "literal")) {
		char *str;
		if(length(d) != 2) {
			throw_special_form(d, NULL, build_syntax_tree_handler);
		} else if(!is_string(d->frst) || strlen(str = to_string(d->frst, reg)) != WORD_BIN_LEN) {
			throw_special_form(d, d->frst, build_syntax_tree_handler);
		}
	
		s->literal.type = literal;
		s->literal.value = 0;
		unsigned long int i;
		for(i = 0; i < strlen(str); i++) {
			s->literal.value <<= 1;
			if(str[i] == '1') {
				s->literal.value += 1;
			} else if(str[i] != '0') {
				throw_special_form(d, d->frst, build_syntax_tree_handler);
			}
		}
	} else if(!strcmp(to_string(d->fst, reg), "invoke") || !strcmp(to_string(d->fst, reg), "jump")) {
		if(length(d) == 1) {
			throw_special_form(d, NULL, build_syntax_tree_handler);
		}
	
		s->invoke.type = !strcmp(to_string(d->fst, reg), "invoke") ? invoke : jump;
		s->invoke.reference = build_syntax_tree(d->frst, s, static_bindings, dynamic_bindings, reg);
	
		list v;
		s->invoke.arguments = nil(reg);
		foreach(v, d->rrst) {
			append(build_syntax_tree(v, s, static_bindings, dynamic_bindings, reg), &s->invoke.arguments, reg);
		}
	} else {
		s->invoke.type = invoke;
		s->invoke.arguments = nil(reg);
		s->invoke.reference = make_literal((unsigned long) execute_macro, reg);
		append(build_syntax_tree(d->fst, s, static_bindings, dynamic_bindings, reg), &s->invoke.arguments, reg);
		append(make_literal((unsigned long) d->rst, reg), &s->invoke.arguments, reg);
		
		union expression *bindings_code = make_invoke1(make_literal((unsigned long) nil, reg),
			make_literal((unsigned long) reg, reg), reg);
		//Loops have special ordering to allow for shadowing
		union expression *ref;
		{foreach(ref, reverse(static_bindings, reg)) {
			bindings_code = make_invoke3(make_literal((unsigned long) lst, reg),
				make_invoke3(make_literal((unsigned long) make_symbol, reg), make_literal((unsigned long) ref->reference.name, reg),
					use_reference(ref, reg), make_literal((unsigned long) reg, reg), reg),
				bindings_code, make_literal((unsigned long) reg, reg), reg);
		}}
		{foreach(ref, reverse(dynamic_bindings, reg)) {
			bindings_code = make_invoke3(make_literal((unsigned long) lst, reg),
				make_invoke3(make_literal((unsigned long) make_symbol, reg), make_literal((unsigned long) ref->reference.name, reg),
					use_reference(ref, reg), make_literal((unsigned long) reg, reg), reg),
				bindings_code, make_literal((unsigned long) reg, reg), reg);
		}}
		bindings_code->base.parent = s;
		append(bindings_code, &s->invoke.arguments, reg);
	}
	return s;
}

#undef WORD_BIN_LEN

unsigned long execute_macro(list (*expander)(list), list arg, list bindings) {
	//list transformed = expander(arg);
	mywrite_str(STDOUT, "hey\n");
}
