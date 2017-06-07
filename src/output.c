char *to_command_line_args(list strs) {
	int args_length = 0;
	char *tmp;
	{foreach(tmp, strs) {
		args_length += strlen(tmp) + 4;
	}}
	char *clas = calloc(args_length + 1, sizeof(char));
	clas[0] = '\0';
	foreach(tmp, strs) {
		strcat(clas, " '");
		strcat(clas, tmp);
		strcat(clas, "' ");
	}
	return clas;
}

list make_shared_library_object_files;

void make_shared_library(bool PIC, char *sofile) {
	char entryfn[] = "./entryXXXXXX.s";
	FILE *entryfile = fdopen(mkstemps(entryfn, 2), "w+");
	fputs(".section .init_array,\"aw\"\n" ".align 4\n" ".long main\n" ".text\n" "main:\n" "pushl %ebp\n" "movl %esp, %ebp\n"
		"pushl %esi\n" "pushl %edi\n" "pushl %ebx\n", entryfile);
	if(PIC) {
		fputs("jmp thunk_end\n" "get_pc_thunk:\n" "movl (%esp), %ebx\n" "ret\n" "thunk_end:\n" "call get_pc_thunk\n"
			"addl $_GLOBAL_OFFSET_TABLE_, %ebx\n", entryfile);
	}
	fclose(entryfile);

	char exitfn[] = "./exitXXXXXX.s";
	FILE *exitfile = fdopen(mkstemps(exitfn, 2), "w+");
	fputs("popl %ebx\n" "popl %edi\n" "popl %esi\n" "movl $0, %eax\n" "leave\n" "ret\n", exitfile);
	fclose(exitfile);
	
	system(cprintf("gcc -m32 -g -o '%s.o' -c '%s'", entryfn, entryfn));
	system(cprintf("gcc -m32 -g -o '%s.o' -c '%s'", exitfn, exitfn));
	system(cprintf("gcc -m32 -g -shared -o '%s' '%s.o' %s '%s.o'", sofile, entryfn,
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
	fputs(".text\n" ".comm argc,4,4\n" ".comm argv,4,4\n" ".globl main\n" "main:\n" "pushl %ebp\n" "movl %esp, %ebp\n"
		"movl 8(%ebp), %eax\n" "movl %eax, argc\n" "movl 12(%ebp), %eax\n" "movl %eax, argv\n" "pushl %esi\n" "pushl %edi\n"
		"pushl %ebx\n", entryfile);
	if(PIC) {
		fputs("jmp thunk_end\n" "get_pc_thunk:\n" "movl (%esp), %ebx\n" "ret\n" "thunk_end:\n" "call get_pc_thunk\n"
			"addl $_GLOBAL_OFFSET_TABLE_, %ebx\n", entryfile);
	}
	fclose(entryfile);

	char exitfn[] = "./exitXXXXXX.s";
	FILE *exitfile = fdopen(mkstemps(exitfn, 2), "w+");
	fputs("popl %ebx\n" "popl %edi\n" "popl %esi\n" "movl $0, %eax\n" "leave\n" "ret\n", exitfile);
	fclose(exitfile);
	
	system(cprintf("gcc -m32 -g -o '%s.o' -c '%s'", entryfn, entryfn));
	system(cprintf("gcc -m32 -g -o '%s.o' -c '%s'", exitfn, exitfn));
	system(cprintf("gcc -m32 -g -o '%s' '%s.o' %s '%s.o'", outfile, entryfn, to_command_line_args(reverse(make_program_object_files)),
		exitfn));
	remove(entryfn);
	remove(cprintf("%s.o", entryfn));
	remove(exitfn);
	remove(cprintf("%s.o", exitfn));
}

bool equals(void *a, void *b) { return a == b; }

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
	visit_expressions_visitor = &vfind_multiple_definitions;
	program = visit_expressions(program);

	vlink_references_program = program; //Static argument to following function
	vlink_references_handler = handler;
	visit_expressions_visitor = &vlink_references;
	program = visit_expressions(program);
	
	visit_expressions_visitor = &vblacklist_references;
	program = visit_expressions(program);
	vrename_definition_references_name_records = nil();
	visit_expressions_visitor = &vrename_definition_references;
	program = visit_expressions(program);
	visit_expressions_visitor = &vrename_usage_references;
	program = visit_expressions(program);
	
	visit_expressions_visitor = &vescape_analysis;
	program = visit_expressions(program);

	program = use_return_value(program, generate_reference());

	generator_PIC = PIC;
	
	visit_expressions_visitor = &vlayout_frames;
	program = visit_expressions(program);

	visit_expressions_visitor = &vgenerate_references;
	program = visit_expressions(program);

	visit_expressions_visitor = &vgenerate_continuation_expressions;
	program = visit_expressions(program);

	visit_expressions_visitor = &vgenerate_constants;
	program = visit_expressions(program);

	visit_expressions_visitor = &vgenerate_ifs;
	program = visit_expressions(program);
	
	visit_expressions_visitor = &vgenerate_function_expressions;
	program = visit_expressions(program);

	program = generate_toplevel(program, toplevel_function_references);

	visit_expressions_visitor = &vmerge_begins;
	program = visit_expressions(program);
	
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
	return ofilefn;
}

char *dynamic_load(list exprs, jmp_buf *handler) {
	char *sofilefn = cprintf("%s", "./so_fileXXXXXX.so");
	int sodes = mkstemps(sofilefn, 3);
	
	char *ofilefn = compile(exprs, true, handler);
	prepend(ofilefn, &make_shared_library_object_files);
	make_shared_library(true, sofilefn);
	make_shared_library_object_files = rst(make_shared_library_object_files);
	remove(ofilefn);
	return sofilefn;
}