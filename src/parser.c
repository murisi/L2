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
	union expression *s = region_alloc(reg, sizeof(union expression));
	
	if(is_string(d)) {
		char *str = to_string(d, reg);
		s->reference.type = reference;
		s->reference.name = str;
		s->reference.referent = NULL;
		s->reference.symbol = NULL;
	} else if(!strcmp(to_string(d->fst, reg), "with")) {
		if(length(d) != 3) {
			throw_special_form(d, NULL, handler);
		} else if(!is_string(d->frst)) {
			throw_special_form(d, d->frst, handler);
		}
	
		s->with.type = with;
		put(s, with.reference, build_syntax_tree(d->frst, reg, handler));
		put(s, with.expression, build_syntax_tree(d->frrst, reg, handler));
		union expression *param = make_reference(NULL, reg);
		param->reference.parent = s;
		s->with.parameter = lst(param, nil(reg), reg);
	} else if(!strcmp(to_string(d->fst, reg), "begin")) {
		s->begin.type = begin;
		s->begin.expressions = nil(reg);
	
		list t = d->rst;
		list v;
		{foreach(v, t) {
			union expression *expr = build_syntax_tree(v, reg, handler);
			append(expr, &s->begin.expressions, reg);
			expr->base.parent = s;
		}}
	} else if(!strcmp(to_string(d->fst, reg), "if")) {
		if(length(d) != 4) {
			throw_special_form(d, NULL, handler);
		}
	
		s->_if.type = _if;
		put(s, _if.condition, build_syntax_tree(d->frst, reg, handler));
		put(s, _if.consequent, build_syntax_tree(d->frrst, reg, handler));
		put(s, _if.alternate, build_syntax_tree(d->frrrst, reg, handler));
	} else if(!strcmp(to_string(d->fst, reg), "function") || !strcmp(to_string(d->fst, reg), "continuation")) {
		if(length(d) != 4) {
			throw_special_form(d, NULL, handler);
		} else if(!is_string(d->frst)) {
			throw_special_form(d, d->frst, handler);
		} else if(!is_nil(d->frrst) && is_string(d->frrst)) {
			throw_special_form(d, d->frrst, handler);
		}
		
		s->function.type = !strcmp(to_string(d->fst, reg), "function") ? function : continuation;
		put(s, function.reference, build_syntax_tree(d->frst, reg, handler));
		
		if(s->function.type == function) {
			s->function.local_symbols = nil(reg);
		}
		s->function.parameters = nil(reg);
		list v;
		foreach(v, d->frrst) {
			if(!is_string(v)) {
				throw_special_form(d, (list) v, handler);
			}
			union expression *expr = build_syntax_tree((list) v, reg, handler);
			append(expr, &s->function.parameters, reg);
			expr->reference.parent = s;
		}
	
		put(s, function.expression, build_syntax_tree(d->frrrst, reg, handler));
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
	} else if(!strcmp(to_string(d->fst, reg), "invoke") || !strcmp(to_string(d->fst, reg), "jump") ||
			!strcmp(to_string(d->fst, reg), "storage")) {
		if(length(d) == 1) {
			throw_special_form(d, NULL, handler);
		} else if(!strcmp(to_string(d->fst, reg), "storage") && !is_string(d->frst)) {
			throw_special_form(d->frst, NULL, handler);
		}
	
		s->invoke.type = !strcmp(to_string(d->fst, reg), "invoke") ? invoke :
			(!strcmp(to_string(d->fst, reg), "jump") ? jump : storage);
		put(s, invoke.reference, build_syntax_tree(d->frst, reg, handler));
	
		list v;
		s->invoke.arguments = nil(reg);
		foreach(v, d->rrst) {
			union expression *expr = build_syntax_tree(v, reg, handler);
			append(expr, &s->invoke.arguments, reg);
			expr->base.parent = s;
		}
	} else {
		s->non_primitive.type = non_primitive;
		put(s, non_primitive.reference, build_syntax_tree(d->fst, reg, handler));
		s->non_primitive.argument = d->rst;
		s->non_primitive.indirections = nil(reg);
	}
	return s;
}

#undef WORD_BIN_LEN
