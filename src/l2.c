//To ensure pointers can be passed through longjmp
char longjmp_hack[sizeof(int) - sizeof(void *)];

#include "setjmp.h"
#include "stdio.h"
#include "ctype.h"
#include "stdlib.h"
#include "string.h"
#include "stdarg.h"
#include "limits.h"
#include "stdlib.h"

#include "dlfcn.h"
#include "signal.h"

#define true (~((int) 0))
#define false ((int) 0)
typedef int bool;

char *cprintf(const char *format, ...) {
	va_list ap, aq;
	va_start(ap, format);
	va_copy(aq, ap);
	
	char *str = calloc(vsnprintf(NULL, 0, format, ap) + 1, sizeof(char));
	vsprintf(str, format, aq);
	
	va_end(aq);
	va_end(ap);
	return str;
}

#include "list.c"
#include "errors.c"
#include "lexer.c"
#include "expressions.c"
#include "preparer.c"
#include "generator.c"
#include "output.c"
#include "parser.c"

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

volatile bool input_finished = false;

void int_handler() {
	input_finished = true;
}

#define INPUT_BUFFER_SIZE 1024

FILE *open_prompt(jmp_buf *handler) {
	printf("- ");
	char str[INPUT_BUFFER_SIZE];
	FILE *l2file = tmpfile();
	if(!l2file) return NULL;
	while(fgets(str, INPUT_BUFFER_SIZE, stdin)) {
		if(input_finished) {
			longjmp(*handler, (int) make_no());
		} else if(strlen(str) + 1 == INPUT_BUFFER_SIZE) {
			fclose(l2file);
			exit(0);
		}
		fputs(str, l2file);
		printf("+ ");
	}
	rewind(l2file);
	return l2file;
}

int main(int argc, char *argv[]) {
	make_shared_library_object_files = nil();
	make_program_object_files = nil();
	generate_string_blacklist = nil();
	init_i386_registers();
	list shared_library_names = nil(), shared_library_handles = nil();
	volatile int processing_from, processing_to, i;
	
	//Initialize the error handler
	jmp_buf handler;
	union evaluate_error *err;
	if(err = (union evaluate_error *) setjmp(handler)) {
		if(err->no.type != NO) {
			printf("Error found between %s and %s inclusive: ", argv[processing_from], argv[processing_to - 1]);
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
				printf("File is missing.\n");
				return MISSING_FILE;
			} case ARGUMENTS: {
				printf("Bad command line arguments.\n");
				printf("Usage: l2compile objects.o ... (- inputs.l2 ...) ... - inputs.l2 ...\n\n"
					"Uses objects.o ... as libraries for remaining stages of the compilation and, if the final output is\n"
					"not an object file, embeds them into the final output. Concatenates the first group inputs.l2 ...,\n"
					"compiles the concatenation, and uses the executable as an environment for the remaining stages of\n"
					"compilation. Does the same process repeatedly until the last group is reached. Finally, concatenates\n"
					"last group, compiles concatenation into either a position independent or dependent object, shared\n"
					"library, or program called output as specified by the flags.\n");
				return ARGUMENTS;
			}
		}
		if(processing_from == argc && err->no.type != NO) {
			i = argc - 1;
			goto prompt;
		} else {
			void *handle;
			{foreach(handle, shared_library_handles) {
				dlclose(handle);
			}}
			char *filename;
			foreach(filename, shared_library_names) {
				remove(filename);
			}
			return err->no.type;
		}
	}
	
	processing_from = 1;
	processing_to = argc;
	
	if(argc < 2) {
		longjmp(handler, (int) make_arguments());
	}
	
	for(i = 1; i < argc && strcmp(argv[i], "-"); i++) {
		prepend(argv[i], &make_shared_library_object_files);
		prepend(argv[i], &make_program_object_files);
	}
	if(i == argc) {
		longjmp(handler, (int) make_arguments());
	}
	signal(SIGINT, int_handler);
	prompt: while(i < argc) {
		int j;
		list expressions = nil();
		list expansion_lists = nil();
		
		for(j = ++i; (j == argc && i == argc) || (i < argc && strcmp(argv[i], "-")); i++) {
			processing_from = i;
			processing_to = i + 1;
			FILE *l2file = (j == argc) ? open_prompt(&handler) : fopen(argv[i], "r");
			if(l2file == NULL) {
				longjmp(handler, (int) make_missing_file());
			}
			
			int c;
			while((c = after_leading_space(l2file)) != EOF) {
				ungetc(c, l2file);
				build_expr_list_handler = &handler;
				list sexpr = build_expr_list(l2file);
				build_syntax_tree_handler = &handler;
				build_syntax_tree_expansion_lists = nil();
				build_syntax_tree(sexpr, append(NULL, &expressions));
				merge_onto(build_syntax_tree_expansion_lists, &expansion_lists);
			}
			fclose(l2file);
		}
		processing_from = j;
		processing_to = i;
		if(j == argc) i -= 2;
		expand_expressions_handler = &handler;
		expand_expressions(expansion_lists);
		
		char *sofn = dynamic_load(expressions, &handler);
		void *handle = dlopen(sofn, RTLD_NOW | RTLD_GLOBAL);
		if(!handle) {
			longjmp(handler, (int) make_environment(cprintf("%s", dlerror())));
		} else if(processing_from == argc) {
			printf("\n");
		}
		prepend(sofn, &shared_library_names);
		prepend(handle, &shared_library_handles);
	}
	printf("\n");
	longjmp(handler, (int) make_no());
}