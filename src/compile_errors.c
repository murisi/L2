#define PARAM_COUNT_MISMATCH 1
#define SPECIAL_FORM 2
#define UNEXPECTED_CHARACTER 3
#define MULTIPLE_DEFINITION 4
#define ENVIRONMENT 5
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

struct environment_error {
	int type;
	char *error_string;
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
	struct environment_error environment;
	struct missing_file_error missing_file;
};

struct param_count_mismatch_error *make_param_count_mismatch(union expression *src_expression, union expression *dest_expression) {
	struct param_count_mismatch_error *err = malloc(sizeof(struct param_count_mismatch_error));
	err->type = PARAM_COUNT_MISMATCH;
	err->src_expression = src_expression;
	err->dest_expression = dest_expression;
	return err;
}

struct special_form_error *make_special_form(list expression_list, list subexpression_list) {
	struct special_form_error *err = malloc(sizeof(struct special_form_error));
	err->type = SPECIAL_FORM;
	err->expression_list = expression_list;
	err->subexpression_list = subexpression_list;
	return err;
}

struct unexpected_character_error *make_unexpected_character(int character, long int position) {
	struct unexpected_character_error *err = malloc(sizeof(struct unexpected_character_error));
	err->type = UNEXPECTED_CHARACTER;
	err->character = character;
	err->position = position;
	return err;
}

struct multiple_definition_error *make_multiple_definition(char *reference_value) {
	struct multiple_definition_error *err = malloc(sizeof(struct multiple_definition_error));
	err->type = MULTIPLE_DEFINITION;
	err->reference_value = reference_value;
	return err;
}

struct environment_error *make_environment(char *error_string) {
	struct environment_error *err = malloc(sizeof(struct environment_error));
	err->type = ENVIRONMENT;
	err->error_string = error_string;
	return err;
}

struct missing_file_error *make_missing_file(char *path) {
	struct missing_file_error *err = malloc(sizeof(struct missing_file_error));
	err->type = MISSING_FILE;
	err->path = path;
	return err;
}
