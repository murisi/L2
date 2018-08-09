bool generator_PIC;
union expression *ebp, *esp, *esi, *edi, *ebx, *ecx, *edx, *eax, *dx, *dl, *cx, *cl;

/*
 * Initializes the generator. If PIC is true, then generator will produce
 * position independent 32-bit code. Otherwise it will generate 32-bit
 * position dependent code. This function must be called before anything
 * else from this file is invoked.
 */
void generator_init(bool PIC) {
	generator_PIC = PIC;
	ebp = make_reference("ebp");
	esp = make_reference("esp");
	esi = make_reference("esi");
	edi = make_reference("edi");
	ebx = make_reference("ebx");
	ecx = make_reference("ecx");
	edx = make_reference("edx");
	eax = make_reference("eax");
	dl = make_reference("dl");
	dx = make_reference("dx");
	cl = make_reference("cl");
	cx = make_reference("cx");
}

#define CONT_SIZE 20
#define CONT_EBX 16
#define CONT_ESI 12
#define CONT_EDI 8
#define CONT_CIR 4
#define CONT_EBP 0
#define WORD_SIZE 4

union expression *vlayout_frames(union expression *n) {
	switch(n->base.type) {
		case function: {
			//Offset of parameters relative to frame pointer is 3 callee saves + frame pointer + return address 
			int parameter_offset = 20;
			union expression *t;
			{foreach(t, n->function.parameters) {
				t->reference.offset = n->function.parent ? make_constant(parameter_offset) : NULL;
				parameter_offset += WORD_SIZE;
			}}
			int local_offset = 0;
			foreach(t, reverse(n->function.locals)) {
				local_offset -= WORD_SIZE;
				t->reference.offset = n->function.parent ? make_constant(local_offset) : NULL;
			}
			break;
		} case continuation: case with: {
			if(n->continuation.escapes) {
				append(n->continuation.reference, &get_zeroth_function(n)->function.locals);
				//Make space for the buffer
				int i;
				for(i = 1; i < (CONT_SIZE / WORD_SIZE); i++) {
					append(generate_reference(), &get_zeroth_function(n)->function.locals);
				}
			}
			append_list(&get_zeroth_function(n)->function.locals, n->continuation.parameters);
			break;
		}
	}
	return n;
}

union expression *make_offset(union expression *a, int b) {
	return make_instr("+", 2, a, make_constant(b));
}

union expression *make_suffixed(union expression *ref, char *suffix) {
	return make_reference(cprintf("%s%s", ref->reference.name, suffix));
}

union expression *make_load(union expression *ref, int offset, union expression *dest_reg, union expression *scratch_reg) {
	union expression *container = make_begin();
	if(ref->reference.referent->reference.offset) {
		emit(make_instr("(movl (mem disp base) reg)", 3, make_offset(ref->reference.referent->reference.offset, offset), use(ebp),
			dest_reg));
	} else if(generator_PIC) {
		emit(make_instr("(movl (mem disp base) reg)", 3, make_suffixed(ref, "@GOT"), use(ebx), scratch_reg));
		emit(make_instr("(movl (mem disp base) reg)", 3, make_constant(offset), scratch_reg, dest_reg));
	} else {
		emit(make_instr("(movl (mem disp) reg)", 2, make_offset(ref, offset), dest_reg));
	}
	return container;
}

union expression *make_store(union expression *src_reg, union expression *ref, int offset, union expression *scratch_reg) {
	union expression *container = make_begin();
	if(ref->reference.referent->reference.offset) {
		emit(make_instr("(movl reg (mem disp base))", 3, src_reg, make_offset(ref->reference.referent->reference.offset, offset),
			use(ebp)));
	} else if(generator_PIC) {
		emit(make_instr("(movl (mem disp base) reg)", 3, make_suffixed(ref, "@GOT"), use(ebx), scratch_reg));
		emit(make_instr("(movl reg (mem disp base))", 3, src_reg, make_constant(offset), scratch_reg));
	} else {
		emit(make_instr("(movl reg (mem disp))", 2, src_reg, make_offset(ref, offset)));
	}
	return container;
}

//Must be used after use_return_reference and init_i386_registers
union expression *vgenerate_ifs(union expression *n) {
	if(n->base.type == _if) {
		union expression *container = make_begin();
		
		emit(make_load(n->_if.condition, 0, use(edx), use(esi)));
		emit(make_instr("(orl reg reg)", 2, use(edx), use(edx)));
		
		union expression *alternate_label = generate_reference();
		emit(make_instr("(je rel)", 1, alternate_label));
		emit(n->_if.consequent);
		union expression *end_label = generate_reference();
		emit(make_instr("(jmp rel)", 1, end_label));
		emit(make_instr("(label name)", 1, alternate_label));
		emit(n->_if.alternate);
		emit(make_instr("(label name)", 1, end_label));
		return container;
	} else {
		return n;
	}
}

union expression *make_load_address(union expression *ref, union expression *dest_reg) {
	union expression *container = make_begin();
	if(ref->reference.referent->reference.offset) {
		emit(make_instr("(leal (mem disp base) reg)", 3, ref->reference.referent->reference.offset, use(ebp), dest_reg));
	} else if(generator_PIC) {
		emit(make_instr("(movl (mem disp base) reg)", 3, make_suffixed(ref, "@GOT"), use(ebx), dest_reg));
	} else {
		emit(make_instr("(leal (mem disp) reg)", 2, ref, dest_reg));
	}
	return container;
}

//Must be used after use_return_reference and init_i386_registers and generate_continuation_expressions
union expression *vgenerate_references(union expression *n) {
	if(n->base.type == reference && n->reference.return_value) {
		union expression *container = make_begin();
		emit(make_load_address(n, use(ecx)));
		emit(make_store(use(ecx), n->base.return_value, 0, use(edx)));
		return container;
	} else {
		return n;
	}
}

union expression *cont_instr_ref(union expression *n) {
	return n->continuation.cont_instr_ref ? n->continuation.cont_instr_ref : (n->continuation.cont_instr_ref = generate_reference());
}

union expression *make_continuation(union expression *n) {
	union expression *container = make_begin();
	emit(make_store(use(ebx), n->continuation.reference, CONT_EBX, use(ecx)));
	emit(make_store(use(esi), n->continuation.reference, CONT_ESI, use(ecx)));
	emit(make_store(use(edi), n->continuation.reference, CONT_EDI, use(ecx)));
	emit(make_load_address(cont_instr_ref(n), use(edx)));
	emit(make_store(use(edx), n->continuation.reference, CONT_CIR, use(ecx)));
	emit(make_store(use(ebp), n->continuation.reference, CONT_EBP, use(ecx)));
	return container;
}

union expression *move_arguments(union expression *n, int offset) {
	union expression *container = make_begin();
	union expression *t;
	foreach(t, n->jump.arguments) {
		emit(make_load(t, 0, use(edx), use(esi)));
		emit(make_instr("(movl reg (mem disp base))", 3, use(edx), make_constant(offset), use(ecx)));
		offset += WORD_SIZE;
	}
	return container;
}

//Must be used after use_return_reference and init_i386_registers
union expression *vgenerate_continuation_expressions(union expression *n) {
	switch(n->base.type) {
		case continuation: {
			union expression *container = make_begin();
			emit(make_load_address(n->continuation.escapes ? n->continuation.reference : cont_instr_ref(n), use(ecx)));
			emit(make_store(use(ecx), n->continuation.return_value, 0, use(edx)));
			if(n->continuation.escapes) {
				emit(make_continuation(n));
			}
			
			//Skip the actual instructions of the continuation
			union expression *after_reference = generate_reference();
			emit(make_instr("(jmp rel)", 1, after_reference));
			emit(make_instr("(label name)", 1, cont_instr_ref(n)));
			emit(n->continuation.expression);
			emit(make_instr("(label name)", 1, after_reference));
			return container;
		} case with: {
			union expression *container = make_begin();
			if(n->with.escapes) {
				emit(make_continuation(n));
			}
			emit(n->with.expression);
			emit(make_instr("(label name)", 1, cont_instr_ref(n)));
			emit(make_load(fst(n->with.parameter), 0, use(ecx), use(edx)));
			emit(make_store(use(ecx), n->with.return_value, 0, use(edx)));
			return container;
		} case jump: {
			union expression *container = make_begin();
			if(n->jump.short_circuit) {
				if(length(n->jump.short_circuit->continuation.parameters) > 0) {
					emit(make_load_address(fst(n->jump.short_circuit->continuation.parameters), use(ecx)));
					emit(move_arguments(n, 0));
				}
				emit(make_instr("(jmp rel)", 1, cont_instr_ref(n->jump.short_circuit)));
			} else {
				emit(make_load(n->jump.reference, 0, use(ecx), use(edx)));
				emit(move_arguments(n, CONT_SIZE));
				emit(make_instr("(movl (mem disp base) reg)", 3, make_constant(CONT_EBX), use(ecx), use(ebx)));
				emit(make_instr("(movl (mem disp base) reg)", 3, make_constant(CONT_ESI), use(ecx), use(esi)));
				emit(make_instr("(movl (mem disp base) reg)", 3, make_constant(CONT_EDI), use(ecx), use(edi)));
				emit(make_instr("(movl (mem disp base) reg)", 3, make_constant(CONT_CIR), use(ecx), use(edx)));
				emit(make_instr("(movl (mem disp base) reg)", 3, make_constant(CONT_EBP), use(ecx), use(ebp)));
				emit(make_instr("(jmp reg)", 1, use(edx)));
			}
			return container;
		} default: {
			return n;
		}
	}
}

//Must be used after use_return_reference and init_i386_registers
union expression *vgenerate_constants(union expression *n) {
	if(n->base.type == constant && n->constant.return_value) {
		union expression *container = make_begin();
		emit(make_instr("(movl imm reg)", 2, make_constant(n->constant.value), use(ecx)));
		emit(make_store(use(ecx), n->constant.return_value, 0, use(esi)));
		return container;
	} else {
		return n;
	}
}

union expression *generate_toplevel(union expression *n, list toplevel_function_references) {
	union expression *container = make_begin();
	emit(make_instr("(bss)", 0));
	emit(make_instr("(align expr expr)", 2, make_constant(WORD_SIZE), make_constant(0)));
	union expression *t;
	{foreach(t, n->function.locals) {
		emit(make_instr("(label name)", 1, t));
		emit(make_instr("(skip expr expr)", 2, make_constant(WORD_SIZE), make_constant(0)));
	}}
	{foreach(t, n->function.parameters) {
		bool symbol_defined = false;
		struct library *l;
		foreach(l, compile_libraries) {
			if(dlsym(l->handle, original_name(t->reference.name))) {
				symbol_defined = true;
			}
		}
		if(!symbol_defined) {
			emit(make_instr("(global sym)", 1, t));
			emit(make_instr("(label name)", 1, t));
			emit(make_instr("(skip expr expr)", 2, make_constant(WORD_SIZE), make_constant(0)));
		}
	}}
	emit(make_instr("(text)", 0));
	foreach(t, toplevel_function_references) {
		emit(make_instr("(global sym)", 1, t));
	}
	emit(n->function.expression);
	return container;
}

int get_current_offset(union expression *function) {
	if(length(function->function.locals) > 0) {
		return ((union expression *) fst(function->function.locals))->reference.offset->constant.value;
	} else {
		return 0;
	}
}

//Must be used after all local references have been made, i.e. after make_continuations
union expression *vgenerate_function_expressions(union expression *n) {
	if(n->base.type == function && n->function.parent) {
		union expression *container = make_begin();
		emit(make_load_address(n->function.reference, use(ecx)));
		emit(make_store(use(ecx), n->function.return_value, 0, use(edx)));
		
		union expression *after_reference = generate_reference();
		
		emit(make_instr("(jmp rel)", 1, after_reference));
		emit(make_instr("(label name)", 1, n->function.reference));
		//Save callee-saved registers
		emit(make_instr("(pushl reg)", 1, use(esi)));
		emit(make_instr("(pushl reg)", 1, use(edi)));
		emit(make_instr("(pushl reg)", 1, use(ebx)));
		
		emit(make_instr("(pushl reg)", 1, use(ebp)));
		emit(make_instr("(movl reg reg)", 2, use(esp), use(ebp)));
		emit(make_instr("(subl imm reg)", 2, make_constant(-get_current_offset(n)), use(esp)));
		
		if(generator_PIC) {
			emit(make_instr("(call rel)", 1, make_reference("get_pc_thunk")));
			emit(make_instr("(addl imm reg)", 2, make_reference("_GLOBAL_OFFSET_TABLE_"), use(ebx)));
		}
		
		//Execute the function body
		emit(n->function.expression);
		
		//Place the return value
		emit(make_load(n->function.expression_return_value, 0, use(eax), use(esi)));
		
		emit(make_instr("(leave)", 0));
		//Restore callee-saved registers
		emit(make_instr("(popl reg)", 1, use(ebx)));
		emit(make_instr("(popl reg)", 1, use(edi)));
		emit(make_instr("(popl reg)", 1, use(esi)));
		
		emit(make_instr("(ret)", 0));
		emit(make_instr("(label name)", 1, after_reference));
		return container;
	} else if(n->base.type == invoke) {
		union expression *container = make_begin();
		
		//Push arguments onto stack
		union expression *t;
		foreach(t, reverse(n->invoke.arguments)) {
			emit(make_load(t, 0, use(ecx), use(edx)));
			emit(make_instr("(pushl reg)", 1, use(ecx)));
		}
		
		emit(make_load(n->invoke.reference, 0, use(ecx), use(edx)));
		emit(make_instr("(call reg)", 1, use(ecx)));
		
		emit(make_store(use(eax), n->invoke.return_value, 0, use(edx)));
		emit(make_instr("(addl imm reg)", 2, make_constant(WORD_SIZE * length(n->invoke.arguments)), use(esp)));
		return container;
	} else {
		return n;
	}
}

char *expr_to_string(union expression *n) {
	switch(n->base.type) {
		case reference: {
			return n->reference.name;
		} case constant: {
			return cprintf("%d", n->constant.value);
		} case instruction: {
			char *str = cprintf("(%s", expr_to_string(fst(n->instruction.arguments)));
			union expression *t;
			foreach(t, rst(n->instruction.arguments)) {
				str = cprintf("%s%s%s", str, n->instruction.reference->reference.name, expr_to_string(t));
			}
			return cprintf("%s)", str);
		}
	}
}

char *fst_string(union expression *n) {
	return expr_to_string(fst(n->invoke.arguments));
}

char *frst_string(union expression *n) {
	return expr_to_string(frst(n->invoke.arguments));
}

char *frrst_string(union expression *n) {
	return expr_to_string(frrst(n->invoke.arguments));
}

void print_assembly(list generated_expressions, FILE *out) {
	fputs(".text\n", out);
	if(generator_PIC) {
		fputs("jmp thunk_end\n" "get_pc_thunk:\n" "movl (%esp), %ebx\n" "ret\n" "thunk_end:\n" "call get_pc_thunk\n"
			"addl $_GLOBAL_OFFSET_TABLE_, %ebx\n", out);
	}
	union expression *n;
	foreach(n, generated_expressions) {
		char *id = n->invoke.reference->reference.name;
		if(!strcmp(id, "(label name)")) {
			fprintf(out, "%s:", fst_string(n));
		} else if(!strcmp(id, "(leal (mem disp base) reg)")) {
			fprintf(out, "leal %s(%%%s), %%%s", fst_string(n), frst_string(n), frrst_string(n));
		} else if(!strcmp(id, "(movl reg (mem disp base))")) {
			fprintf(out, "movl %%%s, %s(%%%s)", fst_string(n), frst_string(n), frrst_string(n));
		} else if(!strcmp(id, "(jmp rel)")) {
			fprintf(out, "jmp %s", fst_string(n));
		} else if(!strcmp(id, "(movl (mem disp base) reg)")) {
			fprintf(out, "movl %s(%%%s), %%%s", fst_string(n), frst_string(n), frrst_string(n));
		} else if(!strcmp(id, "(pushl reg)")) {
			fprintf(out, "pushl %%%s", fst_string(n));
		} else if(!strcmp(id, "(movl reg reg)")) {
			fprintf(out, "movl %%%s, %%%s", fst_string(n), frst_string(n));
		} else if(!strcmp(id, "(subl imm reg)")) {
			fprintf(out, "subl $%s, %%%s", fst_string(n), frst_string(n));
		} else if(!strcmp(id, "(addl imm reg)")) {
			fprintf(out, "addl $%s, %%%s", fst_string(n), frst_string(n));
		} else if(!strcmp(id, "(popl reg)")) {
			fprintf(out, "popl %%%s", fst_string(n));
		} else if(!strcmp(id, "(leave)")) {
			fprintf(out, "leave");
		} else if(!strcmp(id, "(ret)")) {
			fprintf(out, "ret");
		} else if(!strcmp(id, "(call rel)")) {
			fprintf(out, "call %s", fst_string(n));
		} else if(!strcmp(id, "(jmp reg)")) {
			fprintf(out, "jmp *%%%s", fst_string(n));
		} else if(!strcmp(id, "(je rel)")) {
			fprintf(out, "je %s", fst_string(n));
		} else if(!strcmp(id, "(jmp rel)")) {
			fprintf(out, "jmp %s", fst_string(n));
		} else if(!strcmp(id, "(orl reg reg)")) {
			fprintf(out, "orl %%%s, %%%s", fst_string(n), frst_string(n));
		} else if(!strcmp(id, "(movl imm reg)")) {
			fprintf(out, "movl $%s, %%%s", fst_string(n), frst_string(n));
		} else if(!strcmp(id, "(global sym)")) {
			fprintf(out, ".global %s", fst_string(n));
		} else if(!strcmp(id, "(movl (mem disp) reg)")) {
			fprintf(out, "movl %s, %%%s", fst_string(n), frst_string(n));
		} else if(!strcmp(id, "(movl reg (mem disp))")) {
			fprintf(out, "movl %%%s, %s", fst_string(n), frst_string(n));
		} else if(!strcmp(id, "(leal (mem disp) reg)")) {
			fprintf(out, "leal %s, %%%s", fst_string(n), frst_string(n));
		} else if(!strcmp(id, "(call reg)")) {
			fprintf(out, "call *%%%s", fst_string(n));
		} else if(!strcmp(id, "(skip expr expr)")) {
			fprintf(out, ".skip %s, %s", fst_string(n), frst_string(n));
		} else if(!strcmp(id, "(align expr expr)")) {
			fprintf(out, ".align %s, %s", fst_string(n), frst_string(n));
		} else if(!strcmp(id, "(bss)")) {
			fprintf(out, ".bss");
		} else if(!strcmp(id, "(text)")) {
			fprintf(out, ".text");
		}
		fprintf(out, "\n");
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
		"movl %esp, %ebp\n" "movl 20(%ebp), %eax\n" "movl %eax, argc\n" "movl 24(%ebp), %eax\n" "movl %eax, argv\n", entry_file);
	if(generator_PIC) {
		fputs("jmp thunk_end\n" "get_pc_thunk:\n" "movl (%esp), %ebx\n" "ret\n" "thunk_end:\n" "call get_pc_thunk\n"
			"addl $_GLOBAL_OFFSET_TABLE_, %ebx\n", entryfile);
	}
	
	fclose(entryfile);

	char exitfn[] = ".exitXXXXXX.s";
	FILE *exitfile = fdopen(mkstemps(exitfn, 2), "w+");
	fputs("leave\n" "popl %ebx\n" "popl %edi\n" "popl %esi\n" "movl $0, %eax\n" "ret\n", exitfile);
	fclose(exitfile);
	
	system(cprintf("musl-gcc -m32 -g -o '%s.o' -c '%s'", entryfn, entryfn));
	remove(entryfn);
	system(cprintf("musl-gcc -m32 -g -o '%s.o' -c '%s'", exitfn, exitfn));
	remove(exitfn);
	
	char *library_path_string = "";
	struct library *lib;
	foreach(lib, compile_libraries) {
		library_path_string = cprintf("%s \"%s\"", library_path_string, lib->path);
	}
	system(cprintf("musl-gcc -m32 -pie -Wl,-E -o '%s' '%s.o' '%s' '%s.o' %s 2>&1 | grep -v \"type and size of dynamic symbol\" 1>&2",
		outbin, entryfn, in, exitfn, library_path_string));
	remove(cprintf("%s.o", entryfn));
	remove(cprintf("%s.o", exitfn));
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
	
	generator_init(true);
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
	system(cprintf("musl-gcc -m32 -g -c -o '%s' '%s'", ofilefn, sfilefn));
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
	remove(ofilefn);
}

#undef WORD_SIZE
