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
				t->reference.symbol->offset = parameter_offset;
				parameter_offset += WORD_SIZE;
			}}
			int local_offset = 0;
			struct symbol *u;
			foreach(u, reverse(n->function.local_symbols, r)) {
				local_offset -= WORD_SIZE;
				u->offset = local_offset;
			}
			break;
		} case continuation: case with: {
			if(n->continuation.escapes) {
				append(n->continuation.reference->reference.symbol, &get_zeroth_function(n)->function.local_symbols, r);
				//Make space for the buffer
				int i;
				for(i = 1; i < (CONT_SIZE / WORD_SIZE); i++) {
					append(make_symbol(get_zeroth_function(n)->function.parent ?
						dynamic_storage : static_storage, NULL, r), &get_zeroth_function(n)->function.local_symbols, r);
				}
			}
			union expression *t;
			foreach(t, n->continuation.parameters) {
				append(t->reference.symbol, &get_zeroth_function(n)->function.local_symbols, r);
			}
			break;
		} case storage: {
			append(n->storage.reference->reference.symbol, &get_zeroth_function(n)->function.local_symbols, r);
			//Make space for the buffer
			int i;
			for(i = 1; i < length(n->storage.arguments); i++) {
				append(make_symbol(get_zeroth_function(n)->function.parent ?
					dynamic_storage : static_storage, NULL, r), &get_zeroth_function(n)->function.local_symbols, r);
			}
			break;
		}
	}
	return n;
}

union expression *make_load(struct symbol *sym, int offset, union expression *dest_reg, union expression *scratch_reg, region r) {
	union expression *container = make_begin(nil(r), r);
	if(sym->type == dynamic_storage) {
		emit(make_asm3(MOVQ_MDB_TO_REG, make_literal(sym->offset + offset, r), make_asm0(RBP, r), dest_reg, r), r);
	} else {
		emit(make_asm2(MOVQ_IMM_TO_REG, make_asm2(STVAL_ADD_OFF_TO_REF, use_symbol(sym, r), make_literal(offset, r), r), scratch_reg,
			r), r);
		emit(make_asm3(MOVQ_MDB_TO_REG, make_literal(0, r), scratch_reg, dest_reg, r), r);
	}
	return container;
}

union expression *make_store(union expression *src_reg, struct symbol *sym, int offset, union expression *scratch_reg, region r) {
	union expression *container = make_begin(nil(r), r);
	if(sym->type == dynamic_storage) {
		emit(make_asm3(MOVQ_FROM_REG_INTO_MDB, src_reg, make_literal(sym->offset + offset, r), make_asm0(RBP, r), r), r);
	} else {
		emit(make_asm2(MOVQ_IMM_TO_REG, make_asm2(STVAL_ADD_OFF_TO_REF, use_symbol(sym, r), make_literal(offset, r), r), scratch_reg,
			r), r);
		emit(make_asm3(MOVQ_FROM_REG_INTO_MDB, src_reg, make_literal(0, r), scratch_reg, r), r);
	}
	return container;
}

//Must be used after use_return_reference and init_i386_registers
union expression *vgenerate_ifs(union expression *n, region r) {
	if(n->base.type == _if) {
		union expression *container = make_begin(nil(r), r);
		
		emit(make_load(n->_if.condition->reference.symbol, 0, make_asm0(R10, r), make_asm0(R13, r), r), r);
		emit(make_asm2(ORQ_REG_TO_REG, make_asm0(R10, r), make_asm0(R10, r), r), r);
		
		struct symbol *alternate_symbol = make_symbol(_function, NULL, r);
		emit(make_asm1(JE_REL, make_asm1(STVAL_SUB_RIP_FROM_REF, use_symbol(alternate_symbol, r), r), r), r);
		emit(n->_if.consequent, r);
		struct symbol *end_symbol = make_symbol(_function, NULL, r);
		emit(make_asm1(JMP_REL, make_asm1(STVAL_SUB_RIP_FROM_REF, use_symbol(end_symbol, r), r), r), r);
		emit(make_asm1(LOCAL_LABEL, use_symbol(alternate_symbol, r), r), r);
		emit(n->_if.alternate, r);
		emit(make_asm1(LOCAL_LABEL, use_symbol(end_symbol, r), r), r);
		return container;
	} else {
		return n;
	}
}

union expression *make_load_address(struct symbol *sym, union expression *dest_reg, region r) {
	union expression *container = make_begin(nil(r), r);
	if(sym->type == dynamic_storage) {
		emit(make_asm3(LEAQ_OF_MDB_INTO_REG, make_literal(sym->offset, r), make_asm0(RBP, r), dest_reg, r), r);
	} else {
		emit(make_asm2(MOVQ_IMM_TO_REG, use_symbol(sym, r), dest_reg, r), r);
	}
	return container;
}

union expression *vgenerate_storage_expressions(union expression *n, region r) {
	if(n->base.type == storage) {
		union expression *container = make_begin(nil(r), r);
		int offset = 0;
		union expression *t;
		foreach(t, n->storage.arguments) {
			emit(make_load(t->reference.symbol, 0, make_asm0(R10, r), make_asm0(R13, r), r), r);
			emit(make_store(make_asm0(R10, r), n->storage.reference->reference.symbol, offset, make_asm0(R13, r), r), r);
			offset += WORD_SIZE;
		}
		emit(make_load_address(n->storage.reference->reference.symbol, make_asm0(R11, r), r), r);
		emit(make_store(make_asm0(R11, r), n->storage.return_symbol, 0, make_asm0(R10, r), r), r);
		return container;
	} else {
		return n;
	}
}

//Must be used after use_return_reference and generate_continuation_expressions
union expression *vgenerate_references(union expression *n, region r) {
	if(n->base.type == reference && n->reference.return_symbol) {
		union expression *container = make_begin(nil(r), r);
		emit(make_load_address(n->reference.symbol, make_asm0(R11, r), r), r);
		emit(make_store(make_asm0(R11, r), n->reference.return_symbol, 0, make_asm0(R10, r), r), r);
		return container;
	} else {
		return n;
	}
}

union expression *cont_instr_ref(union expression *n, region r) {
	return n->continuation.cont_instr_ref ?
		n->continuation.cont_instr_ref : (n->continuation.cont_instr_ref = use_symbol(make_symbol(_function, NULL, r), r));
}

union expression *make_store_continuation(union expression *n, region r) {
	union expression *container = make_begin(nil(r), r);
	emit(make_store(make_asm0(RBX, r), n->continuation.reference->reference.symbol, CONT_RBX, make_asm0(R11, r), r), r);
	emit(make_store(make_asm0(R12, r), n->continuation.reference->reference.symbol, CONT_R12, make_asm0(R11, r), r), r);
	emit(make_store(make_asm0(R13, r), n->continuation.reference->reference.symbol, CONT_R13, make_asm0(R11, r), r), r);
	emit(make_store(make_asm0(R14, r), n->continuation.reference->reference.symbol, CONT_R14, make_asm0(R11, r), r), r);
	emit(make_store(make_asm0(R15, r), n->continuation.reference->reference.symbol, CONT_R15, make_asm0(R11, r), r), r);
	emit(make_load_address(cont_instr_ref(n, r)->reference.symbol, make_asm0(R10, r), r), r);
	emit(make_store(make_asm0(R10, r), n->continuation.reference->reference.symbol, CONT_CIR, make_asm0(R11, r), r), r);
	emit(make_store(make_asm0(RBP, r), n->continuation.reference->reference.symbol, CONT_RBP, make_asm0(R11, r), r), r);
	return container;
}

union expression *move_arguments(union expression *n, int offset, region r) {
	union expression *container = make_begin(nil(r), r);
	union expression *t;
	foreach(t, n->jump.arguments) {
		emit(make_load(t->reference.symbol, 0, make_asm0(R10, r), make_asm0(R13, r), r), r);
		emit(make_asm3(MOVQ_FROM_REG_INTO_MDB, make_asm0(R10, r), make_literal(offset, r), make_asm0(R11, r), r), r);
		offset += WORD_SIZE;
	}
	return container;
}

//Must be used after use_return_reference
union expression *vgenerate_continuation_expressions(union expression *n, region r) {
	switch(n->base.type) {
		case continuation: {
			union expression *container = make_begin(nil(r), r);
			emit(make_load_address(n->continuation.escapes ?
				n->continuation.reference->reference.symbol : cont_instr_ref(n, r)->reference.symbol, make_asm0(R11, r), r), r);
			emit(make_store(make_asm0(R11, r), n->continuation.return_symbol, 0, make_asm0(R10, r), r), r);
			if(n->continuation.escapes) {
				emit(make_store_continuation(n, r), r);
			}
			
			//Skip the actual instructions of the continuation
			struct symbol *after_symbol = make_symbol(_function, NULL, r);
			emit(make_asm1(JMP_REL, make_asm1(STVAL_SUB_RIP_FROM_REF, use_symbol(after_symbol, r), r), r), r);
			emit(make_asm1(LOCAL_LABEL, cont_instr_ref(n, r), r), r);
			emit(n->continuation.expression, r);
			emit(make_asm1(LOCAL_LABEL, use_symbol(after_symbol, r), r), r);
			return container;
		} case with: {
			union expression *container = make_begin(nil(r), r);
			if(n->with.escapes) {
				emit(make_store_continuation(n, r), r);
			}
			emit(n->with.expression, r);
			emit(make_asm1(LOCAL_LABEL, cont_instr_ref(n, r), r), r);
			emit(make_load(((union expression *) n->with.parameter->fst)->reference.symbol, 0, make_asm0(R11, r), make_asm0(R10, r), r), r);
			emit(make_store(make_asm0(R11, r), n->with.return_symbol, 0, make_asm0(R10, r), r), r);
			return container;
		} case jump: {
			union expression *container = make_begin(nil(r), r);
			if(n->jump.short_circuit) {
				if(length(n->jump.short_circuit->continuation.parameters) > 0) {
					emit(make_load_address(((union expression *) n->jump.short_circuit->continuation.parameters->fst)->reference.symbol, make_asm0(R11, r), r), r);
					emit(move_arguments(n, 0, r), r);
				}
				emit(make_asm1(JMP_REL, make_asm1(STVAL_SUB_RIP_FROM_REF, cont_instr_ref(n->jump.short_circuit, r), r), r), r);
			} else {
				emit(make_load(n->jump.reference->reference.symbol, 0, make_asm0(R11, r), make_asm0(R10, r), r), r);
				emit(move_arguments(n, CONT_SIZE, r), r);
				emit(make_asm3(MOVQ_MDB_TO_REG, make_literal(CONT_RBX, r), make_asm0(R11, r), make_asm0(RBX, r), r), r);
				emit(make_asm3(MOVQ_MDB_TO_REG, make_literal(CONT_R12, r), make_asm0(R11, r), make_asm0(R12, r), r), r);
				emit(make_asm3(MOVQ_MDB_TO_REG, make_literal(CONT_R13, r), make_asm0(R11, r), make_asm0(R13, r), r), r);
				emit(make_asm3(MOVQ_MDB_TO_REG, make_literal(CONT_R14, r), make_asm0(R11, r), make_asm0(R14, r), r), r);
				emit(make_asm3(MOVQ_MDB_TO_REG, make_literal(CONT_R15, r), make_asm0(R11, r), make_asm0(R15, r), r), r);
				emit(make_asm3(MOVQ_MDB_TO_REG, make_literal(CONT_CIR, r), make_asm0(R11, r), make_asm0(R10, r), r), r);
				emit(make_asm3(MOVQ_MDB_TO_REG, make_literal(CONT_RBP, r), make_asm0(R11, r), make_asm0(RBP, r), r), r);
				emit(make_asm1(JMP_TO_REG, make_asm0(R10, r), r), r);
			}
			return container;
		} default: {
			return n;
		}
	}
}

//Must be used after use_return_reference and init_i386_registers
union expression *vgenerate_literals(union expression *n, region r) {
	if(n->base.type == literal && n->literal.return_symbol) {
		union expression *container = make_begin(nil(r), r);
		emit(make_asm2(MOVQ_IMM_TO_REG, make_literal(n->literal.value, r), make_asm0(R11, r), r), r);
		emit(make_store(make_asm0(R11, r), n->literal.return_symbol, 0, make_asm0(R13, r), r), r);
		return container;
	} else {
		return n;
	}
}

union expression *generate_toplevel(union expression *n, region r) {
	union expression *container = make_begin(nil(r), r);
	emit(make_asm1(PUSHQ_REG, make_asm0(R12, r), r), r);
	emit(make_asm1(PUSHQ_REG, make_asm0(R13, r), r), r);
	emit(make_asm1(PUSHQ_REG, make_asm0(R14, r), r), r);
	emit(make_asm1(PUSHQ_REG, make_asm0(R15, r), r), r);
	emit(make_asm1(PUSHQ_REG, make_asm0(RBX, r), r), r);
	emit(make_asm1(PUSHQ_REG, make_asm0(RBP, r), r), r);
	emit(make_asm2(MOVQ_REG_TO_REG, make_asm0(RSP, r), make_asm0(RBP, r), r), r);
	emit(n->function.expression, r);
	emit(make_asm0(LEAVE, r), r);
	emit(make_asm1(POPQ_REG, make_asm0(RBX, r), r), r);
	emit(make_asm1(POPQ_REG, make_asm0(R15, r), r), r);
	emit(make_asm1(POPQ_REG, make_asm0(R14, r), r), r);
	emit(make_asm1(POPQ_REG, make_asm0(R13, r), r), r);
	emit(make_asm1(POPQ_REG, make_asm0(R12, r), r), r);
	emit(make_asm0(RET, r), r);
	return container;
}

int get_current_offset(union expression *function) {
	if(length(function->function.local_symbols) > 0) {
		return ((struct symbol *) function->function.local_symbols->fst)->offset;
	} else {
		return 0;
	}
}

//Must be used after all local references have been made, i.e. after make_store_continuations
union expression *vgenerate_function_expressions(union expression *n, region r) {
	if(n->base.type == function && n->function.parent) {
		union expression *container = make_begin(nil(r), r);
		emit(make_load_address(n->function.reference->reference.symbol, make_asm0(R11, r), r), r);
		emit(make_store(make_asm0(R11, r), n->function.return_symbol, 0, make_asm0(R10, r), r), r);
		
		struct symbol *after_symbol = make_symbol(_function, NULL, r);
		
		emit(make_asm1(JMP_REL, make_asm1(STVAL_SUB_RIP_FROM_REF, use_symbol(after_symbol, r), r), r), r);
		if(root_function_of(n) == n->function.parent->begin.parent) {
			emit(make_asm1(GLOBAL_LABEL, n->function.reference, r), r);
		} else {
			emit(make_asm1(LOCAL_LABEL, n->function.reference, r), r);
		}
		
		//Insert first 6 parameters onto stack
		emit(make_asm1(POPQ_REG, make_asm0(R11, r), r), r);
		emit(make_asm1(PUSHQ_REG, make_asm0(R9, r), r), r);
		emit(make_asm1(PUSHQ_REG, make_asm0(R8, r), r), r);
		emit(make_asm1(PUSHQ_REG, make_asm0(RCX, r), r), r);
		emit(make_asm1(PUSHQ_REG, make_asm0(RDX, r), r), r);
		emit(make_asm1(PUSHQ_REG, make_asm0(RSI, r), r), r);
		emit(make_asm1(PUSHQ_REG, make_asm0(RDI, r), r), r);
		emit(make_asm1(PUSHQ_REG, make_asm0(R11, r), r), r);
		
		//Save callee-saved registers
		emit(make_asm1(PUSHQ_REG, make_asm0(R12, r), r), r);
		emit(make_asm1(PUSHQ_REG, make_asm0(R13, r), r), r);
		emit(make_asm1(PUSHQ_REG, make_asm0(R14, r), r), r);
		emit(make_asm1(PUSHQ_REG, make_asm0(R15, r), r), r);
		emit(make_asm1(PUSHQ_REG, make_asm0(RBX, r), r), r);
		
		emit(make_asm1(PUSHQ_REG, make_asm0(RBP, r), r), r);
		emit(make_asm2(MOVQ_REG_TO_REG, make_asm0(RSP, r), make_asm0(RBP, r), r), r);
		emit(make_asm2(SUBQ_IMM_FROM_REG, make_literal(-get_current_offset(n), r), make_asm0(RSP, r), r), r);
		
		//Execute the function body
		emit(n->function.expression, r);
		
		//Place the return value
		emit(make_load(n->function.expression_return_symbol, 0, make_asm0(RAX, r), make_asm0(R13, r), r), r);
		
		emit(make_asm0(LEAVE, r), r);
		//Restore callee-saved registers
		emit(make_asm1(POPQ_REG, make_asm0(RBX, r), r), r);
		emit(make_asm1(POPQ_REG, make_asm0(R15, r), r), r);
		emit(make_asm1(POPQ_REG, make_asm0(R14, r), r), r);
		emit(make_asm1(POPQ_REG, make_asm0(R13, r), r), r);
		emit(make_asm1(POPQ_REG, make_asm0(R12, r), r), r);
		
		emit(make_asm1(POPQ_REG, make_asm0(R11, r), r), r);
		emit(make_asm2(ADDQ_IMM_TO_REG, make_literal(6*WORD_SIZE, r), make_asm0(RSP, r), r), r);
		emit(make_asm1(PUSHQ_REG, make_asm0(R11, r), r), r);
		emit(make_asm0(RET, r), r);
		emit(make_asm1(LOCAL_LABEL, use_symbol(after_symbol, r), r), r);
		return container;
	} else if(n->base.type == invoke) {
		union expression *container = make_begin(nil(r), r);
		
		//Push arguments onto stack
		if(length(n->invoke.arguments) > 6) {
			union expression *t;
			foreach(t, reverse(n->invoke.arguments->rrrrrrst, r)) {
				emit(make_load(t->reference.symbol, 0, make_asm0(R11, r), make_asm0(R10, r), r), r);
				emit(make_asm1(PUSHQ_REG, make_asm0(R11, r), r), r);
			}
		}
		if(length(n->invoke.arguments) > 5) {
			emit(make_load(((union expression *) n->invoke.arguments->frrrrrst)->reference.symbol, 0, make_asm0(R9, r), make_asm0(R10, r), r), r);
		}
		if(length(n->invoke.arguments) > 4) {
			emit(make_load(((union expression *) n->invoke.arguments->frrrrst)->reference.symbol, 0, make_asm0(R8, r), make_asm0(R10, r), r), r);
		}
		if(length(n->invoke.arguments) > 3) {
			emit(make_load(((union expression *) n->invoke.arguments->frrrst)->reference.symbol, 0, make_asm0(RCX, r), make_asm0(R10, r), r), r);
		}
		if(length(n->invoke.arguments) > 2) {
			emit(make_load(((union expression *) n->invoke.arguments->frrst)->reference.symbol, 0, make_asm0(RDX, r), make_asm0(R10, r), r), r);
		}
		if(length(n->invoke.arguments) > 1) {
			emit(make_load(((union expression *) n->invoke.arguments->frst)->reference.symbol, 0, make_asm0(RSI, r), make_asm0(R10, r), r), r);
		}
		if(length(n->invoke.arguments) > 0) {
			emit(make_load(((union expression *) n->invoke.arguments->fst)->reference.symbol, 0, make_asm0(RDI, r), make_asm0(R10, r), r), r);
		}
		emit(make_asm2(MOVQ_IMM_TO_REG, make_literal(0, r), make_asm0(RAX, r), r), r);
		
		emit(make_load(n->invoke.reference->reference.symbol, 0, make_asm0(R11, r), make_asm0(R10, r), r), r);
		emit(make_asm1(CALL_REG, make_asm0(R11, r), r), r);
		
		emit(make_store(make_asm0(RAX, r), n->invoke.return_symbol, 0, make_asm0(R10, r), r), r);
		if(length(n->invoke.arguments) > 6) {
			emit(make_asm2(ADDQ_IMM_TO_REG, make_literal(WORD_SIZE * (length(n->invoke.arguments) - 6), r), make_asm0(RSP, r), r), r);
		}
		return container;
	} else {
		return n;
	}
}
