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
					append(make_reference(NULL, r), &get_zeroth_function(n)->function.locals, r);
				}
			}
			append_list(&get_zeroth_function(n)->function.locals, n->continuation.parameters);
			break;
		}
	}
	return n;
}

union expression *make_load(union expression *ref, int offset, union expression *dest_reg, union expression *scratch_reg, region r) {
	union expression *container = make_begin(nil(r), r);
	if(ref->reference.referent->reference.offset) {
		emit(make_instr3(MOVQ_MDB_TO_REG, make_literal(ref->reference.referent->reference.offset->literal.value + offset, r),
			use(RBP, r), dest_reg, r), r);
	} else {
		emit(make_instr2(MOVQ_IMM_TO_REG, make_instr2(STVAL_ADD_OFF_TO_REF, ref, make_literal(offset, r), r), scratch_reg, r), r);
		emit(make_instr3(MOVQ_MDB_TO_REG, make_literal(0, r), scratch_reg, dest_reg, r), r);
	}
	return container;
}

union expression *make_store(union expression *src_reg, union expression *ref, int offset, union expression *scratch_reg, region r) {
	union expression *container = make_begin(nil(r), r);
	if(ref->reference.referent->reference.offset) {
		emit(make_instr3(MOVQ_FROM_REG_INTO_MDB,
			src_reg, make_literal(ref->reference.referent->reference.offset->literal.value + offset, r), use(RBP, r), r), r);
	} else {
		emit(make_instr2(MOVQ_IMM_TO_REG, make_instr2(STVAL_ADD_OFF_TO_REF, ref, make_literal(offset, r), r), scratch_reg, r), r);
		emit(make_instr3(MOVQ_FROM_REG_INTO_MDB, src_reg, make_literal(0, r), scratch_reg, r), r);
	}
	return container;
}

//Must be used after use_return_reference and init_i386_registers
union expression *vgenerate_ifs(union expression *n, region r) {
	if(n->base.type == _if) {
		union expression *container = make_begin(nil(r), r);
		
		emit(make_load(n->_if.condition, 0, use(R10, r), use(R13, r), r), r);
		emit(make_instr2(ORQ_REG_TO_REG, use(R10, r), use(R10, r), r), r);
		
		union expression *alternate_label = make_reference(NULL, r);
		emit(make_instr1(JE_REL, make_instr1(STVAL_SUB_RIP_FROM_REF, alternate_label, r), r), r);
		emit(n->_if.consequent, r);
		union expression *end_label = make_reference(NULL, r);
		emit(make_instr1(JMP_REL, make_instr1(STVAL_SUB_RIP_FROM_REF, end_label, r), r), r);
		emit(make_instr1(LOCAL_LABEL, alternate_label, r), r);
		emit(n->_if.alternate, r);
		emit(make_instr1(LOCAL_LABEL, end_label, r), r);
		return container;
	} else {
		return n;
	}
}

union expression *make_load_address(union expression *ref, union expression *dest_reg, region r) {
	union expression *container = make_begin(nil(r), r);
	if(ref->reference.referent->reference.offset) {
		emit(make_instr3(LEAQ_OF_MDB_INTO_REG, ref->reference.referent->reference.offset, use(RBP, r), dest_reg, r), r);
	} else {
		emit(make_instr2(MOVQ_IMM_TO_REG, ref, dest_reg, r), r);
	}
	return container;
}

//Must be used after use_return_reference and generate_continuation_expressions
union expression *vgenerate_references(union expression *n, region r) {
	if(n->base.type == reference && n->reference.return_value) {
		union expression *container = make_begin(nil(r), r);
		emit(make_load_address(n, use(R11, r), r), r);
		emit(make_store(use(R11, r), n->base.return_value, 0, use(R10, r), r), r);
		return container;
	} else {
		return n;
	}
}

union expression *cont_instr_ref(union expression *n, region r) {
	return n->continuation.cont_instr_ref ?
		n->continuation.cont_instr_ref : (n->continuation.cont_instr_ref = make_reference(NULL, r));
}

union expression *make_store_continuation(union expression *n, region r) {
	union expression *container = make_begin(nil(r), r);
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
	union expression *container = make_begin(nil(r), r);
	union expression *t;
	foreach(t, n->jump.arguments) {
		emit(make_load(t, 0, use(R10, r), use(R13, r), r), r);
		emit(make_instr3(MOVQ_FROM_REG_INTO_MDB, use(R10, r), make_literal(offset, r), use(R11, r), r), r);
		offset += WORD_SIZE;
	}
	return container;
}

//Must be used after use_return_reference
union expression *vgenerate_continuation_expressions(union expression *n, region r) {
	switch(n->base.type) {
		case continuation: {
			union expression *container = make_begin(nil(r), r);
			emit(make_load_address(n->continuation.escapes ? n->continuation.reference : cont_instr_ref(n, r), use(R11, r), r), r);
			emit(make_store(use(R11, r), n->continuation.return_value, 0, use(R10, r), r), r);
			if(n->continuation.escapes) {
				emit(make_store_continuation(n, r), r);
			}
			
			//Skip the actual instructions of the continuation
			union expression *after_reference = make_reference(NULL, r);
			emit(make_instr1(JMP_REL, make_instr1(STVAL_SUB_RIP_FROM_REF, after_reference, r), r), r);
			emit(make_instr1(LOCAL_LABEL, cont_instr_ref(n, r), r), r);
			emit(n->continuation.expression, r);
			emit(make_instr1(LOCAL_LABEL, after_reference, r), r);
			return container;
		} case with: {
			union expression *container = make_begin(nil(r), r);
			if(n->with.escapes) {
				emit(make_store_continuation(n, r), r);
			}
			emit(n->with.expression, r);
			emit(make_instr1(LOCAL_LABEL, cont_instr_ref(n, r), r), r);
			emit(make_load(n->with.parameter->fst, 0, use(R11, r), use(R10, r), r), r);
			emit(make_store(use(R11, r), n->with.return_value, 0, use(R10, r), r), r);
			return container;
		} case jump: {
			union expression *container = make_begin(nil(r), r);
			if(n->jump.short_circuit) {
				if(length(n->jump.short_circuit->continuation.parameters) > 0) {
					emit(make_load_address(n->jump.short_circuit->continuation.parameters->fst, use(R11, r), r), r);
					emit(move_arguments(n, 0, r), r);
				}
				emit(make_instr1(JMP_REL, make_instr1(STVAL_SUB_RIP_FROM_REF, cont_instr_ref(n->jump.short_circuit, r), r), r), r);
			} else {
				emit(make_load(n->jump.reference, 0, use(R11, r), use(R10, r), r), r);
				emit(move_arguments(n, CONT_SIZE, r), r);
				emit(make_instr3(MOVQ_MDB_TO_REG, make_literal(CONT_RBX, r), use(R11, r), use(RBX, r), r), r);
				emit(make_instr3(MOVQ_MDB_TO_REG, make_literal(CONT_R12, r), use(R11, r), use(R12, r), r), r);
				emit(make_instr3(MOVQ_MDB_TO_REG, make_literal(CONT_R13, r), use(R11, r), use(R13, r), r), r);
				emit(make_instr3(MOVQ_MDB_TO_REG, make_literal(CONT_R14, r), use(R11, r), use(R14, r), r), r);
				emit(make_instr3(MOVQ_MDB_TO_REG, make_literal(CONT_R15, r), use(R11, r), use(R15, r), r), r);
				emit(make_instr3(MOVQ_MDB_TO_REG, make_literal(CONT_CIR, r), use(R11, r), use(R10, r), r), r);
				emit(make_instr3(MOVQ_MDB_TO_REG, make_literal(CONT_RBP, r), use(R11, r), use(RBP, r), r), r);
				emit(make_instr1(JMP_TO_REG, use(R10, r), r), r);
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
		union expression *container = make_begin(nil(r), r);
		emit(make_instr2(MOVQ_IMM_TO_REG, make_literal(n->literal.value, r), use(R11, r), r), r);
		emit(make_store(use(R11, r), n->literal.return_value, 0, use(R13, r), r), r);
		return container;
	} else {
		return n;
	}
}

union expression *generate_toplevel(union expression *n, region r) {
	union expression *container = make_begin(nil(r), r);
	emit(make_instr1(PUSHQ_REG, use(R12, r), r), r);
	emit(make_instr1(PUSHQ_REG, use(R13, r), r), r);
	emit(make_instr1(PUSHQ_REG, use(R14, r), r), r);
	emit(make_instr1(PUSHQ_REG, use(R15, r), r), r);
	emit(make_instr1(PUSHQ_REG, use(RBX, r), r), r);
	emit(make_instr1(PUSHQ_REG, use(RBP, r), r), r);
	emit(make_instr2(MOVQ_REG_TO_REG, use(RSP, r), use(RBP, r), r), r);
	emit(n->function.expression, r);
	emit(make_instr0(LEAVE, r), r);
	emit(make_instr1(POPQ_REG, use(RBX, r), r), r);
	emit(make_instr1(POPQ_REG, use(R15, r), r), r);
	emit(make_instr1(POPQ_REG, use(R14, r), r), r);
	emit(make_instr1(POPQ_REG, use(R13, r), r), r);
	emit(make_instr1(POPQ_REG, use(R12, r), r), r);
	emit(make_instr0(RET, r), r);
	return container;
}

int get_current_offset(union expression *function) {
	if(length(function->function.locals) > 0) {
		return ((union expression *) function->function.locals->fst)->reference.offset->literal.value;
	} else {
		return 0;
	}
}

//Must be used after all local references have been made, i.e. after make_store_continuations
union expression *vgenerate_function_expressions(union expression *n, region r) {
	if(n->base.type == function && n->function.parent) {
		union expression *container = make_begin(nil(r), r);
		emit(make_load_address(n->function.reference, use(R11, r), r), r);
		emit(make_store(use(R11, r), n->function.return_value, 0, use(R10, r), r), r);
		
		union expression *after_reference = make_reference(NULL, r);
		
		emit(make_instr1(JMP_REL, make_instr1(STVAL_SUB_RIP_FROM_REF, after_reference, r), r), r);
		if(root_function_of(n) == n->function.parent->begin.parent) {
			emit(make_instr1(GLOBAL_LABEL, n->function.reference, r), r);
		} else {
			emit(make_instr1(LOCAL_LABEL, n->function.reference, r), r);
		}
		
		//Insert first 6 parameters onto stack
		emit(make_instr1(POPQ_REG, use(R11, r), r), r);
		emit(make_instr1(PUSHQ_REG, use(R9, r), r), r);
		emit(make_instr1(PUSHQ_REG, use(R8, r), r), r);
		emit(make_instr1(PUSHQ_REG, use(RCX, r), r), r);
		emit(make_instr1(PUSHQ_REG, use(RDX, r), r), r);
		emit(make_instr1(PUSHQ_REG, use(RSI, r), r), r);
		emit(make_instr1(PUSHQ_REG, use(RDI, r), r), r);
		emit(make_instr1(PUSHQ_REG, use(R11, r), r), r);
		
		//Save callee-saved registers
		emit(make_instr1(PUSHQ_REG, use(R12, r), r), r);
		emit(make_instr1(PUSHQ_REG, use(R13, r), r), r);
		emit(make_instr1(PUSHQ_REG, use(R14, r), r), r);
		emit(make_instr1(PUSHQ_REG, use(R15, r), r), r);
		emit(make_instr1(PUSHQ_REG, use(RBX, r), r), r);
		
		emit(make_instr1(PUSHQ_REG, use(RBP, r), r), r);
		emit(make_instr2(MOVQ_REG_TO_REG, use(RSP, r), use(RBP, r), r), r);
		emit(make_instr2(SUBQ_IMM_FROM_REG, make_literal(-get_current_offset(n), r), use(RSP, r), r), r);
		
		//Execute the function body
		emit(n->function.expression, r);
		
		//Place the return value
		emit(make_load(n->function.expression_return_value, 0, use(RAX, r), use(R13, r), r), r);
		
		emit(make_instr0(LEAVE, r), r);
		//Restore callee-saved registers
		emit(make_instr1(POPQ_REG, use(RBX, r), r), r);
		emit(make_instr1(POPQ_REG, use(R15, r), r), r);
		emit(make_instr1(POPQ_REG, use(R14, r), r), r);
		emit(make_instr1(POPQ_REG, use(R13, r), r), r);
		emit(make_instr1(POPQ_REG, use(R12, r), r), r);
		
		emit(make_instr1(POPQ_REG, use(R11, r), r), r);
		emit(make_instr2(ADDQ_IMM_TO_REG, make_literal(6*WORD_SIZE, r), use(RSP, r), r), r);
		emit(make_instr1(PUSHQ_REG, use(R11, r), r), r);
		emit(make_instr0(RET, r), r);
		emit(make_instr1(LOCAL_LABEL, after_reference, r), r);
		return container;
	} else if(n->base.type == invoke) {
		union expression *container = make_begin(nil(r), r);
		
		//Push arguments onto stack
		if(length(n->invoke.arguments) > 6) {
			union expression *t;
			foreach(t, reverse(n->invoke.arguments->rrrrrrst, r)) {
				emit(make_load(t, 0, use(R11, r), use(R10, r), r), r);
				emit(make_instr1(PUSHQ_REG, use(R11, r), r), r);
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
		emit(make_instr2(MOVQ_IMM_TO_REG, make_literal(0, r), use(RAX, r), r), r);
		
		emit(make_load(n->invoke.reference, 0, use(R11, r), use(R10, r), r), r);
		emit(make_instr1(CALL_REG, use(R11, r), r), r);
		
		emit(make_store(use(RAX, r), n->invoke.return_value, 0, use(R10, r), r), r);
		if(length(n->invoke.arguments) > 6) {
			emit(make_instr2(ADDQ_IMM_TO_REG, make_literal(WORD_SIZE * (length(n->invoke.arguments) - 6), r), use(RSP, r), r), r);
		}
		return container;
	} else {
		return n;
	}
}
