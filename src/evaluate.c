#include "signal.h"
#include "compile.c"
#include "evaluate_errors.c"

volatile bool input_cancelled = false;

void int_handler() {
	input_cancelled = true;
}

#define INPUT_BUFFER_SIZE 1024

char *prompt_expressions(jmp_buf *handler) {
	prompt: printf("- ");
	char str[INPUT_BUFFER_SIZE];
	char *outfn = cprintf("%s", ".XXXXXX.l2");
	FILE *l2file = fdopen(mkstemps(outfn, 3), "w+");
	bool input_started = false;
	
	while(fgets(str, INPUT_BUFFER_SIZE, stdin)) {
		if(input_cancelled) {
			fclose(l2file);
			if(input_started) {
				input_cancelled = false;
				goto prompt;
			} else {
				longjmp(*handler, (int) make_no());
			}
		} else if(strlen(str) + 1 == INPUT_BUFFER_SIZE) {
			fclose(l2file);
			longjmp(*handler, (int) make_unexpected_character(str[INPUT_BUFFER_SIZE - 1], INPUT_BUFFER_SIZE));
		}
		input_started = true;
		fputs(str, l2file);
		printf("+ ");
	}
	fflush(l2file);
	return outfn;
}

int main(int argc, char *argv[]) {
	list shared_library_handles = nil();
	volatile int i;
	
	//Initialize the error handler
	jmp_buf handler;
	union evaluate_error *err;
	if(err = (union evaluate_error *) setjmp(handler)) {
		if(err->no.type != NO) {
			printf("Error found in %s: ", argv[i]);
		}
		print_annotated_syntax_tree_annotator = &empty_annotator;
		switch(err->no.type) {
			case PARAM_COUNT_MISMATCH: {
				printf("The number of arguments in ");
				print_annotated_syntax_tree(err->param_count_mismatch.src_expression);
				printf(" does not match the number of parameters in ");
				print_annotated_syntax_tree(err->param_count_mismatch.dest_expression);
				printf(".\n");
				break;
			} case SPECIAL_FORM: {
				if(err->special_form.subexpression_list) {
					printf("The subexpression ");
					print_expr_list(err->special_form.subexpression_list);
					printf(" does not satisfy the requirements of the expression ");
					print_expr_list(err->special_form.expression_list);
					printf(".\n");
				} else {
					printf("The expression ");
					print_expr_list(err->special_form.expression_list);
					printf(" has an incorrect number of subexpressions.\n");
				}
				break;
			} case UNEXPECTED_CHARACTER: {
				printf("Unexpectedly read %c at %ld.\n", err->unexpected_character.character, err->unexpected_character.position);
				break;
			} case MULTIPLE_DEFINITION: {
				printf("The definition of %s erroneously occured multiple times.\n", err->multiple_definition.reference_value);
				break;
			} case ENVIRONMENT: {
				printf("The following occured when trying to use an environment: %s\n", err->environment.error_string);
				break;
			} case MISSING_FILE: {
				printf("There is no file at the path %s.\n", err->missing_file.path);
				return MISSING_FILE;
			} case ARGUMENTS: {
				printf("Bad command line arguments.\n");
				printf("Usage: l2compile libraries.a ... (- inputs.l2 ...) ... - inputs.l2 ...\n\n"
					"Uses libraries.a ... as libraries for remaining stages of the compilation and, if the final output is\n"
					"not an object file, embeds them into the final output. Concatenates the first group inputs.l2 ...,\n"
					"compiles the concatenation, and uses the executable as an environment for the remaining stages of\n"
					"compilation. Does the same process repeatedly until the last group is reached. Finally, concatenates\n"
					"last group, compiles concatenation into either a position independent or dependent object, shared\n"
					"library, or program called output as specified by the flags.\n");
				return ARGUMENTS;
			}
		}
		if(i == argc-1 && strcmp(argv[i], "+") && err->no.type != NO) {
			goto prompt;
		} else {
			void *handle;
			foreach(handle, shared_library_handles) {
				_dlclose(handle, NULL);
			}
			return err->no.type;
		}
	}
	
	if(argc < 2) {
		longjmp(handler, (int) make_arguments());
	}
	for(i = 1; i < argc && strcmp(argv[i], "-"); i++);
	if(i == argc) {
		longjmp(handler, (int) make_arguments());
	}
	
	for(i = 1; i < argc && strcmp(argv[i], "-"); i++) {
		load(argv[i], &handler);
	}
	
	signal(SIGINT, int_handler);
	i++;
	prompt: for(; i < argc; i++) {
		char *source = (i == argc-1 && !strcmp(argv[i], "+")) ? prompt_expressions(&handler) : argv[i];
		char *library_name = cprintf("%s", "./.libXXXXXX.so");
		mkstemps(library_name, 3);
		compile(library_name, source, &handler);
		load(library_name, &handler);
		prepend(_dlopen(library_name, &handler), &shared_library_handles);
		if(i == argc-1 && !strcmp(argv[i], "+")) {
			printf("\n");
			i--;
		}
	}
	longjmp(handler, (int) make_no());
}
