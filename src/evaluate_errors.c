



union evaluate_error {
	struct missing_file_error missing_file;
	
	struct param_count_mismatch_error param_count_mismatch;
	struct special_form_error special_form;
	struct unexpected_character_error unexpected_character;
	struct multiple_definition_error multiple_definition;
	struct object_error object;
	struct undefined_reference_error undefined_reference;
};


