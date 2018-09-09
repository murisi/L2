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

list copy_sexpr_list(list l, region r) {
	list c = nil(r);
	union sexpr *s;
	foreach(s, l) {
		if(is_lst(s)) {
			append(copy_sexpr_list((list) s, r), &c, r);
		} else {
			union sexpr *t = region_malloc(r, sizeof(union sexpr));
			t->character = s->character;
			append(t, &c, r);
		}
	}
	return c;
}

bool char_equals(struct character *a, struct character *b) {
	return a->character == b->character ? true : false;
}

bool sexpr_list_equals(list c, list d) {
	if(length(c) != length(d)) return false;
	union sexpr *a, *b;
	foreachzipped(a, b, c, d) {
		if(!a->list_flag != !b->list_flag) {
			return false;
		} else if(!a->list_flag && a->character.character != b->character.character) {
			return false;
		} else if(a->list_flag && !sexpr_list_equals((list) a, (list) b)) {
			return false;
		}
	}
	return true;
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
