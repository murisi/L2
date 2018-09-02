#include "compile.c"
#include "evaluate_errors.c"

region evaluate_region;

#define char_sexpr(str, ch) \
union sexpr * _ ## str ## _() { \
	return build_character_sexpr(ch, evaluate_region); \
}

#define an_sexpr(ch) char_sexpr(ch, #ch [0])

char_sexpr(exclamation_mark, '!'); char_sexpr(double_quotation, '"'); char_sexpr(dollar_sign, '$'); char_sexpr(percent, '%');
char_sexpr(ampersand, '&'); char_sexpr(apostrophe, '\''); char_sexpr(asterisk, '*'); char_sexpr(plus_sign, '+');
char_sexpr(comma, ','); char_sexpr(hyphen, '-'); char_sexpr(period, '.'); char_sexpr(slash, '/'); an_sexpr(0); an_sexpr(1);
an_sexpr(2); an_sexpr(3); an_sexpr(4); an_sexpr(5); an_sexpr(6); an_sexpr(7); an_sexpr(8); an_sexpr(9); char_sexpr(colon, ':');
char_sexpr(semicolon, ';'); char_sexpr(less_than_sign, '<'); char_sexpr(equal_sign, '='); char_sexpr(greater_than_sign, '>');
char_sexpr(question_mark, '?'); an_sexpr(A); an_sexpr(B); an_sexpr(C); an_sexpr(D); an_sexpr(E); an_sexpr(F); an_sexpr(G);
an_sexpr(H); an_sexpr(I); an_sexpr(J); an_sexpr(K); an_sexpr(L); an_sexpr(M); an_sexpr(N); an_sexpr(O); an_sexpr(P); an_sexpr(Q);
an_sexpr(R); an_sexpr(S); an_sexpr(T); an_sexpr(U); an_sexpr(V); an_sexpr(W); an_sexpr(X); an_sexpr(Y); an_sexpr(Z);
char_sexpr(backslash, '\\'); char_sexpr(caret, '^'); char_sexpr(underscore, '_'); char_sexpr(backquote, '`'); an_sexpr(a);
an_sexpr(b); an_sexpr(c); an_sexpr(d); an_sexpr(e); an_sexpr(f); an_sexpr(g); an_sexpr(h); an_sexpr(i); an_sexpr(j); an_sexpr(k);
an_sexpr(l); an_sexpr(m); an_sexpr(n); an_sexpr(o); an_sexpr(p); an_sexpr(q); an_sexpr(r); an_sexpr(s); an_sexpr(t); an_sexpr(u);
an_sexpr(v); an_sexpr(w); an_sexpr(x); an_sexpr(y); an_sexpr(z); char_sexpr(vertical_bar, '|'); char_sexpr(tilde, '~');

list with_primitive(list arg) { return lst(build_symbol_sexpr("with", evaluate_region), arg, evaluate_region); }
list begin_primitive(list arg) { return lst(build_symbol_sexpr("begin", evaluate_region), arg, evaluate_region); }
list if_primitive(list arg) { return lst(build_symbol_sexpr("if", evaluate_region), arg, evaluate_region); }
list function_primitive(list arg) { return lst(build_symbol_sexpr("function", evaluate_region), arg, evaluate_region); }
list continuation_primitive(list arg) { return lst(build_symbol_sexpr("continuation", evaluate_region), arg, evaluate_region); }
list literal_primitive(list arg) { return lst(build_symbol_sexpr("literal", evaluate_region), arg, evaluate_region); }
list invoke_primitive(list arg) { return lst(build_symbol_sexpr("invoke", evaluate_region), arg, evaluate_region); }
list jump_primitive(list arg) { return lst(build_symbol_sexpr("jump", evaluate_region), arg, evaluate_region); }

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
	myjmp_buf handler;
	handler.ctx = evaluate_region = create_region(0);;
	mysetjmp(&handler);
	if(handler.ctx != evaluate_region) {
		union evaluate_error *err = (union evaluate_error *) handler.ctx;
		mywrite_str(STDOUT, "Error found: ");
		switch(err->arguments.type) {
			case PARAM_COUNT_MISMATCH: {
				mywrite_str(STDOUT, "The number of arguments in ");
				print_syntax_tree(err->param_count_mismatch.src_expression);
				mywrite_str(STDOUT, " does not match the number of parameters in ");
				print_syntax_tree(err->param_count_mismatch.dest_expression);
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
		throw_arguments(&handler);
	}
	
	Symbol env = make_symbol(NULL, NULL);
	
	unsigned char buf[mysize(argv[1])];
	int fd = myopen(argv[1]);
	myread(fd, buf, sizeof(buf));
	myclose(fd);
	
	unsigned char *raw_obj;
	int obj_size;
	compile(&raw_obj, &obj_size, buf, sizeof(buf), &env, evaluate_region, &handler);
	Object *obj = load(raw_obj, obj_size, evaluate_region);
	mutate_symbol(obj, make_symbol("putchar", putchar));
	//mutate_symbol(obj, make_symbol("compile-l2", compile));
	//mutate_symbol(obj, make_symbol("load-library", eval_load_library));
	//mutate_symbol(obj, make_symbol("mutable-symbols", eval_mutable_symbols));
	//mutate_symbol(obj, make_symbol("immutable-symbols", eval_immutable_symbols));
	//mutate_symbol(obj, make_symbol("mutate-symbol", mutate_symbol));
	//mutate_symbol(obj, make_symbol("run-library", run_library));
	//mutate_symbol(obj, make_symbol("unload-library", unload_library));
	
	start(obj)();
	destroy_region(evaluate_region);
	return 0;
}
