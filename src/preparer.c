bool reference_named(union expression *expr, char *ctx) {
	return defined_string_equals(expr->reference.name, ctx);
}

bool function_named(union expression *expr, char *ctx) {
	return expr->base.type == function && reference_named(expr->function.reference, ctx);
}

union expression *vfind_multiple_definitions(union expression *e, void *ctx) {
	jumpbuf *handler = ctx;
	union expression *t;
	list *partial;
	switch(e->base.type) {
		case begin: {
			foreachlist(partial, t, &e->begin.expressions) {
				if(t->base.type == function && t->function.reference->reference.name &&
					exists((bool (*)(void *, void *)) function_named, &(*partial)->rst, t->function.reference->reference.name)) {
						throw_multiple_definition(t->function.reference->reference.name, handler);
				}
			}
			break;
		} case continuation: case function: {
			region tempreg = create_region(0);
			list ref_with_params = lst(e->continuation.reference, e->continuation.parameters, tempreg);
			foreachlist(partial, t, &ref_with_params) {
				if(exists((bool (*)(void *, void *)) reference_named, &(*partial)->rst, t->reference.name)) {
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
	jumpbuf *handler = ((void **) ctx)[0];
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

struct compilation {
	list argument;
	void *macro;
};

struct expansion_context {
	list *ext_binds;
	list *comps;
	region rt_reg;
	jumpbuf *handler;
};

Object *load_expressions(union expression *program, struct expansion_context *ectx, list st_binds, region ct_reg);
union expression *build_syntax_tree(list d, region reg, jumpbuf *handler);

void *np_expansion(list (*expander)(list, region), list argument, struct expansion_context *ectx, list st_binds, list dyn_ref_names, list indirections) {
	struct compilation *pc;
	{foreach(pc, *ectx->comps) {
		if(argument == pc->argument) {
			return pc->macro;
		}
	}}
	
	region ct_reg = create_region(0);
	union expression *func = make_function(ct_reg), *cont = make_continuation(ct_reg), *host_cont_param = make_reference(ct_reg),
		*guest_cont_param = make_reference(ct_reg);
	prepend(host_cont_param, &func->function.parameters, ct_reg);
	host_cont_param->reference.parent = func;
	char *ref_name;
	{foreach(ref_name, reverse(dyn_ref_names, ct_reg)) {
		union expression *param = make_reference(ct_reg);
		param->reference.name = ref_name;
		prepend(param, &cont->continuation.parameters, ct_reg);
		param->reference.parent = cont;
	}}
	prepend(guest_cont_param, &cont->continuation.parameters, ct_reg);
	guest_cont_param->reference.parent = cont;
	put(cont, continuation.expression, make_jump1(make_invoke1(make_literal((unsigned long) _get_, ct_reg),
		use_reference(guest_cont_param, ct_reg), ct_reg), build_syntax_tree(expander(argument, ct_reg), ct_reg, ectx->handler), ct_reg));
	{foreach(ref_name, dyn_ref_names) {
		put(cont, continuation.expression, insert_indirections(cont->continuation.expression, ref_name, ct_reg));
	}}
	{foreach(ref_name, indirections) {
		put(cont, continuation.expression, insert_indirections(cont->continuation.expression, ref_name, ct_reg));
	}}
	put(func, function.expression, make_jump1(make_invoke1(make_literal((unsigned long) _get_, ct_reg),
		use_reference(host_cont_param, ct_reg), ct_reg), cont, ct_reg));
	Object *obj = load_expressions(make_program(lst(func, nil(ct_reg), ct_reg), ct_reg), ectx, st_binds, ct_reg);
	mutate_symbols(obj, *ectx->ext_binds);
	mutate_symbols(obj, st_binds);
	
	pc = region_alloc(ectx->rt_reg, sizeof(struct compilation));
	pc->argument = argument;
	//There is only one immutable symbol: our annonymous function
	pc->macro = ((Symbol *) immutable_symbols(obj, ct_reg)->fst)->address;
	prepend(pc, ectx->comps, ectx->rt_reg);
	destroy_region(ct_reg);
	return pc->macro;
}

Symbol *make_symbol(char *nm, void *addr, region r) {
	Symbol *sym = region_alloc(r, sizeof(Symbol));
	sym->name = nm;
	sym->address = addr;
	return sym;
}

void prepend_binding(union expression *ref, list *binds, region rt_reg) {
	if(ref->reference.name) {
		ref->reference.symbol = make_symbol(rstrcpy(ref->reference.name, rt_reg), NULL, rt_reg);
		prepend(ref->reference.symbol, binds, rt_reg);
	}
}

void cond_prepend_ref(union expression *ref, list *refs, region rt_reg) {
	if(ref->reference.name) {
		prepend(ref, refs, rt_reg);
	}
}

void store_static_bindings(union expression **s, bool is_static, list st_binds, region rt_reg) {
	switch((*s)->base.type) {
		case begin: {
			union expression *expr;
			{foreach(expr, (*s)->begin.expressions) {
				if(expr->base.type == function) {
					prepend_binding(expr->function.reference, &st_binds, rt_reg);
				}
			}}
			union expression **exprr;
			{foreachaddress(exprr, (*s)->begin.expressions) {
				store_static_bindings(exprr, is_static, st_binds, rt_reg);
			}}
			break;
		} case with: {
			if(is_static) {
				prepend_binding((*s)->with.reference, &st_binds, rt_reg);
			}
			store_static_bindings(&(*s)->with.expression, is_static, st_binds, rt_reg);
			break;
		} case _if: {
			store_static_bindings(&(*s)->_if.condition, is_static, st_binds, rt_reg);
			store_static_bindings(&(*s)->_if.consequent, is_static, st_binds, rt_reg);
			store_static_bindings(&(*s)->_if.alternate, is_static, st_binds, rt_reg);
			break;
		} case function: {
			store_static_bindings(&(*s)->function.expression, false, st_binds, rt_reg);
			break;
		} case continuation: {
			if(is_static) {
				prepend_binding((*s)->continuation.reference, &st_binds, rt_reg);
				union expression *param;
				{foreach(param, (*s)->continuation.parameters) {
					prepend_binding(param, &st_binds, rt_reg);
				}}
			}
			store_static_bindings(&(*s)->continuation.expression, is_static, st_binds, rt_reg);
			break;
		} case invoke: case jump: {
			store_static_bindings(&(*s)->invoke.reference, is_static, st_binds, rt_reg);
			union expression **arg;
			{foreachaddress(arg, (*s)->invoke.arguments) {
				store_static_bindings(arg, is_static, st_binds, rt_reg);
			}}
			break;
		} case non_primitive: {
			store_static_bindings(&(*s)->non_primitive.reference, is_static, st_binds, rt_reg);
			(*s)->non_primitive.st_binds = st_binds;
			break;
		}
	}
}

void store_dynamic_refs(union expression **s, bool is_static, list dyn_refs, region ct_reg) {
	switch((*s)->base.type) {
		case begin: {
			union expression **exprr;
			{foreachaddress(exprr, (*s)->begin.expressions) {
				store_dynamic_refs(exprr, is_static, dyn_refs, ct_reg);
			}}
			break;
		} case with: {
			if(!is_static) {
				cond_prepend_ref((*s)->with.reference, &dyn_refs, ct_reg);
			}
			store_dynamic_refs(&(*s)->with.expression, is_static, dyn_refs, ct_reg);
			break;
		} case _if: {
			store_dynamic_refs(&(*s)->_if.condition, is_static, dyn_refs, ct_reg);
			store_dynamic_refs(&(*s)->_if.consequent, is_static, dyn_refs, ct_reg);
			store_dynamic_refs(&(*s)->_if.alternate, is_static, dyn_refs, ct_reg);
			break;
		} case function: {
			dyn_refs = nil(ct_reg);
			union expression *param;
			{foreach(param, (*s)->function.parameters) {
				cond_prepend_ref(param, &dyn_refs, ct_reg);
			}}
			store_dynamic_refs(&(*s)->function.expression, false, dyn_refs, ct_reg);
			break;
		} case continuation: {
			if(!is_static) {
				cond_prepend_ref((*s)->continuation.reference, &dyn_refs, ct_reg);
				union expression *param;
				{foreach(param, (*s)->continuation.parameters) {
					cond_prepend_ref(param, &dyn_refs, ct_reg);
				}}
			}
			store_dynamic_refs(&(*s)->continuation.expression, is_static, dyn_refs, ct_reg);
			break;
		} case invoke: case jump: {
			store_dynamic_refs(&(*s)->invoke.reference, is_static, dyn_refs, ct_reg);
			union expression **arg;
			{foreachaddress(arg, (*s)->invoke.arguments) {
				store_dynamic_refs(arg, is_static, dyn_refs, ct_reg);
			}}
			break;
		} case non_primitive: {
			store_dynamic_refs(&(*s)->non_primitive.reference, is_static, dyn_refs, ct_reg);
			dyn_refs = reverse(dyn_refs, ct_reg);
			(*s)->non_primitive.dyn_refs = nil(ct_reg);
			list *dyn_refs_suffix;
			union expression *dyn_ref;
			{foreachlist(dyn_refs_suffix, dyn_ref, &dyn_refs) {
				if(!exists((bool (*)(void *, void *)) reference_named, &(*dyn_refs_suffix)->rst, dyn_ref->reference.name)) {
					prepend(dyn_ref, &(*s)->non_primitive.dyn_refs, ct_reg);
				}
			}}
			break;
		}
	}
}

void generate_np_expressions(union expression **s, region ct_reg, struct expansion_context *ectx) {
	switch((*s)->base.type) {
		case begin: {
			union expression **exprr;
			{foreachaddress(exprr, (*s)->begin.expressions) {
				generate_np_expressions(exprr, ct_reg, ectx);
			}}
			break;
		} case with: {
			generate_np_expressions(&(*s)->with.expression, ct_reg, ectx);
			break;
		} case _if: {
			generate_np_expressions(&(*s)->_if.condition, ct_reg, ectx);
			generate_np_expressions(&(*s)->_if.consequent, ct_reg, ectx);
			generate_np_expressions(&(*s)->_if.alternate, ct_reg, ectx);
			break;
		} case function: {
			generate_np_expressions(&(*s)->function.expression, ct_reg, ectx);
			break;
		} case continuation: {
			generate_np_expressions(&(*s)->continuation.expression, ct_reg, ectx);
			break;
		} case invoke: case jump: {
			generate_np_expressions(&(*s)->invoke.reference, ct_reg, ectx);
			union expression **arg;
			{foreachaddress(arg, (*s)->invoke.arguments) {
				generate_np_expressions(arg, ct_reg, ectx);
			}}
			break;
		} case non_primitive: {
			generate_np_expressions(&(*s)->non_primitive.reference, ct_reg, ectx);
			
			list param_names_rt = nil(ectx->rt_reg), args_ct = nil(ct_reg);
			union expression *dyn_ref;
			{foreach(dyn_ref, (*s)->non_primitive.dyn_refs) {
				prepend(rstrcpy(dyn_ref->reference.name, ectx->rt_reg), &param_names_rt, ectx->rt_reg);
				prepend(use_reference(dyn_ref, ct_reg), &args_ct, ct_reg);
			}}
			
			union expression *wth = make_with(ct_reg), *cont = make_continuation(ct_reg), *param = make_reference(ct_reg);
			put(wth, with.expression, make_invoke1(make_invoke6(make_literal((unsigned long) np_expansion, ct_reg),
				(*s)->non_primitive.reference, make_literal((unsigned long) copy_sexpr_list((*s)->non_primitive.argument,
				ectx->rt_reg), ct_reg), make_literal((unsigned long) ectx, ct_reg),
				make_literal((unsigned long) (*s)->non_primitive.st_binds, ct_reg),
				make_literal((unsigned long) param_names_rt, ct_reg),
				make_literal((unsigned long) map((*s)->non_primitive.indirections, ectx->rt_reg, (void *(*)(void *, void *)) rstrcpy,
					ectx->rt_reg), ct_reg), ct_reg), cont, ct_reg));
			prepend(param, &cont->continuation.parameters, ct_reg);
			param->reference.parent = cont;
			
			union expression *macro_invocation = make_with(ct_reg);
			macro_invocation->with.parent = (*s)->non_primitive.parent;
			prepend(use_reference(macro_invocation->with.reference, ct_reg), &args_ct, ct_reg);
			put(macro_invocation, with.expression, make_jump(make_invoke1(make_literal((unsigned long) _get_, ct_reg),
				use_reference(param, ct_reg), ct_reg), args_ct, ct_reg));
			union expression *parent_function = get_zeroth_function(*s);
			*s = macro_invocation;
			
			put(cont, continuation.expression, make_jump1(use_reference(wth->with.reference, ct_reg),
				parent_function->function.expression, ct_reg));
			put(parent_function, function.expression, wth);
		}
	}
}
