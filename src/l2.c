//To ensure pointers can be passed through longjmp
char longjmp_hack[sizeof(int) - sizeof(void *)];

#include "setjmp.h"
#include "stdio.h"
#include "ctype.h"
#include "stdlib.h"
#include "assert.h"
#include "string.h"
#include "stdarg.h"
#include "limits.h"
#include "stdlib.h"

#include "dlfcn.h"

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

void help_then_exit() {
	printf("Usage 1: l2compile (-pic | -pdc) -object output objects.o ... (- inputs.l2 ...) ... - inputs.l2 ...\n"
		"Usage 2: l2compile (-pic | -pdc) -library output objects.o ... (- inputs.l2 ...) ... - inputs.l2 ...\n"
		"Usage 3: l2compile (-pic | -pdc) -program output objects.o ... (- inputs.l2 ...) ... - inputs.l2 ...\n\n"
		"Uses objects.o ... as libraries for remaining stages of the compilation and, if the final output is\n"
		"not an object file, embeds them into the final output. Concatenates the first group inputs.l2 ...,\n"
		"compiles the concatenation, and uses the executable as an environment for the remaining stages of\n"
		"compilation. Does the same process repeatedly until the last group is reached. Finally, concatenates\n"
		"last group, compiles concatenation into either a position independent or dependent object, shared\n"
		"library, or program called output as specified by the flags.\n");
	exit(6);
}

int main(int argc, char *argv[]) {
	if(argc < 4 || (strcmp(argv[1], "-pic") && strcmp(argv[1], "-pdc")) || (strcmp(argv[2], "-program") &&
		strcmp(argv[2], "-library") && strcmp(argv[2], "-object"))) {
			help_then_exit();
	}
	
	make_shared_library_object_files = nil();
	make_program_object_files = nil();
	int i;
	for(i = 4; i < argc && strcmp(argv[i], "-"); i++) {
		prepend(argv[i], &make_shared_library_object_files);
		prepend(argv[i], &make_program_object_files);
	}
	if(i == argc) {
		help_then_exit();
	}
	generate_string_blacklist = nil();
	init_i386_registers();
	list shared_library_names = nil(), shared_library_handles = nil();
	int processing_from, processing_to;
	
	//Initialize the error handler
	jmp_buf handler;
	union error *err;
	if(err = (union error *) setjmp(handler)) {
		void *handle;
		{foreach(handle, shared_library_handles) {
			dlclose(handle);
		}}
		char *filename;
		foreach(filename, shared_library_names) {
			remove(filename);
		}
		
		print_annotated_syntax_tree_annotator = &empty_annotator;
		if(err->no.type == no) {
			return 0;
		}
		printf("Error found between %s and %s inclusive: ", argv[processing_from], argv[processing_to - 1]);
		switch(err->no.type) {
			case param_count_mismatch: {
				printf("The number of arguments in ");
				print_annotated_syntax_tree(err->param_count_mismatch.src_expression);
				printf(" does not match the number of parameters in ");
				print_annotated_syntax_tree(err->param_count_mismatch.dest_expression);
				printf(".\n");
				return 1;
			} case special_form: {
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
				return 2;
			} case unexpected_character: {
				printf("Unexpectedly read %c at %ld.\n", err->unexpected_character.character, err->unexpected_character.position);
				return 3;
			} case multiple_definition: {
				printf("The definition of %s erroneously occured multiple times.\n", err->multiple_definition.reference_value);
				return 4;
			} case environment: {
				printf("The following occured when trying to use an environment: %s\n", err->environment.error_string);
				return 5;
			} case missing_file: {
				printf("File is missing.\n");
				return 6;
			}
		}
	}
	list expressions;
	
	for(;;) {
		int j;
		for(expressions = nil(), j = ++i; i < argc && strcmp(argv[i], "-"); i++) {
			processing_from = i;
			processing_to = i + 1;
			FILE *l2file = fopen(argv[i], "r");
			if(l2file == NULL) {
				longjmp(handler, (int) make_missing_file());
			}
			
			int c;
			while((c = after_leading_space(l2file)) != EOF) {
				ungetc(c, l2file);
				build_expr_list_handler = &handler;
				list sexpr = build_expr_list(l2file);
				build_syntax_tree_handler = &handler;
				union expression *expr = build_syntax_tree(sexpr, NULL);
				append(expr, &expressions);
			}
			fclose(l2file);
		}
		processing_from = j;
		processing_to = i;
		if(i == argc) break;
		
		char *sofn = dynamic_load(expressions, &handler);
		void *handle = dlopen(sofn, RTLD_NOW | RTLD_GLOBAL);
		if(!handle) {
			longjmp(handler, (int) make_environment(cprintf("%s", dlerror())));
		}
		prepend(sofn, &shared_library_names);
		prepend(handle, &shared_library_handles);
	}
	bool PIC = !strcmp(argv[1], "-pic");
	if(!strcmp(argv[2], "-program")) {
		char *ofilefn = compile(expressions, PIC, &handler);
		prepend(ofilefn, &make_program_object_files);
		make_program(PIC, argv[3]);
		make_program_object_files = rst(make_program_object_files);
		remove(ofilefn);
	} else if(!strcmp(argv[2], "-library")) {
		char *ofilefn = compile(expressions, PIC, &handler);
		prepend(ofilefn, &make_shared_library_object_files);
		make_shared_library(PIC, argv[3]);
		make_shared_library_object_files = rst(make_shared_library_object_files);
		remove(ofilefn);
	} else if(!strcmp(argv[2], "-object")) {
		rename(compile(expressions, PIC, &handler), argv[3]);
	}
	longjmp(handler, (int) make_no());
}