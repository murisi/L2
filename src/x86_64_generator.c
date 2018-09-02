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
#define JMP_TO_REG 29
#define JE_REL 30
#define ORQ_REG_TO_REG 31
#define MOVQ_IMM_TO_REG 32
#define CALL_REG 34
#define GLOBAL_LABEL 36
#define LOCAL_LABEL 37
#define STVAL_ADD_OFF_TO_REF 43
#define STVAL_SUB_RIP_FROM_REF 44

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
				t->reference.offset = n->function.parent ? make_literal(parameter_offset, r) : NULL;
				parameter_offset += WORD_SIZE;
			}}
			int local_offset = 0;
			foreach(t, reverse(n->function.locals, r)) {
				local_offset -= WORD_SIZE;
				t->reference.offset = n->function.parent ? make_literal(local_offset, r) : NULL;
			}
			break;
		} case continuation: case with: {
			if(n->continuation.escapes) {
				append(n->continuation.reference, &get_zeroth_function(n)->function.locals, r);
				//Make space for the buffer
				int i;
				for(i = 1; i < (CONT_SIZE / WORD_SIZE); i++) {
					append(make_reference(r), &get_zeroth_function(n)->function.locals, r);
				}
			}
			append_list(&get_zeroth_function(n)->function.locals, n->continuation.parameters);
			break;
		}
	}
	return n;
}

union expression *make_load(union expression *ref, int offset, union expression *dest_reg, union expression *scratch_reg, region r) {
	union expression *container = make_begin(r);
	if(ref->reference.referent->reference.offset) {
		emit(make_instr(r, MOVQ_MDB_TO_REG, 3, make_literal(ref->reference.referent->reference.offset->literal.value + offset, r),
			use(RBP, r), dest_reg), r);
	} else {
		emit(make_instr(r, MOVQ_IMM_TO_REG, 2, make_instr(r, STVAL_ADD_OFF_TO_REF, 2, ref, make_literal(offset, r)), scratch_reg), r);
		emit(make_instr(r, MOVQ_MDB_TO_REG, 3, make_literal(0, r), scratch_reg, dest_reg), r);
	}
	return container;
}

union expression *make_store(union expression *src_reg, union expression *ref, int offset, union expression *scratch_reg, region r) {
	union expression *container = make_begin(r);
	if(ref->reference.referent->reference.offset) {
		emit(make_instr(r, MOVQ_FROM_REG_INTO_MDB, 3,
			src_reg, make_literal(ref->reference.referent->reference.offset->literal.value + offset, r), use(RBP, r)), r);
	} else {
		emit(make_instr(r, MOVQ_IMM_TO_REG, 2, make_instr(r, STVAL_ADD_OFF_TO_REF, 2, ref, make_literal(offset, r)), scratch_reg), r);
		emit(make_instr(r, MOVQ_FROM_REG_INTO_MDB, 3, src_reg, make_literal(0, r), scratch_reg), r);
	}
	return container;
}

//Must be used after use_return_reference and init_i386_registers
union expression *vgenerate_ifs(union expression *n, region r) {
	if(n->base.type == _if) {
		union expression *container = make_begin(r);
		
		emit(make_load(n->_if.condition, 0, use(R10, r), use(R13, r), r), r);
		emit(make_instr(r, ORQ_REG_TO_REG, 2, use(R10, r), use(R10, r)), r);
		
		union expression *alternate_label = make_reference(r);
		emit(make_instr(r, JE_REL, 1, make_instr(r, STVAL_SUB_RIP_FROM_REF, 1, alternate_label)), r);
		emit(n->_if.consequent, r);
		union expression *end_label = make_reference(r);
		emit(make_instr(r, JMP_REL, 1, make_instr(r, STVAL_SUB_RIP_FROM_REF, 1, end_label)), r);
		emit(make_instr(r, LOCAL_LABEL, 1, alternate_label), r);
		emit(n->_if.alternate, r);
		emit(make_instr(r, LOCAL_LABEL, 1, end_label), r);
		return container;
	} else {
		return n;
	}
}

union expression *make_load_address(union expression *ref, union expression *dest_reg, region r) {
	union expression *container = make_begin(r);
	if(ref->reference.referent->reference.offset) {
		emit(make_instr(r, LEAQ_OF_MDB_INTO_REG, 3, ref->reference.referent->reference.offset, use(RBP, r), dest_reg), r);
	} else {
		emit(make_instr(r, MOVQ_IMM_TO_REG, 2, ref, dest_reg), r);
	}
	return container;
}

//Must be used after use_return_reference and generate_continuation_expressions
union expression *vgenerate_references(union expression *n, region r) {
	if(n->base.type == reference && n->reference.return_value) {
		union expression *container = make_begin(r);
		emit(make_load_address(n, use(R11, r), r), r);
		emit(make_store(use(R11, r), n->base.return_value, 0, use(R10, r), r), r);
		return container;
	} else {
		return n;
	}
}

union expression *cont_instr_ref(union expression *n, region r) {
	return n->continuation.cont_instr_ref ? n->continuation.cont_instr_ref : (n->continuation.cont_instr_ref = make_reference(r));
}

union expression *make_continuation(union expression *n, region r) {
	union expression *container = make_begin(r);
	emit(make_store(use(RBX, r), n->continuation.reference, CONT_RBX, use(R11, r), r), r);
	emit(make_store(use(R12, r), n->continuation.reference, CONT_R12, use(R11, r), r), r);
	emit(make_store(use(R13, r), n->continuation.reference, CONT_R13, use(R11, r), r), r);
	emit(make_store(use(R14, r), n->continuation.reference, CONT_R14, use(R11, r), r), r);
	emit(make_store(use(R15, r), n->continuation.reference, CONT_R15, use(R11, r), r), r);
	emit(make_load_address(cont_instr_ref(n, r), use(R10, r), r), r);
	emit(make_store(use(R10, r), n->continuation.reference, CONT_CIR, use(R11, r), r), r);
	emit(make_store(use(RBP, r), n->continuation.reference, CONT_RBP, use(R11, r), r), r);
	return container;
}

union expression *move_arguments(union expression *n, int offset, region r) {
	union expression *container = make_begin(r);
	union expression *t;
	foreach(t, n->jump.arguments) {
		emit(make_load(t, 0, use(R10, r), use(R13, r), r), r);
		emit(make_instr(r, MOVQ_FROM_REG_INTO_MDB, 3, use(R10, r), make_literal(offset, r), use(R11, r)), r);
		offset += WORD_SIZE;
	}
	return container;
}

//Must be used after use_return_reference
union expression *vgenerate_continuation_expressions(union expression *n, region r) {
	switch(n->base.type) {
		case continuation: {
			union expression *container = make_begin(r);
			emit(make_load_address(n->continuation.escapes ? n->continuation.reference : cont_instr_ref(n, r), use(R11, r), r), r);
			emit(make_store(use(R11, r), n->continuation.return_value, 0, use(R10, r), r), r);
			if(n->continuation.escapes) {
				emit(make_continuation(n, r), r);
			}
			
			//Skip the actual instructions of the continuation
			union expression *after_reference = make_reference(r);
			emit(make_instr(r, JMP_REL, 1, make_instr(r, STVAL_SUB_RIP_FROM_REF, 1, after_reference)), r);
			emit(make_instr(r, LOCAL_LABEL, 1, cont_instr_ref(n, r)), r);
			emit(n->continuation.expression, r);
			emit(make_instr(r, LOCAL_LABEL, 1, after_reference), r);
			return container;
		} case with: {
			union expression *container = make_begin(r);
			if(n->with.escapes) {
				emit(make_continuation(n, r), r);
			}
			emit(n->with.expression, r);
			emit(make_instr(r, LOCAL_LABEL, 1, cont_instr_ref(n, r)), r);
			emit(make_load(n->with.parameter->fst, 0, use(R11, r), use(R10, r), r), r);
			emit(make_store(use(R11, r), n->with.return_value, 0, use(R10, r), r), r);
			return container;
		} case jump: {
			union expression *container = make_begin(r);
			if(n->jump.short_circuit) {
				if(length(n->jump.short_circuit->continuation.parameters) > 0) {
					emit(make_load_address(n->jump.short_circuit->continuation.parameters->fst, use(R11, r), r), r);
					emit(move_arguments(n, 0, r), r);
				}
				emit(make_instr(r, JMP_REL, 1, make_instr(r, STVAL_SUB_RIP_FROM_REF, 1, cont_instr_ref(n->jump.short_circuit, r))), r);
			} else {
				emit(make_load(n->jump.reference, 0, use(R11, r), use(R10, r), r), r);
				emit(move_arguments(n, CONT_SIZE, r), r);
				emit(make_instr(r, MOVQ_MDB_TO_REG, 3, make_literal(CONT_RBX, r), use(R11, r), use(RBX, r)), r);
				emit(make_instr(r, MOVQ_MDB_TO_REG, 3, make_literal(CONT_R12, r), use(R11, r), use(R12, r)), r);
				emit(make_instr(r, MOVQ_MDB_TO_REG, 3, make_literal(CONT_R13, r), use(R11, r), use(R13, r)), r);
				emit(make_instr(r, MOVQ_MDB_TO_REG, 3, make_literal(CONT_R14, r), use(R11, r), use(R14, r)), r);
				emit(make_instr(r, MOVQ_MDB_TO_REG, 3, make_literal(CONT_R15, r), use(R11, r), use(R15, r)), r);
				emit(make_instr(r, MOVQ_MDB_TO_REG, 3, make_literal(CONT_CIR, r), use(R11, r), use(R10, r)), r);
				emit(make_instr(r, MOVQ_MDB_TO_REG, 3, make_literal(CONT_RBP, r), use(R11, r), use(RBP, r)), r);
				emit(make_instr(r, JMP_TO_REG, 1, use(R10, r)), r);
			}
			return container;
		} default: {
			return n;
		}
	}
}

//Must be used after use_return_reference and init_i386_registers
union expression *vgenerate_literals(union expression *n, region r) {
	if(n->base.type == literal && n->literal.return_value) {
		union expression *container = make_begin(r);
		emit(make_instr(r, MOVQ_IMM_TO_REG, 2, make_literal(n->literal.value, r), use(R11, r)), r);
		emit(make_store(use(R11, r), n->literal.return_value, 0, use(R13, r), r), r);
		return container;
	} else {
		return n;
	}
}

union expression *generate_toplevel(union expression *n, region r) {
	union expression *container = make_begin(r);
	emit(make_instr(r, PUSHQ_REG, 1, use(R12, r)), r);
	emit(make_instr(r, PUSHQ_REG, 1, use(R13, r)), r);
	emit(make_instr(r, PUSHQ_REG, 1, use(R14, r)), r);
	emit(make_instr(r, PUSHQ_REG, 1, use(R15, r)), r);
	emit(make_instr(r, PUSHQ_REG, 1, use(RBX, r)), r);
	emit(make_instr(r, PUSHQ_REG, 1, use(RBP, r)), r);
	emit(make_instr(r, MOVQ_REG_TO_REG, 2, use(RSP, r), use(RBP, r)), r);
	emit(n->function.expression, r);
	emit(make_instr(r, LEAVE, 0), r);
	emit(make_instr(r, POPQ_REG, 1, use(RBX, r)), r);
	emit(make_instr(r, POPQ_REG, 1, use(R15, r)), r);
	emit(make_instr(r, POPQ_REG, 1, use(R14, r)), r);
	emit(make_instr(r, POPQ_REG, 1, use(R13, r)), r);
	emit(make_instr(r, POPQ_REG, 1, use(R12, r)), r);
	emit(make_instr(r, RET, 0), r);
	return container;
}

int get_current_offset(union expression *function) {
	if(length(function->function.locals) > 0) {
		return ((union expression *) function->function.locals->fst)->reference.offset->literal.value;
	} else {
		return 0;
	}
}

//Must be used after all local references have been made, i.e. after make_continuations
union expression *vgenerate_function_expressions(union expression *n, region r) {
	if(n->base.type == function && n->function.parent) {
		union expression *container = make_begin(r);
		emit(make_load_address(n->function.reference, use(R11, r), r), r);
		emit(make_store(use(R11, r), n->function.return_value, 0, use(R10, r), r), r);
		
		union expression *after_reference = make_reference(r);
		
		emit(make_instr(r, JMP_REL, 1, make_instr(r, STVAL_SUB_RIP_FROM_REF, 1, after_reference)), r);
		if(root_function_of(n) == n->function.parent->begin.parent) {
			emit(make_instr(r, GLOBAL_LABEL, 1, n->function.reference), r);
		} else {
			emit(make_instr(r, LOCAL_LABEL, 1, n->function.reference), r);
		}
		
		//Insert first 6 parameters onto stack
		emit(make_instr(r, POPQ_REG, 1, use(R11, r)), r);
		emit(make_instr(r, PUSHQ_REG, 1, use(R9, r)), r);
		emit(make_instr(r, PUSHQ_REG, 1, use(R8, r)), r);
		emit(make_instr(r, PUSHQ_REG, 1, use(RCX, r)), r);
		emit(make_instr(r, PUSHQ_REG, 1, use(RDX, r)), r);
		emit(make_instr(r, PUSHQ_REG, 1, use(RSI, r)), r);
		emit(make_instr(r, PUSHQ_REG, 1, use(RDI, r)), r);
		emit(make_instr(r, PUSHQ_REG, 1, use(R11, r)), r);
		
		//Save callee-saved registers
		emit(make_instr(r, PUSHQ_REG, 1, use(R12, r)), r);
		emit(make_instr(r, PUSHQ_REG, 1, use(R13, r)), r);
		emit(make_instr(r, PUSHQ_REG, 1, use(R14, r)), r);
		emit(make_instr(r, PUSHQ_REG, 1, use(R15, r)), r);
		emit(make_instr(r, PUSHQ_REG, 1, use(RBX, r)), r);
		
		emit(make_instr(r, PUSHQ_REG, 1, use(RBP, r)), r);
		emit(make_instr(r, MOVQ_REG_TO_REG, 2, use(RSP, r), use(RBP, r)), r);
		emit(make_instr(r, SUBQ_IMM_FROM_REG, 2, make_literal(-get_current_offset(n), r), use(RSP, r)), r);
		
		//Execute the function body
		emit(n->function.expression, r);
		
		//Place the return value
		emit(make_load(n->function.expression_return_value, 0, use(RAX, r), use(R13, r), r), r);
		
		emit(make_instr(r, LEAVE, 0), r);
		//Restore callee-saved registers
		emit(make_instr(r, POPQ_REG, 1, use(RBX, r)), r);
		emit(make_instr(r, POPQ_REG, 1, use(R15, r)), r);
		emit(make_instr(r, POPQ_REG, 1, use(R14, r)), r);
		emit(make_instr(r, POPQ_REG, 1, use(R13, r)), r);
		emit(make_instr(r, POPQ_REG, 1, use(R12, r)), r);
		
		emit(make_instr(r, POPQ_REG, 1, use(R11, r)), r);
		emit(make_instr(r, ADDQ_IMM_TO_REG, 2, make_literal(6*WORD_SIZE, r), use(RSP, r)), r);
		emit(make_instr(r, PUSHQ_REG, 1, use(R11, r)), r);
		emit(make_instr(r, RET, 0), r);
		emit(make_instr(r, LOCAL_LABEL, 1, after_reference), r);
		return container;
	} else if(n->base.type == invoke) {
		union expression *container = make_begin(r);
		
		//Push arguments onto stack
		if(length(n->invoke.arguments) > 6) {
			union expression *t;
			foreach(t, reverse(n->invoke.arguments->rrrrrrst, r)) {
				emit(make_load(t, 0, use(R11, r), use(R10, r), r), r);
				emit(make_instr(r, PUSHQ_REG, 1, use(R11, r)), r);
			}
		}
		if(length(n->invoke.arguments) > 5) {
			emit(make_load(n->invoke.arguments->frrrrrst, 0, use(R9, r), use(R10, r), r), r);
		}
		if(length(n->invoke.arguments) > 4) {
			emit(make_load(n->invoke.arguments->frrrrst, 0, use(R8, r), use(R10, r), r), r);
		}
		if(length(n->invoke.arguments) > 3) {
			emit(make_load(n->invoke.arguments->frrrst, 0, use(RCX, r), use(R10, r), r), r);
		}
		if(length(n->invoke.arguments) > 2) {
			emit(make_load(n->invoke.arguments->frrst, 0, use(RDX, r), use(R10, r), r), r);
		}
		if(length(n->invoke.arguments) > 1) {
			emit(make_load(n->invoke.arguments->frst, 0, use(RSI, r), use(R10, r), r), r);
		}
		if(length(n->invoke.arguments) > 0) {
			emit(make_load(n->invoke.arguments->fst, 0, use(RDI, r), use(R10, r), r), r);
		}
		emit(make_instr(r, MOVQ_IMM_TO_REG, 2, make_literal(0, r), use(RAX, r)), r);
		
		emit(make_load(n->invoke.reference, 0, use(R11, r), use(R10, r), r), r);
		emit(make_instr(r, CALL_REG, 1, use(R11, r)), r);
		
		emit(make_store(use(RAX, r), n->invoke.return_value, 0, use(R10, r), r), r);
		if(length(n->invoke.arguments) > 6) {
			emit(make_instr(r, ADDQ_IMM_TO_REG, 2, make_literal(WORD_SIZE * (length(n->invoke.arguments) - 6), r), use(RSP, r)), r);
		}
		return container;
	} else {
		return n;
	}
}
