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
#define JMP_TO_REG 28
#define JE_REL 29
#define ORQ_REG_TO_REG 30
#define MOVQ_IMM_TO_REG 31
#define CALL_REG 32
#define LABEL 33
#define STVAL_ADD_OFF_TO_REF 34
#define STVAL_SUB_RIP_FROM_REF 35

#define CONT_SIZE (7*WORD_SIZE)
#define CONT_R15 (6*WORD_SIZE)
#define CONT_R12 (5*WORD_SIZE)
#define CONT_RBX (4*WORD_SIZE)
#define CONT_R13 (3*WORD_SIZE)
#define CONT_R14 (2*WORD_SIZE)
#define CONT_CIR (1*WORD_SIZE)
#define CONT_RBP (0*WORD_SIZE)

union expression *vlayout_frames(union expression *n, region r) {
	switch(n->base.type) {
		case function: {
			//Offset of parameters relative to frame pointer is 6 callee saves + return address 
			int parameter_offset = 7*WORD_SIZE;
			union expression *t;
			{foreach(t, n->function.parameters) {
				t->reference.symbol->offset = parameter_offset;
				parameter_offset += WORD_SIZE;
			}}
			int symbol_offset = 0;
			struct symbol *u;
			foreach(u, reverse(n->function.symbols, r)) {
				symbol_offset -= pad_size(u->size, WORD_SIZE);
				u->offset = symbol_offset;
			}
			break;
		} case continuation: case with: {
			union expression *parent_function = get_parent_function(n);
			if(true) {
				n->continuation.reference->reference.symbol->size = CONT_SIZE;
				append(n->continuation.reference->reference.symbol, &parent_function->function.symbols, r);
			}
			union expression *t;
			foreach(t, n->continuation.parameters) {
				t->reference.symbol->size = WORD_SIZE;
				append(t->reference.symbol, &parent_function->function.symbols, r);
			}
			break;
		} case storage: {
			n->storage.reference->reference.symbol->size = length(n->storage.arguments) * WORD_SIZE;
			union expression *parent_function = get_parent_function(n);
			append(n->storage.reference->reference.symbol, &parent_function->function.symbols, r);
			break;
		}
	}
	return n;
}

void make_load(struct symbol *sym, int offset, union expression *dest_reg, union expression *scratch_reg, list *c, region r) {
	if(sym->type == dynamic_storage) {
		prepend(make_asm3(MOVQ_MDB_TO_REG, make_literal(sym->offset + offset, r), make_asm0(RBP, r), dest_reg, r), c, r);
	} else {
		prepend(make_asm2(MOVQ_IMM_TO_REG, make_asm2(STVAL_ADD_OFF_TO_REF, use_symbol(sym, r), make_literal(offset, r), r), scratch_reg,
			r), c, r);
		prepend(make_asm3(MOVQ_MDB_TO_REG, make_literal(0, r), scratch_reg, dest_reg, r), c, r);
	}
}

void make_store(union expression *src_reg, struct symbol *sym, int offset, union expression *scratch_reg, list *c, region r) {
	if(sym->type == dynamic_storage) {
		prepend(make_asm3(MOVQ_FROM_REG_INTO_MDB, src_reg, make_literal(sym->offset + offset, r), make_asm0(RBP, r), r), c, r);
	} else {
		prepend(make_asm2(MOVQ_IMM_TO_REG, make_asm2(STVAL_ADD_OFF_TO_REF, use_symbol(sym, r), make_literal(offset, r), r), scratch_reg,
			r), c, r);
		prepend(make_asm3(MOVQ_FROM_REG_INTO_MDB, src_reg, make_literal(0, r), scratch_reg, r), c, r);
	}
}

void generate_expressions(union expression *n, list *c, region r);

void sgenerate_ifs(union expression *n, list *c, region r) {
	generate_expressions(n->_if.condition, c, r);
	prepend(make_asm2(ORQ_REG_TO_REG, make_asm0(RAX, r), make_asm0(RAX, r), r), c, r);
	
	struct symbol *alternate_symbol = make_symbol(static_storage, local_scope, defined_state, NULL, NULL, r);
	prepend(make_asm1(JE_REL, make_asm1(STVAL_SUB_RIP_FROM_REF, use_symbol(alternate_symbol, r), r), r), c, r);
	generate_expressions(n->_if.consequent, c, r);
	struct symbol *end_symbol = make_symbol(static_storage, local_scope, defined_state, NULL, NULL, r);
	prepend(make_asm1(JMP_REL, make_asm1(STVAL_SUB_RIP_FROM_REF, use_symbol(end_symbol, r), r), r), c, r);
	prepend(make_asm1(LABEL, use_symbol(alternate_symbol, r), r), c, r);
	generate_expressions(n->_if.alternate, c, r);
	prepend(make_asm1(LABEL, use_symbol(end_symbol, r), r), c, r);
}

void make_load_address(struct symbol *sym, union expression *dest_reg, list *c, region r) {
	if(sym->type == dynamic_storage) {
		prepend(make_asm3(LEAQ_OF_MDB_INTO_REG, make_literal(sym->offset, r), make_asm0(RBP, r), dest_reg, r), c, r);
	} else {
		prepend(make_asm2(MOVQ_IMM_TO_REG, use_symbol(sym, r), dest_reg, r), c, r);
	}
}

void sgenerate_storage_expressions(union expression *n, list *c, region r) {
	int offset = 0;
	union expression *t;
	foreach(t, n->storage.arguments) {
		generate_expressions(t, c, r);
		make_store(make_asm0(RAX, r), n->storage.reference->reference.symbol, offset, make_asm0(R13, r), c, r);
		offset += WORD_SIZE;
	}
	make_load_address(n->storage.reference->reference.symbol, make_asm0(RAX, r), c, r);
}

void sgenerate_references(union expression *n, list *c, region r) {
	make_load_address(n->reference.symbol, make_asm0(RAX, r), c, r);
}

void make_store_continuation(union expression *n, list *c, region r) {
	make_store(make_asm0(RBX, r), n->continuation.reference->reference.symbol, CONT_RBX, make_asm0(R11, r), c, r);
	make_store(make_asm0(R12, r), n->continuation.reference->reference.symbol, CONT_R12, make_asm0(R11, r), c, r);
	make_store(make_asm0(R13, r), n->continuation.reference->reference.symbol, CONT_R13, make_asm0(R11, r), c, r);
	make_store(make_asm0(R14, r), n->continuation.reference->reference.symbol, CONT_R14, make_asm0(R11, r), c, r);
	make_store(make_asm0(R15, r), n->continuation.reference->reference.symbol, CONT_R15, make_asm0(R11, r), c, r);
	make_load_address(n->continuation.cont_instr_ref->reference.symbol, make_asm0(R10, r), c, r);
	make_store(make_asm0(R10, r), n->continuation.reference->reference.symbol, CONT_CIR, make_asm0(R11, r), c, r);
	make_store(make_asm0(RBP, r), n->continuation.reference->reference.symbol, CONT_RBP, make_asm0(R11, r), c, r);
}

void move_arguments(union expression *n, int offset, list *c, region r) {
	prepend(make_asm1(PUSHQ_REG, make_asm0(R11, r), r), c, r);
	union expression *t;
	{foreach(t, reverse(n->jump.arguments, r)) {
		generate_expressions(t, c, r);
		prepend(make_asm1(PUSHQ_REG, make_asm0(RAX, r), r), c, r);
	}}
	prepend(make_asm3(MOVQ_MDB_TO_REG, make_literal(length(n->jump.arguments) * WORD_SIZE, r), make_asm0(RSP, r), make_asm0(R11, r), r), c, r);
	foreach(t, n->jump.arguments) {
		prepend(make_asm1(POPQ_REG, make_asm0(RAX, r), r), c, r);
		prepend(make_asm3(MOVQ_FROM_REG_INTO_MDB, make_asm0(RAX, r), make_literal(offset, r), make_asm0(R11, r), r), c, r);
		offset += WORD_SIZE;
	}
	prepend(make_asm1(POPQ_REG, make_asm0(R11, r), r), c, r);
}

void sgenerate_continuations(union expression *n, list *c, region r) {
	if(n->continuation.escapes) {
		make_load_address(n->continuation.reference->reference.symbol, make_asm0(RAX, r), c, r);
		make_store_continuation(n, c, r);
	}
	
	//Skip the actual instructions of the continuation
	struct symbol *after_symbol = make_symbol(static_storage, local_scope, defined_state, NULL, NULL, r);
	prepend(make_asm1(JMP_REL, make_asm1(STVAL_SUB_RIP_FROM_REF, use_symbol(after_symbol, r), r), r), c, r);
	prepend(make_asm1(LABEL, n->continuation.cont_instr_ref, r), c, r);
	generate_expressions(n->continuation.expression, c, r);
	prepend(make_asm1(LABEL, use_symbol(after_symbol, r), r), c, r);
}

void sgenerate_withs(union expression *n, list *c, region r) {
	if(n->with.escapes) {
		make_store_continuation(n, c, r);
	}
	generate_expressions(n->with.expression, c, r);
	prepend(make_asm1(LABEL, n->continuation.cont_instr_ref, r), c, r);
	make_load(((union expression *) n->with.parameter->fst)->reference.symbol, 0, make_asm0(RAX, r), make_asm0(R10, r), c, r);
}

void sgenerate_jumps(union expression *n, list *c, region r) {
	generate_expressions(n->jump.reference, c, r);
	if(n->jump.short_circuit) {
		if(length(n->jump.short_circuit->continuation.parameters) > 0) {
			make_load_address(((union expression *) n->jump.short_circuit->continuation.parameters->fst)->reference.symbol,
				make_asm0(R11, r), c, r);
			move_arguments(n, 0, c, r);
		}
		prepend(make_asm1(JMP_REL, make_asm1(STVAL_SUB_RIP_FROM_REF, n->jump.short_circuit->continuation.cont_instr_ref, r), r), c, r);
	} else {
		prepend(make_asm2(MOVQ_REG_TO_REG, make_asm0(RAX, r), make_asm0(R11, r), r), c, r);
		move_arguments(n, CONT_SIZE, c, r);
		prepend(make_asm3(MOVQ_MDB_TO_REG, make_literal(CONT_RBX, r), make_asm0(R11, r), make_asm0(RBX, r), r), c, r);
		prepend(make_asm3(MOVQ_MDB_TO_REG, make_literal(CONT_R12, r), make_asm0(R11, r), make_asm0(R12, r), r), c, r);
		prepend(make_asm3(MOVQ_MDB_TO_REG, make_literal(CONT_R13, r), make_asm0(R11, r), make_asm0(R13, r), r), c, r);
		prepend(make_asm3(MOVQ_MDB_TO_REG, make_literal(CONT_R14, r), make_asm0(R11, r), make_asm0(R14, r), r), c, r);
		prepend(make_asm3(MOVQ_MDB_TO_REG, make_literal(CONT_R15, r), make_asm0(R11, r), make_asm0(R15, r), r), c, r);
		prepend(make_asm3(MOVQ_MDB_TO_REG, make_literal(CONT_CIR, r), make_asm0(R11, r), make_asm0(R10, r), r), c, r);
		prepend(make_asm3(MOVQ_MDB_TO_REG, make_literal(CONT_RBP, r), make_asm0(R11, r), make_asm0(RBP, r), r), c, r);
		prepend(make_asm1(JMP_TO_REG, make_asm0(R10, r), r), c, r);
	}
}

void sgenerate_literals(union expression *n, list *c, region r) {
	prepend(make_asm2(MOVQ_IMM_TO_REG, make_literal(n->literal.value, r), make_asm0(RAX, r), r), c, r);
}

int get_current_offset(union expression *function) {
	if(length(function->function.symbols) > 0) {
		return ((struct symbol *) function->function.symbols->fst)->offset;
	} else {
		return 0;
	}
}

void sgenerate_functions(union expression *n, list *c, region r) {
	make_load_address(n->function.reference->reference.symbol, make_asm0(RAX, r), c, r);
	
	struct symbol *after_symbol = make_symbol(static_storage, local_scope, defined_state, NULL, NULL, r);
	
	prepend(make_asm1(JMP_REL, make_asm1(STVAL_SUB_RIP_FROM_REF, use_symbol(after_symbol, r), r), r), c, r);
	prepend(make_asm1(LABEL, n->function.reference, r), c, r);
	
	prepend(make_asm1(POPQ_REG, make_asm0(R11, r), r), c, r);
	
	//Insert first 6 parameters onto stack
	prepend(make_asm1(PUSHQ_REG, make_asm0(R9, r), r), c, r);
	prepend(make_asm1(PUSHQ_REG, make_asm0(R8, r), r), c, r);
	prepend(make_asm1(PUSHQ_REG, make_asm0(RCX, r), r), c, r);
	prepend(make_asm1(PUSHQ_REG, make_asm0(RDX, r), r), c, r);
	prepend(make_asm1(PUSHQ_REG, make_asm0(RSI, r), r), c, r);
	prepend(make_asm1(PUSHQ_REG, make_asm0(RDI, r), r), c, r);
	
	prepend(make_asm1(PUSHQ_REG, make_asm0(R11, r), r), c, r);
	
	//Save callee-saved registers
	prepend(make_asm1(PUSHQ_REG, make_asm0(R12, r), r), c, r);
	prepend(make_asm1(PUSHQ_REG, make_asm0(R13, r), r), c, r);
	prepend(make_asm1(PUSHQ_REG, make_asm0(R14, r), r), c, r);
	prepend(make_asm1(PUSHQ_REG, make_asm0(R15, r), r), c, r);
	prepend(make_asm1(PUSHQ_REG, make_asm0(RBX, r), r), c, r);
	
	prepend(make_asm1(PUSHQ_REG, make_asm0(RBP, r), r), c, r);
	prepend(make_asm2(MOVQ_REG_TO_REG, make_asm0(RSP, r), make_asm0(RBP, r), r), c, r);
	prepend(make_asm2(SUBQ_IMM_FROM_REG, make_literal(-get_current_offset(n), r), make_asm0(RSP, r), r), c, r);
	
	//Execute the function body
	generate_expressions(n->function.expression, c, r);
	
	prepend(make_asm0(LEAVE, r), c, r);
	//Restore callee-saved registers
	prepend(make_asm1(POPQ_REG, make_asm0(RBX, r), r), c, r);
	prepend(make_asm1(POPQ_REG, make_asm0(R15, r), r), c, r);
	prepend(make_asm1(POPQ_REG, make_asm0(R14, r), r), c, r);
	prepend(make_asm1(POPQ_REG, make_asm0(R13, r), r), c, r);
	prepend(make_asm1(POPQ_REG, make_asm0(R12, r), r), c, r);
	
	prepend(make_asm1(POPQ_REG, make_asm0(R11, r), r), c, r);
	prepend(make_asm2(ADDQ_IMM_TO_REG, make_literal(6*WORD_SIZE, r), make_asm0(RSP, r), r), c, r);
	prepend(make_asm1(PUSHQ_REG, make_asm0(R11, r), r), c, r);
	prepend(make_asm0(RET, r), c, r);
	prepend(make_asm1(LABEL, use_symbol(after_symbol, r), r), c, r);
}

void sgenerate_invokes(union expression *n, list *c, region r) {
	//Push arguments onto stack
	union expression *t;
	foreach(t, reverse(n->invoke.arguments, r)) {
		generate_expressions(t, c, r);
		prepend(make_asm1(PUSHQ_REG, make_asm0(RAX, r), r), c, r);
	}
	generate_expressions(n->invoke.reference, c, r);
	prepend(make_asm2(MOVQ_REG_TO_REG, make_asm0(RAX, r), make_asm0(R11, r), r), c, r);
	
	if(length(n->invoke.arguments) > 0) {
		prepend(make_asm1(POPQ_REG, make_asm0(RDI, r), r), c, r);
	}
	if(length(n->invoke.arguments) > 1) {
		prepend(make_asm1(POPQ_REG, make_asm0(RSI, r), r), c, r);
	}
	if(length(n->invoke.arguments) > 2) {
		prepend(make_asm1(POPQ_REG, make_asm0(RDX, r), r), c, r);
	}
	if(length(n->invoke.arguments) > 3) {
		prepend(make_asm1(POPQ_REG, make_asm0(RCX, r), r), c, r);
	}
	if(length(n->invoke.arguments) > 4) {
		prepend(make_asm1(POPQ_REG, make_asm0(R8, r), r), c, r);
	}
	if(length(n->invoke.arguments) > 5) {
		prepend(make_asm1(POPQ_REG, make_asm0(R9, r), r), c, r);
	}
	
	prepend(make_asm2(MOVQ_IMM_TO_REG, make_literal(0, r), make_asm0(RAX, r), r), c, r);
	
	prepend(make_asm1(CALL_REG, make_asm0(R11, r), r), c, r);
	
	if(length(n->invoke.arguments) > 6) {
		prepend(make_asm2(ADDQ_IMM_TO_REG, make_literal(WORD_SIZE * (length(n->invoke.arguments) - 6), r), make_asm0(RSP, r), r), c, r);
	}
}

void sgenerate_begins(union expression *n, list *c, region r) {
	union expression *t;
	foreach(t, n->begin.expressions) {
		generate_expressions(t, c, r);
	}
}

void generate_expressions(union expression *n, list *c, region r) {
	switch(n->base.type) {
		case begin: {
			sgenerate_begins(n, c, r);
			break;
		} case continuation: {
			sgenerate_continuations(n, c, r);
			break;
		} case with: {
			sgenerate_withs(n, c, r);
			break;
		} case jump: {
			sgenerate_jumps(n, c, r);
			break;
		} case reference: {
			sgenerate_references(n, c, r);
			break;
		} case storage: {
			sgenerate_storage_expressions(n, c, r);
			break;
		} case _if: {
			sgenerate_ifs(n, c, r);
			break;
		} case literal: {
			sgenerate_literals(n, c, r);
			break;
		} case function: {
			sgenerate_functions(n, c, r);
			break;
		} case invoke: {
			sgenerate_invokes(n, c, r);
			break;
		}
	}
}

list generate_program(union expression *n, list *symbols, region r) {
	*symbols = n->function.symbols;
	union expression *l;
	{foreach(l, n->function.parameters) { prepend(l->reference.symbol, symbols, r); }}
	
	list c = nil;
	prepend(make_asm1(PUSHQ_REG, make_asm0(R12, r), r), &c, r);
	prepend(make_asm1(PUSHQ_REG, make_asm0(R13, r), r), &c, r);
	prepend(make_asm1(PUSHQ_REG, make_asm0(R14, r), r), &c, r);
	prepend(make_asm1(PUSHQ_REG, make_asm0(R15, r), r), &c, r);
	prepend(make_asm1(PUSHQ_REG, make_asm0(RBX, r), r), &c, r);
	prepend(make_asm1(PUSHQ_REG, make_asm0(RBP, r), r), &c, r);
	prepend(make_asm2(MOVQ_REG_TO_REG, make_asm0(RSP, r), make_asm0(RBP, r), r), &c, r);
	generate_expressions(n->function.expression, &c, r);
	prepend(make_asm0(LEAVE, r), &c, r);
	prepend(make_asm1(POPQ_REG, make_asm0(RBX, r), r), &c, r);
	prepend(make_asm1(POPQ_REG, make_asm0(R15, r), r), &c, r);
	prepend(make_asm1(POPQ_REG, make_asm0(R14, r), r), &c, r);
	prepend(make_asm1(POPQ_REG, make_asm0(R13, r), r), &c, r);
	prepend(make_asm1(POPQ_REG, make_asm0(R12, r), r), &c, r);
	prepend(make_asm0(RET, r), &c, r);
	return reverse(c, r);
}

