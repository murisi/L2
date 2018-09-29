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

union expression *build_syntax_tree(list d, region reg, jumpbuf *handler) {
	if(is_string(d)) {
		return make_reference(to_string(d, reg), reg);
	} else if(!strcmp(to_string(d->fst, reg), "with")) {
		if(length(d) != 3) {
			throw_special_form(d, NULL, handler);
		} else if(!is_string(d->frst)) {
			throw_special_form(d, d->frst, handler);
		}
		return make_with(build_syntax_tree(d->frst, reg, handler), build_syntax_tree(d->frrst, reg, handler), reg);
	} else if(!strcmp(to_string(d->fst, reg), "begin")) {
		list t = d->rst;
		list v;
		list exprs;
		{foreach(v, t) {
			append(build_syntax_tree(v, reg, handler), &exprs, reg);
		}}
		return make_begin(exprs, reg);
	} else if(!strcmp(to_string(d->fst, reg), "if")) {
		if(length(d) != 4) {
			throw_special_form(d, NULL, handler);
		}
		return make_if(build_syntax_tree(d->frst, reg, handler), build_syntax_tree(d->frrst, reg, handler),
			build_syntax_tree(d->frrrst, reg, handler), reg);
	} else if(!strcmp(to_string(d->fst, reg), "function") || !strcmp(to_string(d->fst, reg), "continuation")) {
		if(length(d) != 4) {
			throw_special_form(d, NULL, handler);
		} else if(!is_string(d->frst)) {
			throw_special_form(d, d->frst, handler);
		} else if(!is_nil(d->frrst) && is_string(d->frrst)) {
			throw_special_form(d, d->frrst, handler);
		}
		
		list parameters = nil;
		list v;
		foreach(v, d->frrst) {
			if(!is_string(v)) {
				throw_special_form(d, (list) v, handler);
			}
			append(build_syntax_tree((list) v, reg, handler), &parameters, reg);
		}
		union expression *(*fptr)(union expression *, list, union expression *, region) =
			!strcmp(to_string(d->fst, reg), "function") ? make_function : make_continuation;
		return fptr(build_syntax_tree(d->frst, reg, handler), parameters, build_syntax_tree(d->frrrst, reg, handler), reg);
	} else if(!strcmp(to_string(d->fst, reg), "literal")) {
		char *str;
		if(length(d) != 2) {
			throw_special_form(d, NULL, handler);
		} else if(!is_string(d->frst) || strlen(str = to_string(d->frst, reg)) != WORD_BIN_LEN) {
			throw_special_form(d, d->frst, handler);
		}
		
		long int value = 0;
		unsigned long int i;
		for(i = 0; i < strlen(str); i++) {
			value <<= 1;
			if(str[i] == '1') {
				value += 1;
			} else if(str[i] != '0') {
				throw_special_form(d, d->frst, handler);
			}
		}
		return make_literal(value, reg);
	} else if(!strcmp(to_string(d->fst, reg), "invoke") || !strcmp(to_string(d->fst, reg), "jump") ||
			!strcmp(to_string(d->fst, reg), "storage")) {
		if(length(d) == 1) {
			throw_special_form(d, NULL, handler);
		} else if(!strcmp(to_string(d->fst, reg), "storage") && !is_string(d->frst)) {
			throw_special_form(d->frst, NULL, handler);
		}
	
		list v;
		list arguments = nil;
		foreach(v, d->rrst) {
			append(build_syntax_tree(v, reg, handler), &arguments, reg);
		}
		union expression *(*fptr)(union expression *, list, region) = !strcmp(to_string(d->fst, reg), "invoke") ? make_invoke :
			(!strcmp(to_string(d->fst, reg), "jump") ? make_jump : make_storage);
		return fptr(build_syntax_tree(d->frst, reg, handler), arguments, reg);
	} else {
		union expression *s = region_alloc(reg, sizeof(union expression));
		s->non_primitive.type = non_primitive;
		put(s, non_primitive.reference, build_syntax_tree(d->fst, reg, handler));
		s->non_primitive.argument = d->rst;
		s->non_primitive.indirections = nil;
		return s;
	}
}

#undef WORD_BIN_LEN
