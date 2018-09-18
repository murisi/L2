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
	Symbol *symbol;
};

struct np_expression {
	enum expression_type type;
	union expression *parent;
	union expression *return_value;
	
	union expression *reference;
	list argument;
	list indirections;
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
	union expression *t = region_alloc(reg, sizeof(union expression));
	t->literal.type = literal;
	t->literal.value = value;
	return t;
}

union expression *make_reference(region reg) {
	union expression *ref = region_alloc(reg, sizeof(union expression));
	ref->reference.type = reference;
	ref->reference.name = NULL;
	ref->reference.symbol = NULL;
	ref->reference.referent = ref;
	return ref;
}

union expression *use_reference(union expression *referent, region reg) {
	union expression *ref = region_alloc(reg, sizeof(union expression));
	ref->reference.type = reference;
	ref->reference.name = referent->reference.name;
	ref->reference.referent = referent;
	return ref;
}

bool strequal(void *a, void *b) {
	return strcmp(a, b) == 0;
}

union expression *make_begin(region reg) {
	union expression *beg = region_alloc(reg, sizeof(union expression));
	beg->begin.type = begin;
	beg->begin.expressions = nil(reg);
	return beg;
}

#define put(expr, part, val) { \
	union expression *_set_expr = expr; \
	union expression *_set_val = val; \
	_set_expr->part = _set_val; \
	_set_val->base.parent = _set_expr; \
}

#define append_expr(val, expr, part, reg) { \
	union expression *_set_expr = expr; \
	union expression *_set_val = val; \
	append(_set_val, &(_set_expr->part), reg); \
	_set_val->base.parent = _set_expr; \
}

union expression *make_function(region reg) {
	union expression *func = region_alloc(reg, sizeof(union expression));
	func->function.type = function;
	func->function.reference = make_reference(reg);
	func->function.reference->reference.parent = func;
	func->function.parameters = nil(reg);
	func->function.locals = nil(reg);
	put(func, function.expression, make_begin(reg));
	return func;
}

union expression *make_continuation(region reg) {
	union expression *cont = region_alloc(reg, sizeof(union expression));
	cont->continuation.type = continuation;
	cont->continuation.reference = make_reference(reg);
	cont->continuation.reference->reference.parent = cont;
	cont->continuation.parameters = nil(reg);
	put(cont, continuation.expression, make_begin(reg));
	return cont;
}

union expression *make_with(region reg) {
	union expression *wth = region_alloc(reg, sizeof(union expression));
	wth->with.type = with;
	wth->with.reference = make_reference(reg);
	wth->with.reference->reference.parent = wth;
	union expression *param = make_reference(reg);
	param->reference.parent = wth;
	wth->with.parameter = lst(param, nil(reg), reg);
	put(wth, with.expression, make_begin(reg));
	return wth;
}

union expression *use(int opcode, region reg) {
	union expression *u = region_alloc(reg, sizeof(union expression));
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
	union expression *u = region_alloc(reg, sizeof(union expression));
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

union expression *make_jump(union expression *ref, list args, region reg) {
	union expression *u = region_alloc(reg, sizeof(union expression));
	u->jump.type = jump;
	u->jump.reference = ref;
	ref->base.parent = u;
	u->jump.arguments = args;
	union expression *arg;
	foreach(arg, args) {
		arg->base.parent = u;
	}
	return u;
}

union expression *make_jump0(union expression *ref, region reg) {
	union expression *u = region_alloc(reg, sizeof(union expression));
	u->jump.type = jump;
	u->jump.reference = ref;
	ref->base.parent = u;
	u->jump.arguments = nil(reg);
	return u;
}

union expression *make_jump1(union expression *ref, union expression *arg1, region reg) {
	union expression *u = make_jump0(ref, reg);
	u->jump.arguments = lst(arg1, u->jump.arguments, reg);
	arg1->base.parent = u;
	return u;
}

union expression *make_jump2(union expression *ref, union expression *arg1, union expression *arg2, region reg) {
	union expression *u = make_jump1(ref, arg2, reg);
	u->jump.arguments = lst(arg1, u->jump.arguments, reg);
	arg1->base.parent = u;
	return u;
}

union expression *make_invoke(union expression *ref, list args, region reg) {
	union expression *u = region_alloc(reg, sizeof(union expression));
	u->invoke.type = invoke;
	u->invoke.reference = ref;
	ref->base.parent = u;
	u->invoke.arguments = args;
	union expression *arg;
	foreach(arg, args) {
		arg->base.parent = u;
	}
	return u;
}

union expression *make_invoke0(union expression *ref, region reg) {
	union expression *u = region_alloc(reg, sizeof(union expression));
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

union expression *make_invoke8(union expression *ref, union expression *arg1, union expression *arg2, union expression *arg3, union expression *arg4, union expression *arg5, union expression *arg6, union expression *arg7, union expression *arg8, region reg) {
	union expression *u = make_invoke7(ref, arg2, arg3, arg4, arg5, arg6, arg7, arg8, reg);
	u->invoke.arguments = lst(arg1, u->invoke.arguments, reg);
	arg1->base.parent = u;
	return u;
}

union expression *make_invoke9(union expression *ref, union expression *arg1, union expression *arg2, union expression *arg3, union expression *arg4, union expression *arg5, union expression *arg6, union expression *arg7, union expression *arg8, union expression *arg9, region reg) {
	union expression *u = make_invoke8(ref, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, reg);
	u->invoke.arguments = lst(arg1, u->invoke.arguments, reg);
	arg1->base.parent = u;
	return u;
}

void print_syntax_tree(union expression *s) {
	switch(s->base.type) {
		case begin: {
			write_str(STDOUT, "(begin ");
			union expression *t;
			foreach(t, s->begin.expressions) {
				print_syntax_tree(t);
				write_str(STDOUT, " ");
			}
			write_str(STDOUT, "\b)");
			break;
		} case with: {
			write_str(STDOUT, "(with ");
			print_syntax_tree(s->with.reference);
			write_str(STDOUT, " ");
			print_syntax_tree(s->with.expression);
			write_str(STDOUT, ")");
			break;
		} case invoke: case jump: {
			write_char(STDOUT, s->base.type == invoke ? '[' : s->base.type == jump ? '{' : '(');
			print_syntax_tree(s->invoke.reference);
			write_str(STDOUT, " ");
			union expression *t;
			foreach(t, s->invoke.arguments) {
				print_syntax_tree(t);
				write_str(STDOUT, " ");
			}
			write_str(STDOUT, "\b");
			write_char(STDOUT, s->base.type == invoke ? ']' : s->base.type == jump ? '}' : ')');
			break;
		} case function: case continuation: {
			write_str(STDOUT, "(");
			write_str(STDOUT, s->base.type == function ? "function" : "continuation");
			write_str(STDOUT, " ");
			print_syntax_tree(s->function.reference);
			write_str(STDOUT, " ( ");
			union expression *t;
			foreach(t, s->function.parameters) {
				print_syntax_tree(t);
				write_str(STDOUT, " ");
			}
			write_str(STDOUT, ") ");
			print_syntax_tree(s->function.expression);
			write_str(STDOUT, ")");
			break;
		} case _if: {
			write_str(STDOUT, "(if ");
			print_syntax_tree(s->_if.condition);
			write_str(STDOUT, " ");
			print_syntax_tree(s->_if.consequent);
			write_str(STDOUT, " ");
			print_syntax_tree(s->_if.alternate);
			write_str(STDOUT, ")");
			break;
		} case reference: {
			if(s->reference.name) {
				write_str(STDOUT, s->reference.name);
			} else {
				write_str(STDOUT, "()");
			}
			break;
		} case literal: {
			write_str(STDOUT, "(literal ");
			write_ulong(STDOUT, s->literal.value);
			write_str(STDOUT, ")");
			break;
		} case non_primitive: {
			write_str(STDOUT, "(");
			print_syntax_tree(s->non_primitive.reference);
			write_str(STDOUT, " ");
			print_expr_list(s->non_primitive.argument);
			write_str(STDOUT, ")");
			break;
		}
	}
}

void *_get_(void *ref) {
	return *((void **) ref);
}

bool defined_string_equals(char *a, char *b) {
	return a && b && !strcmp(a, b);
}

union expression *insert_indirections(union expression *expr, char *ref_name, region reg) {
	switch(expr->base.type) {
		case literal: {
			return expr;
		} case reference: {
			if(defined_string_equals(expr->reference.name, ref_name)) {
				return make_invoke1(make_literal((unsigned long) _get_, reg), expr, reg);
			} else {
				return expr;
			}
		} case _if: {
			put(expr, _if.condition, insert_indirections(expr->_if.condition, ref_name, reg));
			put(expr, _if.consequent, insert_indirections(expr->_if.consequent, ref_name, reg));
			put(expr, _if.alternate, insert_indirections(expr->_if.alternate, ref_name, reg));
			return expr;
		} case begin: {
			union expression *f;
			{foreach(f, expr->begin.expressions) {
				if(f->base.type == function && defined_string_equals(f->function.reference->reference.name, ref_name)) {
					return expr;
				}
			}}
			union expression **e;
			{foreachaddress(e, expr->begin.expressions) {
				*e = insert_indirections(*e, ref_name, reg);
				(*e)->base.parent = expr;
			}}
			return expr;
		} case continuation: case with: {
			if(defined_string_equals(expr->continuation.reference->reference.name, ref_name)) {
				return expr;
			}
			union expression *e;
			foreach(e, expr->continuation.parameters) {
				if(defined_string_equals(e->reference.name, ref_name)) {
					return expr;
				}
			}
			put(expr, continuation.expression, insert_indirections(expr->continuation.expression, ref_name, reg));
			return expr;
		} case function: {
			return expr;
		} case invoke: case jump: {
			put(expr, invoke.reference, insert_indirections(expr->invoke.reference, ref_name, reg));
			union expression **e;
			foreachaddress(e, expr->invoke.arguments) {
				*e = insert_indirections(*e, ref_name, reg);
				(*e)->base.parent = expr;
			}
			return expr;
		} case non_primitive: {
			prepend(ref_name, &expr->non_primitive.indirections, reg);
			return expr;
		}
	}
}

union expression *make_program(list exprs, region r) {
	union expression *program = make_function(r);
	program->function.expression->begin.expressions = exprs;
	union expression *expr;
	{foreach(expr, exprs) {
		expr->base.parent = program->function.expression;
	}}
	return program;
}
