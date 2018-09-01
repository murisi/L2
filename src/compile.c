#define WORD_SIZE 8

#include "setjmp.h"
#include "stdio.h"
#include "ctype.h"
#include "stdlib.h"
#include "string.h"
#include "limits.h"
#include "stdlib.h"

//Essentially longjmp and setjmp with a pointer argument
void *jmp_value = NULL;
#define thelongjmp(env, val) (jmp_value = val, longjmp(env, 1))
#define thesetjmp(env) (jmp_value = NULL, setjmp(env), jmp_value)

#include "x86_64_linux_interface.c"
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

void compile_expressions(char *outbin, list exprs, jmp_buf *handler) {
	union expression *container = make_begin(), *t;
	{foreach(t, exprs) {
		t->base.parent = container;
	}}
	container->begin.expressions = exprs;
	union expression *root_function = make_function(), *program = root_function;
	put(program, function.expression, container);
	
	visit_expressions(vfind_multiple_definitions, &program, handler);
	visit_expressions(vlink_references, &program, handler);
	visit_expressions(vescape_analysis, &program, NULL);
	program = use_return_value(program, make_reference());
	visit_expressions(vlayout_frames, &program, NULL);
	visit_expressions(vgenerate_references, &program, NULL);
	visit_expressions(vgenerate_continuation_expressions, &program, NULL);
	visit_expressions(vgenerate_literals, &program, NULL);
	visit_expressions(vgenerate_ifs, &program, NULL);
	visit_expressions(vgenerate_function_expressions, &program, NULL);
	list locals = program->function.locals;
	list globals = program->function.parameters;
	program = generate_toplevel(program);
	visit_expressions(vmerge_begins, &program, NULL);
	
	int elf_size = 0;
	unsigned char elf[max_elf_size(program->begin.expressions, locals, globals)];
	write_elf(program->begin.expressions, locals, globals, elf, &elf_size);
	
	int fd = myopen(outbin);
	mywrite(fd, elf, elf_size);
	myclose(fd);
}

#include "parser.c"

/*
 * Makes a new binary file at the path outbin from the L2 file at the path inl2.
 * The resulting binary file executes the L2 source file from top to bottom and
 * then makes all the top-level functions visible to the rest of the executable
 * that it is embedded in.
 */

void compile(char *outbin, char *l2src, int l2src_sz, Symbol *env, jmp_buf *handler) {
	/*if(l2file == NULL) {
		thelongjmp(*handler, make_missing_file(inl2));
	}*/
	list expressions = nil();
	list expansion_lists = nil();
	
	int pos = 0;
	while(after_leading_space(l2src, l2src_sz, &pos)) {
		build_expr_list_handler = handler;
		list sexpr = build_expr_list(l2src, l2src_sz, &pos);
		build_syntax_tree_handler = handler;
		build_syntax_tree_expansion_lists = nil();
		build_syntax_tree(sexpr, append(NULL, &expressions));
		merge_onto(build_syntax_tree_expansion_lists, &expansion_lists);
	}
	
	expand_expressions_handler = handler;
	expand_expressions(&expansion_lists, env);
	compile_expressions(outbin, expressions, handler);
}

#undef WORD_SIZE
