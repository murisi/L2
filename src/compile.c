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

char *cprintf(const char *format, ...) {
	va_list ap, aq;
	va_start(ap, format);
	va_copy(aq, ap);
	
	char *str = calloc(vsnprintf(NULL, 0, format, ap) + 1, sizeof(char));
	vsprintf(str, format, aq);
	
	va_end(aq);
	va_end(ap);
	return str;
}

#include "list.c"
#include "compile_errors.c"
#include "lexer.c"
#include "expressions.c"
#include "preparer.c"
#include "generator.c"

char *to_command_line_args(list strs) {
	int args_length = 0;
	char *tmp;
	{foreach(tmp, strs) {
		args_length += strlen(tmp) + 5;
	}}
	char *clas = calloc(args_length + 1, sizeof(char));
	clas[0] = '\0';
	foreach(tmp, strs) {
		strcat(clas, " -l:");
		strcat(clas, tmp);
		strcat(clas, " ");
	}
	return clas;
}

list make_shared_library_object_files;

void make_shared_library(bool PIC, char *sofile) {
	char entryfn[] = "./entryXXXXXX.s";
	FILE *entryfile = fdopen(mkstemps(entryfn, 2), "w+");
	fputs(".section .init_array,\"aw\"\n" ".align 4\n" ".long main\n" ".text\n" "main:\n" "pushl %esi\n" "pushl %edi\n" "pushl %ebx\n"
		"pushl %ebp\n" "movl %esp, %ebp\n", entryfile);
	if(PIC) {
		fputs("jmp thunk_end\n" "get_pc_thunk:\n" "movl (%esp), %ebx\n" "ret\n" "thunk_end:\n" "call get_pc_thunk\n"
			"addl $_GLOBAL_OFFSET_TABLE_, %ebx\n", entryfile);
	}
	fclose(entryfile);

	char exitfn[] = "./exitXXXXXX.s";
	FILE *exitfile = fdopen(mkstemps(exitfn, 2), "w+");
	fputs("leave\n" "popl %ebx\n" "popl %edi\n" "popl %esi\n" "movl $0, %eax\n" "ret\n", exitfile);
	fclose(exitfile);
	
	system(cprintf("gcc -m32 -g -o '%s.o' -c '%s'", entryfn, entryfn));
	system(cprintf("gcc -m32 -g -o '%s.o' -c '%s'", exitfn, exitfn));
	system(cprintf("ld -m elf_i386 -shared -L . --allow-multiple-definition --whole-archive -o '%s' '%s.o' %s '%s.o'", sofile, entryfn,
		to_command_line_args(reverse(make_shared_library_object_files)), exitfn));
	remove(entryfn);
	remove(cprintf("%s.o", entryfn));
	remove(exitfn);
	remove(cprintf("%s.o", exitfn));
}

list make_program_object_files;

void make_program(bool PIC, char *outfile) {
	char entryfn[] = "./entryXXXXXX.s";
	FILE *entryfile = fdopen(mkstemps(entryfn, 2), "w+");
	fputs(".text\n" ".comm argc,4,4\n" ".comm argv,4,4\n" ".globl main\n" "main:\n" "pushl %esi\n" "pushl %edi\n" "pushl %ebx\n"
		"pushl %ebp\n" "movl %esp, %ebp\n" "movl 20(%ebp), %eax\n" "movl %eax, argc\n" "movl 24(%ebp), %eax\n" "movl %eax, argv\n",
		entryfile);
	if(PIC) {
		fputs("jmp thunk_end\n" "get_pc_thunk:\n" "movl (%esp), %ebx\n" "ret\n" "thunk_end:\n" "call get_pc_thunk\n"
			"addl $_GLOBAL_OFFSET_TABLE_, %ebx\n", entryfile);
	}
	fclose(entryfile);

	char exitfn[] = "./exitXXXXXX.s";
	FILE *exitfile = fdopen(mkstemps(exitfn, 2), "w+");
	fputs("leave\n" "popl %ebx\n" "popl %edi\n" "popl %esi\n" "movl $0, %eax\n" "ret\n", exitfile);
	fclose(exitfile);
	
	system(cprintf("gcc -m32 -g -o '%s.o' -c '%s'", entryfn, entryfn));
	system(cprintf("gcc -m32 -g -o '%s.o' -c '%s'", exitfn, exitfn));
	system(cprintf("ld -m elf_i386 -L . --allow-multiple-definition --whole-archive -o '%s' '%s.o' %s '%s.o'", outfile, entryfn,
		to_command_line_args(reverse(make_program_object_files)), exitfn));
	remove(entryfn);
	remove(cprintf("%s.o", entryfn));
	remove(exitfn);
	remove(cprintf("%s.o", exitfn));
}

bool equals(void *a, void *b) { return a == b; }

#define visit_expressions_with(x, y) { \
	visit_expressions_visitor = y; \
	visit_expressions(x); \
}

char *compile(list exprs, bool PIC, jmp_buf *handler) {
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
	
	generator_PIC = PIC;
	visit_expressions_with(&program, vlayout_frames);
	visit_expressions_with(&program, vgenerate_references);
	visit_expressions_with(&program, vgenerate_continuation_expressions);
	visit_expressions_with(&program, vgenerate_constants);
	visit_expressions_with(&program, vgenerate_ifs);
	visit_expressions_with(&program, vgenerate_function_expressions);
	program = generate_toplevel(program, toplevel_function_references);
	visit_expressions_with(&program, vmerge_begins);
	
	char sfilefn[] = "./s_fileXXXXXX.s";
	FILE *sfile = fdopen(mkstemps(sfilefn, 2), "w+");
	print_assembly(program->begin.expressions, sfile);
	fflush(sfile);
	
	char *ofilefn = cprintf("%s", "./o_fileXXXXXX.o");
	int odes = mkstemps(ofilefn, 2);
	system(cprintf("gcc -m32 -g -c -o '%s' '%s'", ofilefn, sfilefn));
	remove(sfilefn);
	fclose(sfile);
	
	char sympairsfn[] = "./sym_pairsXXXXXX";
	FILE *sympairsfile = fdopen(mkstemp(sympairsfn), "w+");
	struct name_record *r;
	foreach(r, vrename_definition_references_name_records) {
		if(exists(equals, toplevel_function_references, r->reference) ||
			exists(equals, root_function->function.parameters, r->reference)) {
				fprintf(sympairsfile, "%s %s\n", r->reference->reference.name, r->original_name);
		}
	}
	fclose(sympairsfile);
	system(cprintf("objcopy --redefine-syms='%s' '%s'", sympairsfn, ofilefn));
	remove(sympairsfn);
	
	char *afn = cprintf("%s", "./libXXXXXX.a");
	mkstemps(afn, 2);
	remove(afn);
	system(cprintf("ar -rcs %s %s\n", afn, ofilefn));
	remove(ofilefn);
	return afn;
}

char *dynamic_load(list exprs, jmp_buf *handler) {
	char *ofilefn = compile(exprs, true, handler);
	prepend(ofilefn, &make_shared_library_object_files);
	char *sofilefn = cprintf("%s", "./so_fileXXXXXX.so");
	int sodes = mkstemps(sofilefn, 3);
	make_shared_library(true, sofilefn);
	make_shared_library_object_files = rst(make_shared_library_object_files);
	remove(ofilefn);
	return sofilefn;
}

/*
 * Copies the file of the path given by the string in, into the path given by the string
 * out and returns the out path.
 */

char *copy(char *out, char *in) {
	system(cprintf("cp '%s' '%s'", in, out));
	return out;
}

/*
 * Returns the path of a newly made file that is the concatenation of the file of the path
 * in the string in2 to the file of the path in the string in1.
 */

char *concatenate(char *in1, char *in2) {
	char *outfn = cprintf("%s", "./catXXXXXX");
	mkstemp(outfn);
	system(cprintf("cat '%s' '%s' > '%s'", in1, in2, outfn));
	return outfn;
}

struct occurrences {
	char *member;
	int count;
};

bool occurrences_for(void *o, void *ctx) {
	return strequal(((struct occurrences *) o)->member, ctx);
}

#define MEMBER_BUFFER_SIZE 1024

/*
 * Makes a new static library whose's ordered list of objects are the concatenation
 * of the ordered list of objects in the static library in2 onto the ordered list of
 * objects in the static library in1, and returns the path to the new static library.
 */

char *sequence(char *in1, char *in2) {
	char *outfn = cprintf("%s", "./libXXXXXX.a");
	mkstemps(outfn, 2);
	copy(outfn, in1);
	
	char *tempdir = cprintf("%s", "./objectsXXXXXX");
	mkdtemp(tempdir);
	chdir(tempdir);
	
	list member_counts = nil();
	char member[MEMBER_BUFFER_SIZE];
	FILE *f2 = popen(cprintf("ar -t '../%s'", in2), "r");
	while(fgets(member, MEMBER_BUFFER_SIZE, f2)) {
		member[strlen(member) - 1] = '\0';
		struct occurrences *o = exists(occurrences_for, member_counts, member);
		if(o) {
			o->count++;
		} else {
			o = malloc(sizeof(struct occurrences));
			o->member = cprintf("%s", member);
			o->count = 1;
			prepend(o, &member_counts);
		}
		system(cprintf("ar -xN %i '../%s' '%s'\n", o->count, in2, o->member));
		system(cprintf("ar -q '../%s' '%s'", outfn, o->member));
		remove(o->member);
	}
	chdir("../");
	remove(tempdir);
	return outfn;
}

/*
 * Makes a new static library whose's ordered list of objects consists
 * of an object that jumps to the label by the name given in the string
 * skiplabel, followed by the ordered list of objects in static library
 * in, followed by an object comprising of a label by the name given in
 * the string skiplabel, and returns a path to this static library.
 */

char *skip(char *in, char *skiplabel) {
	char entryfn[] = "./entryXXXXXX.s";
	FILE *entryfile = fdopen(mkstemps(entryfn, 2), "w+");
	fprintf(entryfile, ".text\njmp %s\n", skiplabel);
	fclose(entryfile);
	
	char exitfn[] = "./exitXXXXXX.s";
	FILE *exitfile = fdopen(mkstemps(exitfn, 2), "w+");
	fprintf(exitfile, ".global %s\n%s:\n", skiplabel, skiplabel);
	fclose(exitfile);
	
	system(cprintf("gcc -m32 -g -o '%s.o' -c '%s'", entryfn, entryfn));
	remove(entryfn);
	system(cprintf("gcc -m32 -g -o '%s.o' -c '%s'", exitfn, exitfn));
	remove(exitfn);
	
	char *outfn = cprintf("%s", "./libXXXXXX.a");
	mkstemps(outfn, 2);
	remove(outfn);
	system(cprintf("ar -rcs '%s' '%s'", outfn, cprintf("%s.o", entryfn)));
	remove(cprintf("%s.o", entryfn));
	
	char *f2fn = sequence(outfn, in);
	remove(outfn);
	system(cprintf("ar -q '%s' '%s'", f2fn, cprintf("%s.o", exitfn)));
	remove(cprintf("%s.o", exitfn));
	
	return f2fn;
}

/*
 * Makes a new dynamic library from the static library in the path
 * given by the string in. When dynamic library is loaded using dlopen,
 * all the object files from the static library are executed sequentially.
 * Afterwords, all the functions from the static library are available
 * for calling, as is expected of a dynamic library. This function returns
 * the path to this newly made dynamic library.
 */

char *dynamic(char *in) {
	char *outfn = cprintf("%s", "./libXXXXXX.so");
	mkstemps(outfn, 3);
	
	char entryfn[] = "./entryXXXXXX.s";
	FILE *entryfile = fdopen(mkstemps(entryfn, 2), "w+");
	fputs(".section .init_array,\"aw\"\n" ".align 4\n" ".long main\n" ".text\n" "main:\n" "pushl %esi\n" "pushl %edi\n" "pushl %ebx\n"
		"pushl %ebp\n" "movl %esp, %ebp\n", entryfile);
	if(true) {
		fputs("jmp thunk_end\n" "get_pc_thunk:\n" "movl (%esp), %ebx\n" "ret\n" "thunk_end:\n" "call get_pc_thunk\n"
			"addl $_GLOBAL_OFFSET_TABLE_, %ebx\n", entryfile);
	}
	fclose(entryfile);

	char exitfn[] = "./exitXXXXXX.s";
	FILE *exitfile = fdopen(mkstemps(exitfn, 2), "w+");
	fputs("leave\n" "popl %ebx\n" "popl %edi\n" "popl %esi\n" "movl $0, %eax\n" "ret\n", exitfile);
	fclose(exitfile);
	
	system(cprintf("gcc -m32 -g -o '%s.o' -c '%s'", entryfn, entryfn));
	remove(entryfn);
	system(cprintf("gcc -m32 -g -o '%s.o' -c '%s'", exitfn, exitfn));
	remove(exitfn);
	system(cprintf("ld -m elf_i386 -shared -L . --allow-multiple-definition --whole-archive -o '%s' '%s.o' '%s' '%s.o'", outfn,
		entryfn, in, exitfn));
	remove(cprintf("%s.o", entryfn));
	remove(cprintf("%s.o", exitfn));
	
	return outfn;
}

/*
 * Makes a new executable from the static library in the path given by
 * the string in. When the executable is executed, all the object files
 * from the static library are executed sequentially. This function returns
 * the path to this newly made dynamic library.
 */

char *executable(char *in) {
	char *outfn = cprintf("%s", "./exeXXXXXX");
	mkstemp(outfn);
	
	char entryfn[] = "./entryXXXXXX.s";
	FILE *entryfile = fdopen(mkstemps(entryfn, 2), "w+");
	fputs(".text\n" ".comm argc,4,4\n" ".comm argv,4,4\n" ".globl main\n" "main:\n" "pushl %esi\n" "pushl %edi\n" "pushl %ebx\n"
		"pushl %ebp\n" "movl %esp, %ebp\n" "movl 20(%ebp), %eax\n" "movl %eax, argc\n" "movl 24(%ebp), %eax\n" "movl %eax, argv\n",
		entryfile);
	if(true) {
		fputs("jmp thunk_end\n" "get_pc_thunk:\n" "movl (%esp), %ebx\n" "ret\n" "thunk_end:\n" "call get_pc_thunk\n"
			"addl $_GLOBAL_OFFSET_TABLE_, %ebx\n", entryfile);
	}
	fclose(entryfile);

	char exitfn[] = "./exitXXXXXX.s";
	FILE *exitfile = fdopen(mkstemps(exitfn, 2), "w+");
	fputs("leave\n" "popl %ebx\n" "popl %edi\n" "popl %esi\n" "movl $0, %eax\n" "ret\n", exitfile);
	fclose(exitfile);
	
	system(cprintf("gcc -m32 -g -o '%s.o' -c '%s'", entryfn, entryfn));
	remove(entryfn);
	system(cprintf("gcc -m32 -g -o '%s.o' -c '%s'", exitfn, exitfn));
	remove(exitfn);
	system(cprintf("ld -m elf_i386 -L . --allow-multiple-definition --whole-archive -o '%s' '%s.o' '%s' '%s.o'", outfn, entryfn, in,
		exitfn));
	remove(cprintf("%s.o", entryfn));
	remove(cprintf("%s.o", exitfn));
	
	return outfn;
}

#include "parser.c"