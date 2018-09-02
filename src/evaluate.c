#include "compile.c"
#include "evaluate_errors.c"

region evaluate_region;
list bss;

#define char_sexpr(str, ch) \
union sexpr * _ ## str ## _() { \
	return build_character_sexpr(ch, evaluate_region); \
}

#define an_sexpr(ch) char_sexpr(ch, #ch [0])

char_sexpr(exclamation_mark, '!'); char_sexpr(double_quotation, '"'); char_sexpr(number_sign, '#'); char_sexpr(dollar_sign, '$');
char_sexpr(percent, '%'); char_sexpr(ampersand, '&'); char_sexpr(apostrophe, '\''); char_sexpr(asterisk, '*');
char_sexpr(plus_sign, '+'); char_sexpr(comma, ','); char_sexpr(hyphen, '-'); char_sexpr(period, '.'); char_sexpr(slash, '/');
an_sexpr(0); an_sexpr(1); an_sexpr(2); an_sexpr(3); an_sexpr(4); an_sexpr(5); an_sexpr(6); an_sexpr(7); an_sexpr(8); an_sexpr(9);
char_sexpr(colon, ':'); char_sexpr(semicolon, ';'); char_sexpr(less_than_sign, '<'); char_sexpr(equal_sign, '=');
char_sexpr(greater_than_sign, '>'); char_sexpr(question_mark, '?'); char_sexpr(at_sign, '@'); an_sexpr(A); an_sexpr(B); an_sexpr(C);
an_sexpr(D); an_sexpr(E); an_sexpr(F); an_sexpr(G); an_sexpr(H); an_sexpr(I); an_sexpr(J); an_sexpr(K); an_sexpr(L); an_sexpr(M);
an_sexpr(N); an_sexpr(O); an_sexpr(P); an_sexpr(Q); an_sexpr(R); an_sexpr(S); an_sexpr(T); an_sexpr(U); an_sexpr(V); an_sexpr(W);
an_sexpr(X); an_sexpr(Y); an_sexpr(Z); char_sexpr(backslash, '\\'); char_sexpr(caret, '^'); char_sexpr(underscore, '_');
char_sexpr(backquote, '`'); an_sexpr(a); an_sexpr(b); an_sexpr(c); an_sexpr(d); an_sexpr(e); an_sexpr(f); an_sexpr(g); an_sexpr(h);
an_sexpr(i); an_sexpr(j); an_sexpr(k); an_sexpr(l); an_sexpr(m); an_sexpr(n); an_sexpr(o); an_sexpr(p); an_sexpr(q); an_sexpr(r);
an_sexpr(s); an_sexpr(t); an_sexpr(u); an_sexpr(v); an_sexpr(w); an_sexpr(x); an_sexpr(y); an_sexpr(z); char_sexpr(vertical_bar, '|');
char_sexpr(tilde, '~');

/*list with_primitive(list arg) { return lst(build_symbol_sexpr("with", evaluate_region), arg, evaluate_region); }
list begin_primitive(list arg) { return lst(build_symbol_sexpr("begin", evaluate_region), arg, evaluate_region); }
list if_primitive(list arg) { return lst(build_symbol_sexpr("if", evaluate_region), arg, evaluate_region); }
list function_primitive(list arg) { return lst(build_symbol_sexpr("function", evaluate_region), arg, evaluate_region); }
list continuation_primitive(list arg) { return lst(build_symbol_sexpr("continuation", evaluate_region), arg, evaluate_region); }
list literal_primitive(list arg) { return lst(build_symbol_sexpr("literal", evaluate_region), arg, evaluate_region); }
list invoke_primitive(list arg) { return lst(build_symbol_sexpr("invoke", evaluate_region), arg, evaluate_region); }
list jump_primitive(list arg) { return lst(build_symbol_sexpr("jump", evaluate_region), arg, evaluate_region); }*/

list _bootstrap_symbols_() {
	return bss;
}

Object *_load_(list name) {
	char *name_str = to_string(name, evaluate_region);
	int obj_sz = mysize(name_str);
	unsigned char *raw_obj = region_malloc(evaluate_region, obj_sz);
	int fd = myopen(name_str);
	myread(fd, raw_obj, obj_sz);
	myclose(fd);
	return load(raw_obj, obj_sz, evaluate_region);
}

list _mutable_symbols_(Object *obj) {
	int msc = mutable_symbol_count(obj);
	Symbol ms_arr[msc];
	mutable_symbols(obj, ms_arr);
	int i;
	list ms_lst = nil(evaluate_region);
	for(i = msc-1; i >= 0; i--) {
		prepend(lst(build_symbol_sexpr(ms_arr[i].name, evaluate_region), lst(ms_arr[i].address, nil(evaluate_region), evaluate_region),
			evaluate_region), &ms_lst, evaluate_region);
	}
	return ms_lst;
}

list _immutable_symbols_(Object *obj) {
	int isc = immutable_symbol_count(obj);
	Symbol is_arr[isc];
	immutable_symbols(obj, is_arr);
	int i;
	list is_lst = nil(evaluate_region);
	for(i = isc-1; i >= 0; i--) {
		prepend(lst(build_symbol_sexpr(is_arr[i].name, evaluate_region), lst(is_arr[i].address, nil(evaluate_region), evaluate_region),
			evaluate_region), &is_lst, evaluate_region);
	}
	return is_lst;
}

Object *_mutate_symbols_(Object *obj, list updates) {
	int sc = length(updates);
	Symbol symbols[sc];
	list symbol;
	foreach(symbol, updates) {
		mutate_symbols(obj, (Symbol []) {make_symbol(to_string(symbol->fst, evaluate_region), symbol->frst)}, 1);
	}
	return obj;
}

int main(int argc, char *argv[]) {
	//Initialize the error handler
	myjmp_buf handler;
	handler.ctx = evaluate_region = create_region(0);
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
				break;
			} case ARGUMENTS: {
				mywrite_str(STDOUT, "Bad command line arguments.\nUsage: l2compile inputs.l2\n"
					"Outcome: Compiles inputs.l2 and runs it.\n");
				break;
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
	unsigned char *raw_obj; int obj_size;
	compile(&raw_obj, &obj_size, buf, sizeof(buf), &env, evaluate_region, &handler);
	Object *obj = load(raw_obj, obj_size, evaluate_region);
	//Give the object that we are about to load just enough symbols for it to bootstrap
	bss = nil(evaluate_region);
	prepend((Symbol []) {make_symbol("-!-", _exclamation_mark_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-\"-", _double_quotation_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-#-", _number_sign_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-$-", _dollar_sign_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-%-", _percent_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-&-", _ampersand_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-'-", _apostrophe_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-*-", _asterisk_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-+-", _plus_sign_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-,-", _comma_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("---", _hyphen_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-.-", _period_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-/-", _slash_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-0-", _0_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-1-", _1_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-2-", _2_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-3-", _3_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-4-", _4_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-5-", _5_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-6-", _6_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-7-", _7_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-8-", _8_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-9-", _9_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-:-", _colon_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-;-", _semicolon_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-<-", _less_than_sign_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-=-", _equal_sign_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("->-", _greater_than_sign_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-?-", _question_mark_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-@-", _at_sign_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-A-", _A_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-B-", _B_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-C-", _C_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-D-", _D_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-E-", _E_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-F-", _F_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-G-", _G_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-H-", _H_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-I-", _I_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-J-", _J_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-K-", _K_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-L-", _L_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-M-", _M_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-N-", _N_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-O-", _O_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-P-", _P_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-Q-", _Q_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-R-", _R_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-S-", _S_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-T-", _T_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-U-", _U_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-V-", _V_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-W-", _W_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-X-", _X_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-Y-", _Y_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-Z-", _Z_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-\\-", _backslash_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-^-", _caret_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-_-", _underscore_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-`-", _backquote_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-a-", _a_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-b-", _b_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-c-", _c_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-d-", _d_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-e-", _e_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-f-", _f_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-g-", _g_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-h-", _h_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-i-", _i_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-j-", _j_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-k-", _k_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-l-", _l_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-m-", _m_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-n-", _n_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-o-", _o_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-p-", _p_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-q-", _q_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-r-", _r_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-s-", _s_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-t-", _t_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-u-", _u_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-v-", _v_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-w-", _w_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-x-", _x_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-y-", _y_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-z-", _z_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-|-", _vertical_bar_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("-~-", _tilde_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("bootstrap-symbols", _bootstrap_symbols_)}, &bss, evaluate_region); //To ease further bootstrapping
	//mutate_symbol(obj, make_symbol("compile-l2", compile));
	prepend((Symbol []) {make_symbol("load", _load_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("mutable-symbols", _mutable_symbols_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("immutable-symbols", _immutable_symbols_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("mutate-symbols", _mutate_symbols_)}, &bss, evaluate_region);
	prepend((Symbol []) {make_symbol("start", start)}, &bss, evaluate_region);
	
	start(obj)();
	destroy_region(evaluate_region);
	return 0;
}
