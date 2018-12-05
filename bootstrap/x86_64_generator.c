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

#define LEAQ_MDB_TO_REG 17
#define MOVQ_REG_TO_MDB 18
#define JMP_REL 19
#define MOVQ_MDB_TO_REG 20
#define PUSHQ_REG 21
#define MOVQ_REG_TO_REG 22
#define SUBQ_IMM_TO_REG 23
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
#define LNKR_ADD_OFF_TO_REF 34
#define LNKR_SUB_RIP_TO_REF 35

#define CONT_SIZE (7*WORD_SIZE)
#define CONT_R15 (6*WORD_SIZE)
#define CONT_R12 (5*WORD_SIZE)
#define CONT_RBX (4*WORD_SIZE)
#define CONT_R13 (3*WORD_SIZE)
#define CONT_R14 (2*WORD_SIZE)
#define CONT_CIR (1*WORD_SIZE)
#define CONT_RBP (0*WORD_SIZE)

union expression *vlayout_frames(union expression *n, buffer r) {
	switch(n->base.type) {
		case function: {
			//Offset of parameters relative to frame pointer is 1 callee saves + return address 
			int parameter_offset = 2*WORD_SIZE;
			union expression *t;
			{foreach(t, n->function.parameters) {
				t->reference.binding_aug->offset = parameter_offset;
				parameter_offset += WORD_SIZE;
			}}
			int binding_aug_offset = 0;
			struct binding_aug *u;
			foreach(u, reverse(n->function.binding_augs, r)) {
				binding_aug_offset -= pad_size(u->size, WORD_SIZE);
				u->offset = binding_aug_offset;
			}
			break;
		} case continuation: case with: {
			union expression *parent_function = get_parent_function(n);
			if(n->continuation.escapes) {
				n->continuation.reference->reference.binding_aug->size = CONT_SIZE;
				append(n->continuation.reference->reference.binding_aug, &parent_function->function.binding_augs, r);
			}
			union expression *t;
			foreach(t, n->continuation.parameters) {
				t->reference.binding_aug->size = WORD_SIZE;
				append(t->reference.binding_aug, &parent_function->function.binding_augs, r);
			}
			break;
		} case storage: {
			n->storage.reference->reference.binding_aug->size = length(n->storage.arguments) * WORD_SIZE;
			union expression *parent_function = get_parent_function(n);
			append(n->storage.reference->reference.binding_aug, &parent_function->function.binding_augs, r);
			break;
		}
	}
	return n;
}

void make_load(struct binding_aug *sym, int offset, union expression *dest_reg, union expression *scratch_reg, list *c, buffer r) {
	if(sym->type == dynamic_storage) {
		prepend(make_asm3(MOVQ_MDB_TO_REG, make_literal(sym->offset + offset, r), make_asm0(RBP, r), dest_reg, r), c, r);
	} else {
		prepend(make_asm2(MOVQ_IMM_TO_REG, make_asm2(LNKR_ADD_OFF_TO_REF, use_binding(sym, r), make_literal(offset, r), r), scratch_reg,
			r), c, r);
		prepend(make_asm3(MOVQ_MDB_TO_REG, make_literal(0, r), scratch_reg, dest_reg, r), c, r);
	}
}

void make_store(union expression *src_reg, struct binding_aug *sym, int offset, union expression *scratch_reg, list *c, buffer r) {
	if(sym->type == dynamic_storage) {
		prepend(make_asm3(MOVQ_REG_TO_MDB, src_reg, make_literal(sym->offset + offset, r), make_asm0(RBP, r), r), c, r);
	} else {
		prepend(make_asm2(MOVQ_IMM_TO_REG, make_asm2(LNKR_ADD_OFF_TO_REF, use_binding(sym, r), make_literal(offset, r), r), scratch_reg,
			r), c, r);
		prepend(make_asm3(MOVQ_REG_TO_MDB, src_reg, make_literal(0, r), scratch_reg, r), c, r);
	}
}

void generate_expressions(union expression *n, list *c, buffer r);

void sgenerate_ifs(union expression *n, list *c, buffer r) {
	generate_expressions(n->_if.condition, c, r);
	prepend(make_asm2(ORQ_REG_TO_REG, make_asm0(RAX, r), make_asm0(RAX, r), r), c, r);
	
	struct binding_aug *alternate_binding = make_binding_aug(static_storage, local_scope, defined_state, NULL, NULL, r);
	prepend(make_asm1(JE_REL, make_asm1(LNKR_SUB_RIP_TO_REF, use_binding(alternate_binding, r), r), r), c, r);
	generate_expressions(n->_if.consequent, c, r);
	struct binding_aug *end_binding = make_binding_aug(static_storage, local_scope, defined_state, NULL, NULL, r);
	prepend(make_asm1(JMP_REL, make_asm1(LNKR_SUB_RIP_TO_REF, use_binding(end_binding, r), r), r), c, r);
	prepend(make_asm1(LABEL, use_binding(alternate_binding, r), r), c, r);
	generate_expressions(n->_if.alternate, c, r);
	prepend(make_asm1(LABEL, use_binding(end_binding, r), r), c, r);
}

void make_load_address(struct binding_aug *sym, union expression *dest_reg, list *c, buffer r) {
	if(sym->type == dynamic_storage) {
		prepend(make_asm3(LEAQ_MDB_TO_REG, make_literal(sym->offset, r), make_asm0(RBP, r), dest_reg, r), c, r);
	} else {
		prepend(make_asm2(MOVQ_IMM_TO_REG, use_binding(sym, r), dest_reg, r), c, r);
	}
}

void sgenerate_storage_expressions(union expression *n, list *c, buffer r) {
	int offset = 0;
	union expression *t;
	foreach(t, n->storage.arguments) {
		generate_expressions(t, c, r);
		make_store(make_asm0(RAX, r), n->storage.reference->reference.binding_aug, offset, make_asm0(R11, r), c, r);
		offset += WORD_SIZE;
	}
	make_load_address(n->storage.reference->reference.binding_aug, make_asm0(RAX, r), c, r);
}

bool equals(void *a, void *b) {
	return a == b;
}

void sgenerate_references(union expression *n, list *c, buffer r) {
	union expression *def = n->reference.binding_aug->definition;
	if((def->reference.parent->base.type == function || def->reference.parent->base.type == continuation) &&
		exists(equals, &def->reference.parent->function.parameters, def)) {
			make_load(n->reference.binding_aug, 0, make_asm0(RAX, r), make_asm0(R11, r), c, r);
	} else {
		make_load_address(n->reference.binding_aug, make_asm0(RAX, r), c, r);
	}
}

void make_store_continuation(union expression *n, list *c, buffer r) {
	make_store(make_asm0(RBX, r), n->continuation.reference->reference.binding_aug, CONT_RBX, make_asm0(R11, r), c, r);
	make_store(make_asm0(R12, r), n->continuation.reference->reference.binding_aug, CONT_R12, make_asm0(R11, r), c, r);
	make_store(make_asm0(R13, r), n->continuation.reference->reference.binding_aug, CONT_R13, make_asm0(R11, r), c, r);
	make_store(make_asm0(R14, r), n->continuation.reference->reference.binding_aug, CONT_R14, make_asm0(R11, r), c, r);
	make_store(make_asm0(R15, r), n->continuation.reference->reference.binding_aug, CONT_R15, make_asm0(R11, r), c, r);
	make_load_address(n->continuation.cont_instr_ref->reference.binding_aug, make_asm0(R10, r), c, r);
	make_store(make_asm0(R10, r), n->continuation.reference->reference.binding_aug, CONT_CIR, make_asm0(R11, r), c, r);
	make_store(make_asm0(RBP, r), n->continuation.reference->reference.binding_aug, CONT_RBP, make_asm0(R11, r), c, r);
}

void move_arguments(union expression *n, int offset, list *c, buffer r) {
	prepend(make_asm1(PUSHQ_REG, make_asm0(R11, r), r), c, r);
	union expression *t;
	{foreach(t, reverse(n->jump.arguments, r)) {
		generate_expressions(t, c, r);
		prepend(make_asm1(PUSHQ_REG, make_asm0(RAX, r), r), c, r);
	}}
	prepend(make_asm3(MOVQ_MDB_TO_REG, make_literal(length(n->jump.arguments) * WORD_SIZE, r), make_asm0(RSP, r), make_asm0(R11, r), r), c, r);
	foreach(t, n->jump.arguments) {
		prepend(make_asm1(POPQ_REG, make_asm0(RAX, r), r), c, r);
		prepend(make_asm3(MOVQ_REG_TO_MDB, make_asm0(RAX, r), make_literal(offset, r), make_asm0(R11, r), r), c, r);
		offset += WORD_SIZE;
	}
	prepend(make_asm1(POPQ_REG, make_asm0(R11, r), r), c, r);
}

void sgenerate_continuations(union expression *n, list *c, buffer r) {
	if(n->continuation.escapes) {
		make_load_address(n->continuation.reference->reference.binding_aug, make_asm0(RAX, r), c, r);
		make_store_continuation(n, c, r);
	}
	
	//Skip the actual instructions of the continuation
	struct binding_aug *after_binding = make_binding_aug(static_storage, local_scope, defined_state, NULL, NULL, r);
	prepend(make_asm1(JMP_REL, make_asm1(LNKR_SUB_RIP_TO_REF, use_binding(after_binding, r), r), r), c, r);
	prepend(make_asm1(LABEL, n->continuation.cont_instr_ref, r), c, r);
	generate_expressions(n->continuation.expression, c, r);
	prepend(make_asm1(LABEL, use_binding(after_binding, r), r), c, r);
}

void sgenerate_withs(union expression *n, list *c, buffer r) {
	if(n->with.escapes) {
		make_store_continuation(n, c, r);
	}
	generate_expressions(n->with.expression, c, r);
	prepend(make_asm1(LABEL, n->continuation.cont_instr_ref, r), c, r);
	make_load(((union expression *) n->with.parameter->fst)->reference.binding_aug, 0, make_asm0(RAX, r), make_asm0(R10, r), c, r);
}

void sgenerate_jumps(union expression *n, list *c, buffer r) {
	if(n->jump.short_circuit) {
		if(n->jump.reference->base.type == continuation) {
			generate_expressions(n->jump.reference, c, r);
		}
		if(length(n->jump.short_circuit->continuation.parameters) > 0) {
			make_load_address(((union expression *) n->jump.short_circuit->continuation.parameters->fst)->reference.binding_aug,
				make_asm0(R11, r), c, r);
			move_arguments(n, 0, c, r);
		}
		prepend(make_asm1(JMP_REL, make_asm1(LNKR_SUB_RIP_TO_REF, n->jump.short_circuit->continuation.cont_instr_ref, r), r), c, r);
	} else {
		generate_expressions(n->jump.reference, c, r);
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

void sgenerate_literals(union expression *n, list *c, buffer r) {
	prepend(make_asm2(MOVQ_IMM_TO_REG, make_literal(n->literal.value, r), make_asm0(RAX, r), r), c, r);
}

int get_current_offset(union expression *function) {
	if(length(function->function.binding_augs) > 0) {
		return ((struct binding_aug *) function->function.binding_augs->fst)->offset;
	} else {
		return 0;
	}
}

void sgenerate_functions(union expression *n, list *c, buffer r) {
	make_load_address(n->function.reference->reference.binding_aug, make_asm0(RAX, r), c, r);
	
	struct binding_aug *after_binding = make_binding_aug(static_storage, local_scope, defined_state, NULL, NULL, r);
	
	prepend(make_asm1(JMP_REL, make_asm1(LNKR_SUB_RIP_TO_REF, use_binding(after_binding, r), r), r), c, r);
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
	
	prepend(make_asm1(PUSHQ_REG, make_asm0(RBP, r), r), c, r);
	prepend(make_asm2(MOVQ_REG_TO_REG, make_asm0(RSP, r), make_asm0(RBP, r), r), c, r);
	prepend(make_asm2(SUBQ_IMM_TO_REG, make_literal(-get_current_offset(n), r), make_asm0(RSP, r), r), c, r);
	
	//Execute the function body
	generate_expressions(n->function.expression, c, r);
	
	prepend(make_asm0(LEAVE, r), c, r);
	
	prepend(make_asm1(POPQ_REG, make_asm0(R11, r), r), c, r);
	prepend(make_asm2(ADDQ_IMM_TO_REG, make_literal(6*WORD_SIZE, r), make_asm0(RSP, r), r), c, r);
	prepend(make_asm1(PUSHQ_REG, make_asm0(R11, r), r), c, r);
	prepend(make_asm0(RET, r), c, r);
	prepend(make_asm1(LABEL, use_binding(after_binding, r), r), c, r);
}

void sgenerate_invokes(union expression *n, list *c, buffer r) {
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

void sgenerate_begins(union expression *n, list *c, buffer r) {
	union expression *t;
	foreach(t, n->begin.expressions) {
		generate_expressions(t, c, r);
	}
}

void generate_expressions(union expression *n, list *c, buffer r) {
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

list generate_program(union expression *n, list *binding_augs, buffer r) {
	*binding_augs = n->function.binding_augs;
	list c = nil;
	prepend(make_asm1(PUSHQ_REG, make_asm0(RBP, r), r), &c, r);
	prepend(make_asm2(MOVQ_REG_TO_REG, make_asm0(RSP, r), make_asm0(RBP, r), r), &c, r);
	generate_expressions(n->function.expression, &c, r);
	prepend(make_asm0(LEAVE, r), &c, r);
	prepend(make_asm0(RET, r), &c, r);
	return reverse(c, r);
}

