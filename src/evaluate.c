#define WORD_SIZE 8
#define NULL 0UL
typedef unsigned long int bool;
#define true 1
#define false 0

#include "x86_64_linux_interface.c"
#include "list.c"
#include "compile_errors.c"
#include "lexer.c"
#include "x86_64_object.c"
#include "expressions.c"
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
	store_dynamic_refs(&program->function.expression, true, nil, manreg);
	visit_expressions(vgenerate_np_expressions, &program, (void* []) {manreg, ectx});
	visit_expressions(vfind_multiple_definitions, &program, ectx->handler);
	visit_expressions(vlink_references, &program, (void* []) {ectx->handler, manreg});
	visit_expressions(vescape_analysis, &program, NULL);
	program = use_return_symbol(program, make_symbol(_function, local_scope, defined_state, NULL, manreg), manreg);
	visit_expressions(vshare_symbols, &program, manreg);
	classify_program_symbols(program->function.expression);
	visit_expressions(vlayout_frames, &program, manreg);
	visit_expressions(vgenerate_references, &program, manreg);
	visit_expressions(vgenerate_continuation_expressions, &program, manreg);
	visit_expressions(vgenerate_literals, &program, manreg);
	visit_expressions(vgenerate_ifs, &program, manreg);
	visit_expressions(vgenerate_function_expressions, &program, manreg);
	visit_expressions(vgenerate_storage_expressions, &program, manreg);
	list local_symbols = program->function.symbols;
	list global_symbols = nil;
	union expression *l;
	{foreach(l, program->function.parameters) {
		append(l->reference.symbol, &global_symbols, manreg);
	}}
	program = generate_toplevel(program, manreg);
	list asms = nil;
	visit_expressions(vmerge_begins, &program, (void* []) {&asms, manreg});
	unsigned char *objdest; int objdest_sz;
	write_elf(reverse(asms, manreg), local_symbols, global_symbols, &objdest, &objdest_sz, manreg);
	Object *obj = load(objdest, objdest_sz, ectx->rt_reg, ectx->handler);
	struct symbol *sym;
	{foreach(sym, concat(local_symbols, global_symbols, manreg)) {
		if(sym->type == static_storage && sym->state == defined_state) {
			sym->offset += (unsigned long) segment(obj, ".bss");
		}
	}}
	{foreach(l, asms) {
		if(l->assembly.opcode == LABEL) {
			union expression *label_ref = l->assembly.arguments->fst;
			label_ref->reference.symbol->offset += (unsigned long) segment(obj, ".text");
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
	list objects = nil;
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
			
			list expressions = nil;
			int pos = 0;
			while(after_leading_space(src_buf, src_sz, &pos)) {
				list sexpr = build_expr_list(src_buf, src_sz, &pos, syntax_tree_region, handler);
				append(build_expression(sexpr, syntax_tree_region, handler), &expressions, syntax_tree_region);
			}
			obj = load_expressions(make_program(expressions, syntax_tree_region), ectx, nil, syntax_tree_region);
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

void *_fst_(list l) {
	return l->fst;
}

list _rst_(list l) {
	return l->rst;
}

void *_buf_() {
	return create_region(0);
}

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
				print_expression(err->param_count_mismatch.src_expression);
				write_str(STDOUT, " does not match the number of parameters in ");
				print_expression(err->param_count_mismatch.dest_expression);
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
	
	object_symbol static_bindings_arr[] = {
		{.name = "-!-", .address = &_exclamation_mark_},
		{.name = "-\"-", .address = &_double_quotation_},
		{.name = "-#-", .address = &_number_sign_},
		{.name = "-$-", .address = &_dollar_sign_},
		{.name = "-%-", .address = &_percent_},
		{.name = "-&-", .address = &_ampersand_},
		{.name = "-'-", .address = &_apostrophe_},
		{.name = "-*-", .address = &_asterisk_},
		{.name = "-+-", .address = &_plus_sign_},
		{.name = "-,-", .address = &_comma_},
		{.name = "---", .address = &_hyphen_},
		{.name = "-.-", .address = &_period_},
		{.name = "-/-", .address = &_slash_},
		{.name = "-0-", .address = &_0_},
		{.name = "-1-", .address = &_1_},
		{.name = "-2-", .address = &_2_},
		{.name = "-3-", .address = &_3_},
		{.name = "-4-", .address = &_4_},
		{.name = "-5-", .address = &_5_},
		{.name = "-6-", .address = &_6_},
		{.name = "-7-", .address = &_7_},
		{.name = "-8-", .address = &_8_},
		{.name = "-9-", .address = &_9_},
		{.name = "-:-", .address = &_colon_},
		{.name = "-;-", .address = &_semicolon_},
		{.name = "-<-", .address = &_less_than_sign_},
		{.name = "-=-", .address = &_equal_sign_},
		{.name = "->-", .address = &_greater_than_sign_},
		{.name = "-?-", .address = &_question_mark_},
		{.name = "-@-", .address = &_at_sign_},
		{.name = "-A-", .address = &_A_},
		{.name = "-B-", .address = &_B_},
		{.name = "-C-", .address = &_C_},
		{.name = "-D-", .address = &_D_},
		{.name = "-E-", .address = &_E_},
		{.name = "-F-", .address = &_F_},
		{.name = "-G-", .address = &_G_},
		{.name = "-H-", .address = &_H_},
		{.name = "-I-", .address = &_I_},
		{.name = "-J-", .address = &_J_},
		{.name = "-K-", .address = &_K_},
		{.name = "-L-", .address = &_L_},
		{.name = "-M-", .address = &_M_},
		{.name = "-N-", .address = &_N_},
		{.name = "-O-", .address = &_O_},
		{.name = "-P-", .address = &_P_},
		{.name = "-Q-", .address = &_Q_},
		{.name = "-R-", .address = &_R_},
		{.name = "-S-", .address = &_S_},
		{.name = "-T-", .address = &_T_},
		{.name = "-U-", .address = &_U_},
		{.name = "-V-", .address = &_V_},
		{.name = "-W-", .address = &_W_},
		{.name = "-X-", .address = &_X_},
		{.name = "-Y-", .address = &_Y_},
		{.name = "-Z-", .address = &_Z_},
		{.name = "-\\-", .address = &_backslash_},
		{.name = "-^-", .address = &_caret_},
		{.name = "-_-", .address = &_underscore_},
		{.name = "-`-", .address = &_backquote_},
		{.name = "-a-", .address = &_a_},
		{.name = "-b-", .address = &_b_},
		{.name = "-c-", .address = &_c_},
		{.name = "-d-", .address = &_d_},
		{.name = "-e-", .address = &_e_},
		{.name = "-f-", .address = &_f_},
		{.name = "-g-", .address = &_g_},
		{.name = "-h-", .address = &_h_},
		{.name = "-i-", .address = &_i_},
		{.name = "-j-", .address = &_j_},
		{.name = "-k-", .address = &_k_},
		{.name = "-l-", .address = &_l_},
		{.name = "-m-", .address = &_m_},
		{.name = "-n-", .address = &_n_},
		{.name = "-o-", .address = &_o_},
		{.name = "-p-", .address = &_p_},
		{.name = "-q-", .address = &_q_},
		{.name = "-r-", .address = &_r_},
		{.name = "-s-", .address = &_s_},
		{.name = "-t-", .address = &_t_},
		{.name = "-u-", .address = &_u_},
		{.name = "-v-", .address = &_v_},
		{.name = "-w-", .address = &_w_},
		{.name = "-x-", .address = &_x_},
		{.name = "-y-", .address = &_y_},
		{.name = "-z-", .address = &_z_},
		{.name = "-|-", .address = &_vertical_bar_},
		{.name = "-~-", .address = &_tilde_},
		{.name = "fst", .address = _fst_},
		{.name = "rst", .address = _rst_},
		{.name = "lst", .address = lst},
		{.name = "lst?", .address = is_lst},
		{.name = "emt?", .address = is_nil},
		{.name = "emt", .address = nil},
		{.name = "char=", .address = char_equals},
		{.name = "buf", .address = _buf_},
		{.name = "dtr", .address = destroy_region}
	};
	
	list static_bindings_list = nil;
	int i;
	for(i = 0; i < sizeof(static_bindings_arr) / sizeof(object_symbol); i++) {
		prepend(&static_bindings_arr[i], &static_bindings_list, evaluate_region);
	}
	evaluate_files(argc - 1, argv + 1, static_bindings_list, &evaluate_handler);
	destroy_region(evaluate_region);
	return 0;
}
