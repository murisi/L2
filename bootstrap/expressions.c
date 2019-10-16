enum expression_type {
  storage,
  function,
  with,
  invoke,
  _if,
  literal,
  symbol,
  jump,
  continuation,
  constrain,
  assembly,
  meta
};

enum binding_aug_type { absolute_storage, frame_relative_storage, top_relative_storage, nil_storage };

enum binding_aug_scope { local_scope, global_scope };

enum binding_aug_state { undefined_state, defined_state };

struct binding_aug {
  char *name;
  long int offset;
  unsigned long int size;
  enum binding_aug_type type;
  enum binding_aug_scope scope;
  enum binding_aug_state state;
  union expression *symbol;
  union expression *expression;
  void *context;
};

struct binding_aug *make_binding_aug(enum binding_aug_type type, enum binding_aug_scope scope, enum binding_aug_state state, char *name, union expression *symbol, union expression *expression, buffer r) {
  struct binding_aug *bndg = buffer_alloc(r, sizeof(struct binding_aug));
  bndg->type = type;
  bndg->scope = scope;
  bndg->state = state;
  bndg->name = name;
  bndg->symbol = symbol;
  bndg->expression = expression;
  return bndg;
}

struct base_expression {
  enum expression_type type;
  list fragment;
  union expression *meta;
  list signature;
  unsigned int lowlink;
  list dependencies;
};

struct assembly_expression {
  enum expression_type type;
  list fragment;
  union expression *meta;
  list signature;
  unsigned int lowlink;
  list dependencies;
  
  unsigned long opcode;
  list arguments; // void * = union expression *
};

struct storage_expression {
  enum expression_type type;
  list fragment;
  union expression *meta;
  list signature;
  unsigned int lowlink;
  list dependencies;
  
  union expression *reference;
  list arguments; // void * = union expression *
};

#define CONTAINS_NONE 0UL
#define CONTAINS_JUMP 1UL
#define CONTAINS_WITH 3UL

struct invoke_expression {
  enum expression_type type;
  list fragment;
  union expression *meta;
  list signature;
  unsigned int lowlink;
  list dependencies;
  
  union expression *reference;
  list arguments; // void * = union expression *
  
  unsigned long contains_flag;
  struct binding_aug *temp_storage_bndg;
  
  list parameter_names_fragment;
};

struct jump_expression {
  enum expression_type type;
  list fragment;
  union expression *meta;
  list signature;
  unsigned int lowlink;
  list dependencies;
  
  union expression *reference;
  list arguments;
  
  unsigned long contains_flag;
  struct binding_aug *temp_storage_bndg;
  
  union expression *short_circuit;
};

struct if_expression {
  enum expression_type type;
  list fragment;
  union expression *meta;
  list signature;
  unsigned int lowlink;
  list dependencies;
  
  union expression *condition;
  union expression *consequent;
  union expression *alternate;
};

struct literal_expression {
  enum expression_type type;
  list fragment;
  union expression *meta;
  list signature;
  unsigned int lowlink;
  list dependencies;
  
  long int value;
};

struct function_expression {
  enum expression_type type;
  list fragment;
  union expression *meta;
  list signature;
  unsigned int lowlink;
  list dependencies;
  
  union expression *reference;
  union expression *expression;
  list parameters; //void * = union expression *
  
  list binding_augs;
};

struct continuation_expression {
  enum expression_type type;
  list fragment;
  union expression *meta;
  list signature;
  unsigned int lowlink;
  list dependencies;
  
  union expression *reference;
  union expression *expression;
  list parameters; //void * = union expression *
  
  struct binding_aug *cont_instr_bndg;
  bool escapes;
};

struct with_expression {
  enum expression_type type;
  list fragment;
  union expression *meta;
  list signature;
  unsigned int lowlink;
  list dependencies;
  
  union expression *reference;
  union expression *expression;
  list parameter; //void * = union expression *
  
  struct binding_aug *cont_instr_bndg;
  bool escapes;
};

struct symbol_expression {
  enum expression_type type;
  list fragment;
  union expression *meta;
  list signature;
  unsigned int lowlink;
  list dependencies;
  
  char *name;
  struct binding_aug *binding_aug;
};

struct meta_expression {
  enum expression_type type;
  list fragment;
  union expression *meta;
  list signature;
  unsigned int lowlink;
  list dependencies;
  
  union expression *reference;
};

struct constrain_expression {
  enum expression_type type;
  list fragment;
  union expression *meta;
  list signature;
  unsigned int lowlink;
  list dependencies;
  
  union expression *reference;
  union expression *expression;
};

union expression {
  struct base_expression base;
  struct storage_expression storage;
  struct function_expression function;
  struct continuation_expression continuation;
  struct with_expression with;
  struct jump_expression jump;
  struct invoke_expression invoke;
  struct if_expression _if;
  struct literal_expression literal;
  struct symbol_expression symbol;
  struct meta_expression meta;
  struct assembly_expression assembly;
  struct constrain_expression constrain;
};

union expression *make_literal(unsigned long value, list frag, union expression *_meta, buffer reg) {
  union expression *t = buffer_alloc(reg, sizeof(union expression));
  t->literal.type = literal;
  t->literal.fragment = frag;
  t->literal.signature = var(reg);
  t->literal.lowlink = 0;
  t->literal.dependencies = nil;
  t->literal.meta = _meta;
  t->literal.value = value;
  return t;
}

union expression *make_symbol(char *name, list frag, union expression *_meta, buffer reg) {
  union expression *sym = buffer_alloc(reg, sizeof(union expression));
  sym->symbol.type = symbol;
  sym->symbol.fragment = frag;
  sym->symbol.signature = var(reg);
  sym->symbol.lowlink = 0;
  sym->symbol.dependencies = nil;
  sym->symbol.meta = _meta;
  sym->symbol.name = name;
  sym->symbol.binding_aug = NULL;
  return sym;
}

void bind_symbol(union expression *sym, union expression *target) {
  sym->symbol.binding_aug = target->symbol.binding_aug;
}

union expression *use_binding(struct binding_aug *bndg, buffer reg) {
  union expression *sym = buffer_alloc(reg, sizeof(union expression));
  sym->symbol.type = symbol;
  sym->symbol.meta = NULL;
  sym->symbol.name = bndg->name;
  sym->symbol.binding_aug = bndg;
  return sym;
}

union expression *make_function(union expression *ref, list params, union expression *expr, list frag, union expression *_meta, buffer reg) {
  union expression *func = buffer_alloc(reg, sizeof(union expression));
  func->function.type = function;
  func->function.fragment = frag;
  func->function.signature = var(reg);
  func->function.lowlink = 0;
  func->function.dependencies = nil;
  func->function.meta = _meta;
  func->function.reference = ref;
  ref->symbol.binding_aug = make_binding_aug(absolute_storage, local_scope, defined_state, ref->symbol.name, ref, func, reg);
  func->function.parameters = params;
  union expression *param;
  foreach(param, params) {
    param->symbol.binding_aug = make_binding_aug(frame_relative_storage, local_scope, defined_state, param->symbol.name, param, func, reg);
  }
  func->function.binding_augs = nil;
  func->function.expression = expr;
  return func;
}

union expression *make_continuation(union expression *ref, list params, union expression *expr, list frag, union expression *_meta, buffer reg) {
  union expression *cont = buffer_alloc(reg, sizeof(union expression));
  cont->continuation.type = continuation;
  cont->continuation.fragment = frag;
  cont->continuation.signature = var(reg);
  cont->continuation.lowlink = 0;
  cont->continuation.dependencies = nil;
  cont->continuation.meta = _meta;
  cont->continuation.escapes = false;
  cont->continuation.cont_instr_bndg = make_binding_aug(absolute_storage, local_scope, defined_state, NULL, NULL, NULL, reg);
  cont->continuation.reference = ref;
  ref->symbol.binding_aug = make_binding_aug(frame_relative_storage, local_scope, defined_state, ref->symbol.name, ref, cont, reg);
  cont->continuation.parameters = params;
  union expression *param;
  foreach(param, params) {
    param->symbol.binding_aug = make_binding_aug(frame_relative_storage, local_scope, defined_state, param->symbol.name, param, cont, reg);
  }
  cont->continuation.expression = expr;
  return cont;
}

union expression *make_with(union expression *ref, union expression *expr, list frag, union expression *_meta, buffer reg) {
  union expression *wth = buffer_alloc(reg, sizeof(union expression));
  wth->with.type = with;
  wth->with.fragment = frag;
  wth->with.signature = var(reg);
  wth->with.lowlink = 0;
  wth->with.dependencies = nil;
  wth->with.meta = _meta;
  wth->with.escapes = false;
  wth->with.cont_instr_bndg = make_binding_aug(absolute_storage, local_scope, defined_state, NULL, NULL, NULL, reg);
  wth->with.reference = ref;
  ref->symbol.binding_aug = make_binding_aug(frame_relative_storage, local_scope, defined_state, ref->symbol.name, ref, wth, reg);
  union expression *param = make_symbol(NULL, NULL, NULL, reg);
  param->symbol.binding_aug = make_binding_aug(frame_relative_storage, local_scope, defined_state, param->symbol.name, param, wth, reg);
  wth->with.parameter = lst(param, nil, reg);
  wth->with.expression = expr;
  return wth;
}

union expression *make_asm0(int opcode, buffer reg) {
  union expression *u = buffer_alloc(reg, sizeof(union expression));
  u->assembly.type = assembly;
  u->assembly.opcode = opcode;
  u->assembly.arguments = nil;
  return u;
}

union expression *make_asm1(int opcode, union expression *arg1, buffer reg) {
  union expression *u = make_asm0(opcode, reg);
  u->assembly.arguments = lst(arg1, u->assembly.arguments, reg);
  return u;
}

union expression *make_asm2(int opcode, union expression *arg1, union expression *arg2, buffer reg) {
  union expression *u = make_asm1(opcode, arg2, reg);
  u->assembly.arguments = lst(arg1, u->assembly.arguments, reg);
  return u;
}

union expression *make_asm3(int opcode, union expression *arg1, union expression *arg2, union expression *arg3, buffer reg) {
  union expression *u = make_asm2(opcode, arg2, arg3, reg);
  u->assembly.arguments = lst(arg1, u->assembly.arguments, reg);
  return u;
}

union expression *make_jump(union expression *ref, list args, list frag, union expression *_meta, buffer reg) {
  union expression *u = buffer_alloc(reg, sizeof(union expression));
  u->jump.type = jump;
  u->jump.fragment = frag;
  u->jump.signature = var(reg);
  u->jump.lowlink = 0;
  u->jump.dependencies = nil;
  u->jump.meta = _meta;
  u->jump.reference = ref;
  u->jump.contains_flag = CONTAINS_WITH;
  u->jump.temp_storage_bndg = make_binding_aug(frame_relative_storage, local_scope, defined_state, NULL, NULL, u, reg);
  u->jump.arguments = args;
  return u;
}

union expression *make_jump0(union expression *ref, list frag, union expression *_meta, buffer reg) {
  return make_jump(ref, nil, frag, _meta, reg);
}

union expression *make_jump1(union expression *ref, union expression *arg1, list frag, union expression *_meta, buffer reg) {
  union expression *u = make_jump0(ref, frag, _meta, reg);
  u->jump.arguments = lst(arg1, u->jump.arguments, reg);
  return u;
}

union expression *make_jump2(union expression *ref, union expression *arg1, union expression *arg2, list frag, union expression *_meta, buffer reg) {
  union expression *u = make_jump1(ref, arg2, frag, _meta, reg);
  u->jump.arguments = lst(arg1, u->jump.arguments, reg);
  return u;
}

union expression *make_storage(union expression *ref, list args, list frag, union expression *_meta, buffer reg) {
  union expression *u = buffer_alloc(reg, sizeof(union expression));
  u->storage.type = storage;
  u->storage.fragment = frag;
  u->storage.signature = var(reg);
  u->storage.lowlink = 0;
  u->storage.dependencies = nil;
  u->storage.meta = _meta;
  u->storage.reference = ref;
  ref->symbol.binding_aug = make_binding_aug(frame_relative_storage, local_scope, defined_state, ref->symbol.name, ref, u, reg);
  u->storage.arguments = args;
  return u;
}

union expression *make_meta(union expression *ref, list frag, union expression *_meta, buffer reg) {
  union expression *u = buffer_alloc(reg, sizeof(union expression));
  u->meta.type = meta;
  u->meta.fragment = frag;
  u->meta.signature = var(reg);
  u->meta.meta = _meta;
  u->meta.reference = ref;
  return u;
}

union expression *make_constrain(union expression *expr, union expression *ref, list frag, union expression *_meta, buffer reg) {
  union expression *u = buffer_alloc(reg, sizeof(union expression));
  u->constrain.type = constrain;
  u->constrain.fragment = frag;
  u->constrain.signature = var(reg);
  u->constrain.lowlink = 0;
  u->constrain.dependencies = nil;
  u->constrain.meta = _meta;
  u->constrain.reference = ref;
  u->constrain.expression = expr;
  return u;
}

union expression *make_if(union expression *condition, union expression *consequent, union expression *alternate, list frag, union expression *_meta, buffer reg) {
  union expression *u = buffer_alloc(reg, sizeof(union expression));
  u->_if.type = _if;
  u->_if.fragment = frag;
  u->_if.signature = var(reg);
  u->_if.lowlink = 0;
  u->_if.dependencies = nil;
  u->_if.meta = _meta;
  u->_if.condition = condition;
  u->_if.consequent = consequent;
  u->_if.alternate = alternate;
  return u;
}

union expression *make_invoke(union expression *ref, list args, list frag, union expression *_meta, buffer reg) {
  union expression *u = buffer_alloc(reg, sizeof(union expression));
  u->invoke.type = invoke;
  u->invoke.fragment = frag;
  u->invoke.signature = var(reg);
  u->invoke.parameter_names_fragment = var(reg);
  u->invoke.lowlink = 0;
  u->invoke.dependencies = nil;
  u->invoke.meta = _meta;
  u->invoke.reference = ref;
  u->invoke.contains_flag = CONTAINS_WITH;
  u->invoke.temp_storage_bndg = make_binding_aug(frame_relative_storage, local_scope, defined_state, NULL, NULL, u, reg);
  u->invoke.arguments = args;
  return u;
}

union expression *make_invoke0(union expression *ref, list frag, union expression *_meta, buffer reg) {
  return make_invoke(ref, nil, frag, _meta, reg);
}

union expression *make_invoke1(union expression *ref, union expression *arg1, list frag, union expression *_meta, buffer reg) {
  union expression *u = make_invoke0(ref, frag, _meta, reg);
  u->invoke.arguments = lst(arg1, u->invoke.arguments, reg);
  return u;
}

union expression *make_invoke2(union expression *ref, union expression *arg1, union expression *arg2, list frag, union expression *_meta, buffer reg) {
  union expression *u = make_invoke1(ref, arg2, frag, _meta, reg);
  u->invoke.arguments = lst(arg1, u->invoke.arguments, reg);
  return u;
}

union expression *make_invoke3(union expression *ref, union expression *arg1, union expression *arg2, union expression *arg3, list frag, union expression *_meta, buffer reg) {
  union expression *u = make_invoke2(ref, arg2, arg3, frag, _meta, reg);
  u->invoke.arguments = lst(arg1, u->invoke.arguments, reg);
  return u;
}

union expression *make_invoke4(union expression *ref, union expression *arg1, union expression *arg2, union expression *arg3, union expression *arg4, list frag, union expression *_meta, buffer reg) {
  union expression *u = make_invoke3(ref, arg2, arg3, arg4, frag, _meta, reg);
  u->invoke.arguments = lst(arg1, u->invoke.arguments, reg);
  return u;
}

union expression *make_invoke5(union expression *ref, union expression *arg1, union expression *arg2, union expression *arg3, union expression *arg4, union expression *arg5, list frag, union expression *_meta, buffer reg) {
  union expression *u = make_invoke4(ref, arg2, arg3, arg4, arg5, frag, _meta, reg);
  u->invoke.arguments = lst(arg1, u->invoke.arguments, reg);
  return u;
}

union expression *make_invoke6(union expression *ref, union expression *arg1, union expression *arg2, union expression *arg3, union expression *arg4, union expression *arg5, union expression *arg6, list frag, union expression *_meta, buffer reg) {
  union expression *u = make_invoke5(ref, arg2, arg3, arg4, arg5, arg6, frag, _meta, reg);
  u->invoke.arguments = lst(arg1, u->invoke.arguments, reg);
  return u;
}

union expression *make_invoke7(union expression *ref, union expression *arg1, union expression *arg2, union expression *arg3, union expression *arg4, union expression *arg5, union expression *arg6, union expression *arg7, list frag, union expression *_meta, buffer reg) {
  union expression *u = make_invoke6(ref, arg2, arg3, arg4, arg5, arg6, arg7, frag, _meta, reg);
  u->invoke.arguments = lst(arg1, u->invoke.arguments, reg);
  return u;
}

union expression *make_invoke8(union expression *ref, union expression *arg1, union expression *arg2, union expression *arg3, union expression *arg4, union expression *arg5, union expression *arg6, union expression *arg7, union expression *arg8, list frag, union expression *_meta, buffer reg) {
  union expression *u = make_invoke7(ref, arg2, arg3, arg4, arg5, arg6, arg7, arg8, frag, _meta, reg);
  u->invoke.arguments = lst(arg1, u->invoke.arguments, reg);
  return u;
}

union expression *make_invoke9(union expression *ref, union expression *arg1, union expression *arg2, union expression *arg3, union expression *arg4, union expression *arg5, union expression *arg6, union expression *arg7, union expression *arg8, union expression *arg9, union expression *_meta, list frag, buffer reg) {
  union expression *u = make_invoke8(ref, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, frag, _meta, reg);
  u->invoke.arguments = lst(arg1, u->invoke.arguments, reg);
  return u;
}

void print_expression(union expression *s) {
  switch(s->base.type) {
    case with: {
      write_str(STDOUT, "(with ");
      print_expression(s->with.reference);
      write_str(STDOUT, " ");
      print_expression(s->with.expression);
      write_str(STDOUT, ")");
      break;
    } case invoke: case jump: case storage: {
      write_str(STDOUT, s->base.type == invoke ? "[" : s->base.type == jump ? "{" : "(storage ");
      print_expression(s->invoke.reference);
      write_str(STDOUT, " ");
      union expression *t;
      foreach(t, s->invoke.arguments) {
        print_expression(t);
        write_str(STDOUT, " ");
      }
      write_str(STDOUT, "\b");
      write_str(STDOUT, s->base.type == invoke ? "]" : s->base.type == jump ? "}" : ")");
      break;
    } case function: case continuation: {
      write_str(STDOUT, "(");
      write_str(STDOUT, s->base.type == function ? "function" : "continuation");
      write_str(STDOUT, " ");
      print_expression(s->function.reference);
      write_str(STDOUT, " ( ");
      union expression *t;
      foreach(t, s->function.parameters) {
        print_expression(t);
        write_str(STDOUT, " ");
      }
      write_str(STDOUT, ") ");
      print_expression(s->function.expression);
      write_str(STDOUT, ")");
      break;
    } case _if: {
      write_str(STDOUT, "(if ");
      print_expression(s->_if.condition);
      write_str(STDOUT, " ");
      print_expression(s->_if.consequent);
      write_str(STDOUT, " ");
      print_expression(s->_if.alternate);
      write_str(STDOUT, ")");
      break;
    } case constrain: {
      write_str(STDOUT, "(constrain ");
      //Printing the signature seems to be more hepful than printing expression that
      //created it.
      print_expression(s->constrain.expression);
      write_str(STDOUT, " ");
      print_fragment(s->constrain.signature);
      write_str(STDOUT, ")");
      break;
    } case symbol: {
      if(s->symbol.name) {
        write_str(STDOUT, s->symbol.name);
      } else {
        write_str(STDOUT, "(reference ");
        write_ulong(STDOUT, (unsigned long) s->symbol.binding_aug);
        write_str(STDOUT, ")");
      }
      break;
    } case literal: {
      write_str(STDOUT, "(literal ");
      write_ulong(STDOUT, s->literal.value);
      write_str(STDOUT, ")");
      break;
    } case meta: {
      print_fragment(s->meta.fragment);
      break;
    }
  }
}

#define WORD_BIN_LEN 64

/*
 * Builds a syntax tree at s from the list of s-expressions d. Expansion
 * expressions are not immediately compiled and executed as this is inefficient.
 * Rather, the list of lists, build_syntax_tree_expansion_lists, is used to
 * store all the expansions that need to happen. More specifically, the first
 * list in build_syntax_tree_expansion_lists stores all the expansions that need
 * to happen first, the second list stores all the expansions that need to
 * happen second, and so on.
 */

union expression *build_expression(list d, union expression *_meta, buffer reg, jumpbuf *handler) {
  if(length(d) == 0) {
    throw_special_form(d, NULL, handler);
  } else if(is_token(d)) {
    return make_symbol(to_string(d, reg), d, _meta, reg);
  } else if(!strcmp(to_string(d->fst, reg), "with")) {
    if(length(d) != 3) {
      throw_special_form(d, NULL, handler);
    } else if(!is_token(d->frst)) {
      throw_special_form(d, d->frst, handler);
    }
    return make_with(build_expression(d->frst, _meta, reg, handler), build_expression(d->frrst, _meta, reg, handler), d, _meta, reg);
  } else if(!strcmp(to_string(d->fst, reg), "if")) {
    if(length(d) != 4) {
      throw_special_form(d, NULL, handler);
    }
    return make_if(build_expression(d->frst, _meta, reg, handler), build_expression(d->frrst, _meta, reg, handler),
      build_expression(d->frrrst, _meta, reg, handler), d, _meta, reg);
  } else if(!strcmp(to_string(d->fst, reg), "function") || !strcmp(to_string(d->fst, reg), "continuation")) {
    if(length(d) != 4) {
      throw_special_form(d, NULL, handler);
    } else if(!is_token(d->frst)) {
      throw_special_form(d, d->frst, handler);
    } else if(is_token(d->frrst)) {
      throw_special_form(d, d->frrst, handler);
    }
    
    list parameters = nil;
    list v;
    foreach(v, d->frrst) {
      if(!is_token(v)) {
        throw_special_form(d, v, handler);
      }
      append(build_expression(v, _meta, reg, handler), &parameters, reg);
    }
    union expression *(*fptr)(union expression *, list, union expression *, list, union expression *, buffer) =
      !strcmp(to_string(d->fst, reg), "function") ? make_function : make_continuation;
    return fptr(build_expression(d->frst, _meta, reg, handler), parameters, build_expression(d->frrrst, _meta, reg, handler), d, _meta, reg);
  } else if(!strcmp(to_string(d->fst, reg), "literal")) {
    char *str;
    if(length(d) != 2) {
      throw_special_form(d, NULL, handler);
    } else if(!is_token(d->frst) || strlen(str = to_string(d->frst, reg)) != WORD_BIN_LEN) {
      throw_special_form(d, d->frst, handler);
    }
    
    long int value = 0;
    unsigned long int i;
    for(i = 0; i < strlen(str); i++) {
      value <<= 1;
      if(str[i] == '1') {
        value += 1;
      } else if(str[i] != '0') {
        throw_special_form(d, d->frst, handler);
      }
    }
    return make_literal(value, d, _meta, reg);
  } else if(!strcmp(to_string(d->fst, reg), "invoke") || !strcmp(to_string(d->fst, reg), "jump") ||
      !strcmp(to_string(d->fst, reg), "storage")) {
    if(length(d) == 1) {
      throw_special_form(d, NULL, handler);
    } else if(!strcmp(to_string(d->fst, reg), "storage") && !is_token(d->frst)) {
      throw_special_form(d, d->frst, handler);
    }
  
    list v;
    list arguments = nil;
    foreach(v, d->rrst) {
      append(build_expression(v, _meta, reg, handler), &arguments, reg);
    }
    union expression *(*fptr)(union expression *, list, list, union expression *, buffer) =
      !strcmp(to_string(d->fst, reg), "invoke") ? make_invoke : (!strcmp(to_string(d->fst, reg), "jump") ? make_jump : make_storage);
    return fptr(build_expression(d->frst, _meta, reg, handler), arguments, d, _meta, reg);
  } else if(!strcmp(to_string(d->fst, reg), "constrain")) {
    if(length(d) != 3) {
      throw_special_form(d, NULL, handler);
    }
    return make_constrain(build_expression(d->frst, _meta, reg, handler), build_expression(d->frrst, _meta, reg, handler), d, _meta, reg);
  } else {
    return make_meta(build_expression(d->fst, _meta, reg, handler), d, _meta, reg);
  }
}

#undef WORD_BIN_LEN
