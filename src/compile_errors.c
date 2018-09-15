#define PARAM_COUNT_MISMATCH 1
#define SPECIAL_FORM 2
#define UNEXPECTED_CHARACTER 3
#define MULTIPLE_DEFINITION 4
#define OBJECT 5
#define MISSING_FILE 6

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

union compile_error {
	struct param_count_mismatch_error param_count_mismatch;
	struct special_form_error special_form;
	struct unexpected_character_error unexpected_character;
	struct multiple_definition_error multiple_definition;
	struct object_error object;
	struct missing_file_error missing_file;
};

void throw_param_count_mismatch(union expression *src_expression, union expression *dest_expression, jumpbuf *jb) {
	struct param_count_mismatch_error *err = region_alloc(jb->ctx, sizeof(struct param_count_mismatch_error));
	err->type = PARAM_COUNT_MISMATCH;
	err->src_expression = src_expression;
	err->dest_expression = dest_expression;
	jb->ctx = err;
	longjump(jb);
}

void throw_special_form(list expression_list, list subexpression_list, jumpbuf *jb) {
	struct special_form_error *err = region_alloc(jb->ctx, sizeof(struct special_form_error));
	err->type = SPECIAL_FORM;
	err->expression_list = expression_list;
	err->subexpression_list = subexpression_list;
	jb->ctx = err;
	longjump(jb);
}

void throw_unexpected_character(int character, long int position, jumpbuf *jb) {
	struct unexpected_character_error *err = region_alloc(jb->ctx, sizeof(struct unexpected_character_error));
	err->type = UNEXPECTED_CHARACTER;
	err->character = character;
	err->position = position;
	jb->ctx = err;
	longjump(jb);
}
void throw_multiple_definition(char *reference_value, jumpbuf *jb) {
	struct multiple_definition_error *err = region_alloc(jb->ctx, sizeof(struct multiple_definition_error));
	err->type = MULTIPLE_DEFINITION;
	err->reference_value = reference_value;
	jb->ctx = err;
	longjump(jb);
}

void throw_object(jumpbuf *jb) {
	struct object_error *err = region_alloc(jb->ctx, sizeof(struct object_error));
	err->type = OBJECT;
	jb->ctx = err;
	longjump(jb);
}

void throw_missing_file(char *path, jumpbuf *jb) {
	struct missing_file_error *err = region_alloc(jb->ctx, sizeof(struct missing_file_error));
	err->type = MISSING_FILE;
	err->path = path;
	jb->ctx = err;
	longjump(jb);
}
