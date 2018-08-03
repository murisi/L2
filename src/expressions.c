enum expression_type {
	function,
	with,
	invoke,
	_if,
	begin,
	constant,
	reference,
	jump,
	continuation,
	instruction
};

struct base_expression {
	enum expression_type type;
	union expression *parent;
	union expression *return_value;
};

struct begin_expression {
	enum expression_type type;
	union expression *parent;
	union expression *return_value;
	
	list expressions; // void * = struct expression *
};

struct instruction_expression {
	enum expression_type type;
	union expression *parent;
	union expression *return_value;
	
	union expression *reference;
	list arguments; // void * = union expression *
};

struct invoke_expression {
	enum expression_type type;
	union expression *parent;
	union expression *return_value;
	
	union expression *reference;
	list arguments; // void * = union expression *
};

struct jump_expression {
	enum expression_type type;
	union expression *parent;
	union expression *return_value;
	
	union expression *reference;
	list arguments;
	
	union expression *short_circuit;
};

struct if_expression {
	enum expression_type type;
	union expression *parent;
	union expression *return_value;
	
	union expression *condition;
	union expression *consequent;
	union expression *alternate;
};

struct constant_expression {
	enum expression_type type;
	union expression *parent;
	union expression *return_value;
	
	int value;
};

struct function_expression {
	enum expression_type type;
	union expression *parent;
	union expression *return_value;
	
	union expression *reference;
	union expression *expression;
	list parameters; //void * = union expression *
	
	list locals;
	union expression *expression_return_value;
};

struct continuation_expression {
	enum expression_type type;
	union expression *parent;
	union expression *return_value;
	
	union expression *reference;
	union expression *expression;
	list parameters; //void * = union expression *
	
	union expression *cont_instr_ref;
	bool escapes;
};

struct with_expression {
	enum expression_type type;
	union expression *parent;
	union expression *return_value;
	
	union expression *reference;
	union expression *expression;
	list parameter; //void * = union expression *
	
	union expression *cont_instr_ref;
	bool escapes;
};

struct reference_expression {
	enum expression_type type;
	union expression *parent;
	union expression *return_value;
	
	char *name;
	union expression *referent;
	union expression *offset;
};

union expression {
	struct base_expression base;
	struct begin_expression begin;
	struct function_expression function;
	struct continuation_expression continuation;
	struct with_expression with;
	struct jump_expression jump;
	struct invoke_expression invoke;
	struct instruction_expression instruction;
	struct if_expression _if;
	struct constant_expression constant;
	struct reference_expression reference;
};

union expression *make_constant(int value) {
	union expression *t = calloc(1, sizeof(union expression));
	t->constant.type = constant;
	t->constant.value = value;
	return t;
}

union expression *make_reference(char *name) {
	union expression *ref = calloc(1, sizeof(union expression));
	ref->reference.type = reference;
	ref->reference.name = name;
	ref->reference.referent = ref;
	return ref;
}

char *cprintf(const char *format, ...) {
	va_list ap, aq;
	va_start(ap, format);
	va_copy(aq, ap);
	
	char *str = calloc(vsnprintf(NULL, 0, format, ap) + 1, sizeof(char));
	vsprintf(str, format, aq);
	
	va_end(aq);
	va_end(ap);
	return str;
}

bool strequal(void *a, void *b) {
	return strcmp(a, b) == 0;
}

list generate_string_blacklist = NULL;

char *generate_string() {
	static int next_id = 0;
	char *attempt;
	while(exists(strequal, &generate_string_blacklist, attempt = cprintf("temp%u", next_id++)));
	return attempt;
}

#define generate_reference_blacklist generate_string_blacklist

union expression *generate_reference() {
	return make_reference(generate_string());
}

union expression *make_begin() {
	union expression *beg = calloc(1, sizeof(union expression));
	beg->begin.type = begin;
	beg->begin.expressions = nil();
	return beg;
}

union expression *make_function(char *name) {
	union expression *func = calloc(1, sizeof(union expression));
	func->function.type = function;
	func->function.reference = strlen(name) > 0 ? make_reference(name) : generate_reference();
	func->function.reference->reference.parent = func;
	func->function.parameters = nil();
	func->function.locals = nil();
	func->function.expression = make_begin();
	return func;
}

#define put(expr, part, val) { \
	union expression *_set_expr = expr; \
	union expression *_set_val = val; \
	_set_expr->part = _set_val; \
	_set_val->base.parent = _set_expr; \
}

union expression *use(union expression *rt) {
	union expression *r = make_reference(rt->reference.name);
	r->reference.referent = rt;
	return r;
}

union expression *prepend_parameter(char *name, union expression *function) {
	union expression *v = make_reference(name);
	v->reference.parent = function;
	v->reference.referent = v;
	prepend(v, &(function->function.parameters));
	return v;
}

union expression *make_instr(char *opcode, int arg_count, ...) {
	union expression *u = calloc(1, sizeof(union expression));
	u->instruction.type = instruction;
	u->instruction.reference = make_reference(opcode);
	u->instruction.arguments = nil();
	
	va_list valist;
	va_start(valist, arg_count);
	int i;
	
	for(i = 0; i < arg_count; i++) {
		union expression *arg = va_arg(valist, union expression *);
		arg->base.parent = u;
		append(arg, &u->instruction.arguments);
	}
	va_end(valist);
	return u;
}

//Required static argument to following function
void (*print_annotated_syntax_tree_annotator)(union expression *);

void print_annotated_syntax_tree(union expression *s) {
	switch(s->base.type) {
		case begin: {
			printf("(begin");
			print_annotated_syntax_tree_annotator(s);
			printf(" ");
			union expression *t;
			foreach(t, s->begin.expressions) {
				print_annotated_syntax_tree(t);
				printf(" ");
			}
			printf("\b)");
			break;
		} case with: {
			printf("(with-continuation");
			print_annotated_syntax_tree_annotator(s);
			printf(" ");
			print_annotated_syntax_tree(s->with.reference);
			printf(" ");
			print_annotated_syntax_tree(s->with.expression);
			printf(")");
			break;
		} case invoke: case jump: case instruction: {
			printf("%c", s->base.type == invoke ? '[' : s->base.type == jump ? '{' : '(');
			print_annotated_syntax_tree_annotator(s);
			if(s->base.type == instruction) {
				printf("%s", s->instruction.reference->reference.name);
			} else {
				print_annotated_syntax_tree(s->invoke.reference);
			}
			printf(" ");
			union expression *t;
			foreach(t, s->invoke.arguments) {
				print_annotated_syntax_tree(t);
				printf(" ");
			}
			printf("\b%c", s->base.type == invoke ? ']' : s->base.type == jump ? '}' : ')');
			break;
		} case function: case continuation: {
			printf("(%s", s->base.type == function ? "function" : "make-continuation");
			print_annotated_syntax_tree_annotator(s);
			printf(" ");
			print_annotated_syntax_tree(s->function.reference);
			printf(" ( ");
			union expression *t;
			foreach(t, s->function.parameters) {
				print_annotated_syntax_tree(t);
				printf(" ");
			}
			printf(") ");
			print_annotated_syntax_tree(s->function.expression);
			printf(")");
			break;
		} case _if: {
			printf("(if");
			print_annotated_syntax_tree_annotator(s);
			printf(" ");
			print_annotated_syntax_tree(s->_if.condition);
			printf(" ");
			print_annotated_syntax_tree(s->_if.consequent);
			printf(" ");
			print_annotated_syntax_tree(s->_if.alternate);
			printf(")");
			break;
		} case reference: {
			printf("%s", s->reference.name);
			print_annotated_syntax_tree_annotator(s);
			break;
		} case constant: {
			printf("(b");
			print_annotated_syntax_tree_annotator(s);
			printf(" %d)", s->constant.value);
			break;
		}
	}
}

void empty_annotator(union expression *s) {}

void return_value_reference_annotator(union expression *s) {
	if(s->base.return_value) {
		printf("<");
		print_annotated_syntax_tree(s->base.return_value);
		printf(">");
	}
}

void frame_layout_annotator(union expression *s) {
	if(s->base.type == function && s->base.parent) {
		printf("< ");
		union expression *t;
		{foreach(t, s->function.locals) {
			printf("%s:%d ", t->reference.name, t->reference.offset->constant.value);
		}}
		foreach(t, s->function.parameters) {
			printf("%s:%d ", t->reference.name, t->reference.offset->constant.value);
		}
		printf(">");
	}
}
