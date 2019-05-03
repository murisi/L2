enum error_type { unification, arguments, param_count_mismatch, special_form,
  unexpected_character, multiple_definition, object, missing_file, undefined_symbol };

struct param_count_mismatch_error {
  int type;
  union expression *src_expression;
  union expression *dest_expression;
};

struct special_form_error {
  int type;
  list expression_list;
  list subexpression_list;
};

struct unexpected_character_error {
  int type;
  int character;
  long int position;
};

struct multiple_definition_error {
  int type;
  char *reference_value;
};

struct object_error {
  int type;
};

struct missing_file_error {
  int type;
  char *path;
};

struct undefined_symbol_error {
  int type;
  char *symbol_value;
};

struct arguments_error {
  int type;
};

struct unification_error {
  int type;
  list lhs;
  list rhs;
  union expression *expr;
};

union evaluate_error {
  struct arguments_error arguments;
  struct param_count_mismatch_error param_count_mismatch;
  struct special_form_error special_form;
  struct unexpected_character_error unexpected_character;
  struct multiple_definition_error multiple_definition;
  struct object_error object;
  struct missing_file_error missing_file;
  struct undefined_symbol_error undefined_symbol;
  struct unification_error unification;
};

void throw_arguments(jumpbuf *jb) {
  struct arguments_error *err = buffer_alloc(jb->ctx, sizeof(struct arguments_error));
  err->type = arguments;
  jb->ctx = err;
  longjump(jb);
}

void throw_param_count_mismatch(union expression *src_expression, union expression *dest_expression, jumpbuf *jb) {
  struct param_count_mismatch_error *err = buffer_alloc(jb->ctx, sizeof(struct param_count_mismatch_error));
  err->type = param_count_mismatch;
  err->src_expression = src_expression;
  err->dest_expression = dest_expression;
  jb->ctx = err;
  longjump(jb);
}

void throw_special_form(list expression_list, list subexpression_list, jumpbuf *jb) {
  struct special_form_error *err = buffer_alloc(jb->ctx, sizeof(struct special_form_error));
  err->type = special_form;
  err->expression_list = expression_list;
  err->subexpression_list = subexpression_list;
  jb->ctx = err;
  longjump(jb);
}

void throw_unexpected_character(int character, long int position, jumpbuf *jb) {
  struct unexpected_character_error *err = buffer_alloc(jb->ctx, sizeof(struct unexpected_character_error));
  err->type = unexpected_character;
  err->character = character;
  err->position = position;
  jb->ctx = err;
  longjump(jb);
}
void throw_multiple_definition(char *reference_value, jumpbuf *jb) {
  struct multiple_definition_error *err = buffer_alloc(jb->ctx, sizeof(struct multiple_definition_error));
  err->type = multiple_definition;
  err->reference_value = reference_value;
  jb->ctx = err;
  longjump(jb);
}

void throw_object(jumpbuf *jb) {
  struct object_error *err = buffer_alloc(jb->ctx, sizeof(struct object_error));
  err->type = object;
  jb->ctx = err;
  longjump(jb);
}

void throw_missing_file(char *path, jumpbuf *jb) {
  struct missing_file_error *err = buffer_alloc(jb->ctx, sizeof(struct missing_file_error));
  err->type = missing_file;
  err->path = path;
  jb->ctx = err;
  longjump(jb);
}

void throw_undefined_symbol(char *symbol_value, jumpbuf *jb) {
  struct undefined_symbol_error *err = buffer_alloc(jb->ctx, sizeof(struct undefined_symbol_error));
  err->type = undefined_symbol;
  err->symbol_value = symbol_value;
  jb->ctx = err;
  longjump(jb);
}

void throw_unification(list lhs, list rhs, union expression *expr, jumpbuf *jb) {
  struct unification_error *err = buffer_alloc(jb->ctx, sizeof(struct unification_error));
  err->type = unification;
  err->lhs = lhs;
  err->rhs = rhs;
  err->expr = expr;
  jb->ctx = err;
  longjump(jb);
}

