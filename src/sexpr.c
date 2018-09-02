struct character {
	void *list_flag;
	char character;
};

union sexpr {
	void *list_flag;
	struct _list_ _list;
	struct character character;
};

bool is_lst(union sexpr *s) {
	return s->list_flag ? true : false;
}

bool sexpr_equals(union sexpr *a, union sexpr *b) {
	if(!a->list_flag != !b->list_flag) {
		return false;
	} else if(!a->list_flag) {
		return a->character.character == b->character.character ? true : false;
	} else if(is_nil((list) a) || is_nil((list) b)) {
		return is_nil((list) a) & is_nil((list) b);
	} else {
		return sexpr_equals(((list) a)->fst, ((list) b)->fst) &
			sexpr_equals((union sexpr *) ((list) a)->rst, (union sexpr *) ((list) b)->rst);
	}
}

union sexpr *build_character_sexpr(char d, region r) {
	union sexpr *c = region_malloc(r, sizeof(union sexpr));
	c->character.list_flag = NULL;
	c->character.character = d;
	return c;
}

list build_symbol_sexpr(char *str, region r) {
	list sexprs = nil(r);
	for(; *str; str++) {
		append(build_character_sexpr(*str, r), &sexprs, r);
	}
	return sexprs;
}
