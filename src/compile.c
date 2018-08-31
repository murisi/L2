#include "setjmp.h"
#include "stdio.h"
#include "ctype.h"
#include "stdlib.h"
#include "string.h"
#include "limits.h"
#include "stdlib.h"
#include "library.c"

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

#if __x86_64__
	#include "x86_64_generator.c"
#endif
#if __i386__
	#include "i386_generator32.c"
#endif

bool equals(void *a, void *b) {
	return a == b;
}

#include "parser.c"

/*
 * Makes a new binary file at the path outbin from the L2 file at the path inl2.
 * The resulting binary file executes the L2 source file from top to bottom and
 * then makes all the top-level functions visible to the rest of the executable
 * that it is embedded in.
 */

void compile(char *outbin, char *inl2, Symbol *env, jmp_buf *handler) {
	int l2file = myopen(inl2);
	/*if(l2file == NULL) {
		thelongjmp(*handler, make_missing_file(inl2));
	}*/
	myseek(l2file, 0, SEEK_SET);
	
	list expressions = nil();
	list expansion_lists = nil();
	
	char c; int pos = 0;
	while(after_leading_space(l2file, &pos, &c)) {
		myseek(l2file, -1, SEEK_CUR);
		build_expr_list_handler = handler;
		list sexpr = build_expr_list(l2file, &pos);
		build_syntax_tree_handler = handler;
		build_syntax_tree_expansion_lists = nil();
		build_syntax_tree(sexpr, append(NULL, &expressions));
		merge_onto(build_syntax_tree_expansion_lists, &expansion_lists);
	}
	myclose(l2file);
	
	expand_expressions_handler = handler;
	expand_expressions(&expansion_lists, env);
	compile_expressions(outbin, expressions, handler);
}
