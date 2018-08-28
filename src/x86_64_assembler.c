#define MOD 6
#define REG 3
#define RM 0
#define BASE 0
#define INDEX 3
#define SS 6
#define REX_W 3
#define REX_R 2
#define REX_X 1
#define REX_B 0
#define MAX_INSTR_LEN 15

void mem_write(char *mem, int *idx, char *bytes, int cnt) {
	int i, end = *idx + cnt;
	for(; *idx != end; (*idx)++, bytes++) {
		mem[*idx] = *bytes;
	}
}

void write_mr_rm_instr(char *bin, int *pos, char opcode, int reg, int rm, bool m, union expression *disp_expr) {
	char mod = m ? 2 : 3;
	char modrm = (mod << MOD) | ((reg & 0x7) << REG) | ((rm & 0x7) << RM);
	int has_SIB = m && (rm == RSP || rm == R12);
	char SIB;
	if(has_SIB) {
		SIB = (4 << INDEX) | ((rm & 0x7) << BASE) | (0 << SS);
	}
	uint32_t disp;
	if(m) {
		if(disp_expr->base.type == literal) {
			disp = disp_expr->literal.value;
		}
	}
	char REX = 4 << 4;
	REX |= (1 << REX_W);
	if(reg & 0x8) {
		REX |= (1 << REX_R);
	}
	REX |= (0 << REX_X);
	if(rm & 0x8) {
		REX |= (1 << REX_B);
	}
	mem_write(bin, pos, &REX, 1);
	mem_write(bin, pos, &opcode, 1);
	mem_write(bin, pos, &modrm, 1);
	if(has_SIB) {
		mem_write(bin, pos, &SIB, 1);
	}
	if(m) {
		mem_write(bin, pos, (char *) &disp, 4);
	}
}

void assemble(list generated_expressions, FILE *out) {
	int pos = 0;
	char bin[MAX_INSTR_LEN * length(generated_expressions)];
	union expression *n;
	foreach(n, generated_expressions) {
		switch(n->instruction.opcode) {
			case LABEL:
				//fprintf(out, "%s:", fst_string(n));
				break;
			case LEAQ_OF_MDB_INTO_REG: {
				printf("leaq %s(%s), %s\n", fst_string(n), frst_string(n), frrst_string(n));
				char opcode = 0x8D;
				int reg = ((union expression *) frrst(n->invoke.arguments))->instruction.opcode; //Dest
				int rm = ((union expression *) frst(n->invoke.arguments))->instruction.opcode; //Src
				write_mr_rm_instr(bin, &pos, opcode, reg, rm, true, (union expression *) fst(n->invoke.arguments));
				break;
			} case MOVQ_FROM_REG_INTO_MDB: {
				printf("movq %s, %s(%s)\n", fst_string(n), frst_string(n), frrst_string(n));
				char opcode = 0x89;
				int reg = ((union expression *) fst(n->invoke.arguments))->instruction.opcode; //Src
				int rm = ((union expression *) frrst(n->invoke.arguments))->instruction.opcode; //Dest
				write_mr_rm_instr(bin, &pos, opcode, reg, rm, true, (union expression *) frst(n->invoke.arguments));
				break;
			} case JMP_REL:
				//fprintf(out, "jmp %s", fst_string(n));
				break;
			case MOVQ_MDB_TO_REG:
				printf("movq %s(%s), %s\n", fst_string(n), frst_string(n), frrst_string(n));
				char opcode = 0x8B;
				int reg = ((union expression *) frrst(n->invoke.arguments))->instruction.opcode; //Dest
				int rm = ((union expression *) frst(n->invoke.arguments))->instruction.opcode; //Src
				write_mr_rm_instr(bin, &pos, opcode, reg, rm, true, (union expression *) fst(n->invoke.arguments));
				break;
			case PUSHQ_REG:
				//fprintf(out, "pushq %s", fst_string(n));
				break;
			case MOVQ_REG_TO_REG: {
				printf("movq %s, %s\n", fst_string(n), frst_string(n));
				char opcode = 0x8B;
				int reg = ((union expression *) frst(n->invoke.arguments))->instruction.opcode; //Dest
				int rm = ((union expression *) fst(n->invoke.arguments))->instruction.opcode; //Src
				write_mr_rm_instr(bin, &pos, opcode, reg, rm, false, NULL);
				break;
			} case SUBQ_IMM_FROM_REG:
				//fprintf(out, "subq $%s, %s", fst_string(n), frst_string(n));
				break;
			case ADDQ_IMM_TO_REG:
				//fprintf(out, "addq $%s, %s", fst_string(n), frst_string(n));
				break;
			case POPQ_REG:
				//fprintf(out, "popq %s", fst_string(n));
				break;
			case LEAVE:
				//fprintf(out, "leave");
				break;
			case RET:
				//fprintf(out, "ret");
				break;
			case CALL_REL:
				//fprintf(out, "call %s", fst_string(n));
				break;
			case JMP_TO_REG:
				//fprintf(out, "jmp *%s", fst_string(n));
				break;
			case JE_REL:
				//fprintf(out, "je %s", fst_string(n));
				break;
			case ORQ_REG_TO_REG: {
				printf("orq %s, %s\n", fst_string(n), frst_string(n));
				char opcode = 0x0B;
				int reg = ((union expression *) frst(n->invoke.arguments))->instruction.opcode; //Dest
				int rm = ((union expression *) fst(n->invoke.arguments))->instruction.opcode; //Src
				write_mr_rm_instr(bin, &pos, opcode, reg, rm, false, NULL);
				break;
			} case MOVQ_IMM_TO_REG:
				//fprintf(out, "movq $%s, %s", fst_string(n), frst_string(n));
				break;
			case GLOBAL:
				//fprintf(out, ".global %s", fst_string(n));
				break;
			case MOVQ_MD_TO_REG:
				//fprintf(out, "movq %s, %s", fst_string(n), frst_string(n));
				break;
			case MOVQ_REG_TO_MD:
				//fprintf(out, "movq %s, %s", fst_string(n), frst_string(n));
				break;
			case LEAQ_OF_MD_INTO_REG:
				//fprintf(out, "leaq %s, %s", fst_string(n), frst_string(n));
				break;
			case CALL_REG:
				//fprintf(out, "call *%s", fst_string(n));
				break;
			case SKIP_EXPR_EXPR:
				//fprintf(out, ".skip %s, %s", fst_string(n), frst_string(n));
				break;
			case ALIGN_EXPR_EXPR:
				//fprintf(out, ".align %s, %s", fst_string(n), frst_string(n));
				break;
			case BSS:
				//fprintf(out, ".bss");
				break;
			case TEXT:
				//fprintf(out, ".text");
				break;
		}
	}
	int fd = myopen("mytest");
	mywrite(fd, bin, pos);
	myclose(fd);
}
