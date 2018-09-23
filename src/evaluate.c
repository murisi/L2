#define WORD_SIZE 8
#define NULL 0UL

#include "x86_64_linux_interface.c"
typedef unsigned long int bool;
#define true (~0UL)
#define false (0UL)
#include "list.c"
#include "compile_errors.c"
#include "lexer.c"
#include "x86_64_object.c"
#include "expressions.c"
#include "parser.c"
#include "preparer.c"
#include "x86_64_generator.c"
#include "x86_64_assembler.c"
#include "evaluate_errors.c"

/*
 * Makes a new binary file at the path outbin from the list of primitive
 * expressions, exprs. The resulting binary file executes the list from top to
 * bottom and then makes all the top-level functions visible to the rest of the
 * executable that it is embedded in.
 */

Object *load_expressions(union expression *program, struct expansion_context *ectx, list st_binds, region manreg) {
	store_static_bindings(&program->function.expression, true, st_binds, ectx->rt_reg);
	store_dynamic_refs(&program->function.expression, true, nil(ectx->rt_reg), manreg);
	union expression **t;
	{foreachaddress(t, program->function.expression->begin.expressions) {
		if((*t)->base.type == function) {
			generate_np_expressions(t, manreg, ectx);
		} else {
			union expression *container = make_function(make_reference(NULL, manreg), nil(manreg), *t, manreg);
			generate_np_expressions(&container->function.expression, manreg, ectx);
			*t = container->function.expression;
			(*t)->base.parent = program->function.expression;
		}
	}}
	
	visit_expressions(vfind_multiple_definitions, &program, ectx->handler);
	visit_expressions(vlink_references, &program, (void* []) {ectx->handler, manreg});
	visit_expressions(vescape_analysis, &program, NULL);
	program = use_return_value(program, make_reference(NULL, manreg), manreg);
	visit_expressions(vlayout_frames, &program, manreg);
	visit_expressions(vgenerate_references, &program, manreg);
	visit_expressions(vgenerate_continuation_expressions, &program, manreg);
	visit_expressions(vgenerate_literals, &program, manreg);
	visit_expressions(vgenerate_ifs, &program, manreg);
	visit_expressions(vgenerate_function_expressions, &program, manreg);
	list locals = program->function.locals;
	list globals = program->function.parameters;
	program = generate_toplevel(program, manreg);
	list asms = nil(manreg);
	visit_expressions(vmerge_begins, &program, (void* []) {&asms, manreg});
	unsigned char *objdest; int objdest_sz;
	write_elf(reverse(asms, manreg), locals, globals, &objdest, &objdest_sz, manreg);
	Object *obj = load(objdest, objdest_sz, ectx->rt_reg, ectx->handler);
	union expression *l;
	{foreach(l, locals) {
		if(l->reference.symbol) {
			l->reference.symbol->address += (unsigned long) segment(obj, ".bss");
		}
	}}
	{foreach(l, asms) {
		if(l->assembly.opcode == LOCAL_LABEL || l->assembly.opcode == GLOBAL_LABEL) {
			union expression *label_ref = l->assembly.arguments->fst;
			if(label_ref->reference.symbol) {
				label_ref->reference.symbol->address += (unsigned long) segment(obj, ".text");
			}
		}
	}}
	return obj;
}

/*
 * Makes a new binary file at the path outbin from the L2 file at the path inl2.
 * The resulting binary file executes the L2 source file from top to bottom and
 * then makes all the top-level functions visible to the rest of the executable
 * that it is embedded in.
 */

void evaluate_files(int srcc, char *srcv[], list bindings, jumpbuf *handler) {
	region syntax_tree_region = create_region(0);
	list objects = nil(syntax_tree_region);
	struct expansion_context *ectx = region_alloc(syntax_tree_region, sizeof(struct expansion_context));
	ectx->rt_reg = syntax_tree_region;
	ectx->handler = handler;
	ectx->ext_binds = bindings;
	
	int i;
	for(i = 0; i < srcc; i++) {
		Object *obj;
		char *dot = strrchr(srcv[i], '.');
		
		if(dot && !strcmp(dot, ".l2")) {
			int fd = open(srcv[i]);
			long int src_sz = size(fd);
			unsigned char *src_buf = region_alloc(syntax_tree_region, src_sz);
			read(fd, src_buf, src_sz);
			close(fd);
			
			list expressions = nil(syntax_tree_region);
			int pos = 0;
			while(after_leading_space(src_buf, src_sz, &pos)) {
				list sexpr = build_expr_list(src_buf, src_sz, &pos, syntax_tree_region, handler);
				append(build_syntax_tree(sexpr, syntax_tree_region, handler), &expressions, syntax_tree_region);
			}
			obj = load_expressions(make_program(expressions, syntax_tree_region), ectx, nil(syntax_tree_region), syntax_tree_region);
		} else if(dot && !strcmp(dot, ".o")) {
			int obj_fd = open(srcv[i]);
			long int obj_sz = size(obj_fd);
			unsigned char *obj_buf = region_alloc(syntax_tree_region, obj_sz);
			read(obj_fd, obj_buf, obj_sz);
			close(obj_fd);
			
			obj = load(obj_buf, obj_sz, syntax_tree_region, handler);
		}
		append(obj, &objects, syntax_tree_region);
		append_list(&ectx->ext_binds, immutable_symbols(obj, syntax_tree_region));
	}
	
	Object *obj;
	{foreach(obj, objects) {
		mutate_symbols(obj, ectx->ext_binds);
	}}
	{foreach(obj, objects) {
		((void (*)()) segment(obj, ".text"))();
	}}
	destroy_region(syntax_tree_region);
}

//The following functions form the interface provided to loaded L2 files

#define char_sexpr(str, ch) \
union sexpr * _ ## str ## _(region r) { \
	return build_character_sexpr(ch, r); \
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
	{.name = "lst", .address = lst},
	{.name = "lst?", .address = is_lst},
	{.name = "emt?", .address = is_nil},
	{.name = "emt", .address = nil},
	{.name = "char=", .address = char_equals}
};

int main(int argc, char *argv[]) {
	//Initialize the error handler
	jumpbuf evaluate_handler;
	region evaluate_region = evaluate_handler.ctx = create_region(0);
	setjump(&evaluate_handler);
	
	if(evaluate_handler.ctx != evaluate_region) {
		union evaluate_error *err = (union evaluate_error *) evaluate_handler.ctx;
		write_str(STDOUT, "Error found: ");
		switch(err->arguments.type) {
			case PARAM_COUNT_MISMATCH: {
				write_str(STDOUT, "The number of arguments in ");
				print_syntax_tree(err->param_count_mismatch.src_expression);
				write_str(STDOUT, " does not match the number of parameters in ");
				print_syntax_tree(err->param_count_mismatch.dest_expression);
				write_str(STDOUT, ".\n");
				break;
			} case SPECIAL_FORM: {
				if(err->special_form.subexpression_list) {
					write_str(STDOUT, "The subexpression ");
					print_expr_list(err->special_form.subexpression_list);
					write_str(STDOUT, " does not satisfy the requirements of the expression ");
					print_expr_list(err->special_form.expression_list);
					write_str(STDOUT, ".\n");
				} else {
					write_str(STDOUT, "The expression ");
					print_expr_list(err->special_form.expression_list);
					write_str(STDOUT, " has an incorrect number of subexpressions.\n");
				}
				break;
			} case UNEXPECTED_CHARACTER: {
				write_str(STDOUT, "Unexpectedly read ");
				write_char(STDOUT, err->unexpected_character.character);
				write_str(STDOUT, " at ");
				write_ulong(STDOUT, err->unexpected_character.position);
				write_str(STDOUT, ".\n");
				break;
			} case MULTIPLE_DEFINITION: {
				write_str(STDOUT, "The definition of ");
				write_str(STDOUT, err->multiple_definition.reference_value);
				write_str(STDOUT, " erroneously occured multiple times.\n");
				break;
			} case OBJECT: {
				write_str(STDOUT, "Bad object file supplied.\n");
				break;
			} case MISSING_FILE: {
				write_str(STDOUT, "There is no file at the path ");
				write_str(STDOUT, err->missing_file.path);
				write_str(STDOUT, ".\n");
				break;
			} case ARGUMENTS: {
				write_str(STDOUT, "Bad command line arguments.\nUsage: l2evaluate (src1.l2 | obj1.o) ...\n"
					"Outcome: Compiles each L2 file into an object file, links all the object files\n"
					"together, and then executes each object file in the order they were specified.\n");
				break;
			} case UNDEFINED_REFERENCE: {
				write_str(STDOUT, "Undefined reference: ");
				write_str(STDOUT, err->undefined_reference.reference_value);
				write_str(STDOUT, ".\n");
				break;
			}
		}
		return err->arguments.type;
	}
	
	list static_bindings = nil(evaluate_region);
	int i;
	for(i = 0; i < sizeof(sexpr_symbols) / sizeof(Symbol); i++) {
		prepend(&sexpr_symbols[i], &static_bindings, evaluate_region);
	}
	evaluate_files(argc - 1, argv + 1, static_bindings, &evaluate_handler);
	destroy_region(evaluate_region);
	return 0;
}
