bool generator_PIC;

#define RAX 0
#define RCX 1
#define RDX 2
#define RBX 3
#define RSP 4
#define RBP 5
#define RSI 6
#define RDI 7
#define R8 8
#define R9 9
#define R10 10
#define R11 11
#define R12 12
#define R13 13
#define R14 14
#define R15 15
#define RIP 16

#define LEAQ_OF_MDB_INTO_REG 17
#define MOVQ_FROM_REG_INTO_MDB 18
#define JMP_REL 19
#define MOVQ_MDB_TO_REG 20
#define PUSHQ_REG 21
#define MOVQ_REG_TO_REG 22
#define SUBQ_IMM_FROM_REG 23
#define ADDQ_IMM_TO_REG 24
#define POPQ_REG 25
#define LEAVE 26
#define RET 27
#define CALL_REL 28
#define JMP_TO_REG 29
#define JE_REL 30
#define ORQ_REG_TO_REG 31
#define MOVQ_IMM_TO_REG 32
#define CALL_REG 34
#define LABEL 37
#define GLOBAL 38
#define SKIP_EXPR_EXPR 39
#define ALIGN_EXPR_EXPR 40
#define BSS 41
#define TEXT 42
#define ADD 43

/*
 * Initializes the generator. If PIC is true, then generator will produce
 * position independent 64-bit code. Otherwise it will generate 64-bit
 * position dependent code. This function must be called before anything
 * else from this file is invoked.
 */
void generator_init(bool PIC) {
	generator_PIC = PIC;
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
				t->reference.offset = n->function.parent ? make_literal(parameter_offset) : NULL;
				parameter_offset += WORD_SIZE;
			}}
			int local_offset = 0;
			foreach(t, reverse(n->function.locals)) {
				local_offset -= WORD_SIZE;
				t->reference.offset = n->function.parent ? make_literal(local_offset) : NULL;
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

union expression *make_suffixed(union expression *ref, char *suffix) {
	return make_reference(cprintf("%s%s", ref->reference.name, suffix));
}

union expression *make_load(union expression *ref, int offset, union expression *dest_reg, union expression *scratch_reg) {
	union expression *container = make_begin();
	if(ref->reference.referent->reference.offset) {
		emit(make_instr(MOVQ_MDB_TO_REG, 3, make_literal(ref->reference.referent->reference.offset->literal.value + offset), use(RBP),
			dest_reg));
	} else if(generator_PIC) {
		emit(make_instr(MOVQ_MDB_TO_REG, 3, make_suffixed(ref, "@GOTPCREL"), use(RIP), scratch_reg));
		emit(make_instr(MOVQ_MDB_TO_REG, 3, make_literal(offset), scratch_reg, dest_reg));
	} else {
		emit(make_instr(MOVQ_IMM_TO_REG, 2, make_instr(ADD, 2, ref, make_literal(offset)), scratch_reg));
		emit(make_instr(MOVQ_MDB_TO_REG, 3, make_literal(0), scratch_reg, dest_reg));
	}
	return container;
}

union expression *make_store(union expression *src_reg, union expression *ref, int offset, union expression *scratch_reg) {
	union expression *container = make_begin();
	if(ref->reference.referent->reference.offset) {
		emit(make_instr(MOVQ_FROM_REG_INTO_MDB, 3,
			src_reg, make_literal(ref->reference.referent->reference.offset->literal.value + offset), use(RBP)));
	} else if(generator_PIC) {
		emit(make_instr(MOVQ_MDB_TO_REG, 3, make_suffixed(ref, "@GOTPCREL"), use(RIP), scratch_reg));
		emit(make_instr(MOVQ_FROM_REG_INTO_MDB, 3, src_reg, make_literal(offset), scratch_reg));
	} else {
		emit(make_instr(MOVQ_IMM_TO_REG, 2, make_instr(ADD, 2, ref, make_literal(offset)), scratch_reg));
		emit(make_instr(MOVQ_FROM_REG_INTO_MDB, 3, src_reg, make_literal(0), scratch_reg));
	}
	return container;
}

//Must be used after use_return_reference and init_i386_registers
union expression *vgenerate_ifs(union expression *n) {
	if(n->base.type == _if) {
		union expression *container = make_begin();
		
		emit(make_load(n->_if.condition, 0, use(R10), use(R13)));
		emit(make_instr(ORQ_REG_TO_REG, 2, use(R10), use(R10)));
		
		union expression *alternate_label = generate_reference();
		emit(make_instr(JE_REL, 1, alternate_label));
		emit(n->_if.consequent);
		union expression *end_label = generate_reference();
		emit(make_instr(JMP_REL, 1, end_label));
		emit(make_instr(LABEL, 1, alternate_label));
		emit(n->_if.alternate);
		emit(make_instr(LABEL, 1, end_label));
		return container;
	} else {
		return n;
	}
}

union expression *make_load_address(union expression *ref, union expression *dest_reg) {
	union expression *container = make_begin();
	if(ref->reference.referent->reference.offset) {
		emit(make_instr(LEAQ_OF_MDB_INTO_REG, 3, ref->reference.referent->reference.offset, use(RBP), dest_reg));
	} else if(generator_PIC) {
		emit(make_instr(MOVQ_MDB_TO_REG, 3, make_suffixed(ref, "@GOTPCREL"), use(RIP), dest_reg));
	} else {
		emit(make_instr(MOVQ_IMM_TO_REG, 2, ref, dest_reg));
	}
	return container;
}

//Must be used after use_return_reference and init_i386_registers and generate_continuation_expressions
union expression *vgenerate_references(union expression *n) {
	if(n->base.type == reference && n->reference.return_value) {
		union expression *container = make_begin();
		emit(make_load_address(n, use(R11)));
		emit(make_store(use(R11), n->base.return_value, 0, use(R10)));
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
	emit(make_store(use(RBX), n->continuation.reference, CONT_RBX, use(R11)));
	emit(make_store(use(R12), n->continuation.reference, CONT_R12, use(R11)));
	emit(make_store(use(R13), n->continuation.reference, CONT_R13, use(R11)));
	emit(make_store(use(R14), n->continuation.reference, CONT_R14, use(R11)));
	emit(make_store(use(R15), n->continuation.reference, CONT_R15, use(R11)));
	emit(make_load_address(cont_instr_ref(n), use(R10)));
	emit(make_store(use(R10), n->continuation.reference, CONT_CIR, use(R11)));
	emit(make_store(use(RBP), n->continuation.reference, CONT_RBP, use(R11)));
	return container;
}

union expression *move_arguments(union expression *n, int offset) {
	union expression *container = make_begin();
	union expression *t;
	foreach(t, n->jump.arguments) {
		emit(make_load(t, 0, use(R10), use(R13)));
		emit(make_instr(MOVQ_FROM_REG_INTO_MDB, 3, use(R10), make_literal(offset), use(R11)));
		offset += WORD_SIZE;
	}
	return container;
}

//Must be used after use_return_reference and init_i386_registers
union expression *vgenerate_continuation_expressions(union expression *n) {
	switch(n->base.type) {
		case continuation: {
			union expression *container = make_begin();
			emit(make_load_address(n->continuation.escapes ? n->continuation.reference : cont_instr_ref(n), use(R11)));
			emit(make_store(use(R11), n->continuation.return_value, 0, use(R10)));
			if(n->continuation.escapes) {
				emit(make_continuation(n));
			}
			
			//Skip the actual instructions of the continuation
			union expression *after_reference = generate_reference();
			emit(make_instr(JMP_REL, 1, after_reference));
			emit(make_instr(LABEL, 1, cont_instr_ref(n)));
			emit(n->continuation.expression);
			emit(make_instr(LABEL, 1, after_reference));
			return container;
		} case with: {
			union expression *container = make_begin();
			if(n->with.escapes) {
				emit(make_continuation(n));
			}
			emit(n->with.expression);
			emit(make_instr(LABEL, 1, cont_instr_ref(n)));
			emit(make_load(fst(n->with.parameter), 0, use(R11), use(R10)));
			emit(make_store(use(R11), n->with.return_value, 0, use(R10)));
			return container;
		} case jump: {
			union expression *container = make_begin();
			if(n->jump.short_circuit) {
				if(length(n->jump.short_circuit->continuation.parameters) > 0) {
					emit(make_load_address(fst(n->jump.short_circuit->continuation.parameters), use(R11)));
					emit(move_arguments(n, 0));
				}
				emit(make_instr(JMP_REL, 1, cont_instr_ref(n->jump.short_circuit)));
			} else {
				emit(make_load(n->jump.reference, 0, use(R11), use(R10)));
				emit(move_arguments(n, CONT_SIZE));
				emit(make_instr(MOVQ_MDB_TO_REG, 3, make_literal(CONT_RBX), use(R11), use(RBX)));
				emit(make_instr(MOVQ_MDB_TO_REG, 3, make_literal(CONT_R12), use(R11), use(R12)));
				emit(make_instr(MOVQ_MDB_TO_REG, 3, make_literal(CONT_R13), use(R11), use(R13)));
				emit(make_instr(MOVQ_MDB_TO_REG, 3, make_literal(CONT_R14), use(R11), use(R14)));
				emit(make_instr(MOVQ_MDB_TO_REG, 3, make_literal(CONT_R15), use(R11), use(R15)));
				emit(make_instr(MOVQ_MDB_TO_REG, 3, make_literal(CONT_CIR), use(R11), use(R10)));
				emit(make_instr(MOVQ_MDB_TO_REG, 3, make_literal(CONT_RBP), use(R11), use(RBP)));
				emit(make_instr(JMP_TO_REG, 1, use(R10)));
			}
			return container;
		} default: {
			return n;
		}
	}
}

//Must be used after use_return_reference and init_i386_registers
union expression *vgenerate_literals(union expression *n) {
	if(n->base.type == literal && n->literal.return_value) {
		union expression *container = make_begin();
		emit(make_instr(MOVQ_IMM_TO_REG, 2, make_literal(n->literal.value), use(R11)));
		emit(make_store(use(R11), n->literal.return_value, 0, use(R13)));
		return container;
	} else {
		return n;
	}
}

union expression *generate_toplevel(union expression *n, list toplevel_function_references) {
	union expression *container = make_begin();
	emit(make_instr(BSS, 0));
	emit(make_instr(ALIGN_EXPR_EXPR, 2, make_literal(WORD_SIZE), make_literal(0)));
	union expression *t;
	{foreach(t, n->function.locals) {
		emit(make_instr(LABEL, 1, t));
		emit(make_instr(SKIP_EXPR_EXPR, 2, make_literal(WORD_SIZE), make_literal(0)));
	}}
	emit(make_instr(TEXT, 0));
	foreach(t, toplevel_function_references) {
		emit(make_instr(GLOBAL, 1, t));
	}
	emit(make_instr(PUSHQ_REG, 1, use(R12)));
	emit(make_instr(PUSHQ_REG, 1, use(R13)));
	emit(make_instr(PUSHQ_REG, 1, use(R14)));
	emit(make_instr(PUSHQ_REG, 1, use(R15)));
	emit(make_instr(PUSHQ_REG, 1, use(RBX)));
	emit(make_instr(PUSHQ_REG, 1, use(RBP)));
	emit(make_instr(MOVQ_REG_TO_REG, 2, use(RSP), use(RBP)));
	emit(n->function.expression);
	emit(make_instr(LEAVE, 0));
	emit(make_instr(POPQ_REG, 1, use(RBX)));
	emit(make_instr(POPQ_REG, 1, use(R15)));
	emit(make_instr(POPQ_REG, 1, use(R14)));
	emit(make_instr(POPQ_REG, 1, use(R13)));
	emit(make_instr(POPQ_REG, 1, use(R12)));
	emit(make_instr(RET, 0));
	return container;
}

int get_current_offset(union expression *function) {
	if(length(function->function.locals) > 0) {
		return ((union expression *) fst(function->function.locals))->reference.offset->literal.value;
	} else {
		return 0;
	}
}

//Must be used after all local references have been made, i.e. after make_continuations
union expression *vgenerate_function_expressions(union expression *n) {
	if(n->base.type == function && n->function.parent) {
		union expression *container = make_begin();
		emit(make_load_address(n->function.reference, use(R11)));
		emit(make_store(use(R11), n->function.return_value, 0, use(R10)));
		
		union expression *after_reference = generate_reference();
		
		emit(make_instr(JMP_REL, 1, after_reference));
		emit(make_instr(LABEL, 1, n->function.reference));
		
		//Insert first 6 parameters onto stack
		emit(make_instr(POPQ_REG, 1, use(R11)));
		emit(make_instr(PUSHQ_REG, 1, use(R9)));
		emit(make_instr(PUSHQ_REG, 1, use(R8)));
		emit(make_instr(PUSHQ_REG, 1, use(RCX)));
		emit(make_instr(PUSHQ_REG, 1, use(RDX)));
		emit(make_instr(PUSHQ_REG, 1, use(RSI)));
		emit(make_instr(PUSHQ_REG, 1, use(RDI)));
		emit(make_instr(PUSHQ_REG, 1, use(R11)));
		
		//Save callee-saved registers
		emit(make_instr(PUSHQ_REG, 1, use(R12)));
		emit(make_instr(PUSHQ_REG, 1, use(R13)));
		emit(make_instr(PUSHQ_REG, 1, use(R14)));
		emit(make_instr(PUSHQ_REG, 1, use(R15)));
		emit(make_instr(PUSHQ_REG, 1, use(RBX)));
		
		emit(make_instr(PUSHQ_REG, 1, use(RBP)));
		emit(make_instr(MOVQ_REG_TO_REG, 2, use(RSP), use(RBP)));
		emit(make_instr(SUBQ_IMM_FROM_REG, 2, make_literal(-get_current_offset(n)), use(RSP)));
		
		//Execute the function body
		emit(n->function.expression);
		
		//Place the return value
		emit(make_load(n->function.expression_return_value, 0, use(RAX), use(R13)));
		
		emit(make_instr(LEAVE, 0));
		//Restore callee-saved registers
		emit(make_instr(POPQ_REG, 1, use(RBX)));
		emit(make_instr(POPQ_REG, 1, use(R15)));
		emit(make_instr(POPQ_REG, 1, use(R14)));
		emit(make_instr(POPQ_REG, 1, use(R13)));
		emit(make_instr(POPQ_REG, 1, use(R12)));
		
		emit(make_instr(POPQ_REG, 1, use(R11)));
		emit(make_instr(ADDQ_IMM_TO_REG, 2, make_literal(6*WORD_SIZE), use(RSP)));
		emit(make_instr(PUSHQ_REG, 1, use(R11)));
		emit(make_instr(RET, 0));
		emit(make_instr(LABEL, 1, after_reference));
		return container;
	} else if(n->base.type == invoke) {
		union expression *container = make_begin();
		
		//Push arguments onto stack
		if(length(n->invoke.arguments) > 6) {
			union expression *t;
			foreach(t, reverse(rrrrrrst(n->invoke.arguments))) {
				emit(make_load(t, 0, use(R11), use(R10)));
				emit(make_instr(PUSHQ_REG, 1, use(R11)));
			}
		}
		if(length(n->invoke.arguments) > 5) {
			emit(make_load(frrrrrst(n->invoke.arguments), 0, use(R9), use(R10)));
		}
		if(length(n->invoke.arguments) > 4) {
			emit(make_load(frrrrst(n->invoke.arguments), 0, use(R8), use(R10)));
		}
		if(length(n->invoke.arguments) > 3) {
			emit(make_load(frrrst(n->invoke.arguments), 0, use(RCX), use(R10)));
		}
		if(length(n->invoke.arguments) > 2) {
			emit(make_load(frrst(n->invoke.arguments), 0, use(RDX), use(R10)));
		}
		if(length(n->invoke.arguments) > 1) {
			emit(make_load(frst(n->invoke.arguments), 0, use(RSI), use(R10)));
		}
		if(length(n->invoke.arguments) > 0) {
			emit(make_load(fst(n->invoke.arguments), 0, use(RDI), use(R10)));
		}
		emit(make_instr(MOVQ_IMM_TO_REG, 2, make_literal(0), use(RAX)));
		
		emit(make_load(n->invoke.reference, 0, use(R11), use(R10)));
		emit(make_instr(CALL_REG, 1, use(R11)));
		
		emit(make_store(use(RAX), n->invoke.return_value, 0, use(R10)));
		if(length(n->invoke.arguments) > 6) {
			emit(make_instr(ADDQ_IMM_TO_REG, 2, make_literal(WORD_SIZE * (length(n->invoke.arguments) - 6)), use(RSP)));
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
		} case literal: {
			return cprintf("%ld", n->literal.value);
		} case instruction: {
			switch(n->instruction.opcode) {
				case ADD: return cprintf("(%s+%s)", expr_to_string(fst(n->invoke.arguments)),
					expr_to_string(frst(n->invoke.arguments)));
				case RBP: return "%rbp";
				case RSP: return "%rsp";
				case R12: return "%r12";
				case R13: return "%r13";
				case R14: return "%r14";
				case R15: return "%r15";
				case RIP: return "%rip";
				case R11: return "%r11";
				case R10: return "%r10";
				case RAX: return "%rax";
				case R9: return "%r9";
				case R8: return "%r8";
				case RDI: return "%rdi";
				case RSI: return "%rsi";
				case RDX: return "%rdx";
				case RCX: return "%rcx";
				case RBX: return "%rbx";
			}
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
	union expression *n;
	foreach(n, generated_expressions) {
		switch(n->instruction.opcode) {
			case LABEL:
				fprintf(out, "%s:", fst_string(n));
				break;
			case LEAQ_OF_MDB_INTO_REG:
				fprintf(out, "leaq %s(%s), %s", fst_string(n), frst_string(n), frrst_string(n));
				break;
			case MOVQ_FROM_REG_INTO_MDB:
				fprintf(out, "movq %s, %s(%s)", fst_string(n), frst_string(n), frrst_string(n));
				break;
			case JMP_REL:
				fprintf(out, "jmp %s", fst_string(n));
				break;
			case MOVQ_MDB_TO_REG:
				fprintf(out, "movq %s(%s), %s", fst_string(n), frst_string(n), frrst_string(n));
				break;
			case PUSHQ_REG:
				fprintf(out, "pushq %s", fst_string(n));
				break;
			case MOVQ_REG_TO_REG:
				fprintf(out, "movq %s, %s", fst_string(n), frst_string(n));
				break;
			case SUBQ_IMM_FROM_REG:
				fprintf(out, "subq $%s, %s", fst_string(n), frst_string(n));
				break;
			case ADDQ_IMM_TO_REG:
				fprintf(out, "addq $%s, %s", fst_string(n), frst_string(n));
				break;
			case POPQ_REG:
				fprintf(out, "popq %s", fst_string(n));
				break;
			case LEAVE:
				fprintf(out, "leave");
				break;
			case RET:
				fprintf(out, "ret");
				break;
			case CALL_REL:
				fprintf(out, "call %s", fst_string(n));
				break;
			case JMP_TO_REG:
				fprintf(out, "jmp *%s", fst_string(n));
				break;
			case JE_REL:
				fprintf(out, "je %s", fst_string(n));
				break;
			case ORQ_REG_TO_REG:
				fprintf(out, "orq %s, %s", fst_string(n), frst_string(n));
				break;
			case MOVQ_IMM_TO_REG:
				fprintf(out, "movq $%s, %s", fst_string(n), frst_string(n));
				break;
			case GLOBAL:
				fprintf(out, ".global %s", fst_string(n));
				break;
			case CALL_REG:
				fprintf(out, "call *%s", fst_string(n));
				break;
			case SKIP_EXPR_EXPR:
				fprintf(out, ".skip %s, %s", fst_string(n), frst_string(n));
				break;
			case ALIGN_EXPR_EXPR:
				fprintf(out, ".align %s, %s", fst_string(n), frst_string(n));
				break;
			case BSS:
				fprintf(out, ".bss");
				break;
			case TEXT:
				fprintf(out, ".text");
				break;
		}
		fprintf(out, "\n");
	}
}

/*
 * Makes a new binary at the path outbin from the object file in the path
 * given by the string in. This binary will both be a static library and
 * an executable.
 */

#include "x86_64_assembler.c"

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
	
	generator_init(false);
	visit_expressions_with(&program, vlayout_frames);
	visit_expressions_with(&program, vgenerate_references);
	visit_expressions_with(&program, vgenerate_continuation_expressions);
	visit_expressions_with(&program, vgenerate_literals);
	visit_expressions_with(&program, vgenerate_ifs);
	visit_expressions_with(&program, vgenerate_function_expressions);
	program = generate_toplevel(program, toplevel_function_references);
	visit_expressions_with(&program, vmerge_begins);
	
	char sfilefn[] = ".s_fileXXXXXX.s";
	FILE *sfile = fdopen(mkstemps(sfilefn, 2), "w+");
	print_assembly(program->begin.expressions, sfile);
	assemble(program->begin.expressions, sfile);
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
	system(cprintf("ar rcs '%s' '%s'", outbin, ofilefn));
	remove(ofilefn);
}

#undef WORD_SIZE
