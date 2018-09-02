enum expression_type {
	function,
	with,
	invoke,
	_if,
	begin,
	literal,
	reference,
	jump,
	continuation,
	instruction,
	non_primitive
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
	
	int opcode;
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

struct literal_expression {
	enum expression_type type;
	union expression *parent;
	union expression *return_value;
	
	long int value;
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
	void *context;
};

struct np_expression {
	enum expression_type type;
	union expression *parent;
	union expression *return_value;
	
	union expression *function;
	list argument;
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
	struct literal_expression literal;
	struct reference_expression reference;
	struct np_expression non_primitive;
};

union expression *make_literal(long int value, region reg) {
	union expression *t = region_malloc(reg, sizeof(union expression));
	t->literal.type = literal;
	t->literal.value = value;
	return t;
}

union expression *make_reference(region reg) {
	union expression *ref = region_malloc(reg, sizeof(union expression));
	ref->reference.type = reference;
	ref->reference.name = "";
	ref->reference.referent = ref;
	return ref;
}

bool strequal(void *a, void *b) {
	return strcmp(a, b) == 0;
}

union expression *make_begin(region reg) {
	union expression *beg = region_malloc(reg, sizeof(union expression));
	beg->begin.type = begin;
	beg->begin.expressions = nil(reg);
	return beg;
}

union expression *make_function(region reg) {
	union expression *func = region_malloc(reg, sizeof(union expression));
	func->function.type = function;
	func->function.reference = make_reference(reg);
	func->function.reference->reference.parent = func;
	func->function.parameters = nil(reg);
	func->function.locals = nil(reg);
	func->function.expression = make_begin(reg);
	return func;
}

#define put(expr, part, val) { \
	union expression *_set_expr = expr; \
	union expression *_set_val = val; \
	_set_expr->part = _set_val; \
	_set_val->base.parent = _set_expr; \
}

union expression *use(int opcode, region reg) {
	union expression *u = region_malloc(reg, sizeof(union expression));
	u->instruction.type = instruction;
	u->instruction.opcode = opcode;
	u->instruction.arguments = nil(reg);
	return u;
}

union expression *prepend_parameter(union expression *function, region reg) {
	union expression *v = make_reference(reg);
	v->reference.parent = function;
	v->reference.referent = v;
	prepend(v, &(function->function.parameters), reg);
	return v;
}

union expression *make_instr0(int opcode, region reg) {
	union expression *u = region_malloc(reg, sizeof(union expression));
	u->instruction.type = instruction;
	u->instruction.opcode = opcode;
	u->instruction.arguments = nil(reg);
	return u;
}

union expression *make_instr1(int opcode, union expression *arg1, region reg) {
	union expression *u = make_instr0(opcode, reg);
	u->instruction.arguments = lst(arg1, u->instruction.arguments, reg);
	arg1->base.parent = u;
	return u;
}

union expression *make_instr2(int opcode, union expression *arg1, union expression *arg2, region reg) {
	union expression *u = make_instr1(opcode, arg2, reg);
	u->instruction.arguments = lst(arg1, u->instruction.arguments, reg);
	arg1->base.parent = u;
	return u;
}

union expression *make_instr3(int opcode, union expression *arg1, union expression *arg2, union expression *arg3, region reg) {
	union expression *u = make_instr2(opcode, arg2, arg3, reg);
	u->instruction.arguments = lst(arg1, u->instruction.arguments, reg);
	arg1->base.parent = u;
	return u;
}

void print_syntax_tree(union expression *s) {
	switch(s->base.type) {
		case begin: {
			mywrite_str(STDOUT, "(begin ");
			union expression *t;
			foreach(t, s->begin.expressions) {
				print_syntax_tree(t);
				mywrite_str(STDOUT, " ");
			}
			mywrite_str(STDOUT, "\b)");
			break;
		} case with: {
			mywrite_str(STDOUT, "(with-continuation ");
			print_syntax_tree(s->with.reference);
			mywrite_str(STDOUT, " ");
			print_syntax_tree(s->with.expression);
			mywrite_str(STDOUT, ")");
			break;
		} case invoke: case jump: {
			mywrite_char(STDOUT, s->base.type == invoke ? '[' : s->base.type == jump ? '{' : '(');
			print_syntax_tree(s->invoke.reference);
			mywrite_str(STDOUT, " ");
			union expression *t;
			foreach(t, s->invoke.arguments) {
				print_syntax_tree(t);
				mywrite_str(STDOUT, " ");
			}
			mywrite_str(STDOUT, "\b");
			mywrite_char(STDOUT, s->base.type == invoke ? ']' : s->base.type == jump ? '}' : ')');
			break;
		} case function: case continuation: {
			mywrite_str(STDOUT, "(");
			mywrite_str(STDOUT, s->base.type == function ? "function" : "make-continuation");
			mywrite_str(STDOUT, " ");
			print_syntax_tree(s->function.reference);
			mywrite_str(STDOUT, " ( ");
			union expression *t;
			foreach(t, s->function.parameters) {
				print_syntax_tree(t);
				mywrite_str(STDOUT, " ");
			}
			mywrite_str(STDOUT, ") ");
			print_syntax_tree(s->function.expression);
			mywrite_str(STDOUT, ")");
			break;
		} case _if: {
			mywrite_str(STDOUT, "(if ");
			print_syntax_tree(s->_if.condition);
			mywrite_str(STDOUT, " ");
			print_syntax_tree(s->_if.consequent);
			mywrite_str(STDOUT, " ");
			print_syntax_tree(s->_if.alternate);
			mywrite_str(STDOUT, ")");
			break;
		} case reference: {
			mywrite_str(STDOUT, s->reference.name);
			break;
		} case literal: {
			mywrite_str(STDOUT, "(b ");
			mywrite_int(STDOUT, s->literal.value);
			mywrite_str(STDOUT, ")");
			break;
		} case non_primitive: {
			mywrite_str(STDOUT, "(");
			print_syntax_tree(s->non_primitive.function);
			mywrite_str(STDOUT, " ");
			print_expr_list(s->non_primitive.argument);
			mywrite_str(STDOUT, ")");
			break;
		}
	}
}
