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

#define emit(x, r) { \
	union expression *_emit_x = x; \
	append(_emit_x, &container->begin.expressions, r); \
	_emit_x->base.parent = container; \
}

union expression *use_return_symbol(union expression *n, struct symbol *ret_sym, region r) {
	switch(n->base.type) {
		case with: case continuation: {
			n->continuation.return_symbol = ret_sym;
			put(n, continuation.expression, use_return_symbol(n->continuation.expression,
				make_symbol(dynamic_storage, local_scope, defined_state, NULL, NULL, r), r));
			return n;
		} case function: {
			n->function.return_symbol = ret_sym;
			n->function.expression_return_symbol = make_symbol(dynamic_storage, local_scope, defined_state, NULL, NULL, r);
			put(n, function.expression, use_return_symbol(n->function.expression, n->function.expression_return_symbol, r));
			return n;
		} case invoke: case jump: case storage: {
			union expression *container = make_begin(nil, r);
			
			if(n->base.type != storage) {
				struct symbol *ref_ret_sym = make_symbol(dynamic_storage, local_scope, defined_state, NULL, NULL, r);
				emit(use_return_symbol(n->invoke.reference, ref_ret_sym, r), r);
				n->invoke.reference = use_symbol(ref_ret_sym, r);
			}
			
			union expression **t;
			foreachaddress(t, n->invoke.arguments) {
				struct symbol *arg_ret_sym = make_symbol(dynamic_storage, local_scope, defined_state, NULL, NULL, r);
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
			
			struct symbol *cond_ret_sym = make_symbol(dynamic_storage, local_scope, defined_state, NULL, NULL, r);
			emit(use_return_symbol(n->_if.condition, cond_ret_sym, r), r);
			put(n, _if.condition, use_symbol(cond_ret_sym, r));
			emit(n, r);
			return container;
		} case begin: {
			union expression **t;
			foreachaddress(t, n->begin.expressions) {
				*t = use_return_symbol(*t, make_symbol(dynamic_storage, local_scope, defined_state, NULL, NULL, r), r);
				(*t)->base.parent = n;
			}
			return n;
		} case reference: case literal: {
			n->reference.return_symbol = ret_sym;
			return n;
		}
	}
}

union expression *vlinearized_expressions(union expression *n, void *ctx) {
	if(n->base.type != begin) {
		list *l = ((void **) ctx)[0];
		region r = ((void **) ctx)[1];
		prepend(n, l, r);
	}
	return n;
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

void store_lexical_environment(union expression *s, bool is_static, list st_binds, list dyn_refs, region ct_reg, region rt_reg) {
	switch(s->base.type) {
		case begin: {
			union expression *expr;
			{foreach(expr, s->begin.expressions) {
				if(expr->base.type == function || (expr->base.type == storage && is_static)) {
					prepend_binding(expr->function.reference, &st_binds, rt_reg);
				} else if(expr->base.type == storage) {
					cond_prepend_ref(expr->storage.reference, &dyn_refs, ct_reg);
				}
			}}
			union expression *exprr;
			{foreach(exprr, s->begin.expressions) {
				store_lexical_environment(exprr, is_static, st_binds, dyn_refs, ct_reg, rt_reg);
			}}
			break;
		} case _if: {
			store_lexical_environment(s->_if.condition, is_static, st_binds, dyn_refs, ct_reg, rt_reg);
			store_lexical_environment(s->_if.consequent, is_static, st_binds, dyn_refs, ct_reg, rt_reg);
			store_lexical_environment(s->_if.alternate, is_static, st_binds, dyn_refs, ct_reg, rt_reg);
			break;
		} case function: {
			prepend_binding(s->function.reference, &st_binds, rt_reg);
			dyn_refs = nil;
			union expression *param;
			{foreach(param, s->function.parameters) {
				cond_prepend_ref(param, &dyn_refs, ct_reg);
			}}
			store_lexical_environment(s->function.expression, false, st_binds, dyn_refs, ct_reg, rt_reg);
			break;
		} case continuation: case with: {
			if(is_static) {
				prepend_binding(s->continuation.reference, &st_binds, rt_reg);
				union expression *param;
				{foreach(param, s->continuation.parameters) {
					prepend_binding(param, &st_binds, rt_reg);
				}}
			} else {
				cond_prepend_ref(s->continuation.reference, &dyn_refs, ct_reg);
				union expression *param;
				{foreach(param, s->continuation.parameters) {
					cond_prepend_ref(param, &dyn_refs, ct_reg);
				}}
			}
			store_lexical_environment(s->continuation.expression, is_static, st_binds, dyn_refs, ct_reg, rt_reg);
			break;
		} case storage: {
			if(is_static) {
				prepend_binding(s->storage.reference, &st_binds, rt_reg);
			} else {
				cond_prepend_ref(s->storage.reference, &dyn_refs, ct_reg);
			}
			union expression *arg;
			{foreach(arg, s->storage.arguments) {
				store_lexical_environment(arg, is_static, st_binds, dyn_refs, ct_reg, rt_reg);
			}}
			break;
		} case invoke: case jump: {
			store_lexical_environment(s->invoke.reference, is_static, st_binds, dyn_refs, ct_reg, rt_reg);
			union expression *arg;
			{foreach(arg, s->invoke.arguments) {
				store_lexical_environment(arg, is_static, st_binds, dyn_refs, ct_reg, rt_reg);
			}}
			break;
		} case non_primitive: {
			store_lexical_environment(s->non_primitive.reference, is_static, st_binds, dyn_refs, ct_reg, rt_reg);
			dyn_refs = reverse(dyn_refs, ct_reg);
			s->non_primitive.dynamic_context = !is_static;
			s->non_primitive.st_binds = st_binds;
			s->non_primitive.dyn_refs = nil;
			list *dyn_refs_suffix;
			union expression *dyn_ref;
			{foreachlist(dyn_refs_suffix, dyn_ref, &dyn_refs) {
				if(!exists((bool (*)(void *, void *)) reference_named, &(*dyn_refs_suffix)->rst, dyn_ref->reference.name)) {
					prepend(dyn_ref, &s->non_primitive.dyn_refs, ct_reg);
				}
			}}
			break;
		}
	}
}

void classify_program_symbols(union expression *expr) {
	if(expr->base.return_symbol) {
		expr->base.return_symbol->type = static_storage;
	}
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
		} case non_primitive: {
			classify_program_symbols(expr->non_primitive.reference);
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

bool symbol_equals(object_symbol *sym1, object_symbol *sym2) {
	return !strcmp(sym1->name, sym2->name);
}

Object *load_expressions(union expression *program, struct expansion_context *ectx, list st_binds, region ct_reg);
union expression *build_syntax_tree(list d, region reg, jumpbuf *handler);

/*
 * This function is called whenever a macro is invoked by L2 code. The basic idea is to call the macro with the
 * s-expression that represents the macro-invocation, then take the s-expression that is generated by the macro,
 * compile it, load it into memory, bind the lexical environment to it, and return the object code (for it to be
 * actually called at the call-site. The following code deviates from the basic idea in order to implement the
 * caching that will enable the code macro invocations after the first to be fast.
 */

void (*dynamic_np_expansion(list (*expander)(list, region, list), list argument, struct expansion_context *ectx, list st_binds,
		list dyn_ref_names, list indirections, void (*(*macro_cache))(void *), list *macro_sexpr_cache))(void *) {
	//Re-executing the macro and recompiling the code generated by a macro everytime a macro is used is far too slow, so
	//a compromise was made in the language design: provide as an argument to the macro, the s-expression it previously
	//produced. If the macro returns exactly this argument, then we can use the function that we compiled from the code
	//that the macro used before.
	list expansion = expander(argument, ectx->rt_reg, *macro_sexpr_cache);
	if(expansion == *macro_sexpr_cache) {
		return *macro_cache;
	}
	
	region ct_reg = create_buffer(0);
	//We pass the dynamic references present at the call site to the code generated by the macro using function arguments
	//to avoid the need to either recompile the code or to rebind the bindings of the loaded object code when a macro is
	//invoked from a different stack-frame. These options are not viable because they are slow (because they create a
	//large overhead on every macro invocation) or they would make macro usage non thread-safe.
	list macro_cont_params = nil;
	char *ref_name;
	{foreach(ref_name, reverse(dyn_ref_names, ct_reg)) {
		prepend(make_reference(ref_name, ct_reg), &macro_cont_params, ct_reg);
	}}
	
	//We want to compile the code generated by the macro, but there is a complication: if code in the macro extends the
	//stack frame, then it needs to extend the frame of the function directly containing the call-site. Hence continuations
	//are used to move control flow back from the macro generated code to the macro call site.
	union expression *guest_cont_param = make_reference(NULL, ct_reg);
	prepend(guest_cont_param, &macro_cont_params, ct_reg);
	union expression *guest_cont_arg = make_reference(NULL, ct_reg);
	//Epression is built in runtime region because symbol persistence is required. More specifically, symbols need to
	//persist if the fragment produced by the macro contains a function becuase then the static function reference needs
	//to be in the lexical environment of the non-primitive expressions it contains.
	union expression *macro_cont = make_continuation(make_reference(NULL, ct_reg), macro_cont_params,
		make_jump1(make_invoke1(make_literal((unsigned long) _get_, ct_reg), guest_cont_arg, ct_reg),
			build_expression(expansion, ectx->rt_reg, ectx->handler), ct_reg), ct_reg);
	refer_reference(guest_cont_arg, guest_cont_param);
	
	//The code generated by macro, though existing in a different stack-frame, can pretend that its dynamic references
	//are those present at the site of expansion because we insert indirections on certain usages of references.
	{foreach(ref_name, dyn_ref_names) {
		put(macro_cont, continuation.expression, insert_indirections(macro_cont->continuation.expression, ref_name, ct_reg));
	}}
	{foreach(ref_name, indirections) {
		put(macro_cont, continuation.expression, insert_indirections(macro_cont->continuation.expression, ref_name, ct_reg));
	}}
	union expression *host_cont_param = make_reference(NULL, ct_reg);
	union expression *host_cont_arg = make_reference(NULL, ct_reg);
	union expression *func_ref = make_reference(NULL, ct_reg);
	union expression *func = make_function(func_ref, lst(host_cont_param, nil, ct_reg),
		make_jump1(make_invoke1(make_literal((unsigned long) _get_, ct_reg), host_cont_arg, ct_reg), macro_cont, ct_reg), ct_reg);
	refer_reference(host_cont_arg, host_cont_param);
	
	Object *obj = load_expressions(make_program(lst(func, nil, ct_reg), ct_reg), ectx, st_binds, ct_reg);
	list is = concat(ectx->ext_binds, st_binds, ct_reg);
	
	//Make sure that the generated code has no undefined bindings
	list ms = mutable_symbols(obj, ct_reg);
	object_symbol *mutable_sym;
	{foreach(mutable_sym, ms) {
		if(!exists((bool (*)(void *, void *)) symbol_equals, &is, mutable_sym)) {
			throw_undefined_reference(mutable_sym->name, ectx->handler);
		}
	}}
	mutate_symbols(obj, is);
	//Get the address of our function in memory
	*macro_cache = (void *) func_ref->reference.symbol->offset;
	*macro_sexpr_cache = expansion;
	destroy_buffer(ct_reg);
	return *macro_cache;
}

void *static_np_expansion(list (*expander)(list, region, list), list argument, struct expansion_context *ectx, list st_binds,
		void (*(*macro_cache))(void *), list *macro_sexpr_cache) {
	list expansion = expander(argument, ectx->rt_reg, *macro_sexpr_cache);
	if(expansion == *macro_sexpr_cache) {
		return *macro_cache;
	}
	
	region ct_reg = create_buffer(0);
	//Make a continuation to the code generated by the macro
	union expression *guest_cont_param = make_reference(NULL, ct_reg);
	union expression *guest_cont_arg = make_reference(NULL, ct_reg);
	//The fragment generated by the macro is parsed into the runtime region because we need the symbols that the parser
	//creates to persist beyond the return of this function. The persistence is required for the case when the fragment
	//just produced also has macros in it as they would require these symbols during their runtime.
	union expression *macro_cont_ref = make_reference(NULL, ct_reg);
	union expression *macro_cont = make_continuation(macro_cont_ref, lst(guest_cont_param, nil, ct_reg),
		make_jump1(make_invoke1(make_literal((unsigned long) _get_, ct_reg), guest_cont_arg, ct_reg),
			build_expression(expansion, ectx->rt_reg, ectx->handler), ct_reg), ct_reg);
	refer_reference(guest_cont_arg, guest_cont_param);
	
	Object *obj = load_expressions(make_program(lst(macro_cont, nil, ct_reg), ct_reg), ectx, st_binds, ct_reg);
	list is = concat(ectx->ext_binds, st_binds, ct_reg);
	
	//Make sure that the generated code has no undefined bindings
	list ms = mutable_symbols(obj, ct_reg);
	object_symbol *mutable_sym;
	{foreach(mutable_sym, ms) {
		if(!exists((bool (*)(void *, void *)) symbol_equals, &is, mutable_sym)) {
			throw_undefined_reference(mutable_sym->name, ectx->handler);
		}
	}}
	mutate_symbols(obj, is);
	//Initialize our continuation in static storage
	((void (*)()) segment(obj, ".text"))();
	//Get the address in memory of the continuation
	*macro_cache = (void *) macro_cont_ref->reference.symbol->offset;
	*macro_sexpr_cache = expansion;
	destroy_buffer(ct_reg);
	return *macro_cache;
}

/*
 * The following functions replace macro invocations with hooks back into the evaluator (via np_expansion).
 * The evaluator then returns the memory address at which the macro is located. (The evaluator might on
 * occasion compile the code generated by the macro and load that into memory.) The motivation behind this
 * approach to implementing macros was that staging the compilation of macros is cumbersome when each of
 * those macros need to be linked to user specified object files. It is much easier to treat macro
 * invocations in the same way that one would treat eval.
 */

union expression *vgenerate_dynamic_np_expressions(union expression *s, void *ctx) {
	region ct_reg = ((void **) ctx)[0];
	struct expansion_context *ectx = ((void **) ctx)[1];
	
	if(s->base.type == non_primitive && s->non_primitive.dynamic_context) {
		list param_names_rt = nil, args_ct = nil;
		union expression *dyn_ref;
		{foreach(dyn_ref, s->non_primitive.dyn_refs) {
			prepend(rstrcpy(dyn_ref->reference.name, ectx->rt_reg), &param_names_rt, ectx->rt_reg);
			union expression *arg = make_reference(dyn_ref->reference.name, ct_reg);
			refer_reference(arg, dyn_ref);
			prepend(arg, &args_ct, ct_reg);
		}}
		
		//Every macro invocation site has a cache for storing the compiled version of the last macro
		//expansion carried out at this site.
		void (*(*macro_cache))(void *) = buffer_alloc(ectx->rt_reg, sizeof(void (*)(void *)));
		*macro_cache = NULL;
		list *macro_sexpr_cache = buffer_alloc(ectx->rt_reg, sizeof(list *));
		*macro_sexpr_cache = nil;
		
		//Essentially generates code to call np_expansion to load a compiled version of the macro expansion
		//into memory, and then call that code providing it with a continuation back to the call site.
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
			make_invoke1(make_invoke8(make_literal((unsigned long) dynamic_np_expansion, ct_reg),
				s->non_primitive.reference, make_literal((unsigned long) copy_fragment(s->non_primitive.argument,
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

union expression *vgenerate_static_np_expressions(union expression *s, void *ctx) {
	region ct_reg = ((void **) ctx)[0];
	struct expansion_context *ectx = ((void **) ctx)[1];
	
	if(s->base.type == non_primitive && !s->non_primitive.dynamic_context) {
		//Every macro invocation site has a cache for storing the compiled version of the last macro
		//expansion carried out at this site.
		void (*(*macro_cache))(void *) = buffer_alloc(ectx->rt_reg, sizeof(void (*)(void *)));
		*macro_cache = NULL;
		list *macro_sexpr_cache = buffer_alloc(ectx->rt_reg, sizeof(list *));
		*macro_sexpr_cache = nil;
		
		//Essentially generates code to call np_expansion to load a compiled version of the macro expansion
		//into memory, and then call that code providing it with a continuation back to the call site.
		union expression *return_with_ref_arg = make_reference(NULL, ct_reg);
		union expression *return_with_ref = make_reference(NULL, ct_reg);
		union expression *return_with = make_with(return_with_ref,
			make_jump1(make_invoke6(make_literal((unsigned long) static_np_expansion, ct_reg),
				s->non_primitive.reference, make_literal((unsigned long) copy_fragment(s->non_primitive.argument,
				ectx->rt_reg), ct_reg), make_literal((unsigned long) ectx, ct_reg),
				make_literal((unsigned long) s->non_primitive.st_binds, ct_reg), make_literal((unsigned long) macro_cache, ct_reg),
					make_literal((unsigned long) macro_sexpr_cache, ct_reg), ct_reg), return_with_ref_arg, ct_reg), ct_reg);
		refer_reference(return_with_ref_arg, return_with_ref);
		return return_with;
	} else {
		return s;
	}
}
