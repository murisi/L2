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
	region tempreg = create_region(0);
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
	destroy_region(tempreg);
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
					if((u->base.type == function || u->base.type == storage) && reference_equals(u->function.reference, reference)) {
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
			union expression *ref = use_symbol(make_symbol(static_storage, global_scope, undefined_state, s->reference.name, r), r);
			prepend(ref, &root_function_of(s)->function.parameters, r);
			ref->reference.parent = root_function_of(s);
			ref->reference.referent = ref;
			s->reference.referent = ref;
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
		} case jump: case invoke: case storage: {
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

struct symbol *make_local(union expression *function, region r) {
	struct symbol *sym = make_symbol(function->function.parent ? dynamic_storage : static_storage, local_scope, defined_state, NULL, r);
	prepend(sym, &function->function.symbols, r);
	return sym;
}

union expression *use_return_symbol(union expression *n, struct symbol *ret_sym, region r) {
	switch(n->base.type) {
		case with: {
		} case continuation: {
			n->continuation.return_symbol = ret_sym;
			put(n, continuation.expression, use_return_symbol(n->continuation.expression, make_local(get_zeroth_function(n), r), r));
			return n;
		} case function: {
			n->function.return_symbol = ret_sym;
			n->function.expression_return_symbol = make_local(n, r);
			put(n, function.expression, use_return_symbol(n->function.expression, n->function.expression_return_symbol, r));
			return n;
		} case invoke: case jump: case storage: {
			union expression *container = make_begin(nil, r);
			
			if(n->base.type == jump && n->jump.short_circuit && n->jump.reference->base.type == reference) {
				n->jump.reference->reference.return_symbol = NULL;
			} else if(n->base.type == storage) {
			} else {
				struct symbol *ref_ret_sym = make_local(get_zeroth_function(n), r);
				emit(use_return_symbol(n->invoke.reference, ref_ret_sym, r), r);
				n->invoke.reference = use_symbol(ref_ret_sym, r);
			}
			
			union expression **t;
			foreachaddress(t, n->invoke.arguments) {
				struct symbol *arg_ret_sym = make_local(get_zeroth_function(n), r);
				emit(use_return_symbol(*t, arg_ret_sym, r), r);
				*t = use_symbol(arg_ret_sym, r);
			}
			n->invoke.return_symbol = ret_sym;
			emit(n, r);
			return container;
		} case _if: {
			union expression *container = make_begin(nil, r);
			
			put(n, _if.consequent, use_return_symbol(n->_if.consequent, ret_sym, r));
			put(n, _if.alternate, use_return_symbol(n->_if.alternate, ret_sym, r));
			
			struct symbol *cond_ret_sym = make_local(get_zeroth_function(n), r);
			emit(use_return_symbol(n->_if.condition, cond_ret_sym, r), r);
			put(n, _if.condition, use_symbol(cond_ret_sym, r));
			emit(n, r);
			return container;
		} case begin: {
			union expression **t;
			foreachaddress(t, n->begin.expressions) {
				*t = use_return_symbol(*t, make_local(get_zeroth_function(n), r), r);
				(*t)->base.parent = n;
			}
			return n;
		} case reference: {
			n->reference.return_symbol = ret_sym;
			return n;
		} case literal: {
			n->literal.return_symbol = ret_sym;
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

bool is_toplevel(union expression *n) {
	return n->base.parent && n->base.parent->base.parent && !n->base.parent->base.parent->base.parent;
}

union expression *vmake_symbols(union expression *n, region r) {
	switch(n->base.type) {
		case function: {
			if(!n->function.reference->reference.symbol) {
				n->function.reference->reference.symbol = make_symbol(_function, is_toplevel(n) ? global_scope : local_scope,
					defined_state, n->function.reference->reference.name, r);
			}
			union expression *param;
			foreach(param, n->function.parameters) {
				if(!param->reference.symbol) {
					param->reference.symbol = make_symbol(n->function.parent ? dynamic_storage : static_storage, local_scope,
						defined_state, param->reference.name, r);
				}
			}
			break;
		} case continuation: case with: {
			enum symbol_type type = get_zeroth_function(n)->function.parent ? dynamic_storage : static_storage;
			if(!n->continuation.reference->reference.symbol) {
				n->continuation.reference->reference.symbol = make_symbol(type, local_scope, defined_state,
					n->continuation.reference->reference.name, r);
			}
			union expression *param;
			foreach(param, n->continuation.parameters) {
				if(!param->reference.symbol) {
					param->reference.symbol = make_symbol(type, local_scope, defined_state, param->reference.name, r);
				}
			}
			break;
		} case storage: {
			bool toplevel = is_toplevel(n);
			if(!n->storage.reference->reference.symbol) {
				n->storage.reference->reference.symbol = make_symbol(toplevel ? static_storage : dynamic_storage,
					toplevel ? global_scope : local_scope, defined_state, n->storage.reference->reference.name, r);
			}
			break;
		}
	}
	return n;
}

union expression *vshare_symbols(union expression *n, region r) {
	if(n->base.type == reference && !n->reference.symbol) {
		n->reference.symbol = n->reference.referent->reference.symbol;
	}
	return n;
}

void *_get_(void *ref) {
	return *((void **) ref);
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
				if((f->base.type == function || f->base.type == storage) &&
						defined_string_equals(f->function.reference->reference.name, ref_name)) {
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
		} case invoke: case jump: case storage: {
			if(expr->base.type != storage) {
				put(expr, invoke.reference, insert_indirections(expr->invoke.reference, ref_name, reg));
			}
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

struct expansion_context {
	list ext_binds;
	region rt_reg;
	jumpbuf *handler;
};

Object *load_expressions(union expression *program, struct expansion_context *ectx, list st_binds, region ct_reg);
union expression *build_syntax_tree(list d, region reg, jumpbuf *handler);

void (*np_expansion(list (*expander)(list, region, list), list argument, struct expansion_context *ectx, list st_binds, list dyn_ref_names, list indirections, void (*(*macro_cache))(void *), list *macro_sexpr_cache))(void *) {
	list expansion = expander(argument, ectx->rt_reg, *macro_sexpr_cache);
	if(expansion == *macro_sexpr_cache) {
		return *macro_cache;
	}
	
	region ct_reg = create_region(0);
	list macro_cont_params = nil;
	char *ref_name;
	{foreach(ref_name, reverse(dyn_ref_names, ct_reg)) {
		prepend(make_reference(ref_name, ct_reg), &macro_cont_params, ct_reg);
	}}
	union expression *guest_cont_param = make_reference(NULL, ct_reg);
	prepend(guest_cont_param, &macro_cont_params, ct_reg);
	union expression *guest_cont_arg = make_reference(NULL, ct_reg);
	union expression *macro_cont = make_continuation(make_reference(NULL, ct_reg), macro_cont_params,
		make_jump1(make_invoke1(make_literal((unsigned long) _get_, ct_reg), guest_cont_arg, ct_reg),
			build_syntax_tree(expansion, ct_reg, ectx->handler), ct_reg), ct_reg);
	refer_reference(guest_cont_arg, guest_cont_param);
	
	{foreach(ref_name, dyn_ref_names) {
		put(macro_cont, continuation.expression, insert_indirections(macro_cont->continuation.expression, ref_name, ct_reg));
	}}
	{foreach(ref_name, indirections) {
		put(macro_cont, continuation.expression, insert_indirections(macro_cont->continuation.expression, ref_name, ct_reg));
	}}
	union expression *host_cont_param = make_reference(NULL, ct_reg);
	union expression *host_cont_arg = make_reference(NULL, ct_reg);
	union expression *func = make_function(make_reference(NULL, ct_reg), lst(host_cont_param, nil, ct_reg),
		make_jump1(make_invoke1(make_literal((unsigned long) _get_, ct_reg), host_cont_arg, ct_reg), macro_cont, ct_reg), ct_reg);
	refer_reference(host_cont_arg, host_cont_param);
	
	Object *obj = load_expressions(make_program(lst(func, nil, ct_reg), ct_reg), ectx, st_binds, ct_reg);
	list ms = mutable_symbols(obj, ct_reg);
	object_symbol *mutable_sym;
	{foreach(mutable_sym, ms) {
		bool found = false;
		object_symbol *bind_sym;
		{foreach(bind_sym, ectx->ext_binds) {
			if(!strcmp(mutable_sym->name, bind_sym->name)) {
				found = true;
				break;
			}
		}}
		{foreach(bind_sym, st_binds) {
			if(!strcmp(mutable_sym->name, bind_sym->name)) {
				found = true;
				break;
			}
		}}
		if(!found) {
			throw_undefined_reference(mutable_sym->name, ectx->handler);
		}
	}}
	mutate_symbols(obj, ectx->ext_binds);
	mutate_symbols(obj, st_binds);
	//There is only one immutable symbol: our annonymous function
	*macro_cache = ((object_symbol *) immutable_symbols(obj, ct_reg)->fst)->address;
	*macro_sexpr_cache = expansion;
	destroy_region(ct_reg);
	return *macro_cache;
}

void prepend_binding(union expression *ref, list *binds, region rt_reg) {
	if(ref->reference.name) {
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
				if(expr->base.type == function || expr->base.type == storage) {
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
		} case storage: {
			if(is_static) {
				prepend_binding((*s)->storage.reference, &st_binds, rt_reg);
			}
			union expression **arg;
			{foreachaddress(arg, (*s)->storage.arguments) {
				store_static_bindings(arg, is_static, st_binds, rt_reg);
			}}
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
			dyn_refs = nil;
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
		} case storage: {
			if(!is_static) {
				cond_prepend_ref((*s)->storage.reference, &dyn_refs, ct_reg);
			}
			union expression **arg;
			{foreachaddress(arg, (*s)->storage.arguments) {
				store_dynamic_refs(arg, is_static, dyn_refs, ct_reg);
			}}
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
			(*s)->non_primitive.dyn_refs = nil;
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

union expression *vgenerate_np_expressions(union expression *s, void *ctx) {
	region ct_reg = ((void **) ctx)[0];
	struct expansion_context *ectx = ((void **) ctx)[1];
	
	if(s->base.type == non_primitive) {
		list param_names_rt = nil, args_ct = nil;
		union expression *dyn_ref;
		{foreach(dyn_ref, s->non_primitive.dyn_refs) {
			prepend(rstrcpy(dyn_ref->reference.name, ectx->rt_reg), &param_names_rt, ectx->rt_reg);
			union expression *arg = make_reference(dyn_ref->reference.name, ct_reg);
			refer_reference(arg, dyn_ref);
			prepend(arg, &args_ct, ct_reg);
		}}
		void (*(*macro_cache))(void *) = region_alloc(ectx->rt_reg, sizeof(void (*)(void *)));
		*macro_cache = NULL;
		list *macro_sexpr_cache = region_alloc(ectx->rt_reg, sizeof(list *));
		*macro_sexpr_cache = nil;
		
		union expression *return_with_ref_arg = make_reference(NULL, ct_reg);
		prepend(return_with_ref_arg, &args_ct, ct_reg);
		union expression *callee_cont_arg = make_reference(NULL, ct_reg);
		union expression *call_jump = make_jump(make_invoke1(make_literal((unsigned long) _get_, ct_reg), callee_cont_arg, ct_reg),
			args_ct, ct_reg);
		union expression *callee_cont_param = make_reference(NULL, ct_reg);
		union expression *calling_cont = make_continuation(make_reference(NULL, ct_reg), lst(callee_cont_param, nil, ct_reg),
			call_jump, ct_reg);
		refer_reference(callee_cont_arg, callee_cont_param);
		union expression *return_with_ref = make_reference(NULL, ct_reg);
		union expression *return_with = make_with(return_with_ref,
			make_invoke1(make_invoke8(make_literal((unsigned long) np_expansion, ct_reg),
				s->non_primitive.reference, make_literal((unsigned long) copy_sexpr_list(s->non_primitive.argument,
				ectx->rt_reg), ct_reg), make_literal((unsigned long) ectx, ct_reg),
				make_literal((unsigned long) s->non_primitive.st_binds, ct_reg),
				make_literal((unsigned long) param_names_rt, ct_reg),
				make_literal((unsigned long) map(s->non_primitive.indirections, ectx->rt_reg, (void *(*)(void *, void *)) rstrcpy,
					ectx->rt_reg), ct_reg), make_literal((unsigned long) macro_cache, ct_reg),
					make_literal((unsigned long) macro_sexpr_cache, ct_reg), ct_reg), calling_cont, ct_reg), ct_reg);
		refer_reference(return_with_ref_arg, return_with_ref);
		return return_with;
	} else {
		return s;
	}
}
