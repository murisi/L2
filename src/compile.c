//To ensure pointers can be passed through longjmp
char longjmp_hack[sizeof(int) - sizeof(void *)];

#include "setjmp.h"
#include "stdio.h"
#include "ctype.h"
#include "stdlib.h"
#include "string.h"
#include "stdarg.h"
#include "limits.h"
#include "stdlib.h"

#include "unistd.h"
#include "dlfcn.h"

#define true (~((int) 0))
#define false ((int) 0)
typedef int bool;
#include "list.c"
#include "compile_errors.c"
#include "lexer.c"
#include "expressions.c"
#include "preparer.c"
#include "generator.c"

list compile_library_names = NULL;

void ensure_compile_library_names() {
	if(compile_library_names == NULL) {
		compile_library_names = nil();
	}
}

void load(char *library_name, jmp_buf *handler) {
	ensure_compile_library_names();
	prepend(library_name, &compile_library_names);
}

void unload(char *library_name, jmp_buf *handler) {
	ensure_compile_library_names();
	list *libsublist = exists(strequal, &compile_library_names, library_name);
	if(libsublist) {
		*libsublist = (*libsublist)->rst;
	} else {
		longjmp(*handler, (int) make_missing_file(library_name));
	}
}

/*
 * Makes a new binary at the path outbin from the object file in the path
 * given by the string in. This binary will both be a static library and
 * an executable.
 */

void compile_object(char *outbin, char *in, jmp_buf *handler) {
	char entryfn[] = ".entryXXXXXX.s";
	FILE *entryfile = fdopen(mkstemps(entryfn, 2), "w+");
	fputs(".section .init_array,\"aw\"\n" ".align 4\n" ".long privmain\n" ".text\n" ".comm argc,4,4\n" ".comm argv,4,4\n"
		".globl main\n" "main:\n" "ret\n" "privmain:\n" "pushl %esi\n" "pushl %edi\n" "pushl %ebx\n" "pushl %ebp\n"
		"movl %esp, %ebp\n" "movl 20(%ebp), %eax\n" "movl %eax, argc\n" "movl 24(%ebp), %eax\n" "movl %eax, argv\n"
		"jmp thunk_end\n" "get_pc_thunk:\n" "movl (%esp), %ebx\n" "ret\n" "thunk_end:\n" "call get_pc_thunk\n"
		"addl $_GLOBAL_OFFSET_TABLE_, %ebx\n", entryfile);
	fclose(entryfile);

	char exitfn[] = ".exitXXXXXX.s";
	FILE *exitfile = fdopen(mkstemps(exitfn, 2), "w+");
	fputs("leave\n" "popl %ebx\n" "popl %edi\n" "popl %esi\n" "movl $0, %eax\n" "ret\n", exitfile);
	fclose(exitfile);
	
	system(cprintf("gcc -m32 -g -o '%s.o' -c '%s'", entryfn, entryfn));
	remove(entryfn);
	system(cprintf("gcc -m32 -g -o '%s.o' -c '%s'", exitfn, exitfn));
	remove(exitfn);
	
	char *library_string = "";
	char *library_name;
	foreach(library_name, compile_library_names) {
		library_string = cprintf("\"%s\" %s", library_name, library_string);
	}
	
	system(cprintf("gcc -m32 -pie -Wl,-E %s -o '%s' '%s.o' '%s' '%s.o' 2>&1 | grep -v \"type and size of dynamic symbol\" 1>&2",
		library_string, outbin, entryfn, in, exitfn));
	remove(cprintf("%s.o", entryfn));
	remove(cprintf("%s.o", exitfn));
}

bool equals(void *a, void *b) {
	return a == b;
}

#define visit_expressions_with(x, y) { \
	visit_expressions_visitor = y; \
	visit_expressions(x); \
}

/*
 * Makes a new binary file at the path outbin from the list of primitive
 * expressions, exprs. The resulting binary file executes the list from top to
 * bottom and then makes all the top-level functions visible to the rest of the
 * executable that it is embedded in.
 */

void compile_expressions(char *outbin, list exprs, jmp_buf *handler) {
	union expression *container = make_begin(), *t;
	list toplevel_function_references = nil();
	{foreach(t, exprs) {
		t->base.parent = container;
		if(t->base.type == function) {
			append(t->function.reference, &toplevel_function_references);
		}
	}}
	container->begin.expressions = exprs;
	union expression *root_function = make_function("()"), *program = root_function;
	put(program, function.expression, container);
	
	vfind_multiple_definitions_handler = handler;
	visit_expressions_with(&program, vfind_multiple_definitions);

	vlink_references_program = program; //Static argument to following function
	vlink_references_handler = handler;
	visit_expressions_with(&program, vlink_references);
	
	visit_expressions_with(&program, vblacklist_references);
	vrename_definition_references_name_records = nil();
	visit_expressions_with(&program, vrename_definition_references);
	visit_expressions_with(&program, vrename_usage_references);
	
	visit_expressions_with(&program, vescape_analysis);
	program = use_return_value(program, generate_reference());
	
	generator_PIC = true;
	generator_init();
	visit_expressions_with(&program, vlayout_frames);
	visit_expressions_with(&program, vgenerate_references);
	visit_expressions_with(&program, vgenerate_continuation_expressions);
	visit_expressions_with(&program, vgenerate_constants);
	visit_expressions_with(&program, vgenerate_ifs);
	visit_expressions_with(&program, vgenerate_function_expressions);
	program = generate_toplevel(program, toplevel_function_references);
	visit_expressions_with(&program, vmerge_begins);
	
	char sfilefn[] = ".s_fileXXXXXX.s";
	FILE *sfile = fdopen(mkstemps(sfilefn, 2), "w+");
	print_assembly(program->begin.expressions, sfile);
	fflush(sfile);
	
	char *ofilefn = cprintf("%s", ".o_fileXXXXXX.o");
	int odes = mkstemps(ofilefn, 2);
	system(cprintf("gcc -m32 -g -c -o '%s' '%s'", ofilefn, sfilefn));
	remove(sfilefn);
	fclose(sfile);
	
	char sympairsfn[] = ".sym_pairsXXXXXX";
	FILE *sympairsfile = fdopen(mkstemp(sympairsfn), "w+");
	struct name_record *r;
	foreach(r, vrename_definition_references_name_records) {
		if(exists(equals, &toplevel_function_references, r->reference) ||
			exists(equals, &root_function->function.parameters, r->reference)) {
				fprintf(sympairsfile, "%s %s\n", r->reference->reference.name, r->original_name);
		}
	}
	fclose(sympairsfile);
	system(cprintf("objcopy --redefine-syms='%s' '%s'", sympairsfn, ofilefn));
	remove(sympairsfn);
	compile_object(outbin, ofilefn, handler);
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

void compile(char *outbin, char *inl2, jmp_buf *handler) {
	FILE *l2file = fopen(inl2, "r");
	if(l2file == NULL) {
		longjmp(*handler, (int) make_missing_file(inl2));
	}
	if(generate_string_blacklist == NULL) {
		generate_string_blacklist = nil();
	}
	
	list expressions = nil();
	list expansion_lists = nil();
	ensure_compile_library_names();
	
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
	expand_expressions(expansion_lists);
	compile_expressions(outbin, expressions, handler);
}
