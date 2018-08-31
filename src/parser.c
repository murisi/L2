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

#if __x86_64__
	#define WORD_HEX_LEN 16
#endif
#if __i386__
	#define WORD_HEX_LEN 8
#endif

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
		prepend(str, &generate_string_blacklist);
	} else if(!strcmp(to_string(fst(d)), "with")) {
		if(length(d) != 3) {
			thelongjmp(*build_syntax_tree_handler, make_special_form(d, NULL));
		} else if(!is_string(frst(d))) {
			thelongjmp(*build_syntax_tree_handler, make_special_form(d, frst(d)));
		}
	
		(*s)->with.type = with;
		build_syntax_tree_under(frst(d), &(*s)->with.reference, *s);
		build_syntax_tree_under(frrst(d), &(*s)->with.expression, *s);
		(*s)->with.parameter = make_list(1, NULL);
	} else if(!strcmp(to_string(fst(d)), "begin")) {
		(*s)->begin.type = begin;
		(*s)->begin.expressions = nil();
	
		list t = rst(d);
		list v;
		foreach(v, t) {
			build_syntax_tree_under(v, append(NULL, &(*s)->begin.expressions), *s);
		}
	} else if(!strcmp(to_string(fst(d)), "if")) {
		if(length(d) != 4) {
			thelongjmp(*build_syntax_tree_handler, make_special_form(d, NULL));
		}
	
		(*s)->_if.type = _if;
		build_syntax_tree_under(frst(d), &(*s)->_if.condition, *s);
		build_syntax_tree_under(frrst(d), &(*s)->_if.consequent, *s);
		build_syntax_tree_under(frrrst(d), &(*s)->_if.alternate, *s);
	} else if(!strcmp(to_string(fst(d)), "function") || !strcmp(to_string(fst(d)), "continuation")) {
		if(length(d) != 4) {
			thelongjmp(*build_syntax_tree_handler, make_special_form(d, NULL));
		} else if(!is_string(frst(d))) {
			thelongjmp(*build_syntax_tree_handler, make_special_form(d, frst(d)));
		} else if(!is_nil(frrst(d)) && is_string(frrst(d))) {
			thelongjmp(*build_syntax_tree_handler, make_special_form(d, frrst(d)));
		}
		
		(*s)->function.type = !strcmp(to_string(fst(d)), "function") ? function : continuation;
		build_syntax_tree_under(frst(d), &(*s)->function.reference, *s);
		
		if((*s)->function.type == function) {
			(*s)->function.locals = nil();
		}
		(*s)->function.parameters = nil();
		list v;
		foreach(v, frrst(d)) {
			if(!is_string(v)) {
				thelongjmp(*build_syntax_tree_handler, make_special_form(d, (list) v));
			}
			build_syntax_tree_under((list) v, append(NULL, &(*s)->function.parameters), *s);
		}
	
		build_syntax_tree_under(frrrst(d), &(*s)->function.expression, *s);
	} else if(!strcmp(to_string(fst(d)), "literal")) {
		char *str;
		if(length(d) != 2) {
			thelongjmp(*build_syntax_tree_handler, make_special_form(d, NULL));
		} else if(!is_string(frst(d)) || strlen(str = to_string(frst(d))) != WORD_HEX_LEN) {
			thelongjmp(*build_syntax_tree_handler, make_special_form(d, frst(d)));
		}
	
		(*s)->literal.type = literal;
		(*s)->literal.value = 0;
		unsigned long int i;
		for(i = 0; i < strlen(str); i++) {
			(*s)->literal.value <<= 4;
			if(str[i] == 'F') {
				(*s)->literal.value += 0xF;
			} else if(str[i] == 'E') {
				(*s)->literal.value += 0xE;
			} else if(str[i] == 'D') {
				(*s)->literal.value += 0xD;
			} else if(str[i] == 'C') {
				(*s)->literal.value += 0xC;
			} else if(str[i] == 'B') {
				(*s)->literal.value += 0xB;
			} else if(str[i] == 'A') {
				(*s)->literal.value += 0xA;
			} else if(str[i] == '9') {
				(*s)->literal.value += 0x9;
			} else if(str[i] == '8') {
				(*s)->literal.value += 0x8;
			} else if(str[i] == '7') {
				(*s)->literal.value += 0x7;
			} else if(str[i] == '6') {
				(*s)->literal.value += 0x6;
			} else if(str[i] == '5') {
				(*s)->literal.value += 0x5;
			} else if(str[i] == '4') {
				(*s)->literal.value += 0x4;
			} else if(str[i] == '3') {
				(*s)->literal.value += 0x3;
			} else if(str[i] == '2') {
				(*s)->literal.value += 0x2;
			} else if(str[i] == '1') {
				(*s)->literal.value += 0x1;
			} else if(str[i] != '0') {
				thelongjmp(*build_syntax_tree_handler, make_special_form(d, frst(d)));
			}
		}
	} else if(!strcmp(to_string(fst(d)), "invoke") || !strcmp(to_string(fst(d)), "jump")) {
		if(length(d) == 1) {
			thelongjmp(*build_syntax_tree_handler, make_special_form(d, NULL));
		}
	
		(*s)->invoke.type = !strcmp(to_string(fst(d)), "invoke") ? invoke : jump;
		build_syntax_tree_under(frst(d), &(*s)->invoke.reference, *s);
	
		list v;
		(*s)->invoke.arguments = nil();
		foreach(v, rrst(d)) {
			build_syntax_tree_under(v, append(NULL, &(*s)->invoke.arguments), *s);
		}
	} else {
		(*s)->non_primitive.type = non_primitive;
		(*s)->non_primitive.argument = rst(d);
		expansion_depth--;
		build_syntax_tree_under(fst(d), &(*s)->non_primitive.function, *s);
		expansion_depth++;
		prepend(s, list_at(expansion_depth, &build_syntax_tree_expansion_lists));
	}
}

#undef WORD_HEX_LEN

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
 * Gets the address of the symbol with name name in the library lib.
 */
void *get_symbol(Library *lib, char *name) {
	int i, isc = immutable_symbol_count(lib);
	Symbol is[isc];
	immutable_symbols(lib, is);
	for(i = 0; i < isc; i++) {
		if(!strcmp(is[i].name, name)) {
			return is[i].address;
		}
	}
}

/*
 * Saturated subtraction: clamps to 0 if result of subtraction would have been less than it.
 */
void *sat_sub(void *ptr, uintptr_t b) {
	void *ans = ptr - b;
	uintptr_t z = 0;
	return ans > ptr ? ((void *) z) : ans;
}

/*
 * Saturated addition: clamps to ~0 if result of subtraction would have been more than it.
 */
void *sat_add(void *ptr, uintptr_t b) {
	void *ans = ptr + b;
	return ans < ptr ? ((void *) -1) : ans;
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

void expand_expressions(list *expansion_lists, Symbol *env) {
	list expansions, *remaining_expansion_lists;
	foreachlist(remaining_expansion_lists, expansions, expansion_lists) {
		list urgent_expansion_lists = nil();
		
		union expression **expansion;
		list expander_containers = nil();
		list expander_container_names = nil();
		foreach(expansion, expansions) {
			union expression *expander_container = make_function();
			put(expander_container, function.expression, (*expansion)->non_primitive.function);
			append(expander_container, &expander_containers);
			append(expander_container->function.reference->reference.name, &expander_container_names);
		}
		
		char *outfn = cprintf("%s", "./.libXXXXXX.a");
		mkstemps(outfn, 2);
		compile_expressions(outfn, expander_containers, build_syntax_tree_handler);
		Library *handle = load_library(fopen(outfn, "r"), (void *) 0x0UL, (void *) ~0x0UL);
		for(; env->name && env->address; env++) {
			mutate_symbol(handle, *env);
		}
		
		char *expander_container_name;
		foreachzipped(expansion, expander_container_name, expansions, expander_container_names) {
			list (*(*macro_container)())(list) = get_symbol(handle, expander_container_name);
			list (*macro)(list) = macro_container();
			list transformed = macro((*expansion)->non_primitive.argument);
			
			build_syntax_tree_handler = expand_expressions_handler;
			build_syntax_tree_expansion_lists = nil();
			build_syntax_tree_under(transformed, expansion, (*expansion)->non_primitive.parent);
			merge_onto(build_syntax_tree_expansion_lists, &urgent_expansion_lists);
		}
		fclose(unload_library(handle));
		remove(outfn);
		
		append_list(&urgent_expansion_lists, (*remaining_expansion_lists)->rst);
		(*remaining_expansion_lists)->rst = urgent_expansion_lists;
	}
}
