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

#include "sexpr.c"
#include "compile_errors.c"
#include "lexer.c"
#include "expressions.c"
#include "preparer.c"

#define visit_expressions_with(x, y) { \
	visit_expressions_visitor = y; \
	visit_expressions(x); \
}

#if __x86_64__
	#include "generator64.c"
#endif
#if __i386__
	#include "generator32.c"
#endif

bool equals(void *a, void *b) {
	return a == b;
}

struct occurrences {
	char *member;
	int count;
};

bool occurrences_for(void *o, void *ctx) {
	return strequal(((struct occurrences *) o)->member, ctx);
}

#include "parser.c"

/*
 * Makes a new binary file at the path outbin from the L2 file at the path inl2.
 * The resulting binary file executes the L2 source file from top to bottom and
 * then makes all the top-level functions visible to the rest of the executable
 * that it is embedded in.
 */

void compile(char *outbin, char *inl2, Symbol *env, jmp_buf *handler) {
	FILE *l2file = fopen(inl2, "r");
	if(l2file == NULL) {
		thelongjmp(*handler, make_missing_file(inl2));
	}
	if(generate_string_blacklist == NULL) {
		generate_string_blacklist = nil();
	}
	
	list expressions = nil();
	list expansion_lists = nil();
	
	int c;
	while((c = after_leading_space(l2file)) != EOF) {
		ungetc(c, l2file);
		build_expr_list_handler = handler;
		list sexpr = build_expr_list(l2file);
		build_syntax_tree_handler = handler;
		build_syntax_tree_expansion_lists = nil();
		build_syntax_tree(sexpr, append(NULL, &expressions));
		merge_onto(build_syntax_tree_expansion_lists, &expansion_lists);
	}
	fclose(l2file);
	
	expand_expressions_handler = handler;
	expand_expressions(&expansion_lists, env);
	compile_expressions(outbin, expressions, handler);
}
