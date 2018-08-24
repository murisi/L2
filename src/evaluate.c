#include "signal.h"
#include "compile.c"
#include "evaluate_errors.c"

int main(int argc, char *argv[]) {
	volatile int i;
	
	//Initialize the error handler
	jmp_buf handler;
	union evaluate_error *err;
	if(err = (union evaluate_error *) thesetjmp(handler)) {
		printf("Error found in %s: ", argv[i]);
		print_annotated_syntax_tree_annotator = &empty_annotator;
		switch(err->arguments.type) {
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
				printf("Bad command line arguments.\n"
					"Usage: l2compile inputs.l2\n"
					"Outcome: Compiles inputs.l2 and runs it.\n");
				return ARGUMENTS;
			}
		}
		return err->arguments.type;
	}
	
	if(argc != 2) {
		thelongjmp(handler, make_arguments());
	}
	
	char *library_name = cprintf("%s", "./.libXXXXXX.a");
	mkstemps(library_name, 2);
	remove(library_name);
	Symbol env = make_symbol(NULL, NULL);
	compile(library_name, argv[1], &env, &handler);
	
	Library *lib = load_library(library_name, (void *) 0x0UL, (void *) ~0x0UL);
	//mutate_symbol(lib, make_symbol("putchar", putchar));
	mutate_symbol(lib, make_symbol("compile-l2", compile));
	mutate_symbol(lib, make_symbol("load-library", load_library));
	mutate_symbol(lib, make_symbol("mutable-symbols", mutable_symbols));
	mutate_symbol(lib, make_symbol("mutable-symbol-count", mutable_symbol_count));
	mutate_symbol(lib, make_symbol("immutable-symbols", immutable_symbols));
	mutate_symbol(lib, make_symbol("immutable-symbol-count", immutable_symbol_count));
	mutate_symbol(lib, make_symbol("mutate-symbol", mutate_symbol));
	mutate_symbol(lib, make_symbol("run-library", run_library));
	mutate_symbol(lib, make_symbol("unload-library", unload_library));
	
	Library *arch_lib = load_library("bin/arch.a",
		sat_sub(lib->segs, 0x0000000000FFFFFFUL),
		sat_add(lib->segs, 0x0000000000FFFFFFUL));
	int alisc = immutable_symbol_count(arch_lib);
	Symbol alis[alisc];
	immutable_symbols(arch_lib, alis);
	int j;
	for(j = 0; j < alisc; j++) {
		mutate_symbol(lib, alis[j]);
	}
	
	run_library(lib);
	unload_library(arch_lib);
	unload_library(lib);
	remove(library_name);
	return 0;
}
