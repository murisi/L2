enum expression_type {
	storage,
	function,
	with,
	invoke,
	_if,
	begin,
	literal,
	reference,
	jump,
	continuation,
	assembly,
	non_primitive
};

enum symbol_type { static_storage, dynamic_storage, _function };

enum symbol_scope { local_scope, global_scope };

enum symbol_state { undefined_state, defined_state };

struct symbol {
	char *name;
	long int offset;
	unsigned long int size;
	enum symbol_type type;
	enum symbol_scope scope;
	enum symbol_state state;
	void *context;
};

struct symbol *make_symbol(enum symbol_type type, enum symbol_scope scope, enum symbol_state state, char *name, region r) {
	struct symbol *sym = region_alloc(r, sizeof(struct symbol));
	sym->type = type;
	sym->scope = scope;
	sym->state = state;
	sym->name = name;
	return sym;
}

struct base_expression {
	enum expression_type type;
	union expression *parent;
	struct symbol *return_symbol;
};

struct begin_expression {
	enum expression_type type;
	union expression *parent;
	struct symbol *return_symbol;
	
	list expressions; // void * = struct expression *
};

struct assembly_expression {
	enum expression_type type;
	union expression *parent;
	struct symbol *return_symbol;
	
	int opcode;
	list arguments; // void * = union expression *
};

struct storage_expression {
	enum expression_type type;
	union expression *parent;
	struct symbol *return_symbol;
	
	union expression *reference;
	list arguments; // void * = union expression *
};

struct invoke_expression {
	enum expression_type type;
	union expression *parent;
	struct symbol *return_symbol;
	
	union expression *reference;
	list arguments; // void * = union expression *
};

struct jump_expression {
	enum expression_type type;
	union expression *parent;
	struct symbol *return_symbol;
	
	union expression *reference;
	list arguments;
	
	union expression *short_circuit;
};

struct if_expression {
	enum expression_type type;
	union expression *parent;
	struct symbol *return_symbol;
	
	union expression *condition;
	union expression *consequent;
	union expression *alternate;
};

struct literal_expression {
	enum expression_type type;
	union expression *parent;
	struct symbol *return_symbol;
	
	long int value;
};

struct function_expression {
	enum expression_type type;
	union expression *parent;
	struct symbol *return_symbol;
	
	union expression *reference;
	union expression *expression;
	list parameters; //void * = union expression *
	
	list symbols;
	struct symbol *expression_return_symbol;
};

struct continuation_expression {
	enum expression_type type;
	union expression *parent;
	struct symbol *return_symbol;
	
	union expression *reference;
	union expression *expression;
	list parameters; //void * = union expression *
	
	union expression *cont_instr_ref;
	bool escapes;
};

struct with_expression {
	enum expression_type type;
	union expression *parent;
	struct symbol *return_symbol;
	
	union expression *reference;
	union expression *expression;
	list parameter; //void * = union expression *
	
	union expression *cont_instr_ref;
	bool escapes;
};

struct reference_expression {
	enum expression_type type;
	union expression *parent;
	struct symbol *return_symbol;
	
	char *name;
	union expression *referent;
	struct symbol *symbol;
};

struct np_expression {
	enum expression_type type;
	union expression *parent;
	struct symbol *return_symbol;
	
	union expression *reference;
	list argument;
	list indirections;
	list st_binds;
	list dyn_refs;
};

union expression {
	struct base_expression base;
	struct begin_expression begin;
	struct storage_expression storage;
	struct function_expression function;
	struct continuation_expression continuation;
	struct with_expression with;
	struct jump_expression jump;
	struct invoke_expression invoke;
	struct if_expression _if;
	struct literal_expression literal;
	struct reference_expression reference;
	struct np_expression non_primitive;
	struct assembly_expression assembly;
};

union expression *make_literal(unsigned long value, region reg) {
	union expression *t = region_alloc(reg, sizeof(union expression));
	t->literal.type = literal;
	t->literal.value = value;
	return t;
}

union expression *make_reference(char *name, region reg) {
	union expression *ref = region_alloc(reg, sizeof(union expression));
	ref->reference.type = reference;
	ref->reference.name = name;
	ref->reference.symbol = NULL;
	ref->reference.referent = NULL;
	return ref;
}

void refer_reference(union expression *reference, union expression *referent) {
	reference->reference.symbol = referent->reference.symbol;
}

union expression *use_symbol(struct symbol *sym, region reg) {
	union expression *ref = region_alloc(reg, sizeof(union expression));
	ref->reference.type = reference;
	ref->reference.name = sym->name;
	ref->reference.symbol = sym;
	return ref;
}

union expression *make_begin(list expressions, region reg) {
	union expression *beg = region_alloc(reg, sizeof(union expression));
	beg->begin.type = begin;
	beg->begin.expressions = expressions;
	union expression *expr;
	foreach(expr, expressions) {
		expr->base.parent = beg;
	}
	return beg;
}

#define put(expr, part, val) { \
	union expression *_set_expr = expr; \
	union expression *_set_val = val; \
	_set_expr->part = _set_val; \
	_set_val->base.parent = _set_expr; \
}

union expression *make_function(union expression *ref, list params, union expression *expr, region reg) {
	union expression *func = region_alloc(reg, sizeof(union expression));
	func->function.type = function;
	put(func, function.reference, ref);
	ref->reference.symbol = make_symbol(static_storage, local_scope, defined_state, ref->reference.name, reg);
	func->function.parameters = params;
	union expression *param;
	foreach(param, params) {
		param->base.parent = func;
		param->reference.symbol = make_symbol(dynamic_storage, local_scope, defined_state, ref->reference.name, reg);
	}
	func->function.symbols = nil;
	put(func, function.expression, expr);
	return func;
}

union expression *make_continuation(union expression *ref, list params, union expression *expr, region reg) {
	union expression *cont = region_alloc(reg, sizeof(union expression));
	cont->continuation.type = continuation;
	put(cont, continuation.reference, ref);
	ref->reference.symbol = make_symbol(dynamic_storage, local_scope, defined_state, ref->reference.name, reg);
	cont->continuation.parameters = params;
	union expression *param;
	foreach(param, params) {
		param->base.parent = cont;
		param->reference.symbol = make_symbol(dynamic_storage, local_scope, defined_state, ref->reference.name, reg);
	}
	put(cont, continuation.expression, expr);
	return cont;
}

union expression *make_with(union expression *ref, union expression *expr, region reg) {
	union expression *wth = region_alloc(reg, sizeof(union expression));
	wth->with.type = with;
	put(wth, with.reference, ref);
	ref->reference.symbol = make_symbol(dynamic_storage, local_scope, defined_state, ref->reference.name, reg);
	union expression *param = make_reference(NULL, reg);
	param->reference.parent = wth;
	param->reference.symbol = make_symbol(dynamic_storage, local_scope, defined_state, ref->reference.name, reg);
	wth->with.parameter = lst(param, nil, reg);
	put(wth, with.expression, expr);
	return wth;
}

union expression *make_asm0(int opcode, region reg) {
	union expression *u = region_alloc(reg, sizeof(union expression));
	u->assembly.type = assembly;
	u->assembly.opcode = opcode;
	u->assembly.arguments = nil;
	return u;
}

union expression *make_asm1(int opcode, union expression *arg1, region reg) {
	union expression *u = make_asm0(opcode, reg);
	u->assembly.arguments = lst(arg1, u->assembly.arguments, reg);
	arg1->base.parent = u;
	return u;
}

union expression *make_asm2(int opcode, union expression *arg1, union expression *arg2, region reg) {
	union expression *u = make_asm1(opcode, arg2, reg);
	u->assembly.arguments = lst(arg1, u->assembly.arguments, reg);
	arg1->base.parent = u;
	return u;
}

union expression *make_asm3(int opcode, union expression *arg1, union expression *arg2, union expression *arg3, region reg) {
	union expression *u = make_asm2(opcode, arg2, arg3, reg);
	u->assembly.arguments = lst(arg1, u->assembly.arguments, reg);
	arg1->base.parent = u;
	return u;
}

union expression *make_jump(union expression *ref, list args, region reg) {
	union expression *u = region_alloc(reg, sizeof(union expression));
	u->jump.type = jump;
	put(u, jump.reference, ref);
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
	put(u, jump.reference, ref);
	u->jump.arguments = nil;
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

union expression *make_storage(union expression *ref, list args, region reg) {
	union expression *u = region_alloc(reg, sizeof(union expression));
	u->storage.type = storage;
	put(u, storage.reference, ref);
	ref->reference.symbol = make_symbol(dynamic_storage, local_scope, defined_state, ref->reference.name, reg);
	u->storage.arguments = args;
	union expression *arg;
	foreach(arg, args) {
		arg->base.parent = u;
	}
	return u;
}

union expression *make_non_primitive(union expression *ref, list arg, region reg) {
	union expression *u = region_alloc(reg, sizeof(union expression));
	u->non_primitive.type = non_primitive;
	put(u, non_primitive.reference, ref);
	u->non_primitive.argument = arg;
	u->non_primitive.indirections = nil;
	u->non_primitive.st_binds = nil;
	u->non_primitive.dyn_refs = nil;
	return u;
}

union expression *make_if(union expression *condition, union expression *consequent, union expression *alternate, region reg) {
	union expression *u = region_alloc(reg, sizeof(union expression));
	u->_if.type = _if;
	put(u, _if.condition, condition);
	put(u, _if.consequent, consequent);
	put(u, _if.alternate, alternate);
	return u;
}

union expression *make_invoke(union expression *ref, list args, region reg) {
	union expression *u = region_alloc(reg, sizeof(union expression));
	u->invoke.type = invoke;
	put(u, invoke.reference, ref);
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
	put(u, invoke.reference, ref);
	u->invoke.arguments = nil;
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

void make_program_aux(union expression *expr) {
	switch(expr->base.type) {
		case begin: {
			union expression *t;
			foreach(t, expr->begin.expressions) {
				make_program_aux(t);
			}
			break;
		} case storage: {
			expr->storage.reference->reference.symbol->type = static_storage;
			expr->storage.reference->reference.symbol->scope = expr->storage.parent->base.parent->base.parent ?
				local_scope : global_scope;
			union expression *t;
			foreach(t, expr->storage.arguments) {
				make_program_aux(t);
			}
			break;
		} case jump: case invoke: {
			make_program_aux(expr->invoke.reference);
			union expression *t;
			foreach(t, expr->invoke.arguments) {
				make_program_aux(t);
			}
			break;
		} case non_primitive: {
			make_program_aux(expr->non_primitive.reference);
			break;
		} case continuation: case with: {
			expr->continuation.reference->reference.symbol->type = static_storage;
			union expression *t;
			foreach(t, expr->continuation.parameters) {
				t->reference.symbol->type = static_storage;
			}
			make_program_aux(expr->continuation.expression);
			break;
		} case function: {
			break;
		}
	}
}

union expression *make_program(list exprs, region r) {
	union expression *program = make_function(make_reference(NULL, r), nil, make_begin(exprs, r), r);
	make_program_aux(program->function.expression);
	return program;
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
