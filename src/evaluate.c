#include "compile.c"
#include "evaluate_errors.c"

myjmp_buf evaluate_handler;
region evaluate_region;

//The following functions form the interface provided to loaded L2 files

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

void *_fst_(list l) {
	return l->fst;
}

list _rst_(list l) {
	return l->rst;
}

list _lst_(void *data, list l) {
	return lst(data, l, evaluate_region);
}

list _nil_() {
	return nil(evaluate_region);
}

void *_get_(void *ref) {
	return *((void **) ref);
}

//These symbols are enough for any L2 code to bootstrap
Symbol sexpr_symbols[] = {
	{.name = "-!-", .address = _exclamation_mark_},
	{.name = "-\"-", .address = _double_quotation_},
	{.name = "-#-", .address = _number_sign_},
	{.name = "-$-", .address = _dollar_sign_},
	{.name = "-%-", .address = _percent_},
	{.name = "-&-", .address = _ampersand_},
	{.name = "-'-", .address = _apostrophe_},
	{.name = "-*-", .address = _asterisk_},
	{.name = "-+-", .address = _plus_sign_},
	{.name = "-,-", .address = _comma_},
	{.name = "---", .address = _hyphen_},
	{.name = "-.-", .address = _period_},
	{.name = "-/-", .address = _slash_},
	{.name = "-0-", .address = _0_},
	{.name = "-1-", .address = _1_},
	{.name = "-2-", .address = _2_},
	{.name = "-3-", .address = _3_},
	{.name = "-4-", .address = _4_},
	{.name = "-5-", .address = _5_},
	{.name = "-6-", .address = _6_},
	{.name = "-7-", .address = _7_},
	{.name = "-8-", .address = _8_},
	{.name = "-9-", .address = _9_},
	{.name = "-:-", .address = _colon_},
	{.name = "-;-", .address = _semicolon_},
	{.name = "-<-", .address = _less_than_sign_},
	{.name = "-=-", .address = _equal_sign_},
	{.name = "->-", .address = _greater_than_sign_},
	{.name = "-?-", .address = _question_mark_},
	{.name = "-@-", .address = _at_sign_},
	{.name = "-A-", .address = _A_},
	{.name = "-B-", .address = _B_},
	{.name = "-C-", .address = _C_},
	{.name = "-D-", .address = _D_},
	{.name = "-E-", .address = _E_},
	{.name = "-F-", .address = _F_},
	{.name = "-G-", .address = _G_},
	{.name = "-H-", .address = _H_},
	{.name = "-I-", .address = _I_},
	{.name = "-J-", .address = _J_},
	{.name = "-K-", .address = _K_},
	{.name = "-L-", .address = _L_},
	{.name = "-M-", .address = _M_},
	{.name = "-N-", .address = _N_},
	{.name = "-O-", .address = _O_},
	{.name = "-P-", .address = _P_},
	{.name = "-Q-", .address = _Q_},
	{.name = "-R-", .address = _R_},
	{.name = "-S-", .address = _S_},
	{.name = "-T-", .address = _T_},
	{.name = "-U-", .address = _U_},
	{.name = "-V-", .address = _V_},
	{.name = "-W-", .address = _W_},
	{.name = "-X-", .address = _X_},
	{.name = "-Y-", .address = _Y_},
	{.name = "-Z-", .address = _Z_},
	{.name = "-\\-", .address = _backslash_},
	{.name = "-^-", .address = _caret_},
	{.name = "-_-", .address = _underscore_},
	{.name = "-`-", .address = _backquote_},
	{.name = "-a-", .address = _a_},
	{.name = "-b-", .address = _b_},
	{.name = "-c-", .address = _c_},
	{.name = "-d-", .address = _d_},
	{.name = "-e-", .address = _e_},
	{.name = "-f-", .address = _f_},
	{.name = "-g-", .address = _g_},
	{.name = "-h-", .address = _h_},
	{.name = "-i-", .address = _i_},
	{.name = "-j-", .address = _j_},
	{.name = "-k-", .address = _k_},
	{.name = "-l-", .address = _l_},
	{.name = "-m-", .address = _m_},
	{.name = "-n-", .address = _n_},
	{.name = "-o-", .address = _o_},
	{.name = "-p-", .address = _p_},
	{.name = "-q-", .address = _q_},
	{.name = "-r-", .address = _r_},
	{.name = "-s-", .address = _s_},
	{.name = "-t-", .address = _t_},
	{.name = "-u-", .address = _u_},
	{.name = "-v-", .address = _v_},
	{.name = "-w-", .address = _w_},
	{.name = "-x-", .address = _x_},
	{.name = "-y-", .address = _y_},
	{.name = "-z-", .address = _z_},
	{.name = "-|-", .address = _vertical_bar_},
	{.name = "-~-", .address = _tilde_},
	{.name = "fst", .address = _fst_},
	{.name = "rst", .address = _rst_},
	{.name = "lst", .address = _lst_},
	{.name = "lst?", .address = is_lst},
	{.name = "nil?", .address = is_nil},
	{.name = "nil", .address = _nil_},
	{.name = "char=", .address = char_equals},
	{.name = "get", .address = _get_},
	{.name = "set", .address = _set_},
	{.name = "mywrite-uint", .address = mywrite_uint}
};

int main(int argc, char *argv[]) {
	//Initialize the error handler
	evaluate_handler.ctx = evaluate_region = create_region(0);
	mysetjmp(&evaluate_handler);
	if(evaluate_handler.ctx != evaluate_region) {
		union evaluate_error *err = (union evaluate_error *) evaluate_handler.ctx;
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
	
	if(argc != 3) {
		throw_arguments(&evaluate_handler);
	}
	
	list env = nil(evaluate_region);
	int i;
	for(i = 0; i < sizeof(sexpr_symbols) / sizeof(Symbol); i++) {
		prepend(&sexpr_symbols[i], &env, evaluate_region);
	}
	//{
		unsigned char obj_buf[mysize(argv[2])];
		int obj_fd = myopen(argv[2]);
		myread(obj_fd, obj_buf, sizeof(obj_buf));
		myclose(obj_fd);
		Object *obj = load(obj_buf, sizeof(obj_buf), evaluate_region);
		int isc = immutable_symbol_count(obj);
		Symbol is[isc];
		immutable_symbols(obj, is);
		for(i = 0; i < isc; i++) {
			prepend(&is[i], &env, evaluate_region);
		}
	//}
	
	unsigned char src_buf[mysize(argv[1])];
	int fd = myopen(argv[1]);
	myread(fd, src_buf, sizeof(src_buf));
	myclose(fd);
	evaluate_source(src_buf, sizeof(src_buf), env, &evaluate_handler);
	destroy_region(evaluate_region);
	return 0;
}
