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
    } case constrain: {
      visit_expressions(visitor, &(*s)->constrain.expression, ctx);
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
    } case constrain: {
      pre_visit_expressions(visitor, &(*s)->constrain.expression, ctx);
      break;
    }
  }
}

bool defined_string_equals(char *a, char *b) {
  return a && b && !strcmp(a, b);
}

bool reference_named(union expression *expr, char *ctx) {
  return defined_string_equals(expr->symbol.name, ctx);
}

union expression *vfind_multiple_definitions(union expression *e, void *ctx) {
  jumpbuf *handler = ctx;
  union expression *t;
  list *partial;
  buffer tempreg = create_buffer(0);
  switch(e->base.type) {
    case begin: {
      list definitions = nil;
      foreach(t, e->begin.expressions) {
        if(t->base.type == storage || t->base.type == function) {
          prepend(t->storage.reference->symbol.name, &definitions, tempreg);
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
        if(exists((bool (*)(void *, void *)) reference_named, &(*partial)->rst, t->symbol.name)) {
          throw_multiple_definition(t->symbol.name, handler);
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
  return a == b || (a->symbol.name && b->symbol.name && !strcmp(a->symbol.name, b->symbol.name));
}

struct binding_aug *binding_aug_of(union expression *reference) {
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
              u->storage.reference->symbol.binding_aug->type == absolute_storage))) &&
              reference_equals(u->function.reference, reference)) {
            return u->function.reference->symbol.binding_aug;
          }
        }
        break;
      } case function: {
        //Either link up to ancestral function expression reference or link up to the parameters of (the
        //root function or (the function that provides the stack-frame for this reference.) 
        if(reference_equals(t->function.reference, reference)) {
          return t->function.reference->symbol.binding_aug;
        }
        union expression *u;
        foreach(u, t->function.parameters) {
          if((same_func || u->symbol.binding_aug->type == absolute_storage) && reference_equals(u, reference)) {
            return u->symbol.binding_aug;
          }
        }
        same_func = false;
        break;
      } case continuation: case with: case storage: {
        //Either link up to (the reference of continuation/with/storage expression in the same stack-frame
        //or the reference of the same with static storage) or link up to the parameters of a non-storage
        //expression that are either (in the same stack-fram or have static storage).
        if((same_func || t->continuation.reference->symbol.binding_aug->type == absolute_storage) &&
            reference_equals(t->function.reference, reference)) {
          return t->function.reference->symbol.binding_aug;
        } else if(t->base.type != storage) {
          union expression *u;
          foreach(u, t->function.parameters) {
            if((same_func || u->symbol.binding_aug->type == absolute_storage) && reference_equals(u, reference)) {
              return u->symbol.binding_aug;
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

bool is_continuation_reference(union expression *s) {
  return s->base.parent->base.type == continuation && s->base.parent->continuation.reference == s;
}

bool is_function_reference(union expression *s) {
  return s->base.parent->base.type == function && s->base.parent->function.reference == s;
}

union expression *target_expression(union expression *s) {
  return s->symbol.binding_aug->definition->symbol.parent;
}

union expression *root_function_of(union expression *s) {
  for(; s->base.parent; s = s->base.parent);
  return s;
}

union expression *vlink_symbols(union expression *s, void *ctx) {
  jumpbuf *handler = ((void **) ctx)[0];
  buffer r = ((void **) ctx)[1];
  if(s->base.type == symbol) {
    s->symbol.binding_aug = s->symbol.binding_aug ? s->symbol.binding_aug : binding_aug_of(s);
    if(!s->symbol.binding_aug) {
      union expression *stg = make_storage(make_symbol(s->symbol.name, r), nil, r);
      struct binding_aug *bndg = stg->storage.reference->symbol.binding_aug;
      bndg->type = absolute_storage;
      bndg->scope = global_scope;
      bndg->state = undefined_state;
      prepend(stg, &root_function_of(s)->function.expression->begin.expressions, r);
      stg->storage.parent = root_function_of(s)->function.expression;
      s->symbol.binding_aug = bndg;
    }
  }
  return s;
}

void vescape_analysis_aux(union expression *ref, union expression *target) {
  if(is_jump_reference(ref)) {
    ref->symbol.parent->jump.short_circuit = target;
  } else {
    target->continuation.escapes = true;
  }
}

union expression *vescape_analysis(union expression *s, void *ctx) {
  if(s->base.type == symbol && s->symbol.binding_aug->definition != s && is_c_reference(s->symbol.binding_aug->definition)) {
    vescape_analysis_aux(s, target_expression(s));
  } else if(s->base.type == continuation) {
    vescape_analysis_aux(s, s);

  }
  return s;
}

union expression *vfind_dependencies(union expression *s, buffer r) {
  switch(s->base.type) {
    case begin: {
      union expression *t;
      foreach(t, s->begin.expressions) {
        prepend(t, &s->begin.dependencies, r);
      }
      break;
    } case _if: {
      prepend(s->_if.condition, &s->_if.dependencies, r);
      prepend(s->_if.consequent, &s->_if.dependencies, r);
      prepend(s->_if.alternate, &s->_if.dependencies, r);
      break;
    } case function: case continuation: case with: {
      prepend(s->function.expression, &s->function.dependencies, r);
      break;
    } case storage: case jump: case invoke: {
      prepend(s->storage.reference, &s->storage.dependencies, r);
      union expression *t;
      foreach(t, s->storage.arguments) {
        prepend(t, &s->storage.dependencies, r);
      }
      break;
    } case symbol: {
      union expression *definition = s->symbol.binding_aug->definition;
      prepend(definition->symbol.parent, &s->symbol.dependencies, r);
      prepend(s, &definition->symbol.parent->base.dependencies, r);
      break;
    } case constrain: {
      prepend(s->constrain.expression, &s->constrain.dependencies, r);
      prepend(s, &s->constrain.expression->base.dependencies, r);
      break;
    }
  }
  return s;
}

void construct_sccs(union expression *s, int preorder, list *stack, list *sccs, buffer r) {
  if(s->function.lowlink == 0) {
    list marker = *stack;
    s->function.lowlink = preorder;
    union expression *t;
    {foreach(t, s->base.dependencies) {
      construct_sccs(t, preorder + 1, stack, sccs, r);
      if(t->base.lowlink < s->base.lowlink) {
        s->base.lowlink = t->base.lowlink;
      }
    }}
    prepend(s, stack, r);
    
    if(s->base.lowlink == preorder) {
      list scc = nil, *t;
      union expression *u;
      foreachlist(t, u, stack) {
        if(*t == marker) break;
        prepend(u, &scc, r);
        u->base.lowlink = -1;
      }
      *stack = marker;
      prepend(scc, sccs, r);
    }
  }
}

bool occurs_in(list var, list val) {
  list varl = get_var(var);
  list vall = is_var(val) ? get_var(val) : val;
  
  if(is_var(vall)) {
    return var_equals(varl, vall);
  } else if(is_token(vall)) {
    return false;
  } else {
    list a;
    foreach(a, vall) {
      if(occurs_in(varl, a)) {
        return true;
      }
    }
    return false;
  }
}

bool unify_var(list var, list val) {
  if(!occurs_in(var, val)) {
    set_var(var, val);
    return true;
  } else {
    return false;
  }
}

bool unify(list x, list y) {
  list xl = is_var(x) ? get_var(x) : x;
  list yl = is_var(y) ? get_var(y) : y;
  
  if(is_var(xl) && is_var(yl) && var_equals(xl, yl)) {
    return true;
  } else if(is_var(xl)) {
    return unify_var(xl, yl);
  } else if(is_var(yl)) {
    return unify_var(yl, xl);
  } else if(is_token(xl) && is_token(yl)) {
    return token_equals(xl, yl);
  } else if(is_token(xl) || is_token(yl)) {
    return false;
  } else if(length(xl) == length(yl)) {
    list a, b;
    foreachzipped(a, b, xl, yl) {
      if(!unify(a, b)) {
        return false;
      }
    }
    return true;
  } else {
    return false;
  }
}

list substitute_variables(list fragment, buffer reg) {
  list d = is_var(fragment) ? get_var(fragment) : fragment;
  if(is_var(d) || is_token(d)) {
    return d;
  } else {
    list res = nil, t;
    foreach(t, d) {
      append(substitute_variables(t, reg), &res, reg);
    }
    return res;
  }
}

list scoped_signature(union expression *e, list scc, buffer reg) {
  if(exists(equals, &scc, e)) {
    return e->base.signature;
  } else {
    return copy_fragment(e->base.signature, reg);
  }
}

void infer_types(union expression *program, buffer expr_buf, jumpbuf *handler) {
  // These (parameterized) types are the only ones built into L2
  list function_token = build_token("function", expr_buf);
  list continuation_token = build_token("continuation", expr_buf);
  
  // Type inferencing is done on strongly connected components
  list stack = nil, sccs = nil, scc;
  visit_expressions(vfind_dependencies, &program, expr_buf);
  construct_sccs(program, 1, &stack, &sccs, expr_buf);
  
  foreach(scc, reverse(sccs, expr_buf)) {
    union expression *e;
    {foreach(e, scc) {
      // The left-hand-sides and right-hand-sides of the signature equations.
      list lhss = nil, rhss = nil;
      switch(e->base.type) {
        case function: {
          list params_signature = nil;
          union expression *param;
          foreach(param, e->function.parameters) {
            append(param->symbol.signature, &params_signature, expr_buf);
          }
          prepend(e->function.signature, &lhss, expr_buf);
          prepend(lst(function_token, lst(params_signature,
            lst(e->function.expression->base.signature, nil, expr_buf), expr_buf), expr_buf), &rhss, expr_buf);
          
          prepend(e->function.signature, &lhss, expr_buf);
          prepend(e->function.reference->symbol.signature, &rhss, expr_buf);
          break;
        } case continuation: {
          list params_signature = nil;
          union expression *param;
          foreach(param, e->continuation.parameters) {
            append(param->symbol.signature, &params_signature, expr_buf);
          }
          prepend(e->continuation.signature, &lhss, expr_buf);
          prepend(lst(continuation_token, lst(params_signature, nil, expr_buf), expr_buf), &rhss, expr_buf);
          
          prepend(e->continuation.signature, &lhss, expr_buf);
          prepend(e->continuation.reference->symbol.signature, &rhss, expr_buf);
          break;
        } case constrain: {
          prepend(e->constrain.signature, &lhss, expr_buf);
          prepend(e->constrain.expression->base.signature, &rhss, expr_buf);
          break;
        } case invoke: {
          list params_signature = nil;
          union expression *arg;
          foreach(arg, e->invoke.arguments) {
            append(scoped_signature(arg, scc, expr_buf), &params_signature, expr_buf);
          }
          prepend(scoped_signature(e->invoke.reference, scc, expr_buf), &lhss, expr_buf);
          prepend(lst(function_token, lst(params_signature,
            lst(e->invoke.signature, nil, expr_buf), expr_buf), expr_buf), &rhss, expr_buf);
          break;
        } case jump: {
          list params_signature = nil;
          union expression *arg;
          foreach(arg, e->jump.arguments) {
            append(scoped_signature(arg, scc, expr_buf), &params_signature, expr_buf);
          }
          prepend(scoped_signature(e->jump.reference, scc, expr_buf), &lhss, expr_buf);
          prepend(lst(continuation_token, lst(params_signature, nil, expr_buf), expr_buf), &rhss, expr_buf);
          break;
        } case with: {
          prepend(e->with.reference->symbol.signature, &lhss, expr_buf);
          prepend(lst(continuation_token, lst(lst(e->with.signature, nil, expr_buf), nil, expr_buf), expr_buf), &rhss, expr_buf);
          break;
        } case _if: {
          list consequent_sig = scoped_signature(e->_if.consequent, scc, expr_buf);
          list alternate_sig = scoped_signature(e->_if.alternate, scc, expr_buf);
          prepend(consequent_sig, &lhss, expr_buf);
          prepend(alternate_sig, &rhss, expr_buf);
          prepend(e->_if.signature, &lhss, expr_buf);
          prepend(consequent_sig, &rhss, expr_buf);
          break;
        } case symbol: {
          prepend(e->symbol.signature, &lhss, expr_buf);
          prepend(e->symbol.binding_aug->definition->symbol.signature, &rhss, expr_buf);
          break;
        }
      }
      list lhs, rhs;
      foreachzipped(lhs, rhs, lhss, rhss) {
        if(!unify(lhs, rhs)) {
          throw_unification(substitute_variables(lhs, expr_buf), substitute_variables(rhs, expr_buf), e, handler);
        }
      }
    }}
    foreach(e, scc) {
      e->base.signature = substitute_variables(e->base.signature, expr_buf);
    }
  }
}

unsigned long containment_analysis(union expression *s) {
  unsigned long contains_flag = CONTAINS_NONE;
  switch(s->base.type) {
    case begin: {
      union expression *t;
      foreach(t, s->begin.expressions) {
        contains_flag |= containment_analysis(t);
      }
      break;
    } case constrain: {
      contains_flag |= containment_analysis(s->constrain.expression);
      break;
    } case _if: {
      contains_flag |= containment_analysis(s->_if.condition);
      unsigned long consequent_flag = containment_analysis(s->_if.consequent);
      unsigned long alternate_flag = containment_analysis(s->_if.alternate);
      if(consequent_flag + alternate_flag == 1) {
        contains_flag = CONTAINS_WITH;
      } else {
        contains_flag |= consequent_flag;
        contains_flag |= alternate_flag;
      }
      break;
    } case function: case continuation: case with: {
      containment_analysis(s->function.expression);
      if(s->base.type == with) {
        contains_flag = CONTAINS_WITH;
      }
      break;
    } case storage: {
      union expression *t;
      foreach(t, s->storage.arguments) {
        contains_flag |= containment_analysis(t);
      }
      break;
    } case jump: case invoke: {
      contains_flag |= containment_analysis(s->invoke.reference);
      union expression *t;
      foreach(t, s->invoke.arguments) {
        contains_flag |= containment_analysis(t);
      }
      s->invoke.contains_flag = contains_flag;
      if(s->base.type == jump) {
        contains_flag = CONTAINS_JUMP;
      }
      break;
    }
  }
  return contains_flag;
}

void classify_program_binding_augs(union expression *expr) {
  switch(expr->base.type) {
    case begin: {
      union expression *t;
      foreach(t, expr->begin.expressions) {
        classify_program_binding_augs(t);
      }
      break;
    } case constrain: {
      classify_program_binding_augs(expr->constrain.expression);
      break;
    } case storage: case jump: case invoke: {
      if(expr->base.type == storage) {
        expr->storage.reference->symbol.binding_aug->type = absolute_storage;
      } else {
        expr->invoke.temp_storage_bndg->type = absolute_storage;
        classify_program_binding_augs(expr->invoke.reference);
      }
      union expression *t;
      foreach(t, expr->storage.arguments) {
        classify_program_binding_augs(t);
      }
      break;
    } case continuation: case with: {
      expr->continuation.reference->symbol.binding_aug->type = absolute_storage;
      union expression *t;
      foreach(t, expr->continuation.parameters) {
        t->symbol.binding_aug->type = absolute_storage;
      }
      classify_program_binding_augs(expr->continuation.expression);
      break;
    } case function: {
      break;
    } case _if: {
      classify_program_binding_augs(expr->_if.condition);
      classify_program_binding_augs(expr->_if.consequent);
      classify_program_binding_augs(expr->_if.alternate);
      break;
    }
  }
}

void _set_(unsigned long *ref, unsigned long val) {
  *ref = val;
}

bool binding_equals(struct binding *bndg1, struct binding *bndg2) {
  return !strcmp(bndg1->name, bndg2->name);
}

Object *load_program_and_mutate(union expression *program, list bindings, buffer expr_buf, buffer obj_buf, jumpbuf *handler);

union expression *generate_metaprogram(union expression *program, list *bindings, buffer expr_buf, buffer obj_buf, jumpbuf *handler);

void *preprocessed_expression_address(union expression *s, list bindings, buffer expr_buf, buffer obj_buf, jumpbuf *handler) {
  union expression *expr_container = make_function(make_symbol(NULL, expr_buf), nil, s, expr_buf);
  union expression *expr_container_program = make_program(lst(expr_container, nil, expr_buf), expr_buf);
  union expression *program_preprocessed = generate_metaprogram(expr_container_program, &bindings, expr_buf, obj_buf, handler);
  load_program_and_mutate(program_preprocessed, bindings, expr_buf, obj_buf, handler);
  union expression *expr_container_preprocessed = program_preprocessed->function.expression->begin.expressions->fst;
  void* (*container_addr)() = (void *) expr_container_preprocessed->function.reference->symbol.binding_aug->offset;
  return container_addr();
}

union expression *vgenerate_metas(union expression *s, void *ctx) {
  jumpbuf *handler = ((void **) ctx)[2];
  buffer expr_buf = ((void **) ctx)[1];
  list bindings = ((void **) ctx)[0];
  
  if(s->base.type == meta) {
    list (*macro)(list, buffer) = preprocessed_expression_address(s->meta.reference, bindings, expr_buf, expr_buf, handler);
    return vgenerate_metas(build_expression(macro(s->meta.argument, expr_buf), expr_buf, handler), ctx);
  } else if(s->base.type == constrain) {
    list (*macro)(buffer) = preprocessed_expression_address(s->constrain.reference, bindings, expr_buf, expr_buf, handler);
    s->constrain.signature = macro(expr_buf);
    return s;
  } else {
    return s;
  }
}

void *init_storage(unsigned long *data, union expression *storage_expr, list *bindings, buffer expr_buf, buffer obj_buf, jumpbuf *handler, void **cache) {
  if(*cache) {
    return *cache;
  } else {
    list sets = nil;
    union expression *arg;
    foreach(arg, storage_expr->storage.arguments) {
      pre_visit_expressions(vgenerate_metas, &arg, (void *[]) {*bindings, expr_buf, handler});
      append(make_invoke2(make_literal((unsigned long) _set_, expr_buf),
        make_literal((unsigned long) data++, expr_buf), arg, expr_buf), &sets, expr_buf);
    }
    *cache = segment(load_program_and_mutate(make_program(sets, expr_buf), *bindings, expr_buf, obj_buf, handler), ".text");
    return *cache;
  }
}

void *init_function(union expression *function_expr, list *bindings, buffer expr_buf, buffer obj_buf, jumpbuf *handler, void **cache) {
  if(*cache) {
    return *cache;
  } else {
    pre_visit_expressions(vgenerate_metas, &function_expr, (void *[]) {*bindings, expr_buf, handler});
    load_program_and_mutate(make_program(lst(function_expr, nil, expr_buf), expr_buf), *bindings, expr_buf, obj_buf, handler);
    *cache = (void *) function_expr->function.reference->symbol.binding_aug->offset;
    return *cache;
  }
}

void *init_expression(union expression *expr, list *bindings, buffer expr_buf, buffer obj_buf, jumpbuf *handler, void **cache) {
  if(*cache) {
    return *cache;
  } else {
    pre_visit_expressions(vgenerate_metas, &expr, (void *[]) {*bindings, expr_buf, handler});
    *cache = segment(load_program_and_mutate(make_program(lst(expr, nil, expr_buf), expr_buf),
      *bindings, expr_buf, obj_buf, handler), ".text");
    return *cache;
  }
}

union expression *generate_metaprogram(union expression *program, list *bindings, buffer expr_buf, buffer obj_buf, jumpbuf *handler) {
  union expression *s;
  list c = nil;
  foreach(s, program->function.expression->begin.expressions) {
    void **cache = buffer_alloc(obj_buf, sizeof(void *));
    *cache = NULL;
    if(s->base.type == storage) {
      list args = nil;
      int i;
      for(i = 0; i < length(s->storage.arguments); i++) {
        prepend(make_begin(nil, expr_buf), &args, expr_buf);
      }
      union expression *storage_ref = make_symbol(s->storage.reference->symbol.name, expr_buf);
      append(make_storage(storage_ref, args, expr_buf), &c, expr_buf);
      union expression *storage_ref_arg = make_symbol(NULL, expr_buf);
      bind_symbol(storage_ref_arg, storage_ref);
      append(make_invoke0(make_invoke7(make_literal((unsigned long) init_storage, expr_buf), storage_ref_arg,
        make_literal((unsigned long) s, expr_buf), make_literal((unsigned long) bindings, expr_buf),
        make_literal((unsigned long) expr_buf, expr_buf), make_literal((unsigned long) obj_buf, expr_buf),
        make_literal((unsigned long) handler, expr_buf), make_literal((unsigned long) cache, expr_buf), expr_buf), expr_buf),
        &c, expr_buf);
    } else if(s->base.type == function) {
      list params = nil, args = nil;
      int i;
      for(i = 0; i < length(s->function.parameters); i++) {
        prepend(make_symbol(NULL, expr_buf), &params, expr_buf);
        prepend(make_symbol(NULL, expr_buf), &args, expr_buf);
      }
      append(make_function(make_symbol(s->function.reference->symbol.name, expr_buf), params,
        make_invoke(make_invoke6(make_literal((unsigned long) init_function, expr_buf),
          make_literal((unsigned long) s, expr_buf), make_literal((unsigned long) bindings, expr_buf),
          make_literal((unsigned long) expr_buf, expr_buf), make_literal((unsigned long) obj_buf, expr_buf),
          make_literal((unsigned long) handler, expr_buf), make_literal((unsigned long) cache, expr_buf), expr_buf),
          args, expr_buf), expr_buf), &c, expr_buf);
      union expression *a, *t;
      foreachzipped(a, t, params, args) {
        bind_symbol(t, a);
      }
    } else {
      append(make_invoke0(make_invoke6(make_literal((unsigned long) init_expression, expr_buf),
        make_literal((unsigned long) s, expr_buf), make_literal((unsigned long) bindings, expr_buf),
        make_literal((unsigned long) expr_buf, expr_buf), make_literal((unsigned long) obj_buf, expr_buf),
        make_literal((unsigned long) handler, expr_buf), make_literal((unsigned long) cache, expr_buf), expr_buf), expr_buf),
        &c, expr_buf);
    }
  }
  return make_program(c, expr_buf);
}

