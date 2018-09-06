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
#include "preparer.c"
#include "x86_64_generator.c"
#include "x86_64_object.c"
#include "x86_64_assembler.c"

bool equals(void *a, void *b) {
	return a == b;
}

/*
 * Makes a new binary file at the path outbin from the list of primitive
 * expressions, exprs. The resulting binary file executes the list from top to
 * bottom and then makes all the top-level functions visible to the rest of the
 * executable that it is embedded in.
 */

void evaluate_expressions(list exprs, list env, myjmp_buf *handler) {
	region manreg = create_region(0);
	union expression *container = make_begin(manreg), *t;
	{foreach(t, exprs) {
		t->base.parent = container;
	}}
	container->begin.expressions = exprs;
	union expression *root_function = make_function(manreg), *program = root_function;
	put(program, function.expression, container);
	
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
	Object *obj = load(objdest, objdest_sz, manreg);
	Symbol *sym;
	foreach(sym, env) {
		mutate_symbols(obj, sym, 1);
	}
	start(obj)();
	destroy_region(manreg);
}

#include "parser.c"

/*
 * Makes a new binary file at the path outbin from the L2 file at the path inl2.
 * The resulting binary file executes the L2 source file from top to bottom and
 * then makes all the top-level functions visible to the rest of the executable
 * that it is embedded in.
 */

void evaluate_source(char *l2src, int l2src_sz, list env, myjmp_buf *handler) {
	region syntax_tree_region = create_region(0);
	list expressions = nil(syntax_tree_region);
	list expansion_lists = nil(syntax_tree_region);
	
	int pos = 0;
	while(after_leading_space(l2src, l2src_sz, &pos)) {
		build_expr_list_handler = handler;
		list sexpr = build_expr_list(l2src, l2src_sz, &pos, syntax_tree_region);
		build_syntax_tree_handler = handler;
		append(build_syntax_tree(sexpr, NULL, nil(syntax_tree_region), nil(syntax_tree_region), syntax_tree_region), &expressions,
			syntax_tree_region);
	}
	
	evaluate_expressions(expressions, env, handler);
	destroy_region(syntax_tree_region);
}

#undef WORD_SIZE
