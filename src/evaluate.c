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
	classify_program_symbols(program->function.expression);
	program = use_return_symbol(program, make_symbol(_function, local_scope, defined_state, NULL, manreg), manreg);
	visit_expressions(vshare_symbols, &program, manreg);
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
		{.name = "-!-", .address = character_sexprs['!']},
		{.name = "-\"-", .address = character_sexprs['"']},
		{.name = "-#-", .address = character_sexprs['#']},
		{.name = "-$-", .address = character_sexprs['$']},
		{.name = "-%-", .address = character_sexprs['%']},
		{.name = "-&-", .address = character_sexprs['&']},
		{.name = "-'-", .address = character_sexprs['\'']},
		{.name = "-*-", .address = character_sexprs['*']},
		{.name = "-+-", .address = character_sexprs['+']},
		{.name = "-,-", .address = character_sexprs[',']},
		{.name = "---", .address = character_sexprs['-']},
		{.name = "-.-", .address = character_sexprs['.']},
		{.name = "-/-", .address = character_sexprs['/']},
		{.name = "-0-", .address = character_sexprs['0']},
		{.name = "-1-", .address = character_sexprs['1']},
		{.name = "-2-", .address = character_sexprs['2']},
		{.name = "-3-", .address = character_sexprs['3']},
		{.name = "-4-", .address = character_sexprs['4']},
		{.name = "-5-", .address = character_sexprs['5']},
		{.name = "-6-", .address = character_sexprs['6']},
		{.name = "-7-", .address = character_sexprs['7']},
		{.name = "-8-", .address = character_sexprs['8']},
		{.name = "-9-", .address = character_sexprs['9']},
		{.name = "-:-", .address = character_sexprs[':']},
		{.name = "-;-", .address = character_sexprs[';']},
		{.name = "-<-", .address = character_sexprs['<']},
		{.name = "-=-", .address = character_sexprs['=']},
		{.name = "->-", .address = character_sexprs['>']},
		{.name = "-?-", .address = character_sexprs['?']},
		{.name = "-@-", .address = character_sexprs['@']},
		{.name = "-A-", .address = character_sexprs['A']},
		{.name = "-B-", .address = character_sexprs['B']},
		{.name = "-C-", .address = character_sexprs['C']},
		{.name = "-D-", .address = character_sexprs['D']},
		{.name = "-E-", .address = character_sexprs['E']},
		{.name = "-F-", .address = character_sexprs['F']},
		{.name = "-G-", .address = character_sexprs['G']},
		{.name = "-H-", .address = character_sexprs['H']},
		{.name = "-I-", .address = character_sexprs['I']},
		{.name = "-J-", .address = character_sexprs['J']},
		{.name = "-K-", .address = character_sexprs['K']},
		{.name = "-L-", .address = character_sexprs['L']},
		{.name = "-M-", .address = character_sexprs['M']},
		{.name = "-N-", .address = character_sexprs['N']},
		{.name = "-O-", .address = character_sexprs['O']},
		{.name = "-P-", .address = character_sexprs['P']},
		{.name = "-Q-", .address = character_sexprs['Q']},
		{.name = "-R-", .address = character_sexprs['R']},
		{.name = "-S-", .address = character_sexprs['S']},
		{.name = "-T-", .address = character_sexprs['T']},
		{.name = "-U-", .address = character_sexprs['U']},
		{.name = "-V-", .address = character_sexprs['V']},
		{.name = "-W-", .address = character_sexprs['W']},
		{.name = "-X-", .address = character_sexprs['X']},
		{.name = "-Y-", .address = character_sexprs['Y']},
		{.name = "-Z-", .address = character_sexprs['Z']},
		{.name = "-\\-", .address = character_sexprs['\\']},
		{.name = "-^-", .address = character_sexprs['^']},
		{.name = "-_-", .address = character_sexprs['_']},
		{.name = "-`-", .address = character_sexprs['`']},
		{.name = "-a-", .address = character_sexprs['a']},
		{.name = "-b-", .address = character_sexprs['b']},
		{.name = "-c-", .address = character_sexprs['c']},
		{.name = "-d-", .address = character_sexprs['d']},
		{.name = "-e-", .address = character_sexprs['e']},
		{.name = "-f-", .address = character_sexprs['f']},
		{.name = "-g-", .address = character_sexprs['g']},
		{.name = "-h-", .address = character_sexprs['h']},
		{.name = "-i-", .address = character_sexprs['i']},
		{.name = "-j-", .address = character_sexprs['j']},
		{.name = "-k-", .address = character_sexprs['k']},
		{.name = "-l-", .address = character_sexprs['l']},
		{.name = "-m-", .address = character_sexprs['m']},
		{.name = "-n-", .address = character_sexprs['n']},
		{.name = "-o-", .address = character_sexprs['o']},
		{.name = "-p-", .address = character_sexprs['p']},
		{.name = "-q-", .address = character_sexprs['q']},
		{.name = "-r-", .address = character_sexprs['r']},
		{.name = "-s-", .address = character_sexprs['s']},
		{.name = "-t-", .address = character_sexprs['t']},
		{.name = "-u-", .address = character_sexprs['u']},
		{.name = "-v-", .address = character_sexprs['v']},
		{.name = "-w-", .address = character_sexprs['w']},
		{.name = "-x-", .address = character_sexprs['x']},
		{.name = "-y-", .address = character_sexprs['y']},
		{.name = "-z-", .address = character_sexprs['z']},
		{.name = "-|-", .address = character_sexprs['|']},
		{.name = "-~-", .address = character_sexprs['~']},
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
