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
#include "expressions.c"
#include "x86_64_object.c"
#include "preparer.c"
#include "x86_64_generator.c"
#include "x86_64_assembler.c"

/*
 * Makes a new binary file at the path outbin from the list of primitive
 * expressions, exprs. The resulting binary file executes the list from top to
 * bottom and then makes all the top-level functions visible to the rest of the
 * executable that it is embedded in.
 */

Object *load_expressions(list exprs, list st_ref_nms, list dyn_ref_nms, region obj_reg, myjmp_buf *handler) {
	region manreg = create_region(0);
	union expression *container = make_begin(manreg), *t;
	{foreach(t, exprs) {
		t->base.parent = container;
	}}
	container->begin.expressions = exprs;
	union expression *root_function = make_function(manreg), *program = root_function;
	put(program, function.expression, container);
	program->function.expression = generate_macros(program->function.expression, st_ref_nms, dyn_ref_nms, obj_reg, handler);
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
	visit_expressions(vmerge_begins, &program, NULL);
	
	unsigned char *objdest; int objdest_sz;
	write_elf(program->begin.expressions, locals, globals, &objdest, &objdest_sz, manreg);
	Object *obj = load(objdest, objdest_sz, obj_reg);
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

void evaluate_source(int srcc, char *srcv[], list static_bindings, myjmp_buf *handler) {
	region syntax_tree_region = create_region(0);
	list expressions = nil(syntax_tree_region), objects = nil(syntax_tree_region);
	
	int i;
	for(i = 0; i < srcc; i++) {
		char *dot = strrchr(srcv[i], '.');
		if(dot && !strcmp(dot, ".l2")) {
			long int src_sz = mysize(srcv[i]);
			unsigned char *src_buf = region_malloc(syntax_tree_region, src_sz);
			int fd = myopen(srcv[i]);
			myread(fd, src_buf, src_sz);
			myclose(fd);
			
			int pos = 0;
			while(after_leading_space(src_buf, src_sz, &pos)) {
				build_expr_list_handler = handler;
				list sexpr = build_expr_list(src_buf, src_sz, &pos, syntax_tree_region);
				append(build_syntax_tree(sexpr, NULL, syntax_tree_region, handler), &expressions, syntax_tree_region);
			}
		} else if(dot && !strcmp(dot, ".o")) {
			long int obj_sz = mysize(srcv[i]);
			unsigned char *obj_buf = region_malloc(syntax_tree_region, obj_sz);
			int obj_fd = myopen(srcv[i]);
			myread(obj_fd, obj_buf, obj_sz);
			myclose(obj_fd);
			
			Object *obj = load(obj_buf, obj_sz, syntax_tree_region);
			prepend(obj, &objects, syntax_tree_region);
			append(make_invoke0(make_literal((unsigned long) start(obj), syntax_tree_region), syntax_tree_region),
				&expressions, syntax_tree_region);
			int isc = immutable_symbol_count(obj);
			Symbol *is = region_malloc(syntax_tree_region, isc * sizeof(Symbol));
			immutable_symbols(obj, is);
			int j;
			for(j = 0; j < isc; j++) {
				prepend(&is[j], &static_bindings, syntax_tree_region);
			}
		}
	}
	
	list st_ref_nms = nil(syntax_tree_region);
	Symbol *sym;
	{foreach(sym, static_bindings) {
		prepend(sym->name, &st_ref_nms, syntax_tree_region);
	}}
	Object *main_obj = load_expressions(expressions, st_ref_nms, nil(syntax_tree_region), syntax_tree_region, handler);
	int isc = immutable_symbol_count(main_obj);
	Symbol *is = region_malloc(syntax_tree_region, isc * sizeof(Symbol));
	immutable_symbols(main_obj, is);
	for(i = 0; i < isc; i++) {
		prepend(&is[i], &static_bindings, syntax_tree_region);
	}
	
	int sbc = length(static_bindings);
	Symbol *sb = region_malloc(syntax_tree_region, sbc * sizeof(Symbol));
	i = 0;
	{foreach(sym, static_bindings) {
		sb[i++] = *sym;
	}}
	mutate_symbols(main_obj, sb, sbc);
	Object *obj;
	{foreach(obj, objects) {
		mutate_symbols(obj, sb, sbc);
	}}
	start(main_obj)();
	destroy_region(syntax_tree_region);
}

#undef WORD_SIZE
