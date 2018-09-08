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

union expression *build_syntax_tree(list d, union expression *parent, region reg, myjmp_buf *handler) {
	union expression *s = region_malloc(reg, sizeof(union expression));
	s->base.parent = parent;
	
	if(is_string(d)) {
		char *str = to_string(d, reg);
		s->reference.type = reference;
		s->reference.name = str;
	} else if(!strcmp(to_string(d->fst, reg), "with")) {
		if(length(d) != 3) {
			throw_special_form(d, NULL, handler);
		} else if(!is_string(d->frst)) {
			throw_special_form(d, d->frst, handler);
		}
	
		s->with.type = with;
		s->with.reference = build_syntax_tree(d->frst, s, reg, handler);
		s->with.expression = build_syntax_tree(d->frrst, s, reg, handler);
		s->with.parameter = lst(NULL, nil(reg), reg);
	} else if(!strcmp(to_string(d->fst, reg), "begin")) {
		s->begin.type = begin;
		s->begin.expressions = nil(reg);
	
		list t = d->rst;
		list v;
		{foreach(v, t) {
			append(build_syntax_tree(v, s, reg, handler), &s->begin.expressions, reg);
		}}
	} else if(!strcmp(to_string(d->fst, reg), "if")) {
		if(length(d) != 4) {
			throw_special_form(d, NULL, handler);
		}
	
		s->_if.type = _if;
		s->_if.condition = build_syntax_tree(d->frst, s, reg, handler);
		s->_if.consequent = build_syntax_tree(d->frrst, s, reg, handler);
		s->_if.alternate = build_syntax_tree(d->frrrst, s, reg, handler);
	} else if(!strcmp(to_string(d->fst, reg), "function") || !strcmp(to_string(d->fst, reg), "continuation")) {
		if(length(d) != 4) {
			throw_special_form(d, NULL, handler);
		} else if(!is_string(d->frst)) {
			throw_special_form(d, d->frst, handler);
		} else if(!is_nil(d->frrst) && is_string(d->frrst)) {
			throw_special_form(d, d->frrst, handler);
		}
		
		s->function.type = !strcmp(to_string(d->fst, reg), "function") ? function : continuation;
		s->function.reference = build_syntax_tree(d->frst, s, reg, handler);
		
		if(s->function.type == function) {
			s->function.locals = nil(reg);
		}
		s->function.parameters = nil(reg);
		list v;
		foreach(v, d->frrst) {
			if(!is_string(v)) {
				throw_special_form(d, (list) v, handler);
			}
			append(build_syntax_tree((list) v, s, reg, handler), &s->function.parameters, reg);
		}
	
		s->function.expression = build_syntax_tree(d->frrrst, s, reg, handler);
	} else if(!strcmp(to_string(d->fst, reg), "literal")) {
		char *str;
		if(length(d) != 2) {
			throw_special_form(d, NULL, handler);
		} else if(!is_string(d->frst) || strlen(str = to_string(d->frst, reg)) != WORD_BIN_LEN) {
			throw_special_form(d, d->frst, handler);
		}
	
		s->literal.type = literal;
		s->literal.value = 0;
		unsigned long int i;
		for(i = 0; i < strlen(str); i++) {
			s->literal.value <<= 1;
			if(str[i] == '1') {
				s->literal.value += 1;
			} else if(str[i] != '0') {
				throw_special_form(d, d->frst, handler);
			}
		}
	} else if(!strcmp(to_string(d->fst, reg), "invoke") || !strcmp(to_string(d->fst, reg), "jump")) {
		if(length(d) == 1) {
			throw_special_form(d, NULL, handler);
		}
	
		s->invoke.type = !strcmp(to_string(d->fst, reg), "invoke") ? invoke : jump;
		s->invoke.reference = build_syntax_tree(d->frst, s, reg, handler);
	
		list v;
		s->invoke.arguments = nil(reg);
		foreach(v, d->rrst) {
			append(build_syntax_tree(v, s, reg, handler), &s->invoke.arguments, reg);
		}
	} else {
		s->non_primitive.type = non_primitive;
		s->non_primitive.reference = build_syntax_tree(d->fst, s, reg, handler);
		s->non_primitive.argument = d->rst;
	}
	return s;
}

#undef WORD_BIN_LEN
