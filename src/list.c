struct _list_ {
	void *fst;
	struct _list_ *rst;
};
typedef struct _list_* list;

list nil() {
	list s = malloc(sizeof(struct _list_));
	s->fst = s;
	s->rst = NULL;
	return s;
}

bool is_nil(list s) {
	return s->rst == NULL ? true : false;
}

list rst(list l) {
	return l->rst;
}

list rrst(list l) {
	return l->rst->rst;
}

list rrrrrrst(list l) {
	return l->rst->rst->rst->rst->rst->rst;
}

void *fst(list l) {
	return l->fst;
}

void *frst(list l) {
	return l->rst->fst;
}

void *frrst(list l) {
	return l->rst->rst->fst;
}

void *frrrst(list l) {
	return l->rst->rst->rst->fst;
}

void *frrrrst(list l) {
	return l->rst->rst->rst->rst->fst;
}

void *frrrrrst(list l) {
	return l->rst->rst->rst->rst->rst->fst;
}

void set_fst(list l, void *data) {
	l->fst = data;
}

void set_frst(list l, void *data) {
	l->rst->fst = data;
}

#define foreach(t, u) \
	list _foreach_v = u; \
	for(; !is_nil(_foreach_v) ? (t = _foreach_v->fst, true) : (t = NULL, false); \
		_foreach_v = _foreach_v->rst)

#define foreachzipped(t, v, u, w) \
	list _foreach_s = u; \
	list _foreach_t = w; \
	for(; !is_nil(_foreach_s) ? (t = _foreach_s->fst, v = _foreach_t->fst, true) : (t = NULL, v = NULL, false); \
		_foreach_s = _foreach_s->rst, _foreach_t = _foreach_t->rst)

#define foreachlist(w, t, u) \
	w = u; \
	for(; !is_nil(*w) ? (t = (*w)->fst, true) : false; w = &((*w)->rst))

void *append(void *data, list *l) {
	while(!is_nil(*l)) {
		l = &((*l)->rst);
	}
	*l = malloc(sizeof(struct _list_));
	(*l)->fst = data;
	(*l)->rst = nil();
	return &(*l)->fst;
}

list lst(void *data, list l) {
	list ret = calloc(1, sizeof(struct _list_));
	ret->fst = data;
	ret->rst = l;
	return ret;
}

void append_list(list *fst, list snd) {
	while(!is_nil(*fst)) {
		fst = &((*fst)->rst);
	}
	*fst = snd;
}

void prepend(void *data, list *l) {
	list ret = malloc(sizeof(struct _list_));
	ret->fst = data;
	ret->rst = *l;
	*l = ret;
}

list address_list(list l) {
	list mapped = nil();
	for(; !is_nil(l); l = l->rst) {
		append(&l->fst, &mapped);
	}
	return mapped;
}

int length(list l) {
	int size;
	for(size = 0; !is_nil(l); l = rst(l), size++);
	return size;
}

list make_list(int len, ...) {
	list lst = nil(), *t = &lst;
	va_list vlist;
	va_start(vlist, len);
	
	int i;
	for(i = 0; i < len; i++) {
		append(va_arg(vlist, void *), t);
		t = &((*t)->rst);
	}
	return lst;
}

list reverse(list l) {
	list ret = nil();
	void *data;
	foreach(data, l) {
		prepend(data, &ret);
	}
	return ret;
}

list *exists(bool (*pred)(void *, void *), list *l, void *ctx) {
	void *d;
	list *s;
	foreachlist(s, d, l) {
		if(pred(d, ctx)) {
			return s;
		}
	}
	return NULL;
}
