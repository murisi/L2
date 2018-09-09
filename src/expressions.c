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
	
	union expression *reference;
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

union expression *make_literal(unsigned long value, region reg) {
	union expression *t = region_malloc(reg, sizeof(union expression));
	t->literal.type = literal;
	t->literal.value = value;
	return t;
}

union expression *make_reference(region reg) {
	union expression *ref = region_malloc(reg, sizeof(union expression));
	ref->reference.type = reference;
	ref->reference.name = NULL;
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

union expression *make_invoke0(union expression *ref, region reg) {
	union expression *u = region_malloc(reg, sizeof(union expression));
	u->invoke.type = invoke;
	u->invoke.reference = ref;
	ref->base.parent = u;
	u->invoke.arguments = nil(reg);
	return u;
}

union expression *make_invoke1(union expression *ref, union expression *arg1, region reg) {
	union expression *u = make_invoke0(ref, reg);
	u->invoke.arguments = lst(arg1, u->invoke.arguments, reg);
	arg1->base.parent = u;
	return u;
}

union expression *make_invoke2(union expression *ref, union expression *arg1, union expression *arg2, region reg) {
	union expression *u = make_invoke1(ref, arg2, reg);
	u->invoke.arguments = lst(arg1, u->invoke.arguments, reg);
	arg1->base.parent = u;
	return u;
}

union expression *make_invoke3(union expression *ref, union expression *arg1, union expression *arg2, union expression *arg3, region reg) {
	union expression *u = make_invoke2(ref, arg2, arg3, reg);
	u->invoke.arguments = lst(arg1, u->invoke.arguments, reg);
	arg1->base.parent = u;
	return u;
}

union expression *make_invoke4(union expression *ref, union expression *arg1, union expression *arg2, union expression *arg3, union expression *arg4, region reg) {
	union expression *u = make_invoke3(ref, arg2, arg3, arg4, reg);
	u->invoke.arguments = lst(arg1, u->invoke.arguments, reg);
	arg1->base.parent = u;
	return u;
}

union expression *make_invoke5(union expression *ref, union expression *arg1, union expression *arg2, union expression *arg3, union expression *arg4, union expression *arg5, region reg) {
	union expression *u = make_invoke4(ref, arg2, arg3, arg4, arg5, reg);
	u->invoke.arguments = lst(arg1, u->invoke.arguments, reg);
	arg1->base.parent = u;
	return u;
}

union expression *make_invoke6(union expression *ref, union expression *arg1, union expression *arg2, union expression *arg3, union expression *arg4, union expression *arg5, union expression *arg6, region reg) {
	union expression *u = make_invoke5(ref, arg2, arg3, arg4, arg5, arg6, reg);
	u->invoke.arguments = lst(arg1, u->invoke.arguments, reg);
	arg1->base.parent = u;
	return u;
}

union expression *make_invoke7(union expression *ref, union expression *arg1, union expression *arg2, union expression *arg3, union expression *arg4, union expression *arg5, union expression *arg6, union expression *arg7, region reg) {
	union expression *u = make_invoke6(ref, arg2, arg3, arg4, arg5, arg6, arg7, reg);
	u->invoke.arguments = lst(arg1, u->invoke.arguments, reg);
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
			print_syntax_tree(s->non_primitive.reference);
			mywrite_str(STDOUT, " ");
			print_expr_list(s->non_primitive.argument);
			mywrite_str(STDOUT, ")");
			break;
		}
	}
}

bool expression_equals(union expression *expr1, union expression *expr2) {
	if(expr1->base.type != expr2->base.type) return false;
	switch(expr1->base.type) {
		case literal: {
			return expr1->literal.value == expr2->literal.value ? true : false;
		} case reference: {
			return (expr1->reference.name == expr2->reference.name) ||
				(expr1->reference.name && expr2->reference.name && strcmp(expr1->reference.name, expr2->reference.name) == 0) ?
				true : false;
		} case invoke: case jump: {
			if(!expression_equals(expr1->invoke.reference, expr2->invoke.reference)) return false;
			if(length(expr1->invoke.arguments) != length(expr2->invoke.arguments)) return false;
			union expression *u, *v;
			foreachzipped(u, v, expr1->invoke.arguments, expr2->invoke.arguments) {
				if(!expression_equals(u, v)) return false;
			}
			return true;
		} case _if: {
			return (expression_equals(expr1->_if.condition, expr2->_if.condition) &&
				expression_equals(expr1->_if.consequent, expr2->_if.consequent) &&
				expression_equals(expr1->_if.alternate, expr2->_if.alternate)) ? true : false;
		} case function: case continuation: case with: {
			if(!expression_equals(expr1->function.reference, expr2->function.reference)) return false;
			if(!expression_equals(expr1->function.expression, expr2->function.expression)) return false;
			union expression *u, *v;
			foreachzipped(u, v, expr1->function.parameters, expr2->function.parameters) {
				if(!expression_equals(u, v)) return false;
			}
			return true;
		} case non_primitive: case instruction: {
			return false;
		}
	}
}

union expression *copy_expression(union expression *expr, region reg) {
	union expression *copy = region_malloc(reg, sizeof(union expression));
	copy->base.type = expr->base.type;
	switch(expr->base.type) {
		case literal: {
			copy->literal.value = expr->literal.value;
			break;
		} case reference: {
			copy->reference.name = expr->reference.name;
			break;
		} case invoke: case jump: {
			put(copy, invoke.reference, copy_expression(expr->invoke.reference, reg));
			copy->invoke.arguments = nil(reg);
			union expression *arg;
			foreach(arg, expr->invoke.arguments) {
				union expression *arg_copy = copy_expression(arg, reg);
				append(arg_copy, &copy->invoke.arguments, reg);
				arg_copy->base.parent = copy;
			}
			break;
		} case _if: {
			put(copy, _if.condition, copy_expression(expr->_if.condition, reg));
			put(copy, _if.consequent, copy_expression(expr->_if.consequent, reg));
			put(copy, _if.alternate, copy_expression(expr->_if.alternate, reg));
			break;
		} case function: case continuation: case with: {
			put(copy, function.reference, copy_expression(expr->function.reference, reg));
			put(copy, function.expression, copy_expression(expr->function.expression, reg));
			if(expr->base.type == function) {
				copy->function.locals = nil(reg);
			}
			copy->function.parameters = nil(reg);
			union expression *param;
			foreach(param, expr->function.parameters) {
				union expression *param_copy = copy_expression(param, reg);
				append(param_copy, &copy->function.parameters, reg);
				param_copy->reference.parent = copy;
			}
			break;
		} case begin: {
			copy->begin.expressions = nil(reg);
			union expression *e;
			foreach(e, expr->begin.expressions) {
				union expression *e_copy = copy_expression(e, reg);
				append(e_copy, &copy->begin.expressions, reg);
				e_copy->base.parent = copy;
			}
			break;
		} case non_primitive: {
			put(copy, non_primitive.reference, copy_expression(expr->non_primitive.reference, reg));
			copy->non_primitive.argument = copy_sexpr_list(expr->non_primitive.argument, reg);
			break;
		}
	}
	return copy;
}
