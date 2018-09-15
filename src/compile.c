#define WORD_SIZE 8
#define NULL 0UL

#include "x86_64_linux_interface.c"
typedef unsigned long int bool;
#define true (~0UL)
#define false (0UL)
#include "list.c"
#include "sexpr.c"
#include "compile_errors.c"
#include "lexer.c"
#include "x86_64_object.c"
#include "expressions.c"
#include "preparer.c"
#include "x86_64_generator.c"
#include "x86_64_assembler.c"

/*
 * Makes a new binary file at the path outbin from the list of primitive
 * expressions, exprs. The resulting binary file executes the list from top to
 * bottom and then makes all the top-level functions visible to the rest of the
 * executable that it is embedded in.
 */

Object *load_expressions(list exprs, list *ext_binds, list st_binds, list *comps, region obj_reg, myjmp_buf *handler) {
	region manreg = create_region(0);
	union expression *program = make_function(manreg), *t;
	{foreach(t, exprs) {
		append(copy_expression(t, manreg), &program->function.expression->begin.expressions, manreg);
	}}
	put(program, function.expression, generate_np_expressions(program->function.expression, true, ext_binds, st_binds, nil(obj_reg),
		comps, manreg, obj_reg, handler));
	visit_expressions(vfind_multiple_definitions, &program, handler);
	visit_expressions(vlink_references, &program, (void* []) {handler, manreg});
	visit_expressions(vescape_analysis, &program, NULL);
	program = use_return_value(program, make_reference(manreg), manreg);
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
	Object *obj = load(objdest, objdest_sz, obj_reg);
	union expression *l;
	{foreach(l, locals) {
		if(l->reference.symbol) {
			l->reference.symbol->address += (unsigned long) segment(obj, ".bss");
		}
	}}
	{foreach(l, asms) {
		if(l->instruction.opcode == LOCAL_LABEL || l->instruction.opcode == GLOBAL_LABEL) {
			union expression *label_ref = l->instruction.arguments->fst;
			if(label_ref->reference.symbol) {
				label_ref->reference.symbol->address += (unsigned long) segment(obj, ".text");
			}
		}
	}}
	destroy_region(manreg);
	return obj;
}

#include "parser.c"

/*
 * Makes a new binary file at the path outbin from the L2 file at the path inl2.
 * The resulting binary file executes the L2 source file from top to bottom and
 * then makes all the top-level functions visible to the rest of the executable
 * that it is embedded in.
 */

void evaluate_source(int srcc, char *srcv[], list bindings, myjmp_buf *handler) {
	region syntax_tree_region = create_region(0);
	list objects = nil(syntax_tree_region), comps = nil(syntax_tree_region);
	
	int i;
	for(i = 0; i < srcc; i++) {
		char *dot = strrchr(srcv[i], '.');
		if(dot && !strcmp(dot, ".l2")) {
			long int src_sz = mysize(srcv[i]);
			unsigned char *src_buf = region_malloc(syntax_tree_region, src_sz);
			int fd = myopen(srcv[i]);
			myread(fd, src_buf, src_sz);
			myclose(fd);
			
			list expressions = nil(syntax_tree_region);
			int pos = 0;
			while(after_leading_space(src_buf, src_sz, &pos)) {
				build_expr_list_handler = handler;
				list sexpr = build_expr_list(src_buf, src_sz, &pos, syntax_tree_region);
				append(build_syntax_tree(sexpr, syntax_tree_region, handler), &expressions, syntax_tree_region);
			}
			Object *obj = load_expressions(expressions, &bindings, nil(syntax_tree_region), &comps, syntax_tree_region, handler);
			append(obj, &objects, syntax_tree_region);
			append_list(&bindings, immutable_symbols(obj, syntax_tree_region));
		} else if(dot && !strcmp(dot, ".o")) {
			long int obj_sz = mysize(srcv[i]);
			unsigned char *obj_buf = region_malloc(syntax_tree_region, obj_sz);
			int obj_fd = myopen(srcv[i]);
			myread(obj_fd, obj_buf, obj_sz);
			myclose(obj_fd);
			
			Object *obj = load(obj_buf, obj_sz, syntax_tree_region);
			append(obj, &objects, syntax_tree_region);
			append_list(&bindings, immutable_symbols(obj, syntax_tree_region));
		}
	}
	
	Object *obj;
	{foreach(obj, objects) {
		mutate_symbols(obj, bindings);
	}}
	{foreach(obj, objects) {
		((void (*)()) segment(obj, ".text"))();
	}}
	destroy_region(syntax_tree_region);
}

#undef WORD_SIZE
