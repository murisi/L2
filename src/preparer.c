bool function_named(void *expr_void, void *ctx) {
	union expression *expr = expr_void;
	return expr->base.type == function && strequal(ctx, expr->function.reference->reference.source_name);
}

bool reference_named(void *expr_void, void *ctx) {
	return strequal(ctx, ((union expression *) expr_void)->reference.source_name);
}

jmp_buf *vfind_multiple_definitions_handler;

union expression *vfind_multiple_definitions(union expression *e) {
	union expression *t;
	list *partial;
	switch(e->base.type) {
		case begin: {
			foreachlist(partial, t, e->begin.expressions) {
				if(t->base.type == function && exists(function_named, &(*partial)->rst, t->function.reference->reference.source_name)) {
					thelongjmp(*vfind_multiple_definitions_handler,
						make_multiple_definition(t->function.reference->reference.source_name));
				}
			}
			break;
		} case continuation: case function: {
			list ref_with_params = lst(e->continuation.reference, e->continuation.parameters);
			foreachlist(partial, t, ref_with_params) {
				if(exists(reference_named, &(*partial)->rst, t->reference.source_name)) {
					thelongjmp(*vfind_multiple_definitions_handler, make_multiple_definition(t->reference.source_name));
				}
			}
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

union expression *vblacklist_references(union expression *s) {
	if(s->base.type == reference) {
		prepend(s->reference.source_name, &generate_string_blacklist);
	}
	return s;
}

union expression *referent_of(union expression *reference) {
	union expression *t;
	for(t = reference; t != NULL; t = (t->base.type == function ? get_zeroth_static(t->base.parent) : t->base.parent)) {
		switch(t->base.type) {
			case begin: {
				union expression *u;
				foreach(u, t->begin.expressions) {
					if(u->base.type == function && !strcmp(u->function.reference->reference.source_name, reference->reference.source_name)) {
						return u->function.reference;
					}
				}
				break;
			} case function: case continuation: case with: {
				if(!strcmp(t->function.reference->reference.source_name, reference->reference.source_name)) {
					return t->function.reference;
				} else if(t->base.type == function || t->base.type == continuation) {
					union expression *u;
					foreach(u, t->function.parameters) {
						if(!strcmp(u->reference.source_name, reference->reference.source_name)) {
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

/*
 * Parameter to link_references. It is not modified by link_references. It is not an
 * argument to avoid it being passed recursively.
 */
union expression *vlink_references_program;
jmp_buf *vlink_references_handler;

union expression *vlink_references(union expression *s) {
	if(s->base.type == reference) {
		s->reference.referent = referent_of(s);
		if(s->reference.referent == NULL) {
			s->reference.referent = prepend_parameter("", vlink_references_program);
			s->reference.referent->reference.source_name = s->reference.source_name;
		} else if(is_jump_reference(s) && is_c_reference(s->reference.referent) &&
			length(s->reference.parent->jump.arguments) != length(target_expression(s)->continuation.parameters)) {
				thelongjmp(*vlink_references_handler, make_param_count_mismatch(s->reference.parent, target_expression(s)));
		} else if(is_invoke_reference(s) && is_function_reference(s->reference.referent) &&
			length(s->reference.parent->invoke.arguments) != length(target_expression(s)->function.parameters)) {
				thelongjmp(*vlink_references_handler, make_param_count_mismatch(s->reference.parent, target_expression(s)));
		}
	} else if(s->base.type == continuation && is_jump_reference(s) &&
		length(s->continuation.parent->jump.arguments) != length(s->continuation.parameters)) {
			thelongjmp(*vlink_references_handler, make_param_count_mismatch(s->continuation.parent, s));
	} else if(s->base.type == function && s->function.parent && s->function.parent->base.type == invoke &&
		s->function.parent->invoke.reference == s && length(s->function.parent->invoke.arguments) != length(s->function.parameters)) {
			thelongjmp(*vlink_references_handler, make_param_count_mismatch(s->function.parent, s));
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

union expression *vescape_analysis(union expression *s) {
	if(s->base.type == reference && s->reference.referent != s && is_c_reference(s->reference.referent)) {
		vescape_analysis_aux(s, target_expression(s));
	} else if(s->base.type == continuation) {
		vescape_analysis_aux(s, s);

	}
	return s;
}

/*
 * Following function renames definitions to newly generated names in order to avoid
 * name collisions in the code generation process. Its "return-by-reference" parameter
 * vrename_definition_references_renames must be initialized before the following
 * function's invokation.
 */

union expression *vrename_definition_references(union expression *s) {
	switch(s->base.type) {
		case function: case continuation: case with: {
			s->function.reference->reference.name = generate_string();
			
			if(s->base.type == function || s->base.type == continuation) {
				union expression *t;
				foreach(t, s->function.parameters) {
					t->reference.name = generate_string();
				}
			}
			break;
		}
	}
	return s;
}

union expression *vrename_usage_references(union expression *s) {
	if(s->base.type == reference) {
		s->reference.name = s->reference.referent->reference.name;
	}
	return s;
}

union expression *(*visit_expressions_visitor)(union expression *);

void visit_expressions(union expression **s) {
	switch((*s)->base.type) {
		case begin: {
			union expression **t;
			foreach(t, address_list((*s)->begin.expressions)) {
				visit_expressions(t);
			}
			break;
		} case _if: {
			visit_expressions(&(*s)->_if.condition);
			visit_expressions(&(*s)->_if.consequent);
			visit_expressions(&(*s)->_if.alternate);
			break;
		} case function: case continuation: case with: {
			if((*s)->base.type == function || (*s)->base.type == continuation) {
				union expression **t;
				foreach(t, address_list((*s)->function.parameters)) {
					visit_expressions(t);
				}
			}
			visit_expressions(&(*s)->function.reference);
			visit_expressions(&(*s)->function.expression);
			break;
		} case jump: case invoke: {
			union expression **t;
			foreach(t, lst(&(*s)->invoke.reference, address_list((*s)->invoke.arguments))) {
				visit_expressions(t);
			}
			break;
		}
	}
	union expression *parent = (*s)->base.parent;
	*s = (*visit_expressions_visitor)(*s);
	(*s)->base.parent = parent;
}

#define emit(x) { \
	union expression *_emit_x = x; \
	append(_emit_x, &container->begin.expressions); \
	_emit_x->base.parent = container; \
}

union expression *make_local(union expression *function) {
	union expression *r = generate_reference();
	r->reference.parent = function;
	prepend(r, &function->function.locals);
	return r;
}

// Renders the "parent" field meaningless
union expression *use_return_value(union expression *n, union expression *ret_val) {
	switch(n->base.type) {
		case with: {
			set_fst(n->with.parameter, generate_reference());
		} case continuation: {
			n->continuation.return_value = ret_val;
			put(n, continuation.expression, use_return_value(n->continuation.expression, make_local(get_zeroth_function(n))));
			return n;
		} case function: {
			n->function.return_value = ret_val;
			n->function.expression_return_value = make_local(n);
			put(n, function.expression, use_return_value(n->function.expression, n->function.expression_return_value));
			return n;
		} case invoke: case jump: {
			union expression *container = make_begin();
			
			if(n->base.type == jump && n->jump.short_circuit && n->jump.reference->base.type == reference) {
				n->jump.reference->reference.return_value = NULL;
			} else {
				union expression *ref_ret_val = make_local(get_zeroth_function(n));
				emit(use_return_value(n->invoke.reference, ref_ret_val));
				n->invoke.reference = ref_ret_val;
			}
			
			union expression **t;
			foreach(t, address_list(n->invoke.arguments)) {
				union expression *arg_ret_val = make_local(get_zeroth_function(n));
				emit(use_return_value(*t, arg_ret_val));
				*t = arg_ret_val;
			}
			n->invoke.return_value = ret_val;
			emit(n);
			return container;
		} case _if: {
			union expression *container = make_begin();
			
			put(n, _if.consequent, use_return_value(n->_if.consequent, ret_val));
			put(n, _if.alternate, use_return_value(n->_if.alternate, ret_val));
			
			union expression *cond_ret_val = make_local(get_zeroth_function(n));
			emit(use_return_value(n->_if.condition, cond_ret_val));
			put(n, _if.condition, cond_ret_val);
			emit(n);
			return container;
		} case begin: {
			union expression **t;
			foreach(t, address_list(n->begin.expressions)) {
				*t = use_return_value(*t, make_local(get_zeroth_function(n)));
				(*t)->base.parent = n;
			}
			return n;
		} case reference: {
			n->reference.return_value = ret_val;
			return n;
		} case constant: {
			n->constant.return_value = ret_val;
			return n;
		}
	}
}

union expression *vmerge_begins(union expression *n) {
	if(n->base.type == begin) {
		union expression *t;
		list *l;
		
		foreachlist(l, t, n->begin.expressions) {
			repeat:
			if(t->base.type == begin) {
				append_list(&t->begin.expressions, rst(*l));
				*l = t->begin.expressions;
				if(is_nil(t->begin.expressions)) {
					break;
				} else {
					t = fst(*l);
					goto repeat;
				}
			}
		}
	}
	return n;
}
