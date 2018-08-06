#include "signal.h"
#include "compile.c"
#include "evaluate_errors.c"

#define INPUT_BUFFER_SIZE 1024

volatile bool input_cancelled = false;

void int_handler() {
	input_cancelled = true;
}

char *prompt_expressions(jmp_buf *handler) {
	prompt:
	printf("\n- ");
	fflush(stdout);
	char str[INPUT_BUFFER_SIZE];
	char *outfn = cprintf("%s", ".XXXXXX.l2");
	FILE *l2file = fdopen(mkstemps(outfn, 3), "w+");
	bool input_started;
	
	while(fgets(str, INPUT_BUFFER_SIZE, stdin)) {
		input_started = true;
		fputs(str, l2file);
		if(input_cancelled) {
			fclose(l2file);
			remove(outfn);
			input_cancelled = false;
			goto prompt;
		} else if(str[strlen(str)-1] == '\n') {
			printf("+ ");
			fflush(stdout);
		} else if(feof(stdin)) {
			fclose(l2file);
			clearerr(stdin);
			printf("\n");
			return outfn;
		}
	}
	if(!input_started) {
		fclose(l2file);
		thelongjmp(*handler, make_no());
	} else {
		fclose(l2file);
		clearerr(stdin);
		printf("\b\b");
		return outfn;
	}
}

int main(int argc, char *argv[]) {
	list library_names = nil();
	volatile int i;
	
	//Initialize the error handler
	jmp_buf handler;
	union evaluate_error *err;
	if(err = (union evaluate_error *) thesetjmp(handler)) {
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
		if(i == argc-1 && !strcmp(argv[i], "-") && err->no.type != NO) {
			goto prompt;
		} else {
			void *name;
			foreach(name, library_names) {
				unload(name, NULL);
			}
			return err->no.type;
		}
	}
	
	if(argc < 2) {
		thelongjmp(handler, make_arguments());
	}
	for(i = 1; i < argc && strcmp(argv[i], "+"); i++);
	if(i == argc) {
		thelongjmp(handler, make_arguments());
	}
	for(i = 1; i < argc && strcmp(argv[i], "+"); i++) {
		load(argv[i], &handler);
	}
	signal(SIGINT, int_handler);
	
	for(i++; i < argc;) prompt: {
		char *source = !strcmp(argv[i], "-") ? prompt_expressions(&handler) : argv[i];
		char *library_name = cprintf("%s", "./.libXXXXXX.so");
		mkstemps(library_name, 3);
		compile(library_name, source, &handler);
		load(library_name, &handler);
		prepend(library_name, &library_names);
		if(i != argc-1 || strcmp(argv[i], "-")) i++;
	}
	thelongjmp(handler, make_no());
}
