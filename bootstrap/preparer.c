bool defined_string_equals(char *a, char *b) {
	return a && b && !strcmp(a, b);
}

bool reference_named(union expression *expr, char *ctx) {
	return defined_string_equals(expr->reference.name, ctx);
}

union expression *vfind_multiple_definitions(union expression *e, void *ctx) {
	jumpbuf *handler = ctx;
	union expression *t;
	list *partial;
	region tempreg = create_buffer(0);
	switch(e->base.type) {
		case begin: {
			list definitions = nil;
			foreach(t, e->begin.expressions) {
				if(t->base.type == storage || t->base.type == function) {
					prepend(t->storage.reference->reference.name, &definitions, tempreg);
				}
			}
			char *n;
			foreachlist(partial, n, &definitions) {
				if(exists((bool (*)(void *, void *)) defined_string_equals, &(*partial)->rst, n)) {
					throw_multiple_definition(n, handler);
				}
			}
			break;
		} case continuation: case function: {
			list ref_with_params = lst(e->continuation.reference, e->continuation.parameters, tempreg);
			foreachlist(partial, t, &ref_with_params) {
				if(exists((bool (*)(void *, void *)) reference_named, &(*partial)->rst, t->reference.name)) {
					throw_multiple_definition(t->reference.name, handler);
				}
			}
			break;
		}
	}
	destroy_buffer(tempreg);
	return e;
}

union expression *get_parent_function(union expression *n) {
	do {
		n = n->base.parent;
	} while(n->base.type != function);
	return n;
}

bool reference_equals(union expression *a, union expression *b) {
	return a == b || (a->reference.name && b->reference.name && !strcmp(a->reference.name, b->reference.name));
}

struct symbol *symbol_of(union expression *reference) {
	bool same_func = true;
	union expression *t;
	for(t = reference; t != NULL; t = t->base.parent) {
		//Goal of following logic is to maximize what is in the scope of a reference without the overhead of access
		//links.
		switch(t->base.type) {
			case begin: {
				union expression *u;
				foreach(u, t->begin.expressions) {
					//Either link up to function expression contained in ancestral begin expression or link up to
					//storage expression in ancestral expression that is (in the same stack-frame as the reference
					//or has static storage).
					if((u->base.type == function || (u->base.type == storage && (same_func ||
							u->storage.reference->reference.symbol->type == static_storage))) &&
							reference_equals(u->function.reference, reference)) {
						return u->function.reference->reference.symbol;
					}
				}
				break;
			} case function: {
				//Either link up to ancestral function expression reference or link up to the parameters of (the
				//root function or (the function that provides the stack-frame for this reference.) 
				if(reference_equals(t->function.reference, reference)) {
					return t->function.reference->reference.symbol;
				}
				union expression *u;
				foreach(u, t->function.parameters) {
					if((same_func || u->reference.symbol->type == static_storage) && reference_equals(u, reference)) {
						return u->reference.symbol;
					}
				}
				same_func = false;
				break;
			} case continuation: case with: case storage: {
				//Either link up to (the reference of continuation/with/storage expression in the same stack-frame
				//or the reference of the same with static storage) or link up to the parameters of a non-storage
				//expression that are either (in the same stack-fram or have static storage).
				if((same_func || t->continuation.reference->reference.symbol->type == static_storage) &&
						reference_equals(t->function.reference, reference)) {
					return t->function.reference->reference.symbol;
				} else if(t->base.type != storage) {
					union expression *u;
					foreach(u, t->function.parameters) {
						if((same_func || u->reference.symbol->type == static_storage) && reference_equals(u, reference)) {
							return u->reference.symbol;
						}
					}
				}
				break;
			}
		}
	}
	return NULL;
}

bool is_jump_reference(union expression *s) {
	return s->base.parent->base.type == jump && s->base.parent->jump.reference == s;
}

bool is_invoke_reference(union expression *s) {
	return s->base.parent->base.type == invoke && s->base.parent->invoke.reference == s;
}

bool is_c_reference(union expression *s) {
	return (s->base.parent->base.type == continuation || s->base.parent->base.type == with) && s->base.parent->continuation.reference == s;
}

bool is_function_reference(union expression *s) {
	return s->base.parent->base.type == function && s->base.parent->function.reference == s;
}

union expression *target_expression(union expression *s) {
	return s->reference.symbol->definition->reference.parent;
}

union expression *root_function_of(union expression *s) {
	for(; s->base.parent; s = s->base.parent);
	return s;
}

union expression *vlink_references(union expression *s, void *ctx) {
	jumpbuf *handler = ((void **) ctx)[0];
	region r = ((void **) ctx)[1];
	if(s->base.type == reference) {
		s->reference.symbol = s->reference.symbol ? s->reference.symbol : symbol_of(s);
		if(!s->reference.symbol) {
			union expression *ref = make_reference(s->reference.name, r);
			struct symbol *sym = make_symbol(static_storage, global_scope, undefined_state, s->reference.name, ref, r);
			ref->reference.symbol = sym;
			prepend(ref, &root_function_of(s)->function.parameters, r);
			ref->reference.parent = root_function_of(s);
			s->reference.symbol = sym;
		} else if(((is_jump_reference(s) && is_c_reference(s->reference.symbol->definition)) ||
			(is_invoke_reference(s) && is_function_reference(s->reference.symbol->definition))) &&
			length(s->reference.parent->jump.arguments) != length(target_expression(s)->continuation.parameters)) {
				throw_param_count_mismatch(s->reference.parent, target_expression(s), handler);
		}
	} else if(((s->base.type == continuation && is_jump_reference(s)) ||
		(s->base.type == function && is_invoke_reference(s))) &&
		length(s->continuation.parent->jump.arguments) != length(s->continuation.parameters)) {
			throw_param_count_mismatch(s->continuation.parent, s, handler);
	}
	return s;
}

void vescape_analysis_aux(union expression *ref, union expression *target) {
	if(is_jump_reference(ref)) {
		ref->reference.parent->jump.short_circuit = target;
	} else {
		target->continuation.escapes = true;
	}
}

union expression *vescape_analysis(union expression *s, void *ctx) {
	if(s->base.type == reference && s->reference.symbol->definition != s && is_c_reference(s->reference.symbol->definition)) {
		vescape_analysis_aux(s, target_expression(s));
	} else if(s->base.type == continuation) {
		vescape_analysis_aux(s, s);

	}
	return s;
}

void visit_expressions(union expression *(*visitor)(union expression *, void *), union expression **s, void *ctx) {
	switch((*s)->base.type) {
		case begin: {
			union expression **t;
			foreachaddress(t, (*s)->begin.expressions) {
				visit_expressions(visitor, t, ctx);
			}
			break;
		} case _if: {
			visit_expressions(visitor, &(*s)->_if.condition, ctx);
			visit_expressions(visitor, &(*s)->_if.consequent, ctx);
			visit_expressions(visitor, &(*s)->_if.alternate, ctx);
			break;
		} case function: case continuation: case with: {
			visit_expressions(visitor, &(*s)->function.expression, ctx);
			break;
		} case jump: case invoke: case storage: {
			if((*s)->base.type != storage) {
				visit_expressions(visitor, &(*s)->invoke.reference, ctx);
			}
			union expression **t;
			foreachaddress(t, (*s)->invoke.arguments) {
				visit_expressions(visitor, t, ctx);
			}
			break;
		}
	}
	union expression *parent = (*s)->base.parent;
	*s = (*visitor)(*s, ctx);
	(*s)->base.parent = parent;
}

void pre_visit_expressions(union expression *(*visitor)(union expression *, void *), union expression **s, void *ctx) {
	union expression *parent = (*s)->base.parent;
	*s = (*visitor)(*s, ctx);
	(*s)->base.parent = parent;
	
	switch((*s)->base.type) {
		case begin: {
			union expression **t;
			foreachaddress(t, (*s)->begin.expressions) {
				pre_visit_expressions(visitor, t, ctx);
			}
			break;
		} case _if: {
			pre_visit_expressions(visitor, &(*s)->_if.condition, ctx);
			pre_visit_expressions(visitor, &(*s)->_if.consequent, ctx);
			pre_visit_expressions(visitor, &(*s)->_if.alternate, ctx);
			break;
		} case function: case continuation: case with: {
			pre_visit_expressions(visitor, &(*s)->function.expression, ctx);
			break;
		} case jump: case invoke: case storage: {
			if((*s)->base.type != storage) {
				pre_visit_expressions(visitor, &(*s)->invoke.reference, ctx);
			}
			union expression **t;
			foreachaddress(t, (*s)->invoke.arguments) {
				pre_visit_expressions(visitor, t, ctx);
			}
			break;
		}
	}
}

void classify_program_symbols(union expression *expr) {
	switch(expr->base.type) {
		case begin: {
			union expression *t;
			foreach(t, expr->begin.expressions) {
				classify_program_symbols(t);
			}
			break;
		} case storage: case jump: case invoke: {
			if(expr->base.type == storage) {
				expr->storage.reference->reference.symbol->type = static_storage;
			} else {
				classify_program_symbols(expr->invoke.reference);
			}
			union expression *t;
			foreach(t, expr->storage.arguments) {
				classify_program_symbols(t);
			}
			break;
		} case continuation: case with: {
			expr->continuation.reference->reference.symbol->type = static_storage;
			union expression *t;
			foreach(t, expr->continuation.parameters) {
				t->reference.symbol->type = static_storage;
			}
			classify_program_symbols(expr->continuation.expression);
			break;
		} case function: {
			break;
		} case _if: {
			classify_program_symbols(expr->_if.condition);
			classify_program_symbols(expr->_if.consequent);
			classify_program_symbols(expr->_if.alternate);
			break;
		}
	}
}

void *_get_(void *ref) {
	return *((void **) ref);
}

void _set_(unsigned long *ref, unsigned long val) {
	*ref = val;
}

struct expansion_context {
	list symbols;
	region expr_buf;
	region obj_buf;
	jumpbuf *handler;
};

bool symbol_equals(object_symbol *sym1, object_symbol *sym2) {
	return !strcmp(sym1->name, sym2->name);
}

Object *load_program_and_mutate(union expression *program, struct expansion_context *ectx);

union expression *vgenerate_metas(union expression *s, void *ctx) {
	struct expansion_context *ectx = ctx;
	if(s->base.type == meta) {
		object_symbol *sym;
		foreach(sym, ectx->symbols) {
			if(!strcmp(sym->name, s->meta.reference->reference.name)) {
				return vgenerate_metas(build_expression(((list (*)(list, region)) sym->address)(s->meta.argument, ectx->expr_buf),
					ectx->expr_buf, ectx->handler), ctx);
			}
		}
		throw_undefined_reference(s->meta.reference->reference.name, ectx->handler);
	} else {
		return s;
	}
}

void *init_storage(unsigned long *data, union expression *storage_expr, struct expansion_context *ectx, void **cache) {
	if(*cache) {
		return *cache;
	} else {
		list sets = nil;
		union expression *arg;
		foreach(arg, storage_expr->storage.arguments) {
			pre_visit_expressions(vgenerate_metas, &arg, ectx);
			append(make_invoke2(make_literal((unsigned long) _set_, ectx->expr_buf),
				make_literal((unsigned long) data++, ectx->expr_buf), arg, ectx->expr_buf), &sets, ectx->expr_buf);
		}
		*cache = segment(load_program_and_mutate(make_program(sets, ectx->expr_buf), ectx), ".text");
		return *cache;
	}
}

void *init_function(union expression *function_expr, struct expansion_context *ectx, void **cache) {
	if(*cache) {
		return *cache;
	} else {
		pre_visit_expressions(vgenerate_metas, &function_expr, ectx);
		load_program_and_mutate(make_program(lst(function_expr, nil, ectx->expr_buf), ectx->expr_buf), ectx);
		*cache = (void *) function_expr->function.reference->reference.symbol->offset;
		return *cache;
	}
}

void *init_expression(union expression *expr, struct expansion_context *ectx, void **cache) {
	if(*cache) {
		return *cache;
	} else {
		pre_visit_expressions(vgenerate_metas, &expr, ectx);
		*cache = segment(load_program_and_mutate(make_program(lst(expr, nil, ectx->expr_buf), ectx->expr_buf), ectx), ".text");
		return *cache;
	}
}

union expression *generate_metaprogram(union expression *program, struct expansion_context *ectx) {
	union expression *s;
	list c = nil;
	foreach(s, program->function.expression->begin.expressions) {
		void **cache = buffer_alloc(ectx->obj_buf, sizeof(void *));
		*cache = NULL;
		if(s->base.type == storage) {
			list args = nil;
			int i;
			for(i = 0; i < length(s->storage.arguments); i++) {
				prepend(make_begin(nil, ectx->expr_buf), &args, ectx->expr_buf);
			}
			union expression *storage_ref = make_reference(s->storage.reference->reference.name, ectx->expr_buf);
			append(make_storage(storage_ref, args, ectx->expr_buf), &c, ectx->expr_buf);
			union expression *storage_ref_arg = make_reference(NULL, ectx->expr_buf);
			refer_reference(storage_ref_arg, storage_ref);
			append(make_invoke0(make_invoke4(make_literal((unsigned long) init_storage, ectx->expr_buf), storage_ref_arg,
				make_literal((unsigned long) s, ectx->expr_buf), make_literal((unsigned long) ectx, ectx->expr_buf),
				make_literal((unsigned long) cache, ectx->expr_buf), ectx->expr_buf), ectx->expr_buf), &c, ectx->expr_buf);
		} else if(s->base.type == function) {
			list params = nil, args = nil;
			int i;
			for(i = 0; i < length(s->function.parameters); i++) {
				prepend(make_reference(NULL, ectx->expr_buf), &params, ectx->expr_buf);
				prepend(make_invoke1(make_literal((unsigned long) _get_, ectx->expr_buf), make_reference(NULL, ectx->expr_buf),
					ectx->expr_buf), &args, ectx->expr_buf);
			}
			append(make_function(make_reference(s->function.reference->reference.name, ectx->expr_buf), params,
				make_invoke(make_invoke3(make_literal((unsigned long) init_function, ectx->expr_buf),
					make_literal((unsigned long) s, ectx->expr_buf), make_literal((unsigned long) ectx, ectx->expr_buf),
					make_literal((unsigned long) cache, ectx->expr_buf), ectx->expr_buf), args, ectx->expr_buf), ectx->expr_buf), &c,
					ectx->expr_buf);
			union expression *a, *t;
			foreachzipped(a, t, params, args) {
				refer_reference(t->invoke.arguments->fst, a);
			}
		} else {
			append(make_invoke0(make_invoke3(make_literal((unsigned long) init_expression, ectx->expr_buf),
				make_literal((unsigned long) s, ectx->expr_buf), make_literal((unsigned long) ectx, ectx->expr_buf),
				make_literal((unsigned long) cache, ectx->expr_buf), ectx->expr_buf), ectx->expr_buf), &c, ectx->expr_buf);
		}
	}
	return make_program(c, ectx->expr_buf);
}
