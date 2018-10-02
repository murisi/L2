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
	classify_program_symbols(program->function.expression);
	visit_expressions(vlink_references, &program, (void* []) {ectx->handler, manreg});
	visit_expressions(vescape_analysis, &program, NULL);
	program = use_return_symbol(program, NULL, manreg);
	classify_program_symbols(program->function.expression);
	visit_expressions(vlayout_frames, &program->function.expression, manreg);
	visit_expressions(vgenerate_references, &program, manreg);
	visit_expressions(vgenerate_continuation_expressions, &program, manreg);
	visit_expressions(vgenerate_literals, &program, manreg);
	visit_expressions(vgenerate_ifs, &program, manreg);
	visit_expressions(vgenerate_function_expressions, &program, manreg);
	visit_expressions(vgenerate_storage_expressions, &program, manreg);
	list symbols = program->function.symbols;
	union expression *l;
	{foreach(l, program->function.parameters) {
		append(l->reference.symbol, &symbols, manreg);
	}}
	program = generate_toplevel(program, manreg);
	list asms = nil;
	visit_expressions(vlinearized_expressions, &program, (void* []) {&asms, manreg});
	unsigned char *objdest; int objdest_sz;
	write_elf(reverse(asms, manreg), symbols, &objdest, &objdest_sz, manreg);
	Object *obj = load(objdest, objdest_sz, ectx->rt_reg, ectx->handler);
	struct symbol *sym;
	{foreach(sym, symbols) {
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
		{.name = "-!-", .address = _char('!')},
		{.name = "-\"-", .address = _char('"')},
		{.name = "-#-", .address = _char('#')},
		{.name = "-$-", .address = _char('$')},
		{.name = "-%-", .address = _char('%')},
		{.name = "-&-", .address = _char('&')},
		{.name = "-'-", .address = _char('\'')},
		{.name = "-*-", .address = _char('*')},
		{.name = "-+-", .address = _char('+')},
		{.name = "-,-", .address = _char(',')},
		{.name = "---", .address = _char('-')},
		{.name = "-.-", .address = _char('.')},
		{.name = "-/-", .address = _char('/')},
		{.name = "-0-", .address = _char('0')},
		{.name = "-1-", .address = _char('1')},
		{.name = "-2-", .address = _char('2')},
		{.name = "-3-", .address = _char('3')},
		{.name = "-4-", .address = _char('4')},
		{.name = "-5-", .address = _char('5')},
		{.name = "-6-", .address = _char('6')},
		{.name = "-7-", .address = _char('7')},
		{.name = "-8-", .address = _char('8')},
		{.name = "-9-", .address = _char('9')},
		{.name = "-:-", .address = _char(':')},
		{.name = "-;-", .address = _char(';')},
		{.name = "-<-", .address = _char('<')},
		{.name = "-=-", .address = _char('=')},
		{.name = "->-", .address = _char('>')},
		{.name = "-?-", .address = _char('?')},
		{.name = "-@-", .address = _char('@')},
		{.name = "-A-", .address = _char('A')},
		{.name = "-B-", .address = _char('B')},
		{.name = "-C-", .address = _char('C')},
		{.name = "-D-", .address = _char('D')},
		{.name = "-E-", .address = _char('E')},
		{.name = "-F-", .address = _char('F')},
		{.name = "-G-", .address = _char('G')},
		{.name = "-H-", .address = _char('H')},
		{.name = "-I-", .address = _char('I')},
		{.name = "-J-", .address = _char('J')},
		{.name = "-K-", .address = _char('K')},
		{.name = "-L-", .address = _char('L')},
		{.name = "-M-", .address = _char('M')},
		{.name = "-N-", .address = _char('N')},
		{.name = "-O-", .address = _char('O')},
		{.name = "-P-", .address = _char('P')},
		{.name = "-Q-", .address = _char('Q')},
		{.name = "-R-", .address = _char('R')},
		{.name = "-S-", .address = _char('S')},
		{.name = "-T-", .address = _char('T')},
		{.name = "-U-", .address = _char('U')},
		{.name = "-V-", .address = _char('V')},
		{.name = "-W-", .address = _char('W')},
		{.name = "-X-", .address = _char('X')},
		{.name = "-Y-", .address = _char('Y')},
		{.name = "-Z-", .address = _char('Z')},
		{.name = "-\\-", .address = _char('\\')},
		{.name = "-^-", .address = _char('^')},
		{.name = "-_-", .address = _char('_')},
		{.name = "-`-", .address = _char('`')},
		{.name = "-a-", .address = _char('a')},
		{.name = "-b-", .address = _char('b')},
		{.name = "-c-", .address = _char('c')},
		{.name = "-d-", .address = _char('d')},
		{.name = "-e-", .address = _char('e')},
		{.name = "-f-", .address = _char('f')},
		{.name = "-g-", .address = _char('g')},
		{.name = "-h-", .address = _char('h')},
		{.name = "-i-", .address = _char('i')},
		{.name = "-j-", .address = _char('j')},
		{.name = "-k-", .address = _char('k')},
		{.name = "-l-", .address = _char('l')},
		{.name = "-m-", .address = _char('m')},
		{.name = "-n-", .address = _char('n')},
		{.name = "-o-", .address = _char('o')},
		{.name = "-p-", .address = _char('p')},
		{.name = "-q-", .address = _char('q')},
		{.name = "-r-", .address = _char('r')},
		{.name = "-s-", .address = _char('s')},
		{.name = "-t-", .address = _char('t')},
		{.name = "-u-", .address = _char('u')},
		{.name = "-v-", .address = _char('v')},
		{.name = "-w-", .address = _char('w')},
		{.name = "-x-", .address = _char('x')},
		{.name = "-y-", .address = _char('y')},
		{.name = "-z-", .address = _char('z')},
		{.name = "-|-", .address = _char('|')},
		{.name = "-~-", .address = _char('~')},
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
