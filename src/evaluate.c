#include "signal.h"
#include "compile.c"
#include "evaluate_errors.c"

/*Library *eval_load_library(list name, void *low_mem, void *upper_mem) {
	return load_library(fopen(to_string(name), "r"), low_mem, upper_mem);
}

list eval_mutable_symbols(Library *lib) {
	int msc = mutable_symbol_count(lib);
	Symbol ms_arr[msc];
	mutable_symbols(lib, ms_arr);
	int i;
	list ms_lst = nil();
	for(i = msc-1; i >= 0; i--) {
		prepend(lst(build_symbol_sexpr(ms_arr[i].name), lst(ms_arr[i].address, nil())), &ms_lst);
	}
	return ms_lst;
}*/

int main(int argc, char *argv[]) {
	//Initialize the error handler
	jmp_buf handler;
	union evaluate_error *err;
	if(err = (union evaluate_error *) thesetjmp(handler)) {
		mywrite_str(STDOUT, "Error found: ");
		print_annotated_syntax_tree_annotator = &empty_annotator;
		switch(err->arguments.type) {
			case PARAM_COUNT_MISMATCH: {
				mywrite_str(STDOUT, "The number of arguments in ");
				print_annotated_syntax_tree(err->param_count_mismatch.src_expression);
				mywrite_str(STDOUT, " does not match the number of parameters in ");
				print_annotated_syntax_tree(err->param_count_mismatch.dest_expression);
				mywrite_str(STDOUT, ".\n");
				break;
			} case SPECIAL_FORM: {
				if(err->special_form.subexpression_list) {
					mywrite_str(STDOUT, "The subexpression ");
					print_expr_list(err->special_form.subexpression_list);
					mywrite_str(STDOUT, " does not satisfy the requirements of the expression ");
					print_expr_list(err->special_form.expression_list);
					mywrite_str(STDOUT, ".\n");
				} else {
					mywrite_str(STDOUT, "The expression ");
					print_expr_list(err->special_form.expression_list);
					mywrite_str(STDOUT, " has an incorrect number of subexpressions.\n");
				}
				break;
			} case UNEXPECTED_CHARACTER: {
				mywrite_str(STDOUT, "Unexpectedly read ");
				mywrite_char(STDOUT, err->unexpected_character.character);
				mywrite_str(STDOUT, " at ");
				mywrite_uint(STDOUT, err->unexpected_character.position);
				mywrite_str(STDOUT, ".\n");
				break;
			} case MULTIPLE_DEFINITION: {
				mywrite_str(STDOUT, "The definition of ");
				mywrite_str(STDOUT, err->multiple_definition.reference_value);
				mywrite_str(STDOUT, " erroneously occured multiple times.\n");
				break;
			} case ENVIRONMENT: {
				mywrite_str(STDOUT, "The following occured when trying to use an environment: ");
				mywrite_str(STDOUT, err->environment.error_string);
				mywrite_str(STDOUT, "\n");
				break;
			} case MISSING_FILE: {
				mywrite_str(STDOUT, "There is no file at the path ");
				mywrite_str(STDOUT, err->missing_file.path);
				mywrite_str(STDOUT, ".\n");
				return MISSING_FILE;
			} case ARGUMENTS: {
				mywrite_str(STDOUT, "Bad command line arguments.\nUsage: l2compile inputs.l2\n"
					"Outcome: Compiles inputs.l2 and runs it.\n");
				return ARGUMENTS;
			}
		}
		return err->arguments.type;
	}
	
	if(argc != 2) {
		thelongjmp(handler, make_arguments());
	}
	
	Symbol env = make_symbol(NULL, NULL);
	
	unsigned char buf[mysize(argv[1])];
	int fd = myopen(argv[1]);
	myread(fd, buf, sizeof(buf));
	myclose(fd);
	
	unsigned char *raw_obj;
	int obj_size;
	compile(&raw_obj, &obj_size, buf, sizeof(buf), &env, &handler);
	Object *obj = load(raw_obj, obj_size);
	mutate_symbol(obj, make_symbol("putchar", putchar));
	//mutate_symbol(obj, make_symbol("compile-l2", compile));
	//mutate_symbol(obj, make_symbol("load-library", eval_load_library));
	//mutate_symbol(obj, make_symbol("mutable-symbols", eval_mutable_symbols));
	//mutate_symbol(obj, make_symbol("immutable-symbols", eval_immutable_symbols));
	//mutate_symbol(obj, make_symbol("mutate-symbol", mutate_symbol));
	//mutate_symbol(obj, make_symbol("run-library", run_library));
	//mutate_symbol(obj, make_symbol("unload-library", unload_library));
	
	start(obj)();
	unload(obj);
	return 0;
}
