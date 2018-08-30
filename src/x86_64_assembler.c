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
#define ALIGNMENT 8

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

int count_labels(list generated_expressions) {
	int label_count = 0;
	union expression *n;
	foreach(n, generated_expressions) {
		switch(n->instruction.opcode) {
			case LABEL:
				label_count++;
				break;
		}
	}
	return label_count;
}

void assemble(list generated_expressions, char *strtab, unsigned char *bin, int *pos, Elf64_Sym *symtab) {
	*pos = 0;
	union expression *n;
	foreach(n, generated_expressions) {printf("%x ", *pos);
		switch(n->instruction.opcode) {
			case LABEL:
				printf("%s:\n", fst_string(n));
				symtab->st_name = fst_string(n)-strtab;
				symtab->st_value = *pos;
				symtab->st_size = 0;
				symtab->st_info = ELF64_ST_INFO(STB_LOCAL, STT_NOTYPE);
				symtab->st_other = 0;
				symtab->st_shndx = 2;
				symtab++;
				break;
			case LEAQ_OF_MDB_INTO_REG: {
				printf("leaq %s(%s), %s\n", fst_string(n), frst_string(n), frrst_string(n));
				unsigned char opcode = 0x8D;
				int reg = ((union expression *) frrst(n->invoke.arguments))->instruction.opcode; //Dest
				int rm = ((union expression *) frst(n->invoke.arguments))->instruction.opcode; //Src
				write_mr_rm_instr(bin, pos, opcode, reg, rm, true, true);
				write_displacement(bin, pos, fst(n->invoke.arguments));
				break;
			} case MOVQ_FROM_REG_INTO_MDB: {
				printf("movq %s, %s(%s)\n", fst_string(n), frst_string(n), frrst_string(n));
				unsigned char opcode = 0x89;
				int reg = ((union expression *) fst(n->invoke.arguments))->instruction.opcode; //Src
				int rm = ((union expression *) frrst(n->invoke.arguments))->instruction.opcode; //Dest
				write_mr_rm_instr(bin, pos, opcode, reg, rm, true, true);
				write_displacement(bin, pos, frst(n->invoke.arguments));
				break;
			} case JMP_REL:
				//printf("jmp %s\n", fst_string(n));
				//unsigned char opcode = 0xE9;
				//mem_write(bin, pos, &opcode, 1);
				break;
			case MOVQ_MDB_TO_REG:
				printf("movq %s(%s), %s\n", fst_string(n), frst_string(n), frrst_string(n));
				unsigned char opcode = 0x8B;
				int reg = ((union expression *) frrst(n->invoke.arguments))->instruction.opcode; //Dest
				int rm = ((union expression *) frst(n->invoke.arguments))->instruction.opcode; //Src
				write_mr_rm_instr(bin, pos, opcode, reg, rm, true, true);
				write_displacement(bin, pos, fst(n->invoke.arguments));
				break;
			case PUSHQ_REG: {
				printf("pushq %s\n", fst_string(n));
				unsigned char opcode = 0x50;
				int reg = ((union expression *) fst(n->invoke.arguments))->instruction.opcode; //Dest
				write_o_instr(bin, pos, opcode, reg);
				break;
			} case MOVQ_REG_TO_REG: {
				printf("movq %s, %s\n", fst_string(n), frst_string(n));
				unsigned char opcode = 0x8B;
				int reg = ((union expression *) frst(n->invoke.arguments))->instruction.opcode; //Dest
				int rm = ((union expression *) fst(n->invoke.arguments))->instruction.opcode; //Src
				write_mr_rm_instr(bin, pos, opcode, reg, rm, false, true);
				break;
			} case SUBQ_IMM_FROM_REG: {
				printf("subq $%s, %s\n", fst_string(n), frst_string(n));
				unsigned char opcode = 0x81;
				unsigned char reg = 5;
				int rm = ((union expression *) frst(n->invoke.arguments))->instruction.opcode; //Dest
				write_mr_rm_instr(bin, pos, opcode, reg, rm, false, true);
				mem_write(bin, pos, &((union expression *) fst(n->invoke.arguments))->literal.value, 4);
				break;
			} case ADDQ_IMM_TO_REG: {
				printf("addq $%s, %s\n", fst_string(n), frst_string(n));
				unsigned char opcode = 0x81;
				unsigned char reg = 0;
				int rm = ((union expression *) frst(n->invoke.arguments))->instruction.opcode; //Dest
				write_mr_rm_instr(bin, pos, opcode, reg, rm, false, true);
				mem_write(bin, pos, &((union expression *) fst(n->invoke.arguments))->literal.value, 4);
				break;
			} case POPQ_REG: {
				printf("popq %s\n", fst_string(n));
				unsigned char opcode = 0x58;
				int reg = ((union expression *) fst(n->invoke.arguments))->instruction.opcode; //Dest
				write_o_instr(bin, pos, opcode, reg);
				break;
			} case LEAVE: {
				printf("leave\n");
				unsigned char opcode = 0xC9;
				mem_write(bin, pos, &opcode, 1);
				break;
			} case RET: {
				printf("ret\n");
				unsigned char opcode = 0xC3;
				mem_write(bin, pos, &opcode, 1);
				break;
			} case CALL_REL:
				//fprintf(out, "call %s", fst_string(n));
				break;
			case JMP_TO_REG: {
				printf("jmp *%s\n", fst_string(n));
				unsigned char opcode = 0xFF;
				unsigned char reg = 4;
				int rm = ((union expression *) fst(n->invoke.arguments))->instruction.opcode;
				write_mr_rm_instr(bin, pos, opcode, reg, rm, false, false);
				break;
			} case JE_REL:
				//fprintf(out, "je %s", fst_string(n));
				break;
			case ORQ_REG_TO_REG: {
				printf("orq %s, %s\n", fst_string(n), frst_string(n));
				char opcode = 0x0B;
				int reg = ((union expression *) frst(n->invoke.arguments))->instruction.opcode; //Dest
				int rm = ((union expression *) fst(n->invoke.arguments))->instruction.opcode; //Src
				write_mr_rm_instr(bin, pos, opcode, reg, rm, false, true);
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
				mem_write(bin, pos, &REX, 1);
				mem_write(bin, pos, &opcoderd, 1);
				mem_write(bin, pos, &imm, 8);
				break;
			} case CALL_REG: {
				printf("call *%s\n", fst_string(n));
				unsigned char opcode = 0xFF;
				unsigned char reg = 2;
				int rm = ((union expression *) fst(n->invoke.arguments))->instruction.opcode;
				write_mr_rm_instr(bin, pos, opcode, reg, rm, false, false);
				break;
			}
		}
	}
}

int round_size(int x, int nearest) {
	return x + (nearest - (x % nearest));
}

void write_elf(union expression *program, int outfd) {
	Elf64_Ehdr ehdr;
	ehdr.e_ident[EI_MAG0] = ELFMAG0;
	ehdr.e_ident[EI_MAG1] = ELFMAG1;
	ehdr.e_ident[EI_MAG2] = ELFMAG2;
	ehdr.e_ident[EI_MAG3] = ELFMAG3;
	ehdr.e_ident[EI_CLASS] = ELFCLASS64;
	ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
	ehdr.e_ident[EI_VERSION] = 1;
	ehdr.e_ident[EI_OSABI] = ELFOSABI_SYSV;
	ehdr.e_ident[EI_ABIVERSION] = 0;
	int i;
	for(i = 7; i < EI_NIDENT; i++) {
		ehdr.e_ident[i] = 0;
	}
	ehdr.e_type = ET_REL;
	ehdr.e_machine = EM_X86_64;
	ehdr.e_version = 1;
	ehdr.e_entry = 0;
	ehdr.e_phoff = 0;
	ehdr.e_shoff = sizeof(Elf64_Ehdr);
	ehdr.e_ehsize = sizeof(Elf64_Ehdr);
	ehdr.e_phentsize = sizeof(Elf64_Phdr);
	ehdr.e_phnum = 0;
	ehdr.e_shentsize = sizeof(Elf64_Shdr);
	ehdr.e_shnum = 5; //
	ehdr.e_shstrndx = 1;
	mywrite(outfd, &ehdr, sizeof(Elf64_Ehdr));
	
	int total_references_len = 0;
	visit_assembly_visitor = vmeasure_references;
	visit_assembly(&program, &total_references_len);
	char strtab[round_size(1 + total_references_len, ALIGNMENT)];
	strtab[0] = '\0';
	char *strtabptr = strtab + 1;
	visit_assembly_visitor = vuse_string_table;
	visit_assembly(&program, &strtabptr);
	
	Elf64_Sym syms[count_labels(program->begin.expressions)+1];
	//Mandatory undefined symbol
	syms[0].st_name = 0;
	syms[0].st_value = 0;
	syms[0].st_size = 0;
	syms[0].st_info = 0;
	syms[0].st_other = 0;
	syms[0].st_shndx = SHN_UNDEF;
	
	int text_len;
	unsigned char text[round_size(MAX_INSTR_LEN * length(program->begin.expressions), ALIGNMENT)];
	assemble(program->begin.expressions, strtab, text, &text_len, syms+1);
	
	//Mandatory undefined section
	Elf64_Shdr undef_shdr;
	undef_shdr.sh_name = 0;
	undef_shdr.sh_type = SHT_NULL;
	undef_shdr.sh_flags = 0;
	undef_shdr.sh_addr = 0;
	undef_shdr.sh_offset = 0;
	undef_shdr.sh_size = 0;
	undef_shdr.sh_link = SHN_UNDEF;
	undef_shdr.sh_info = 0;
	undef_shdr.sh_addralign = 0;
	undef_shdr.sh_entsize = 0;
	mywrite(outfd, &undef_shdr, sizeof(Elf64_Shdr));
	
	char shstrtab[] = "\0.shstrtab\0.text\0.strtab\0.symtab";
	char shstrtab_padded[round_size(sizeof(shstrtab), ALIGNMENT)];
	memcpy(shstrtab_padded, shstrtab, sizeof(shstrtab));
	
	Elf64_Shdr shstrtab_shdr;
	shstrtab_shdr.sh_name = 1;
	shstrtab_shdr.sh_type = SHT_STRTAB;
	shstrtab_shdr.sh_flags = 0;
	shstrtab_shdr.sh_addr = 0;
	shstrtab_shdr.sh_offset = sizeof(Elf64_Ehdr) + 5*sizeof(Elf64_Shdr);
	shstrtab_shdr.sh_size = sizeof(shstrtab);
	shstrtab_shdr.sh_link = SHN_UNDEF;
	shstrtab_shdr.sh_info = 0;
	shstrtab_shdr.sh_addralign = 0;
	shstrtab_shdr.sh_entsize = 0;
	mywrite(outfd, &shstrtab_shdr, sizeof(Elf64_Shdr));
	
	Elf64_Shdr text_shdr;
	text_shdr.sh_name = 1 + strlen(".shstrtab") + 1;
	text_shdr.sh_type = SHT_PROGBITS;
	text_shdr.sh_flags = SHF_ALLOC | SHF_EXECINSTR;
	text_shdr.sh_addr = 0;
	text_shdr.sh_offset = sizeof(Elf64_Ehdr) + 5*sizeof(Elf64_Shdr) + sizeof(shstrtab_padded);
	text_shdr.sh_size = text_len;
	text_shdr.sh_link = SHN_UNDEF;
	text_shdr.sh_info = 0;
	text_shdr.sh_addralign = 1;
	text_shdr.sh_entsize = 0;
	mywrite(outfd, &text_shdr, sizeof(Elf64_Shdr));
	
	Elf64_Shdr strtab_shdr;
	strtab_shdr.sh_name = 1 + strlen(".shstrtab") + 1 + strlen(".text") + 1;
	strtab_shdr.sh_type = SHT_STRTAB;
	strtab_shdr.sh_flags = 0;
	strtab_shdr.sh_addr = 0;
	strtab_shdr.sh_offset = sizeof(Elf64_Ehdr) + 5*sizeof(Elf64_Shdr) + sizeof(shstrtab_padded) + sizeof(text);
	strtab_shdr.sh_size = 1 + total_references_len;
	strtab_shdr.sh_link = SHN_UNDEF;
	strtab_shdr.sh_info = 0;
	strtab_shdr.sh_addralign = 0;
	strtab_shdr.sh_entsize = 0;
	mywrite(outfd, &strtab_shdr, sizeof(Elf64_Shdr));
	
	Elf64_Shdr symtab_shdr;
	symtab_shdr.sh_name = 1 + strlen(".shstrtab") + 1 + strlen(".text") + 1 + strlen(".strtab") + 1;
	symtab_shdr.sh_type = SHT_SYMTAB;
	symtab_shdr.sh_flags = 0;
	symtab_shdr.sh_addr = 0;
	symtab_shdr.sh_offset = sizeof(Elf64_Ehdr) + 5*sizeof(Elf64_Shdr) + sizeof(shstrtab_padded) + sizeof(text) + sizeof(strtab);
	symtab_shdr.sh_size = sizeof(syms);
	symtab_shdr.sh_link = 3;
	symtab_shdr.sh_info = count_labels(program->begin.expressions) + 1; //
	symtab_shdr.sh_addralign = 0;
	symtab_shdr.sh_entsize = sizeof(Elf64_Sym);
	mywrite(outfd, &symtab_shdr, sizeof(Elf64_Shdr));
	
	mywrite(outfd, shstrtab_padded, sizeof(shstrtab_padded));
	mywrite(outfd, text, sizeof(text));
	mywrite(outfd, strtab, sizeof(strtab));
	mywrite(outfd, syms, sizeof(syms));
}
