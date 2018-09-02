#define WORD_SIZE 8

#include "setjmp.h"
#include "stdio.h"
#include "ctype.h"
#include "stdlib.h"
#include "string.h"
#include "limits.h"

//Essentially longjmp and setjmp with a pointer argument
void *jmp_value = NULL;
#define thelongjmp(env, val) (jmp_value = val, longjmp(env, 1))
#define thesetjmp(env) (jmp_value = NULL, setjmp(env), jmp_value)

#include "x86_64_linux_interface.c"
#include "stdlib.h"
#include "stdarg.h"
#include "stdint.h"
typedef int64_t bool;
#define true (~((bool) 0))
#define false ((bool) 0)
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

void compile_expressions(unsigned char **objdest, int *objdest_sz, list exprs, region elfreg, jmp_buf *handler) {
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
	write_elf(program->begin.expressions, locals, globals, objdest, objdest_sz, elfreg);
	destroy_region(manreg);
}

#include "parser.c"

/*
 * Makes a new binary file at the path outbin from the L2 file at the path inl2.
 * The resulting binary file executes the L2 source file from top to bottom and
 * then makes all the top-level functions visible to the rest of the executable
 * that it is embedded in.
 */

void compile(unsigned char **objdest, int *objdest_sz, char *l2src, int l2src_sz, Symbol *env, region objreg, jmp_buf *handler) {
	region syntax_tree_region = create_region(0);
	list expressions = nil(syntax_tree_region);
	list expansion_lists = nil(syntax_tree_region);
	
	int pos = 0;
	while(after_leading_space(l2src, l2src_sz, &pos)) {
		build_expr_list_handler = handler;
		list sexpr = build_expr_list(l2src, l2src_sz, &pos, syntax_tree_region);
		build_syntax_tree_handler = handler;
		build_syntax_tree_expansion_lists = nil(syntax_tree_region);
		build_syntax_tree(sexpr, append(NULL, &expressions, syntax_tree_region), syntax_tree_region);
		merge_onto(build_syntax_tree_expansion_lists, &expansion_lists);
	}
	
	expand_expressions_handler = handler;
	expand_expressions(&expansion_lists, env, syntax_tree_region);
	compile_expressions(objdest, objdest_sz, expressions, objreg, handler);
	destroy_region(syntax_tree_region);
}

#undef WORD_SIZE
