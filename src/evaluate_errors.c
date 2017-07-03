#define NO 0
#define MISSING_FILE 6
#define ARGUMENTS 7

struct no_error {
	int type;
};

struct missing_file_error {
	int type;
};

struct arguments_error {
	int type;
};

union evaluate_error {
	struct no_error no;
	struct missing_file_error missing_file;
	struct arguments_error arguments;
	struct param_count_mismatch_error param_count_mismatch;
	struct special_form_error special_form;
	struct unexpected_character_error unexpected_character;
	struct multiple_definition_error multiple_definition;
	struct environment_error environment;
};

struct no_error *make_no() {
	struct no_error *err = malloc(sizeof(struct no_error));
	err->type = NO;
	return err;
}

struct missing_file_error *make_missing_file() {
	struct missing_file_error *err = malloc(sizeof(struct missing_file_error));
	err->type = MISSING_FILE;
	return err;
}

struct arguments_error *make_arguments() {
	struct arguments_error *err = malloc(sizeof(struct arguments_error));
	err->type = ARGUMENTS;
	return err;
}