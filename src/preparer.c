bool function_named(void *expr_void, void *ctx) {
	union expression *expr = expr_void;
	return expr->base.type == function && strequal(ctx, expr->function.reference->reference.name);
}

bool reference_named(void *expr_void, void *ctx) {
	return strequal(ctx, ((union expression *) expr_void)->reference.name);
}

jmp_buf *vfind_multiple_definitions_handler;

union expression *vfind_multiple_definitions(union expression *e) {
	union expression *t;
	list *partial;
	switch(e->base.type) {
		case begin: {
			foreachlist(partial, t, e->begin.expressions) {
				if(t->base.type == function && exists(function_named, rst(*partial), t->function.reference->reference.name)) {
					longjmp(*vfind_multiple_definitions_handler,
						(int) make_multiple_definition(t->function.reference->reference.name));
				}
			}
			break;
		} case makec: case function: {
			list ref_with_params = lst(e->makec.reference, e->makec.parameters);
			foreachlist(partial, t, ref_with_params) {
				if(exists(reference_named, rst(*partial), t->reference.name)) {
					longjmp(*vfind_multiple_definitions_handler, (int) make_multiple_definition(t->reference.name));
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
		prepend(s->reference.name, &generate_string_blacklist);
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
					if(u->base.type == function && !strcmp(u->function.reference->reference.name, reference->reference.name)) {
						return u->function.reference;
					}
				}
				break;
			} case function: case makec: case withc: {
				if(!strcmp(t->function.reference->reference.name, reference->reference.name)) {
					return t->function.reference;
				} else if(t->base.type == function || t->base.type == makec) {
					union expression *u;
					foreach(u, t->function.parameters) {
						if(!strcmp(u->reference.name, reference->reference.name)) {
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

bool is_continue_reference(union expression *s) {
	return s->base.parent->base.type == _continue && s->base.parent->_continue.reference == s;
}

bool is_invoke_reference(union expression *s) {
	return s->base.parent->base.type == invoke && s->base.parent->invoke.reference == s;
}

bool is_c_reference(union expression *s) {
	return (s->base.parent->base.type == makec || s->base.parent->base.type == withc) && s->base.parent->makec.reference == s;
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
			s->reference.referent = prepend_parameter(s->reference.name, vlink_references_program);
		} else if(is_continue_reference(s) && is_c_reference(s->reference.referent) &&
			length(s->reference.parent->_continue.arguments) != length(target_expression(s)->makec.parameters)) {
				longjmp(*vlink_references_handler, (int) make_param_count_mismatch(s->reference.parent, target_expression(s)));
		} else if(is_invoke_reference(s) && is_function_reference(s->reference.referent) &&
			length(s->reference.parent->invoke.arguments) != length(target_expression(s)->function.parameters)) {
				longjmp(*vlink_references_handler, (int) make_param_count_mismatch(s->reference.parent, target_expression(s)));
		}
	} else if(s->base.type == makec && is_continue_reference(s) &&
		length(s->makec.parent->_continue.arguments) != length(s->makec.parameters)) {
			longjmp(*vlink_references_handler, (int) make_param_count_mismatch(s->makec.parent, s));
	} else if(s->base.type == function && s->function.parent && s->function.parent->base.type == invoke &&
		s->function.parent->invoke.reference == s && length(s->function.parent->invoke.arguments) != length(s->function.parameters)) {
			longjmp(*vlink_references_handler, (int) make_param_count_mismatch(s->function.parent, s));
	}
	return s;
}

void vescape_analysis_aux(union expression *ref, union expression *target) {
	if(is_continue_reference(ref)) {
		ref->reference.parent->_continue.short_circuit = target;
	} else {
		target->makec.escapes = true;
	}
}

union expression *vescape_analysis(union expression *s) {
	if(s->base.type == reference && s->reference.referent != s && is_c_reference(s->reference.referent)) {
		vescape_analysis_aux(s, target_expression(s));
	} else if(s->base.type == makec) {
		vescape_analysis_aux(s, s);

	}
	return s;
}

struct name_record {
	union expression *reference;
	char *original_name;
};

struct name_record *make_name_record(union expression *reference, char *original_name) {
	struct name_record *r = malloc(sizeof(struct name_record));
	r->reference = reference;
	r->original_name = original_name;
	return r;
}

/*
 * Following function renames definitions to newly generated names in order to avoid
 * name collisions in the code generation process. Its "return-by-reference" parameter
 * vrename_definition_references_renames must be initialized before the following
 * function's invokation.
 */

list vrename_definition_references_name_records;

union expression *vrename_definition_references(union expression *s) {
	switch(s->base.type) {
		case function: case makec: case withc: {
			char *original_name = s->function.reference->reference.name;
			s->function.reference->reference.name = generate_string();
			prepend(make_name_record(s->function.reference, original_name), &vrename_definition_references_name_records);
			
			if(s->base.type == function || s->base.type == makec) {
				union expression *t;
				foreach(t, s->function.parameters) {
					original_name = t->reference.name;
					t->reference.name = generate_string();
					prepend(make_name_record(t, original_name), &vrename_definition_references_name_records);
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
		} case function: case makec: case withc: {
			if((*s)->base.type == function || (*s)->base.type == makec) {
				union expression **t;
				foreach(t, address_list((*s)->function.parameters)) {
					visit_expressions(t);
				}
			}
			visit_expressions(&(*s)->function.reference);
			visit_expressions(&(*s)->function.expression);
			break;
		} case _continue: case invoke: {
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
		case withc: {
			set_fst(n->withc.parameter, generate_reference());
		} case makec: {
			n->makec.return_value = ret_val;
			put(n, makec.expression, use_return_value(n->makec.expression, make_local(get_zeroth_function(n))));
			return n;
		} case function: {
			n->function.return_value = ret_val;
			n->function.expression_return_value = make_local(n);
			put(n, function.expression, use_return_value(n->function.expression, n->function.expression_return_value));
			return n;
		} case invoke: case _continue: {
			union expression *container = make_begin();
			
			if(n->base.type == _continue && n->_continue.short_circuit && n->_continue.reference->base.type == reference) {
				n->_continue.reference->reference.return_value = NULL;
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
