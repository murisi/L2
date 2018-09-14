bool function_named(void *expr_void, void *ctx) {
	union expression *expr = expr_void;
	return expr->base.type == function && expr->function.reference->reference.name &&
		strequal(ctx, expr->function.reference->reference.name);
}

bool reference_named(void *expr_void, void *ctx) {
	return ((union expression *) expr_void)->reference.name && strequal(ctx, ((union expression *) expr_void)->reference.name);
}

union expression *vfind_multiple_definitions(union expression *e, void *ctx) {
	myjmp_buf *handler = ctx;
	union expression *t;
	list *partial;
	switch(e->base.type) {
		case begin: {
			foreachlist(partial, t, &e->begin.expressions) {
				if(t->base.type == function && t->function.reference->reference.name &&
					exists(function_named, &(*partial)->rst, t->function.reference->reference.name)) {
						throw_multiple_definition(t->function.reference->reference.name, handler);
				}
			}
			break;
		} case continuation: case function: {
			region tempreg = create_region(0);
			list ref_with_params = lst(e->continuation.reference, e->continuation.parameters, tempreg);
			foreachlist(partial, t, &ref_with_params) {
				if(t->reference.name && exists(reference_named, &(*partial)->rst, t->reference.name)) {
					throw_multiple_definition(t->reference.name, handler);
				}
			}
			destroy_region(tempreg);
			break;
		}
	}
	return e;
}

union expression *get_zeroth_function(union expression *n) {
	while(n && n->base.type != function) {
		n = n->base.parent;
	}
	return n;
}

//Get zeroth expression enclosing e that has static storage
union expression *get_zeroth_static(union expression *e) {
	union expression *static_expr = e;
	for(; e; e = e->base.parent) {
		if(e->base.type == function && e->function.parent) {
			static_expr = e->function.parent;
		}
	}
	return static_expr;
}

bool reference_equals(union expression *a, union expression *b) {
	return a == b || (a->reference.name && b->reference.name && !strcmp(a->reference.name, b->reference.name));
}

union expression *referent_of(union expression *reference) {
	union expression *t;
	for(t = reference; t != NULL; t = (t->base.type == function ? get_zeroth_static(t->base.parent) : t->base.parent)) {
		switch(t->base.type) {
			case begin: {
				union expression *u;
				foreach(u, t->begin.expressions) {
					if(u->base.type == function && reference_equals(u->function.reference, reference)) {
						return u->function.reference;
					}
				}
				break;
			} case function: case continuation: case with: {
				if(reference_equals(t->function.reference, reference)) {
					return t->function.reference;
				} else if(t->base.type == function || t->base.type == continuation) {
					union expression *u;
					foreach(u, t->function.parameters) {
						if(reference_equals(u, reference)) {
							return u;
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
	return s->reference.referent->reference.parent;
}

union expression *root_function_of(union expression *s) {
	for(; s->base.parent; s = s->base.parent);
	return s;
}

union expression *vlink_references(union expression *s, void *ctx) {
	myjmp_buf *handler = ((void **) ctx)[0];
	region r = ((void **) ctx)[1];
	if(s->base.type == reference) {
		s->reference.referent = s->reference.referent ? s->reference.referent : referent_of(s);
		if(s->reference.referent == NULL) {
			s->reference.referent = prepend_parameter(root_function_of(s), r);
			s->reference.referent->reference.name = s->reference.name;
		} else if(is_jump_reference(s) && is_c_reference(s->reference.referent) &&
			length(s->reference.parent->jump.arguments) != length(target_expression(s)->continuation.parameters)) {
				throw_param_count_mismatch(s->reference.parent, target_expression(s), handler);
		} else if(is_invoke_reference(s) && is_function_reference(s->reference.referent) &&
			length(s->reference.parent->invoke.arguments) != length(target_expression(s)->function.parameters)) {
				throw_param_count_mismatch(s->reference.parent, target_expression(s), handler);
		}
	} else if(s->base.type == continuation && is_jump_reference(s) &&
		length(s->continuation.parent->jump.arguments) != length(s->continuation.parameters)) {
			throw_param_count_mismatch(s->continuation.parent, s, handler);
	} else if(s->base.type == function && s->function.parent && s->function.parent->base.type == invoke &&
		s->function.parent->invoke.reference == s && length(s->function.parent->invoke.arguments) != length(s->function.parameters)) {
			throw_param_count_mismatch(s->function.parent, s, handler);
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
	if(s->base.type == reference && s->reference.referent != s && is_c_reference(s->reference.referent)) {
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
			if((*s)->base.type == function || (*s)->base.type == continuation) {
				union expression **t;
				foreachaddress(t, (*s)->function.parameters) {
					visit_expressions(visitor, t, ctx);
				}
			}
			visit_expressions(visitor, &(*s)->function.reference, ctx);
			visit_expressions(visitor, &(*s)->function.expression, ctx);
			break;
		} case jump: case invoke: {
			visit_expressions(visitor, &(*s)->invoke.reference, ctx);
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

#define emit(x, r) { \
	union expression *_emit_x = x; \
	append(_emit_x, &container->begin.expressions, r); \
	_emit_x->base.parent = container; \
}

union expression *make_local(union expression *function, region r) {
	union expression *ref = make_reference(r);
	ref->reference.parent = function;
	prepend(ref, &function->function.locals, r);
	return ref;
}

// Renders the "parent" field meaningless
union expression *use_return_value(union expression *n, union expression *ret_val, region r) {
	switch(n->base.type) {
		case with: {
			n->with.parameter->fst = make_reference(r);
		} case continuation: {
			n->continuation.return_value = ret_val;
			put(n, continuation.expression, use_return_value(n->continuation.expression, make_local(get_zeroth_function(n), r), r));
			return n;
		} case function: {
			n->function.return_value = ret_val;
			n->function.expression_return_value = make_local(n, r);
			put(n, function.expression, use_return_value(n->function.expression, n->function.expression_return_value, r));
			return n;
		} case invoke: case jump: {
			union expression *container = make_begin(r);
			
			if(n->base.type == jump && n->jump.short_circuit && n->jump.reference->base.type == reference) {
				n->jump.reference->reference.return_value = NULL;
			} else {
				union expression *ref_ret_val = make_local(get_zeroth_function(n), r);
				emit(use_return_value(n->invoke.reference, ref_ret_val, r), r);
				n->invoke.reference = ref_ret_val;
			}
			
			union expression **t;
			foreachaddress(t, n->invoke.arguments) {
				union expression *arg_ret_val = make_local(get_zeroth_function(n), r);
				emit(use_return_value(*t, arg_ret_val, r), r);
				*t = arg_ret_val;
			}
			n->invoke.return_value = ret_val;
			emit(n, r);
			return container;
		} case _if: {
			union expression *container = make_begin(r);
			
			put(n, _if.consequent, use_return_value(n->_if.consequent, ret_val, r));
			put(n, _if.alternate, use_return_value(n->_if.alternate, ret_val, r));
			
			union expression *cond_ret_val = make_local(get_zeroth_function(n), r);
			emit(use_return_value(n->_if.condition, cond_ret_val, r), r);
			put(n, _if.condition, cond_ret_val);
			emit(n, r);
			return container;
		} case begin: {
			union expression **t;
			foreachaddress(t, n->begin.expressions) {
				*t = use_return_value(*t, make_local(get_zeroth_function(n), r), r);
				(*t)->base.parent = n;
			}
			return n;
		} case reference: {
			n->reference.return_value = ret_val;
			return n;
		} case literal: {
			n->literal.return_value = ret_val;
			return n;
		}
	}
}

union expression *vmerge_begins(union expression *n, void *ctx) {
	if(n->base.type != begin) {
		list *l = ((void **) ctx)[0];
		region r = ((void **) ctx)[1];
		prepend(n, l, r);
	}
	return n;
}

Object *load_expressions(list exprs, list *ext_binds, list st_binds, list *comps, region obj_reg, myjmp_buf *handler);
union expression *build_syntax_tree(list d, region reg, myjmp_buf *handler);
void *execute_macro(list (*expander)(list), union expression *np, list *ext_binds, list st_binds, list dyn_refs, list *comps, region comps_reg, myjmp_buf *handler);

Symbol *make_symbol(char *nm, void *addr, region r) {
	Symbol *sym = region_malloc(r, sizeof(Symbol));
	sym->name = nm;
	sym->address = addr;
	return sym;
}

union expression *generate_macros(union expression *s, bool is_static, list *ext_binds, list st_binds, list dyn_refs, list *comps, region ct_reg, region rt_reg, myjmp_buf *handler) {
	switch(s->base.type) {
		case begin: {
			union expression *expr;
			{foreach(expr, s->begin.expressions) {
				if(expr->base.type == function && expr->function.reference->reference.name) {
					expr->function.reference->reference.symbol =
						make_symbol(rstrcpy(expr->function.reference->reference.name, rt_reg), NULL, rt_reg);
					prepend(expr->function.reference->reference.symbol, &st_binds, rt_reg);
				}
			}}
			union expression **exprr;
			{foreachaddress(exprr, s->begin.expressions) {
				*exprr = generate_macros(*exprr, is_static, ext_binds, st_binds, dyn_refs, comps, ct_reg, rt_reg, handler);
				(*exprr)->base.parent = s;
			}}
			break;
		} case with: {
			if(is_static) {
				s->with.reference->reference.symbol = make_symbol(rstrcpy(s->with.reference->reference.name, rt_reg), NULL, rt_reg);
				prepend(s->with.reference->reference.symbol, &st_binds, rt_reg);
			} else {
				prepend(s->with.reference, &dyn_refs, ct_reg);
			}
			put(s, with.expression, generate_macros(s->with.expression, is_static, ext_binds, st_binds, dyn_refs, comps, ct_reg, rt_reg, handler));
			break;
		} case _if: {
			put(s, _if.condition, generate_macros(s->_if.condition, is_static, ext_binds, st_binds, dyn_refs, comps, ct_reg, rt_reg, handler));
			put(s, _if.consequent, generate_macros(s->_if.consequent, is_static, ext_binds, st_binds, dyn_refs, comps, ct_reg, rt_reg, handler));
			put(s, _if.alternate, generate_macros(s->_if.alternate, is_static, ext_binds, st_binds, dyn_refs, comps, ct_reg, rt_reg, handler));
			break;
		} case function: {
			dyn_refs = nil(ct_reg);
			union expression *param;
			{foreach(param, s->function.parameters) {
				prepend(param, &dyn_refs, ct_reg);
			}}
			put(s, function.expression, generate_macros(s->function.expression, false, ext_binds, st_binds, dyn_refs, comps, ct_reg, rt_reg, handler));
			break;
		} case continuation: {
			if(is_static) {
				s->continuation.reference->reference.symbol =
					make_symbol(rstrcpy(s->continuation.reference->reference.name, rt_reg), NULL, rt_reg);
				prepend(s->continuation.reference->reference.symbol, &st_binds, rt_reg);
				union expression *param;
				{foreach(param, s->continuation.parameters) {
					param->reference.symbol = make_symbol(rstrcpy(param->reference.name, rt_reg), NULL, rt_reg);
					prepend(param->reference.symbol, &st_binds, rt_reg);
				}}
			} else {
				prepend(s->continuation.reference, &dyn_refs, ct_reg);
				union expression *param;
				{foreach(param, s->continuation.parameters) {
					prepend(param, &dyn_refs, ct_reg);
				}}
			}
			put(s, continuation.expression, generate_macros(s->continuation.expression, is_static, ext_binds, st_binds, dyn_refs, comps, ct_reg, rt_reg, handler));
			break;
		} case invoke: case jump: {
			put(s, invoke.reference, generate_macros(s->invoke.reference, is_static, ext_binds, st_binds, dyn_refs, comps, ct_reg, rt_reg, handler));
			union expression **arg;
			{foreachaddress(arg, s->invoke.arguments) {
				*arg = generate_macros(*arg, is_static, ext_binds, st_binds, dyn_refs, comps, ct_reg, rt_reg, handler);
				(*arg)->base.parent = s;
			}}
			break;
		} case non_primitive: {
			put(s, non_primitive.reference, generate_macros(s->non_primitive.reference, is_static, ext_binds, st_binds, dyn_refs,
				comps, ct_reg, rt_reg, handler));
			dyn_refs = reverse(dyn_refs, ct_reg);
			list *dyn_refs_suffix, params_rt = nil(rt_reg), args_ct = nil(ct_reg);
			union expression *dyn_ref;
			{foreachlist(dyn_refs_suffix, dyn_ref, &dyn_refs) {
				if(!exists(reference_named, &(*dyn_refs_suffix)->rst, dyn_ref->reference.name)) {
					prepend(copy_expression(dyn_ref, rt_reg), &params_rt, rt_reg);
					prepend(use_reference(dyn_ref, ct_reg), &args_ct, ct_reg);
				}
			}}
			union expression *macro_invocation = make_invoke(make_invoke8(make_literal((unsigned long) execute_macro, ct_reg),
				s->non_primitive.reference, make_literal((unsigned long) copy_expression(s, rt_reg), ct_reg),
				make_literal((unsigned long) ext_binds, ct_reg), make_literal((unsigned long) st_binds, ct_reg),
				make_literal((unsigned long) params_rt, ct_reg), make_literal((unsigned long) comps, ct_reg),
				make_literal((unsigned long) rt_reg, ct_reg), make_literal((unsigned long) handler, ct_reg), ct_reg), args_ct, ct_reg);
			return macro_invocation;
		}
	}
	return s;
}

struct compilation {
	union expression *np_expression;
	list np_expanded;
	void *macro;
};

void *execute_macro(list (*expander)(list), union expression *np, list *ext_binds, list st_binds, list dyn_refs, list *comps, region comps_reg, myjmp_buf *handler) {
	list np_expanded = expander(np->non_primitive.argument);
	struct compilation *pc;
	{foreach(pc, *comps) {
		if(np == pc->np_expression && sexpr_list_equals(np_expanded, pc->np_expanded)) {
			return pc->macro;
		}
	}}
	
	region reg = create_region(0);
	union expression *func = make_function(comps_reg), *ref;
	{foreach(ref, reverse(dyn_refs, comps_reg)) {
		union expression *param = copy_expression(ref, reg);
		prepend(param, &func->function.parameters, reg);
		param->reference.parent = func;
	}}
	put(func, function.expression, build_syntax_tree(np_expanded, comps_reg, handler));
	{foreach(ref, func->function.parameters) {
		put(func, function.expression, insert_indirections(func->function.expression, ref, reg));
	}}
	{foreach(ref, np->non_primitive.indirections) {
		put(func, function.expression, insert_indirections(func->function.expression, ref, reg));
	}}
	Object *obj = load_expressions(lst(func, nil(reg), reg), ext_binds, st_binds, comps, comps_reg, handler);
	mutate_symbols(obj, *ext_binds);
	mutate_symbols(obj, st_binds);
	
	pc = region_malloc(comps_reg, sizeof(struct compilation));
	pc->np_expression = np;
	pc->np_expanded = np_expanded;
	//There is only one immutable symbol: our annonymous function
	pc->macro = ((Symbol *) immutable_symbols(obj, reg)->fst)->address;
	prepend(pc, comps, comps_reg);
	destroy_region(reg);
	return pc->macro;
}
