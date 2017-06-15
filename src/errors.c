enum error_type {
	no,
	missing_file,
	param_count_mismatch,
	special_form,
	unexpected_character,
	multiple_definition,
	environment
};

struct no_error {
	enum error_type type;
};

struct missing_file_error {
	enum error_type type;
};

struct param_count_mismatch_error {
	enum error_type type;
	union expression *src_expression;
	union expression *dest_expression;
};

struct special_form_error {
	enum error_type type;
	list expression_list;
	list subexpression_list;
};

struct unexpected_character_error {
	enum error_type type;
	int character;
	long int position;
};

struct multiple_definition_error {
	enum error_type type;
	char *reference_value;
};

struct environment_error {
	enum error_type type;
	char *error_string;
};

union error {
	struct no_error no;
	struct missing_file_error missing_file;
	struct param_count_mismatch_error param_count_mismatch;
	struct special_form_error special_form;
	struct unexpected_character_error unexpected_character;
	struct multiple_definition_error multiple_definition;
	struct environment_error environment;
};

union error *make_no() {
	union error *err = malloc(sizeof(union error));
	err->no.type = no;
	return err;
}

union error *make_missing_file() {
	union error *err = malloc(sizeof(union error));
	err->missing_file.type = missing_file;
	return err;
}

union error *make_param_count_mismatch(union expression *src_expression, union expression *dest_expression) {
	union error *err = malloc(sizeof(union error));
	err->param_count_mismatch.type = param_count_mismatch;
	err->param_count_mismatch.src_expression = src_expression;
	err->param_count_mismatch.dest_expression = dest_expression;
	return err;
}

union error *make_special_form(list expression_list, list subexpression_list) {
	union error *err = malloc(sizeof(union error));
	err->special_form.type = special_form;
	err->special_form.expression_list = expression_list;
	err->special_form.subexpression_list = subexpression_list;
	return err;
}

union error *make_unexpected_character(int character, long int position) {
	union error *err = malloc(sizeof(union error));
	err->unexpected_character.type = unexpected_character;
	err->unexpected_character.character = character;
	err->unexpected_character.position = position;
	return err;
}

union error *make_multiple_definition(char *reference_value) {
	union error *err = malloc(sizeof(union error));
	err->multiple_definition.type = multiple_definition;
	err->multiple_definition.reference_value = reference_value;
	return err;
}

union error *make_environment(char *error_string) {
	union error *err = malloc(sizeof(union error));
	err->environment.type = environment;
	err->environment.error_string = error_string;
	return err;
}