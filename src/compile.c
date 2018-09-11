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

struct compilation {
	unsigned char *objdest;
	int objdest_sz;
	union expression *program;
	list ref_nms;
};

/*
 * Makes a new binary file at the path outbin from the list of primitive
 * expressions, exprs. The resulting binary file executes the list from top to
 * bottom and then makes all the top-level functions visible to the rest of the
 * executable that it is embedded in.
 */

void compile_expressions(unsigned char **objdest, int *objdest_sz, list exprs, list ref_nms, list *comps, region obj_reg, myjmp_buf *handler) {
	region manreg = create_region(0);
	union expression *oprogram = make_function(obj_reg), *program;
	union expression *ocontainer = make_begin(obj_reg), *t;
	{foreach(t, exprs) {
		append(t, &ocontainer->begin.expressions, obj_reg);
	}}
	put(oprogram, function.expression, ocontainer);
	struct compilation *pc;
	{foreach(pc, *comps) {
		if(expression_equals(oprogram, pc->program)) {
			*objdest = pc->objdest;
			*objdest_sz = pc->objdest_sz;
			destroy_region(manreg);
			return;
		}
	}}
	
	program = copy_expression(oprogram, manreg);
	put(program, function.expression, generate_macros(program->function.expression, ref_nms, nil(obj_reg), comps, obj_reg, handler));
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
	write_elf(reverse(asms, manreg), locals, globals, objdest, objdest_sz, obj_reg);
	destroy_region(manreg);
	
	struct compilation *c = region_malloc(obj_reg, sizeof(struct compilation));
	c->objdest = *objdest;
	c->objdest_sz = *objdest_sz;
	c->program = oprogram;
	c->ref_nms = ref_nms;
	prepend(c, comps, obj_reg);
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
				append(build_syntax_tree(sexpr, syntax_tree_region, handler), &expressions, syntax_tree_region);
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
			append_list(&bindings, immutable_symbols(obj, syntax_tree_region));
		}
	}
	
	list ref_nms = nil(syntax_tree_region);
	Symbol *sym;
	{foreach(sym, bindings) {
		prepend(sym->name, &ref_nms, syntax_tree_region);
	}}
	unsigned char *objdest; int objdest_sz; list comps = nil(syntax_tree_region);
	compile_expressions(&objdest, &objdest_sz, expressions, ref_nms, &comps, syntax_tree_region, handler);
	Object *main_obj = load(objdest, objdest_sz, syntax_tree_region);
	list is = immutable_symbols(main_obj, syntax_tree_region);
	append_list(&bindings, is);
	mutate_symbols(main_obj, bindings);
	Object *obj;
	{foreach(obj, objects) {
		mutate_symbols(obj, bindings);
	}}
	start(main_obj)();
	destroy_region(syntax_tree_region);
}

#undef WORD_SIZE
