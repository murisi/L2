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

bool char_equals(struct character *a, struct character *b) {
	return a->character == b->character ? true : false;
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
