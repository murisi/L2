#define build_syntax_tree_under(x, y, z, r) { \
	union expression *_build_syntax_tree_under_u = z; \
	union expression **_build_syntax_tree_under_v = y; \
	build_syntax_tree(x, _build_syntax_tree_under_v, r); \
	(*_build_syntax_tree_under_v)->base.parent = _build_syntax_tree_under_u; \
}

/*
 * Argument l is a pointer to a list of lists. An index of -1 refers to the last
 * element of l, an index of -2 refers to the second last element in l, and so
 * on. The following function returns the element at index index of l. If the
 * element doesn't exist, just enough empty lists are inserted at the beginning
 * of the list of lists to make it exist.
 */

list *list_at(int index, list *l, region reg) {
	int len = length(*l);
	while(-len < index) {
		l = &(*l)->rst;
		len--;
	}
	while(index < -len) {
		*l = lst(nil(reg), *l, reg);
		len++;
	}
	return (list *) &(*l)->fst;
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
list build_syntax_tree_expansion_lists;

void build_syntax_tree(list d, union expression **s, region reg) {
	static int expansion_depth = -1;
	*s = region_malloc(reg, sizeof(union expression));
	
	if(is_string(d)) {
		char *str = to_string(d, reg);
		(*s)->reference.type = reference;
		(*s)->reference.name = str;
	} else if(!strcmp(to_string(d->fst, reg), "with")) {
		if(length(d) != 3) {
			throw_special_form(d, NULL, build_syntax_tree_handler);
		} else if(!is_string(d->frst)) {
			throw_special_form(d, d->frst, build_syntax_tree_handler);
		}
	
		(*s)->with.type = with;
		build_syntax_tree_under(d->frst, &(*s)->with.reference, *s, reg);
		build_syntax_tree_under(d->frrst, &(*s)->with.expression, *s, reg);
		(*s)->with.parameter = lst(NULL, nil(reg), reg);
	} else if(!strcmp(to_string(d->fst, reg), "begin")) {
		(*s)->begin.type = begin;
		(*s)->begin.expressions = nil(reg);
	
		list t = d->rst;
		list v;
		foreach(v, t) {
			build_syntax_tree_under(v, append(NULL, &(*s)->begin.expressions, reg), *s, reg);
		}
	} else if(!strcmp(to_string(d->fst, reg), "if")) {
		if(length(d) != 4) {
			throw_special_form(d, NULL, build_syntax_tree_handler);
		}
	
		(*s)->_if.type = _if;
		build_syntax_tree_under(d->frst, &(*s)->_if.condition, *s, reg);
		build_syntax_tree_under(d->frrst, &(*s)->_if.consequent, *s, reg);
		build_syntax_tree_under(d->frrrst, &(*s)->_if.alternate, *s, reg);
	} else if(!strcmp(to_string(d->fst, reg), "function") || !strcmp(to_string(d->fst, reg), "continuation")) {
		if(length(d) != 4) {
			throw_special_form(d, NULL, build_syntax_tree_handler);
		} else if(!is_string(d->frst)) {
			throw_special_form(d, d->frst, build_syntax_tree_handler);
		} else if(!is_nil(d->frrst) && is_string(d->frrst)) {
			throw_special_form(d, d->frrst, build_syntax_tree_handler);
		}
		
		(*s)->function.type = !strcmp(to_string(d->fst, reg), "function") ? function : continuation;
		build_syntax_tree_under(d->frst, &(*s)->function.reference, *s, reg);
		
		if((*s)->function.type == function) {
			(*s)->function.locals = nil(reg);
		}
		(*s)->function.parameters = nil(reg);
		list v;
		foreach(v, d->frrst) {
			if(!is_string(v)) {
				throw_special_form(d, (list) v, build_syntax_tree_handler);
			}
			build_syntax_tree_under((list) v, append(NULL, &(*s)->function.parameters, reg), *s, reg);
		}
	
		build_syntax_tree_under(d->frrrst, &(*s)->function.expression, *s, reg);
	} else if(!strcmp(to_string(d->fst, reg), "literal")) {
		char *str;
		if(length(d) != 2) {
			throw_special_form(d, NULL, build_syntax_tree_handler);
		} else if(!is_string(d->frst) || strlen(str = to_string(d->frst, reg)) != WORD_BIN_LEN) {
			throw_special_form(d, d->frst, build_syntax_tree_handler);
		}
	
		(*s)->literal.type = literal;
		(*s)->literal.value = 0;
		unsigned long int i;
		for(i = 0; i < strlen(str); i++) {
			(*s)->literal.value <<= 1;
			if(str[i] == '1') {
				(*s)->literal.value += 1;
			} else if(str[i] != '0') {
				throw_special_form(d, d->frst, build_syntax_tree_handler);
			}
		}
	} else if(!strcmp(to_string(d->fst, reg), "invoke") || !strcmp(to_string(d->fst, reg), "jump")) {
		if(length(d) == 1) {
			throw_special_form(d, NULL, build_syntax_tree_handler);
		}
	
		(*s)->invoke.type = !strcmp(to_string(d->fst, reg), "invoke") ? invoke : jump;
		build_syntax_tree_under(d->frst, &(*s)->invoke.reference, *s, reg);
	
		list v;
		(*s)->invoke.arguments = nil(reg);
		foreach(v, d->rrst) {
			build_syntax_tree_under(v, append(NULL, &(*s)->invoke.arguments, reg), *s, reg);
		}
	} else {
		(*s)->non_primitive.type = non_primitive;
		(*s)->non_primitive.argument = d->rst;
		expansion_depth--;
		build_syntax_tree_under(d->fst, &(*s)->non_primitive.function, *s, reg);
		expansion_depth++;
		prepend(s, list_at(expansion_depth, &build_syntax_tree_expansion_lists, reg), reg);
	}
}

#undef WORD_BIN_LEN

/*
 * Argument src is a list of lists and dest is a pointer to a list of lists. The
 * function appends each list in src to the corresponding list in dest. If the
 * corresponding list does not exist, it is implicitly assummed to be the empty
 * list.
 */

void merge_onto(list src, list *dest) {
	while(!is_nil(src) && !is_nil(*dest)) {
		append_list((list *) &(*dest)->fst, src->fst);
		src = src->rst;
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

myjmp_buf *expand_expressions_handler;

void expand_expressions(list *expansion_lists, list env, region exprreg) {
	list expansions, *remaining_expansion_lists;
	foreachlist(remaining_expansion_lists, expansions, expansion_lists) {
		list urgent_expansion_lists = nil(exprreg);
		region objreg = create_region(0);
		list expander_refs = nil(objreg);
		union expression **expansion;
		list setup_expander_refs = nil(objreg);
		
		{foreach(expansion, expansions) {
			append(make_invoke3(make_literal((unsigned long) &append, objreg), (*expansion)->non_primitive.function,
				make_literal((unsigned long) &expander_refs, objreg), make_literal((unsigned long) objreg, objreg), objreg),
				&setup_expander_refs, objreg);
		}}
		
		unsigned char *raw_obj; int obj_sz;
		compile_expressions(&raw_obj, &obj_sz, setup_expander_refs, objreg, build_syntax_tree_handler);
		Object *handle = load(raw_obj, obj_sz, objreg);
		Symbol *sym;
		{foreach(sym, env) {
			mutate_symbols(handle, sym, 1);
		}}
		start(handle)(); //Populate expander_containers_refs using the just compiled L2 code
		list (*expander_ref)(list);
		foreachzipped(expansion, expander_ref, expansions, expander_refs) {
			list transformed = expander_ref((*expansion)->non_primitive.argument);
			build_syntax_tree_handler = expand_expressions_handler;
			build_syntax_tree_expansion_lists = nil(exprreg);
			build_syntax_tree_under(transformed, expansion, (*expansion)->non_primitive.parent, exprreg);
			merge_onto(build_syntax_tree_expansion_lists, &urgent_expansion_lists);
		}
		destroy_region(objreg);
		
		append_list(&urgent_expansion_lists, (*remaining_expansion_lists)->rst);
		(*remaining_expansion_lists)->rst = urgent_expansion_lists;
	}
}
