#define ARGUMENTS 7

struct arguments_error {
	int type;
};

union evaluate_error {
	struct missing_file_error missing_file;
	struct arguments_error arguments;
	struct param_count_mismatch_error param_count_mismatch;
	struct special_form_error special_form;
	struct unexpected_character_error unexpected_character;
	struct multiple_definition_error multiple_definition;
	struct environment_error environment;
};

struct arguments_error *make_arguments() {
	struct arguments_error *err = malloc(sizeof(struct arguments_error));
	err->type = ARGUMENTS;
	return err;
}
