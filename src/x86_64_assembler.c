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

void mem_write(char *mem, int *idx, void *bytes, int cnt) {
	int i, end = *idx + cnt;
	for(; *idx != end; (*idx)++, bytes++) {
		mem[*idx] = *((unsigned char *) bytes);
	}
}

//static value

void write_mr_rm_instr(char *bin, int *pos, char opcode, int reg, int rm, bool m, bool rexw) {
	char mod = m ? 2 : 3;
	char modrm = (mod << MOD) | ((reg & 0x7) << REG) | ((rm & 0x7) << RM);
	int has_SIB = m && (rm == RSP || rm == R12);
	char SIB;
	if(has_SIB) {
		SIB = (4 << INDEX) | ((rm & 0x7) << BASE) | (0 << SS);
	}
	char REX = 4 << 4;
	if(rexw) {
		REX |= (1 << REX_W);
	}
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
}

void write_displacement(char *bin, int *pos, union expression *disp_expr) {
	uint32_t disp;
	if(disp_expr->base.type == literal) {
		disp = disp_expr->literal.value;
	}
	mem_write(bin, pos, (char *) &disp, 4);
}

void write_o_instr(char *bin, int *pos, unsigned char opcode, int reg) {
	unsigned char rd = (0x7 & reg);
	unsigned char opcoderd = opcode + rd;
	unsigned char REX = 4 << 4;
	REX |= (0 << REX_W);
	REX |= (0 << REX_R);
	REX |= (0 << REX_X);
	if(reg & 0x8) {
		REX |= (1 << REX_B);
		mem_write(bin, pos, &REX, 1);
	}
	mem_write(bin, pos, &opcoderd, 1);
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
				unsigned char opcode = 0x8D;
				int reg = ((union expression *) frrst(n->invoke.arguments))->instruction.opcode; //Dest
				int rm = ((union expression *) frst(n->invoke.arguments))->instruction.opcode; //Src
				write_mr_rm_instr(bin, &pos, opcode, reg, rm, true, true);
				write_displacement(bin, &pos, fst(n->invoke.arguments));
				break;
			} case MOVQ_FROM_REG_INTO_MDB: {
				printf("movq %s, %s(%s)\n", fst_string(n), frst_string(n), frrst_string(n));
				unsigned char opcode = 0x89;
				int reg = ((union expression *) fst(n->invoke.arguments))->instruction.opcode; //Src
				int rm = ((union expression *) frrst(n->invoke.arguments))->instruction.opcode; //Dest
				write_mr_rm_instr(bin, &pos, opcode, reg, rm, true, true);
				write_displacement(bin, &pos, frst(n->invoke.arguments));
				break;
			} case JMP_REL:
				//fprintf(out, "jmp %s", fst_string(n));
				break;
			case MOVQ_MDB_TO_REG:
				printf("movq %s(%s), %s\n", fst_string(n), frst_string(n), frrst_string(n));
				unsigned char opcode = 0x8B;
				int reg = ((union expression *) frrst(n->invoke.arguments))->instruction.opcode; //Dest
				int rm = ((union expression *) frst(n->invoke.arguments))->instruction.opcode; //Src
				write_mr_rm_instr(bin, &pos, opcode, reg, rm, true, true);
				write_displacement(bin, &pos, fst(n->invoke.arguments));
				break;
			case PUSHQ_REG: {
				printf("pushq %s\n", fst_string(n));
				unsigned char opcode = 0x50;
				int reg = ((union expression *) fst(n->invoke.arguments))->instruction.opcode; //Dest
				write_o_instr(bin, &pos, opcode, reg);
				break;
			} case MOVQ_REG_TO_REG: {
				printf("movq %s, %s\n", fst_string(n), frst_string(n));
				unsigned char opcode = 0x8B;
				int reg = ((union expression *) frst(n->invoke.arguments))->instruction.opcode; //Dest
				int rm = ((union expression *) fst(n->invoke.arguments))->instruction.opcode; //Src
				write_mr_rm_instr(bin, &pos, opcode, reg, rm, false, true);
				break;
			} case SUBQ_IMM_FROM_REG: {
				printf("subq $%s, %s\n", fst_string(n), frst_string(n));
				unsigned char opcode = 0x81;
				unsigned char reg = 5;
				int rm = ((union expression *) frst(n->invoke.arguments))->instruction.opcode; //Dest
				write_mr_rm_instr(bin, &pos, opcode, reg, rm, false, true);
				mem_write(bin, &pos, &((union expression *) fst(n->invoke.arguments))->literal.value, 4);
				break;
			} case ADDQ_IMM_TO_REG: {
				printf("addq $%s, %s\n", fst_string(n), frst_string(n));
				unsigned char opcode = 0x81;
				unsigned char reg = 0;
				int rm = ((union expression *) frst(n->invoke.arguments))->instruction.opcode; //Dest
				write_mr_rm_instr(bin, &pos, opcode, reg, rm, false, true);
				mem_write(bin, &pos, &((union expression *) fst(n->invoke.arguments))->literal.value, 4);
				break;
			} case POPQ_REG: {
				printf("popq %s\n", fst_string(n));
				unsigned char opcode = 0x58;
				int reg = ((union expression *) fst(n->invoke.arguments))->instruction.opcode; //Dest
				write_o_instr(bin, &pos, opcode, reg);
				break;
			} case LEAVE: {
				printf("leave\n");
				unsigned char opcode = 0xC9;
				mem_write(bin, &pos, &opcode, 1);
				break;
			} case RET: {
				printf("ret\n");
				unsigned char opcode = 0xC3;
				mem_write(bin, &pos, &opcode, 1);
				break;
			} case CALL_REL:
				//fprintf(out, "call %s", fst_string(n));
				break;
			case JMP_TO_REG: {
				printf("jmp *%s\n", fst_string(n));
				unsigned char opcode = 0xFF;
				unsigned char reg = 4;
				int rm = ((union expression *) fst(n->invoke.arguments))->instruction.opcode;
				write_mr_rm_instr(bin, &pos, opcode, reg, rm, false, false);
				break;
			} case JE_REL:
				//fprintf(out, "je %s", fst_string(n));
				break;
			case ORQ_REG_TO_REG: {
				printf("orq %s, %s\n", fst_string(n), frst_string(n));
				char opcode = 0x0B;
				int reg = ((union expression *) frst(n->invoke.arguments))->instruction.opcode; //Dest
				int rm = ((union expression *) fst(n->invoke.arguments))->instruction.opcode; //Src
				write_mr_rm_instr(bin, &pos, opcode, reg, rm, false, true);
				break;
			} case MOVQ_IMM_TO_REG: {
				printf("movq $%s, %s\n", fst_string(n), frst_string(n));
				unsigned char opcode = 0xB8;
				union expression *imm_expr = ((union expression *) fst(n->invoke.arguments));
				int reg = ((union expression *) frst(n->invoke.arguments))->instruction.opcode; //Dest
				unsigned char rd = (0x7 & reg);
				unsigned char opcoderd = opcode + rd;
				uint64_t imm = 0;
				if(imm_expr->base.type == literal) {
					imm = imm_expr->literal.value;
				}
				unsigned char REX = 4 << 4;
				REX |= (1 << REX_W);
				REX |= (0 << REX_R);
				REX |= (0 << REX_X);
				if(reg & 0x8) {
					REX |= (1 << REX_B);
				}
				mem_write(bin, &pos, &REX, 1);
				mem_write(bin, &pos, &opcoderd, 1);
				mem_write(bin, &pos, &imm, 8);
				break;
			} case CALL_REG: {
				printf("call *%s\n", fst_string(n));
				unsigned char opcode = 0xFF;
				unsigned char reg = 2;
				int rm = ((union expression *) fst(n->invoke.arguments))->instruction.opcode;
				write_mr_rm_instr(bin, &pos, opcode, reg, rm, false, false);
				break;
			}
		}
	}
	int fd = myopen("mytest");
	mywrite(fd, bin, pos);
	myclose(fd);
}
