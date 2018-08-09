bool generator_PIC;
union expression *rbp, *rsp, *r12, *r13, *r14, *r15, *rip, *r11, *r10, *rax, *r9, *r8, *rdi, *rsi, *rdx, *rcx, *rbx;

/*
 * Initializes the generator. If PIC is true, then generator will produce
 * position independent 64-bit code. Otherwise it will generate 64-bit
 * position dependent code. This function must be called before anything
 * else from this file is invoked.
 */
void generator_init(bool PIC) {
	generator_PIC = PIC;
	rbp = make_reference("rbp");
	rsp = make_reference("rsp");
	rip = make_reference("rip");
	rdi = make_reference("rdi");
	rsi = make_reference("rsi");
	rdx = make_reference("rdx");
	rcx = make_reference("rcx");
	r8 = make_reference("r8");
	r9 = make_reference("r9");
	r10 = make_reference("r10");
	r11 = make_reference("r11");
	r12 = make_reference("r12");
	r13 = make_reference("r13");
	r14 = make_reference("r14");
	r15 = make_reference("r15");
	rax = make_reference("rax");
	rbx = make_reference("rbx");
}

#define WORD_SIZE 8
#define CONT_SIZE (7*WORD_SIZE)
#define CONT_R15 (6*WORD_SIZE)
#define CONT_R12 (5*WORD_SIZE)
#define CONT_RBX (4*WORD_SIZE)
#define CONT_R13 (3*WORD_SIZE)
#define CONT_R14 (2*WORD_SIZE)
#define CONT_CIR (1*WORD_SIZE)
#define CONT_RBP (0*WORD_SIZE)

union expression *vlayout_frames(union expression *n) {
	switch(n->base.type) {
		case function: {
			//Offset of parameters relative to frame pointer is 6 callee saves + return address 
			int parameter_offset = 7*WORD_SIZE;
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
		emit(make_instr("(movq (mem disp base) reg)", 3, make_offset(ref->reference.referent->reference.offset, offset), use(rbp),
			dest_reg));
	} else if(generator_PIC) {
		emit(make_instr("(movq (mem disp base) reg)", 3, make_suffixed(ref, "@GOTPCREL"), use(rip), scratch_reg));
		emit(make_instr("(movq (mem disp base) reg)", 3, make_constant(offset), scratch_reg, dest_reg));
	} else {
		emit(make_instr("(movq (mem disp) reg)", 2, make_offset(ref, offset), dest_reg));
	}
	return container;
}

union expression *make_store(union expression *src_reg, union expression *ref, int offset, union expression *scratch_reg) {
	union expression *container = make_begin();
	if(ref->reference.referent->reference.offset) {
		emit(make_instr("(movq reg (mem disp base))", 3, src_reg, make_offset(ref->reference.referent->reference.offset, offset),
			use(rbp)));
	} else if(generator_PIC) {
		emit(make_instr("(movq (mem disp base) reg)", 3, make_suffixed(ref, "@GOTPCREL"), use(rip), scratch_reg));
		emit(make_instr("(movq reg (mem disp base))", 3, src_reg, make_constant(offset), scratch_reg));
	} else {
		emit(make_instr("(movq reg (mem disp))", 2, src_reg, make_offset(ref, offset)));
	}
	return container;
}

//Must be used after use_return_reference and init_i386_registers
union expression *vgenerate_ifs(union expression *n) {
	if(n->base.type == _if) {
		union expression *container = make_begin();
		
		emit(make_load(n->_if.condition, 0, use(r10), use(r13)));
		emit(make_instr("(orq reg reg)", 2, use(r10), use(r10)));
		
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
		emit(make_instr("(leaq (mem disp base) reg)", 3, ref->reference.referent->reference.offset, use(rbp), dest_reg));
	} else if(generator_PIC) {
		emit(make_instr("(movq (mem disp base) reg)", 3, make_suffixed(ref, "@GOTPCREL"), use(rip), dest_reg));
	} else {
		emit(make_instr("(leaq (mem disp) reg)", 2, ref, dest_reg));
	}
	return container;
}

//Must be used after use_return_reference and init_i386_registers and generate_continuation_expressions
union expression *vgenerate_references(union expression *n) {
	if(n->base.type == reference && n->reference.return_value) {
		union expression *container = make_begin();
		emit(make_load_address(n, use(r11)));
		emit(make_store(use(r11), n->base.return_value, 0, use(r10)));
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
	emit(make_store(use(rbx), n->continuation.reference, CONT_RBX, use(r11)));
	emit(make_store(use(r12), n->continuation.reference, CONT_R12, use(r11)));
	emit(make_store(use(r13), n->continuation.reference, CONT_R13, use(r11)));
	emit(make_store(use(r14), n->continuation.reference, CONT_R14, use(r11)));
	emit(make_store(use(r15), n->continuation.reference, CONT_R15, use(r11)));
	emit(make_load_address(cont_instr_ref(n), use(r10)));
	emit(make_store(use(r10), n->continuation.reference, CONT_CIR, use(r11)));
	emit(make_store(use(rbp), n->continuation.reference, CONT_RBP, use(r11)));
	return container;
}

union expression *move_arguments(union expression *n, int offset) {
	union expression *container = make_begin();
	union expression *t;
	foreach(t, n->jump.arguments) {
		emit(make_load(t, 0, use(r10), use(r13)));
		emit(make_instr("(movq reg (mem disp base))", 3, use(r10), make_constant(offset), use(r11)));
		offset += WORD_SIZE;
	}
	return container;
}

//Must be used after use_return_reference and init_i386_registers
union expression *vgenerate_continuation_expressions(union expression *n) {
	switch(n->base.type) {
		case continuation: {
			union expression *container = make_begin();
			emit(make_load_address(n->continuation.escapes ? n->continuation.reference : cont_instr_ref(n), use(r11)));
			emit(make_store(use(r11), n->continuation.return_value, 0, use(r10)));
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
			emit(make_load(fst(n->with.parameter), 0, use(r11), use(r10)));
			emit(make_store(use(r11), n->with.return_value, 0, use(r10)));
			return container;
		} case jump: {
			union expression *container = make_begin();
			if(n->jump.short_circuit) {
				if(length(n->jump.short_circuit->continuation.parameters) > 0) {
					emit(make_load_address(fst(n->jump.short_circuit->continuation.parameters), use(r11)));
					emit(move_arguments(n, 0));
				}
				emit(make_instr("(jmp rel)", 1, cont_instr_ref(n->jump.short_circuit)));
			} else {
				emit(make_load(n->jump.reference, 0, use(r11), use(r10)));
				emit(move_arguments(n, CONT_SIZE));
				emit(make_instr("(movq (mem disp base) reg)", 3, make_constant(CONT_RBX), use(r11), use(rbx)));
				emit(make_instr("(movq (mem disp base) reg)", 3, make_constant(CONT_R12), use(r11), use(r12)));
				emit(make_instr("(movq (mem disp base) reg)", 3, make_constant(CONT_R13), use(r11), use(r13)));
				emit(make_instr("(movq (mem disp base) reg)", 3, make_constant(CONT_R14), use(r11), use(r14)));
				emit(make_instr("(movq (mem disp base) reg)", 3, make_constant(CONT_R15), use(r11), use(r15)));
				emit(make_instr("(movq (mem disp base) reg)", 3, make_constant(CONT_CIR), use(r11), use(r10)));
				emit(make_instr("(movq (mem disp base) reg)", 3, make_constant(CONT_RBP), use(r11), use(rbp)));
				emit(make_instr("(jmp reg)", 1, use(r10)));
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
		emit(make_instr("(movq imm reg)", 2, make_constant(n->constant.value), use(r11)));
		emit(make_store(use(r11), n->constant.return_value, 0, use(r13)));
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
			if(dlsym(l->handle, t->reference.source_name)) {
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
		emit(make_load_address(n->function.reference, use(r11)));
		emit(make_store(use(r11), n->function.return_value, 0, use(r10)));
		
		union expression *after_reference = generate_reference();
		
		emit(make_instr("(jmp rel)", 1, after_reference));
		emit(make_instr("(label name)", 1, n->function.reference));
		
		//Insert first 6 parameters onto stack
		emit(make_instr("(popq reg)", 1, use(r11)));
		emit(make_instr("(pushq reg)", 1, use(r9)));
		emit(make_instr("(pushq reg)", 1, use(r8)));
		emit(make_instr("(pushq reg)", 1, use(rcx)));
		emit(make_instr("(pushq reg)", 1, use(rdx)));
		emit(make_instr("(pushq reg)", 1, use(rsi)));
		emit(make_instr("(pushq reg)", 1, use(rdi)));
		emit(make_instr("(pushq reg)", 1, use(r11)));
		
		//Save callee-saved registers
		emit(make_instr("(pushq reg)", 1, use(r12)));
		emit(make_instr("(pushq reg)", 1, use(r13)));
		emit(make_instr("(pushq reg)", 1, use(r14)));
		emit(make_instr("(pushq reg)", 1, use(r15)));
		emit(make_instr("(pushq reg)", 1, use(rbx)));
		
		emit(make_instr("(pushq reg)", 1, use(rbp)));
		emit(make_instr("(movq reg reg)", 2, use(rsp), use(rbp)));
		emit(make_instr("(subq imm reg)", 2, make_constant(-get_current_offset(n)), use(rsp)));
		
		//Execute the function body
		emit(n->function.expression);
		
		//Place the return value
		emit(make_load(n->function.expression_return_value, 0, use(rax), use(r13)));
		
		emit(make_instr("(leave)", 0));
		//Restore callee-saved registers
		emit(make_instr("(popq reg)", 1, use(rbx)));
		emit(make_instr("(popq reg)", 1, use(r15)));
		emit(make_instr("(popq reg)", 1, use(r14)));
		emit(make_instr("(popq reg)", 1, use(r13)));
		emit(make_instr("(popq reg)", 1, use(r12)));
		
		emit(make_instr("(popq reg)", 1, use(r11)));
		emit(make_instr("(addq imm reg)", 2, make_constant(6*WORD_SIZE), use(rsp)));
		emit(make_instr("(pushq reg)", 1, use(r11)));
		emit(make_instr("(ret)", 0));
		emit(make_instr("(label name)", 1, after_reference));
		return container;
	} else if(n->base.type == invoke) {
		union expression *container = make_begin();
		
		//Push arguments onto stack
		if(length(n->invoke.arguments) > 6) {
			union expression *t;
			foreach(t, reverse(rrrrrrst(n->invoke.arguments))) {
				emit(make_load(t, 0, use(r11), use(r10)));
				emit(make_instr("(pushq reg)", 1, use(r11)));
			}
		}
		if(length(n->invoke.arguments) > 5) {
			emit(make_load(frrrrrst(n->invoke.arguments), 0, use(r9), use(r10)));
		}
		if(length(n->invoke.arguments) > 4) {
			emit(make_load(frrrrst(n->invoke.arguments), 0, use(r8), use(r10)));
		}
		if(length(n->invoke.arguments) > 3) {
			emit(make_load(frrrst(n->invoke.arguments), 0, use(rcx), use(r10)));
		}
		if(length(n->invoke.arguments) > 2) {
			emit(make_load(frrst(n->invoke.arguments), 0, use(rdx), use(r10)));
		}
		if(length(n->invoke.arguments) > 1) {
			emit(make_load(frst(n->invoke.arguments), 0, use(rsi), use(r10)));
		}
		if(length(n->invoke.arguments) > 0) {
			emit(make_load(fst(n->invoke.arguments), 0, use(rdi), use(r10)));
		}
		emit(make_instr("(movq imm reg)", 2, make_constant(0), use(rax)));
		
		emit(make_load(n->invoke.reference, 0, use(r11), use(r10)));
		emit(make_instr("(call reg)", 1, use(r11)));
		
		emit(make_store(use(rax), n->invoke.return_value, 0, use(r10)));
		if(length(n->invoke.arguments) > 6) {
			emit(make_instr("(addq imm reg)", 2, make_constant(WORD_SIZE * (length(n->invoke.arguments) - 6)), use(rsp)));
		}
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
	union expression *n;
	foreach(n, generated_expressions) {
		char *id = n->invoke.reference->reference.name;
		if(!strcmp(id, "(label name)")) {
			fprintf(out, "%s:", fst_string(n));
		} else if(!strcmp(id, "(leaq (mem disp base) reg)")) {
			fprintf(out, "leaq %s(%%%s), %%%s", fst_string(n), frst_string(n), frrst_string(n));
		} else if(!strcmp(id, "(movq reg (mem disp base))")) {
			fprintf(out, "movq %%%s, %s(%%%s)", fst_string(n), frst_string(n), frrst_string(n));
		} else if(!strcmp(id, "(jmp rel)")) {
			fprintf(out, "jmp %s", fst_string(n));
		} else if(!strcmp(id, "(movq (mem disp base) reg)")) {
			fprintf(out, "movq %s(%%%s), %%%s", fst_string(n), frst_string(n), frrst_string(n));
		} else if(!strcmp(id, "(pushq reg)")) {
			fprintf(out, "pushq %%%s", fst_string(n));
		} else if(!strcmp(id, "(movq reg reg)")) {
			fprintf(out, "movq %%%s, %%%s", fst_string(n), frst_string(n));
		} else if(!strcmp(id, "(subq imm reg)")) {
			fprintf(out, "subq $%s, %%%s", fst_string(n), frst_string(n));
		} else if(!strcmp(id, "(addq imm reg)")) {
			fprintf(out, "addq $%s, %%%s", fst_string(n), frst_string(n));
		} else if(!strcmp(id, "(popq reg)")) {
			fprintf(out, "popq %%%s", fst_string(n));
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
		} else if(!strcmp(id, "(orq reg reg)")) {
			fprintf(out, "orq %%%s, %%%s", fst_string(n), frst_string(n));
		} else if(!strcmp(id, "(movq imm reg)")) {
			fprintf(out, "movq $%s, %%%s", fst_string(n), frst_string(n));
		} else if(!strcmp(id, "(global sym)")) {
			fprintf(out, ".global %s", fst_string(n));
		} else if(!strcmp(id, "(movq (mem disp) reg)")) {
			fprintf(out, "movq %s, %%%s", fst_string(n), frst_string(n));
		} else if(!strcmp(id, "(movq reg (mem disp))")) {
			fprintf(out, "movq %%%s, %s", fst_string(n), frst_string(n));
		} else if(!strcmp(id, "(leaq (mem disp) reg)")) {
			fprintf(out, "leaq %s, %%%s", fst_string(n), frst_string(n));
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
	fputs(".section .init_array,\"aw\"\n" ".align 8\n" ".quad privmain\n" ".text\n" ".globl main\n" "main:\n" "ret\n" "privmain:\n"
		"pushq %r12\n" "pushq %r13\n" "pushq %r14\n" "pushq %r15\n" "pushq %rbx\n" "pushq %rbp\n" "movq %rsp, %rbp\n", entryfile);
	fclose(entryfile);

	char exitfn[] = ".exitXXXXXX.s";
	FILE *exitfile = fdopen(mkstemps(exitfn, 2), "w+");
	fputs("leave\n" "popq %rbx\n" "popq %r15\n" "popq %r14\n" "popq %r13\n" "popq %r12\n" "movq $0, %rax\n" "ret\n", exitfile);
	fclose(exitfile);
	
	system(cprintf("musl-gcc -m64 -g -o '%s.o' -c '%s'", entryfn, entryfn));
	remove(entryfn);
	system(cprintf("musl-gcc -m64 -g -o '%s.o' -c '%s'", exitfn, exitfn));
	remove(exitfn);
	
	char *library_path_string = "";
	struct library *lib;
	foreach(lib, compile_libraries) {
		library_path_string = cprintf("%s \"%s\"", library_path_string, lib->path);
	}
	system(cprintf("musl-gcc -m64 -pie -Wl,-E -o '%s' '%s.o' '%s' '%s.o' %s 2>&1 | grep -v \"type and size of dynamic symbol\" 1>&2",
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
	union expression *root_function = make_function(), *program = root_function;
	root_function->function.reference->reference.source_name = generate_string();
	put(program, function.expression, container);
	
	vfind_multiple_definitions_handler = handler;
	visit_expressions_with(&program, vfind_multiple_definitions);

	vlink_references_program = program; //Static argument to following function
	vlink_references_handler = handler;
	visit_expressions_with(&program, vlink_references);
	
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
	system(cprintf("musl-gcc -m64 -g -c -o '%s' '%s'", ofilefn, sfilefn));
	remove(sfilefn);
	fclose(sfile);
	
	char sympairsfn[] = ".sym_pairsXXXXXX";
	FILE *sympairsfile = fdopen(mkstemp(sympairsfn), "w+");
	union expression *r;
	{foreach(r, toplevel_function_references) {
		fprintf(sympairsfile, "%s %s\n", r->reference.name, r->reference.source_name);
	}}
	foreach(r, root_function->function.parameters) {
		fprintf(sympairsfile, "%s %s\n", r->reference.name, r->reference.source_name);
	}
	fclose(sympairsfile);
	system(cprintf("objcopy --redefine-syms='%s' '%s'", sympairsfn, ofilefn));
	remove(sympairsfn);
	compile_object(outbin, ofilefn, handler);
	remove(ofilefn);
}

#undef WORD_SIZE
