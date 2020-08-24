void visit_expressions(union expression *(*visitor)(union expression *, void *), union expression **s, void *ctx) {
  switch((*s)->base.type) {
    case _if: {
      visit_expressions(visitor, &(*s)->_if.condition, ctx);
      visit_expressions(visitor, &(*s)->_if.consequent, ctx);
      visit_expressions(visitor, &(*s)->_if.alternate, ctx);
      break;
    } case function: case continuation: case with: case constrain: {
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
  *s = (*visitor)(*s, ctx);
}

void pre_visit_expressions(union expression *(*visitor)(union expression *, void *), union expression **s, void *ctx) {
  *s = (*visitor)(*s, ctx);
  
  switch((*s)->base.type) {
    case _if: {
      pre_visit_expressions(visitor, &(*s)->_if.condition, ctx);
      pre_visit_expressions(visitor, &(*s)->_if.consequent, ctx);
      pre_visit_expressions(visitor, &(*s)->_if.alternate, ctx);
      break;
    } case function: case continuation: case with: case constrain: {
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
  region tempreg = create_region(0);
  switch(e->base.type) {
    case continuation: case function: {
      list ref_with_params = lst(e->continuation.reference, e->continuation.parameters, tempreg);
      foreachlist(partial, t, &ref_with_params) {
        if(exists((bool (*)(void *, void *)) reference_named, &(*partial)->rst, t->symbol.name)) {
          throw_multiple_definition(t->symbol.name, handler);
        }
      }
      break;
    }
  }
  destroy_region(tempreg);
  return e;
}

bool reference_equals(union expression *a, union expression *b) {
  return a == b || (a->symbol.name && b->symbol.name && !strcmp(a->symbol.name, b->symbol.name));
}

list global_binding_augs_of(list exprs, region r) {
  list binding_augs = nil;
  union expression *t;
  foreach(t, exprs) {
    if(t->base.type == function || t->base.type == storage) {
      t->function.reference->symbol.binding_aug->scope = global_scope;
      prepend(t->function.reference->symbol.binding_aug, &binding_augs, r);
    }
  }
  return binding_augs;
}

union expression *vunlink_symbols(union expression *s, void *ctx) {
  list blacklist = ctx;
  if(s->base.type == symbol) {
    if(exists(equals, &blacklist, s->symbol.binding_aug)) {
      s->symbol.binding_aug = NULL;
    }
  }
  return s;
}

bool assign_binding(union expression *s, list bindings) {
  struct binding_aug *ba;
  {foreach(ba, bindings) {
    if(ba->name && s->symbol.name && !strcmp(ba->name, s->symbol.name)) {
      s->symbol.binding_aug = ba;
      return true;
    }
  }}
  return false;
}

void link_symbols(union expression *s, bool static_storage, list *undefined_bindings, list static_bindings, list dynamic_bindings, region r) {
  list *bindings = static_storage ? &static_bindings : &dynamic_bindings;
  switch(s->base.type) {
    case function: {
      prepend(s->function.reference->symbol.binding_aug, &static_bindings, r);
      dynamic_bindings = nil;
      union expression *u;
      foreach(u, s->function.parameters) {
        prepend(u->symbol.binding_aug, &dynamic_bindings, r);
      }
      link_symbols(s->function.expression, false, undefined_bindings, static_bindings, dynamic_bindings, r);
      break;
    } case with: case continuation: {
      prepend(s->function.reference->symbol.binding_aug, bindings, r);
      union expression *u;
      foreach(u, s->function.parameters) {
        prepend(u->symbol.binding_aug, bindings, r);
      }
      link_symbols(s->function.expression, static_storage, undefined_bindings, static_bindings, dynamic_bindings, r);
      break;
    } case storage: {
      prepend(s->storage.reference->symbol.binding_aug, bindings, r);
      union expression *u;
      foreach(u, s->storage.arguments) {
        link_symbols(u, static_storage, undefined_bindings, static_bindings, dynamic_bindings, r);
      }
      break;
    } case symbol: {
      if(!s->symbol.binding_aug && !assign_binding(s, dynamic_bindings)&&
        !assign_binding(s, static_bindings) && !assign_binding(s, *undefined_bindings)) {
          union expression *stg = make_storage(make_symbol(s->symbol.name, NULL, NULL, r), nil, NULL, NULL, r);
          struct binding_aug *bndg = stg->storage.reference->symbol.binding_aug;
          bndg->type = absolute_storage;
          bndg->scope = global_scope;
          bndg->state = undefined_state;
          s->symbol.binding_aug = bndg;
          prepend(s->symbol.binding_aug, undefined_bindings, r);
      }
      break;
    } case _if: {
      link_symbols(s->_if.condition, static_storage, undefined_bindings, static_bindings, dynamic_bindings, r);
      link_symbols(s->_if.consequent, static_storage, undefined_bindings, static_bindings, dynamic_bindings, r);
      link_symbols(s->_if.alternate, static_storage, undefined_bindings, static_bindings, dynamic_bindings, r);
      break;
    } case invoke: case jump: {
      link_symbols(s->invoke.reference, static_storage, undefined_bindings, static_bindings, dynamic_bindings, r);
      union expression *u;
      foreach(u, s->invoke.arguments) {
        link_symbols(u, static_storage, undefined_bindings, static_bindings, dynamic_bindings, r);
      }
      break;
    } case constrain: {
      link_symbols(s->constrain.expression, static_storage, undefined_bindings, static_bindings, dynamic_bindings, r);
      break;
    }
  }
}

void escape_analysis(union expression *s, bool escaping) {
  switch(s->base.type) {
    case symbol: {
      union expression *target_expr = s->symbol.binding_aug->expression;
      if(escaping && (target_expr->base.type == continuation || target_expr->base.type == with) &&
          target_expr->continuation.reference == s->symbol.binding_aug->symbol) {
        target_expr->continuation.escapes = escaping;
      }
      break;
    } case _if: {
      escape_analysis(s->_if.condition, true);
      escape_analysis(s->_if.consequent, true);
      escape_analysis(s->_if.alternate, true);
      break;
    } case function: case continuation: case with: {
      if(escaping && s->base.type == continuation) {
        s->continuation.escapes = escaping;
      }
      escape_analysis(s->function.expression, true);
      break;
    } case storage: case jump: case invoke: {
      if(s->base.type == jump) {
        if(s->jump.reference->base.type == symbol) {
          union expression *target_expr = s->jump.reference->symbol.binding_aug->expression;
          if((target_expr->base.type == continuation || target_expr->base.type == with) &&
              target_expr->continuation.reference == s->jump.reference->symbol.binding_aug->symbol) {
            s->jump.short_circuit = target_expr;
          }
        } else if(s->jump.reference->base.type == continuation) {
          s->jump.short_circuit = s->jump.reference;
        }
        escape_analysis(s->jump.reference, false);
      } else if(s->base.type == invoke) {
        escape_analysis(s->jump.reference, true);
      }
      union expression *t;
      foreach(t, s->storage.arguments) {
        escape_analysis(t, true);
      }
      break;
    } case constrain: {
      escape_analysis(s->constrain.expression, true);
      break;
    }
  }
}

union expression *vfind_dependencies(union expression *s, region r) {
  switch(s->base.type) {
    case _if: {
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
      union expression *target_expr = s->symbol.binding_aug->expression;
      prepend(target_expr, &s->symbol.dependencies, r);
      prepend(s, &target_expr->base.dependencies, r);
      break;
    } case constrain: {
      prepend(s->constrain.expression, &s->constrain.dependencies, r);
      prepend(s, &s->constrain.expression->base.dependencies, r);
      break;
    }
  }
  return s;
}

void construct_sccs(union expression *s, int preorder, list *stack, list *sccs, region r) {
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
  list eval = evaluate(val);
  
  if(is_var(eval)) {
    return var_equals(var, eval);
  } else if(is_token(eval)) {
    return false;
  } else {
    list a;
    foreach(a, eval) {
      if(occurs_in(var, a)) {
        return true;
      }
    }
    return false;
  }
}

bool unify_var(list var, list val) {
  if(!occurs_in(var, val)) {
    set_val(var, val);
    return true;
  } else {
    return false;
  }
}

bool unify(list x, list y) {
  list xl = evaluate(x);
  list yl = evaluate(y);
  
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

list scoped_signature(union expression *e, list scc, region reg) {
  if(exists(equals, &scc, e)) {
    return e->base.signature;
  } else {
    list old_vars = nil, new_vars = nil;
    return copy_fragment(e->base.signature, &old_vars, &new_vars, reg);
  }
}

void infer_types(list exprs, region expr_buf, jumpbuf *handler) {
  // These (parameterized) types are the only ones built into L2
  list function_token = build_token("function", expr_buf);
  list continuation_token = build_token("continuation", expr_buf);
  
  // Type inferencing is done on strongly connected components
  list stack = nil, sccs = nil, scc;
  union expression *expr;
  {foreach(expr, exprs) { visit_expressions(vfind_dependencies, &expr, expr_buf); }}
  {foreach(expr, exprs) { construct_sccs(expr, 1, &stack, &sccs, expr_buf); }}
  
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
          
          prepend(e->with.signature, &lhss, expr_buf);
          prepend(e->with.expression->base.signature, &rhss, expr_buf);
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
          prepend(e->symbol.binding_aug->symbol->symbol.signature, &rhss, expr_buf);
          break;
        }
      }
      list lhs, rhs;
      foreachzipped(lhs, rhs, lhss, rhss) {
        if(!unify(lhs, rhs)) {
          throw_unification(recursive_evaluate(lhs, expr_buf), recursive_evaluate(rhs, expr_buf), e, handler);
        }
      }
    }}
    foreach(e, scc) {
      e->base.signature = recursive_evaluate(e->base.signature, expr_buf);
    }
  }
}

unsigned long containment_analysis(union expression *s) {
  unsigned long contains_flag = CONTAINS_NONE;
  switch(s->base.type) {
    case constrain: {
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
    case constrain: {
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

Object *load_program_and_mutate(list exprs, list bindings, region expr_buf, region obj_buf, jumpbuf *handler);

list generate_metaprogram(list exprs, list *bindings, region expr_buf, region obj_buf, jumpbuf *handler);

char *preprocessed_expression_address(void **addr, union expression *s, list bindings, region expr_buf, region obj_buf, jumpbuf *handler) {
  if(s->base.type == symbol) {
    struct binding *bndg;
    foreach(bndg, bindings) {
      if(!strcmp(bndg->name, s->symbol.name)) {
        *addr = bndg->address;
        return NULL;
      }
    }
    return s->symbol.name;
  } else {
    union expression *expr_container = make_function(make_symbol(NULL, NULL, NULL, expr_buf), nil, s, NULL, NULL, expr_buf);
    list exprs_preprocessed = generate_metaprogram(lst(expr_container, nil, expr_buf),
      &bindings, expr_buf, obj_buf, handler);
    load_program_and_mutate(exprs_preprocessed, bindings, expr_buf, obj_buf, handler);
    union expression *expr_container_preprocessed = exprs_preprocessed->fst;
    void* (*container_addr)() = (void *) expr_container_preprocessed->function.reference->symbol.binding_aug->offset;
    *addr = container_addr();
    return NULL;
  }
}

char *vgenerate_metas_no_throw(union expression **out, union expression *s, void *ctx) {
  jumpbuf *handler = ((void **) ctx)[2];
  region expr_buf = ((void **) ctx)[1];
  list bindings = ((void **) ctx)[0];
  
  if(s->base.type == meta) {
    list (*macro)(list, region);
    char *missing_sym_name = preprocessed_expression_address((void **) &macro, s->meta.reference, bindings, expr_buf, expr_buf, handler);
    if(missing_sym_name) return missing_sym_name;
    return vgenerate_metas_no_throw(out, build_expression(macro(s->meta.fragment->rst, expr_buf), s, expr_buf, handler), ctx);
  } else if(s->base.type == constrain) {
    list (*macro)(region);
    char *missing_sym_name = preprocessed_expression_address((void **) &macro, s->constrain.reference, bindings, expr_buf, expr_buf, handler);
    if(missing_sym_name) return missing_sym_name;
    s->constrain.signature = macro(expr_buf);
    *out = s;
    return NULL;
  } else {
    *out = s;
    return NULL;
  }
}

union expression *vgenerate_metas(union expression *s, void *ctx) {
  jumpbuf *handler = ((void **) ctx)[2];
  union expression *out;
  char *missing_sym_name = vgenerate_metas_no_throw(&out, s, ctx);
  if(missing_sym_name) {
    throw_undefined_symbol(missing_sym_name, handler);
  } else {
    return out;
  }
}

void *init_function(union expression *function_expr, list *bindings, region expr_buf, region obj_buf, jumpbuf *handler, void **cache) {
  if(*cache) {
    return *cache;
  } else {
    pre_visit_expressions(vgenerate_metas, &function_expr, (void *[]) {*bindings, expr_buf, handler});
    load_program_and_mutate(lst(function_expr, nil, expr_buf), *bindings, expr_buf, obj_buf, handler);
    *cache = (void *) function_expr->function.reference->symbol.binding_aug->offset;
    return *cache;
  }
}

// Loop through list of expresssions and replace functions with thunks that will compile the bodies in-time.

list generate_metaprogram(list exprs, list *bindings, region expr_buf, region obj_buf, jumpbuf *handler) {
  union expression *s;
  list c = nil;
  foreach(s, exprs) {
    if(s->base.type == function) {
      void **cache = region_alloc(obj_buf, sizeof(void *));
      *cache = NULL;
      list params = nil, args = nil;
      int i;
      for(i = 0; i < length(s->function.parameters); i++) {
        prepend(make_symbol(NULL, NULL, NULL, expr_buf), &params, expr_buf);
        prepend(make_symbol(NULL, NULL, NULL, expr_buf), &args, expr_buf);
      }
      append(make_function(make_symbol(s->function.reference->symbol.name, NULL, NULL, expr_buf), params,
        make_invoke(make_invoke6(make_literal((unsigned long) init_function, NULL, NULL, expr_buf),
          make_literal((unsigned long) s, NULL, NULL, expr_buf), make_literal((unsigned long) bindings, NULL, NULL, expr_buf),
          make_literal((unsigned long) expr_buf, NULL, NULL, expr_buf), make_literal((unsigned long) obj_buf, NULL, NULL, expr_buf),
          make_literal((unsigned long) handler, NULL, NULL, expr_buf), make_literal((unsigned long) cache, NULL, NULL, expr_buf),
          NULL, NULL, expr_buf), args, NULL, NULL, expr_buf), NULL, NULL, expr_buf), &c, expr_buf);
      union expression *a, *t;
      foreachzipped(a, t, params, args) {
        bind_symbol(t, a);
      }
    }
  }
  return c;
}

// Loop through list of expressions and try to expand meta-expressions. Return the newly created expressions.

list try_generate_metas(list exprs, list bindings, region expr_buf, region obj_buf, jumpbuf *handler) {
  list extension = nil;
  union expression **s;
  foreachaddress(s, exprs) {
    if((*s)->base.type == meta) {
      union expression *out_expr;
      char *missing_sym_name = vgenerate_metas_no_throw(&out_expr, *s, (void * []) {bindings, expr_buf, handler});
      if(!missing_sym_name) {
        *s = out_expr;
        prepend(out_expr, &extension, expr_buf);
      }
    }
  }
  return extension;
}
