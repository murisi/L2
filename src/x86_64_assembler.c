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
#define MAX_INSTR_FIELDS 2
#define ALIGNMENT 8

void mem_write(char *mem, int *idx, void *bytes, int cnt) {
	int i, end = *idx + cnt;
	for(; *idx != end; (*idx)++, bytes++) {
		mem[*idx] = *((unsigned char *) bytes);
	}
}

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

void write_static_value(char *bin, int *pos, union expression *expr, int bytes, Elf64_Sym *symtab, Elf64_Rela **relas) {
	uint64_t val = 0;
	if(expr->base.type == literal) {
		val = expr->literal.value;
	} else if(expr->base.type == reference && bytes == 8) {
		(*relas)->r_offset = *pos;
		(*relas)->r_info = ELF64_R_INFO((Elf64_Sym *) expr->reference.referent->reference.context - symtab, R_X86_64_64);
		(*relas)->r_addend = 0;
		(*relas)++;
	} else if(expr->base.type == instruction && expr->instruction.opcode == STVAL_ADD_OFF_TO_REF && bytes == 8) {
		union expression *ref = fst(expr->instruction.arguments);
		union expression *offset = frst(expr->instruction.arguments);
		(*relas)->r_offset = *pos;
		(*relas)->r_info = ELF64_R_INFO((Elf64_Sym *) ref->reference.referent->reference.context - symtab, R_X86_64_64);
		(*relas)->r_addend = offset->literal.value;
		(*relas)++;
	} else if(expr->base.type == instruction && expr->instruction.opcode == STVAL_SUB_RIP_FROM_REF && bytes == 4) {
		union expression *ref = fst(expr->instruction.arguments);
		(*relas)->r_offset = *pos;
		(*relas)->r_info = ELF64_R_INFO((Elf64_Sym *) ref->reference.referent->reference.context - symtab, R_X86_64_PC32);
		(*relas)->r_addend = -bytes;
		(*relas)++;
	}
	mem_write(bin, pos, (char *) &val, bytes);
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

/*int count_labels(list generated_expressions) {
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
}*/

void assemble(list generated_expressions, unsigned char *bin, int *pos, Elf64_Sym *symtab, Elf64_Rela **relas) {
	*pos = 0;
	union expression *n;
	foreach(n, generated_expressions) {printf("%x ", *pos);
		switch(n->instruction.opcode) {
			case LOCAL_LABEL: case GLOBAL_LABEL:
				printf("%s:\n", fst_string(n));
				Elf64_Sym *sym = ((union expression *) fst(n->instruction.arguments))->reference.context;
				sym->st_value = *pos;
				break;
			case LEAQ_OF_MDB_INTO_REG: {
				printf("leaq %s(%s), %s\n", fst_string(n), frst_string(n), frrst_string(n));
				unsigned char opcode = 0x8D;
				int reg = ((union expression *) frrst(n->invoke.arguments))->instruction.opcode; //Dest
				int rm = ((union expression *) frst(n->invoke.arguments))->instruction.opcode; //Src
				write_mr_rm_instr(bin, pos, opcode, reg, rm, true, true);
				write_static_value(bin, pos, fst(n->invoke.arguments), 4, symtab, relas);
				break;
			} case MOVQ_FROM_REG_INTO_MDB: {
				printf("movq %s, %s(%s)\n", fst_string(n), frst_string(n), frrst_string(n));
				unsigned char opcode = 0x89;
				int reg = ((union expression *) fst(n->invoke.arguments))->instruction.opcode; //Src
				int rm = ((union expression *) frrst(n->invoke.arguments))->instruction.opcode; //Dest
				write_mr_rm_instr(bin, pos, opcode, reg, rm, true, true);
				write_static_value(bin, pos, frst(n->invoke.arguments), 4, symtab, relas);
				break;
			} case JMP_REL: {
				printf("jmp %s\n", fst_string(fst(n->instruction.arguments)));
				unsigned char opcode = 0xE9;
				mem_write(bin, pos, &opcode, 1);
				write_static_value(bin, pos, fst(n->invoke.arguments), 4, symtab, relas);
				break;
			} case MOVQ_MDB_TO_REG: {
				printf("movq %s(%s), %s\n", fst_string(n), frst_string(n), frrst_string(n));
				unsigned char opcode = 0x8B;
				int reg = ((union expression *) frrst(n->invoke.arguments))->instruction.opcode; //Dest
				int rm = ((union expression *) frst(n->invoke.arguments))->instruction.opcode; //Src
				write_mr_rm_instr(bin, pos, opcode, reg, rm, true, true);
				write_static_value(bin, pos, fst(n->invoke.arguments), 4, symtab, relas);
				break;
			} case PUSHQ_REG: {
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
			} case JMP_TO_REG: {
				printf("jmp *%s\n", fst_string(n));
				unsigned char opcode = 0xFF;
				unsigned char reg = 4;
				int rm = ((union expression *) fst(n->invoke.arguments))->instruction.opcode;
				write_mr_rm_instr(bin, pos, opcode, reg, rm, false, false);
				break;
			} case JE_REL: {
				printf("je %s\n", fst_string(fst(n->instruction.arguments)));
				unsigned char opcode1 = 0x0F;
				unsigned char opcode2 = 0x84;
				mem_write(bin, pos, &opcode1, 1);
				mem_write(bin, pos, &opcode2, 1);
				write_static_value(bin, pos, fst(n->invoke.arguments), 4, symtab, relas);
				break;
			} case ORQ_REG_TO_REG: {
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
				unsigned char REX = 4 << 4;
				REX |= (1 << REX_W);
				REX |= (0 << REX_R);
				REX |= (0 << REX_X);
				if(reg & 0x8) {
					REX |= (1 << REX_B);
				}
				mem_write(bin, pos, &REX, 1);
				mem_write(bin, pos, &opcoderd, 1);
				write_static_value(bin, pos, imm_expr, 8, symtab, relas);
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

void write_elf(list generated_expressions, list locals, list globals, int outfd) {
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
	ehdr.e_shnum = 7; //
	ehdr.e_shstrndx = 1;
	mywrite(outfd, &ehdr, sizeof(Elf64_Ehdr));
	
	int strtab_len = 1, sym_count = 1;
	union expression *e;
	{foreach(e, locals) {
		strtab_len += strlen(e->reference.name) + 1;
		sym_count++;
	}}
	{foreach(e, globals) {
		strtab_len += strlen(e->reference.name) + 1;
		sym_count++;
	}}
	{foreach(e, generated_expressions) {
		if(e->instruction.opcode == LOCAL_LABEL || e->instruction.opcode == GLOBAL_LABEL) {
			strtab_len += strlen(((union expression *) fst(e->instruction.arguments))->reference.name) + 1;
			sym_count++;
		}
	}}
	Elf64_Sym syms[sym_count];
	Elf64_Sym *sym_ptr = syms;
	//Mandatory undefined symbol
	sym_ptr->st_name = 0;
	sym_ptr->st_value = 0;
	sym_ptr->st_size = 0;
	sym_ptr->st_info = 0;
	sym_ptr->st_other = 0;
	sym_ptr->st_shndx = SHN_UNDEF;
	sym_ptr++;
	
	char strtab[round_size(strtab_len, ALIGNMENT)];
	char *strtabptr = strtab;
	*(strtabptr++) = '\0';
	
	{foreach(e, locals) {
		strcpy(strtabptr, e->reference.name);
		sym_ptr->st_name = strtabptr - strtab;
		sym_ptr->st_value = (sym_ptr - syms - 1) * WORD_SIZE;
		sym_ptr->st_size = 0;
		sym_ptr->st_info = ELF64_ST_INFO(STB_LOCAL, STT_NOTYPE);
		sym_ptr->st_other = 0;
		sym_ptr->st_shndx = 5;
		e->reference.context = sym_ptr;
		sym_ptr++;
		strtabptr += strlen(e->reference.name) + 1;
	}}
	{foreach(e, generated_expressions) {
		if(e->instruction.opcode == LOCAL_LABEL) {
			union expression *ref = (union expression *) fst(e->instruction.arguments);
			strcpy(strtabptr, ref->reference.name);
			sym_ptr->st_name = strtabptr - strtab;
			sym_ptr->st_value = 0;
			sym_ptr->st_size = 0;
			sym_ptr->st_info = ELF64_ST_INFO(STB_LOCAL, STT_NOTYPE);
			sym_ptr->st_other = 0;
			sym_ptr->st_shndx = 2;
			ref->reference.context = sym_ptr;
			sym_ptr++;
			strtabptr += strlen(ref->reference.name) + 1;
		}
	}}
	int local_symbol_count = sym_ptr - syms;
	{foreach(e, globals) {
		strcpy(strtabptr, e->reference.name);
		sym_ptr->st_name = strtabptr - strtab;
		sym_ptr->st_value = 0;
		sym_ptr->st_size = 0;
		sym_ptr->st_info = ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE);
		sym_ptr->st_other = 0;
		sym_ptr->st_shndx = SHN_UNDEF;
		e->reference.context = sym_ptr;
		sym_ptr++;
		strtabptr += strlen(e->reference.name) + 1;
	}}
	{foreach(e, generated_expressions) {
		if(e->instruction.opcode == GLOBAL_LABEL) {
			union expression *ref = (union expression *) fst(e->instruction.arguments);
			strcpy(strtabptr, ref->reference.name);
			sym_ptr->st_name = strtabptr - strtab;
			sym_ptr->st_value = 0;
			sym_ptr->st_size = 0;
			sym_ptr->st_info = ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE);
			sym_ptr->st_other = 0;
			sym_ptr->st_shndx = 2;
			ref->reference.context = sym_ptr;
			sym_ptr++;
			strtabptr += strlen(ref->reference.name) + 1;
		}
	}}
	
	int text_len;
	unsigned char text[round_size(MAX_INSTR_LEN * length(generated_expressions), ALIGNMENT)];
	Elf64_Rela relas[MAX_INSTR_FIELDS * length(generated_expressions)];
	Elf64_Rela *rela_ptr = relas;
	assemble(generated_expressions, text, &text_len, syms, &rela_ptr);
	printf("!: %p\n", rela_ptr - relas);
	
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
	
	char shstrtab[] = "\0.shstrtab\0.text\0.strtab\0.symtab\0.bss\0.rela.text";
	char shstrtab_padded[round_size(sizeof(shstrtab), ALIGNMENT)];
	memcpy(shstrtab_padded, shstrtab, sizeof(shstrtab));
	
	Elf64_Shdr shstrtab_shdr;
	shstrtab_shdr.sh_name = 1;
	shstrtab_shdr.sh_type = SHT_STRTAB;
	shstrtab_shdr.sh_flags = 0;
	shstrtab_shdr.sh_addr = 0;
	shstrtab_shdr.sh_offset = sizeof(Elf64_Ehdr) + 7*sizeof(Elf64_Shdr);
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
	text_shdr.sh_offset = sizeof(Elf64_Ehdr) + 7*sizeof(Elf64_Shdr) + sizeof(shstrtab_padded);
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
	strtab_shdr.sh_offset = sizeof(Elf64_Ehdr) + 7*sizeof(Elf64_Shdr) + sizeof(shstrtab_padded) + sizeof(text);
	strtab_shdr.sh_size = strtab_len;
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
	symtab_shdr.sh_offset = sizeof(Elf64_Ehdr) + 7*sizeof(Elf64_Shdr) + sizeof(shstrtab_padded) + sizeof(text) + sizeof(strtab);
	symtab_shdr.sh_size = sizeof(syms);
	symtab_shdr.sh_link = 3;
	symtab_shdr.sh_info = local_symbol_count;
	symtab_shdr.sh_addralign = 0;
	symtab_shdr.sh_entsize = sizeof(Elf64_Sym);
	mywrite(outfd, &symtab_shdr, sizeof(Elf64_Shdr));
	
	Elf64_Shdr bss_shdr;
	bss_shdr.sh_name = 1 + strlen(".shstrtab") + 1 + strlen(".text") + 1 + strlen(".strtab") + 1 + strlen(".symtab") + 1;
	bss_shdr.sh_type = SHT_NOBITS;
	bss_shdr.sh_flags = SHF_WRITE | SHF_ALLOC;
	bss_shdr.sh_addr = 0;
	bss_shdr.sh_offset = sizeof(Elf64_Ehdr) + 7*sizeof(Elf64_Shdr) + sizeof(shstrtab_padded) + sizeof(text) + sizeof(strtab) + sizeof(syms);
	bss_shdr.sh_size = length(locals) * WORD_SIZE;
	bss_shdr.sh_link = SHN_UNDEF;
	bss_shdr.sh_info = 0;
	bss_shdr.sh_addralign = WORD_SIZE;
	bss_shdr.sh_entsize = 0;
	mywrite(outfd, &bss_shdr, sizeof(Elf64_Shdr));
	
	Elf64_Shdr rela_shdr;
	rela_shdr.sh_name = 1 + strlen(".shstrtab") + 1 + strlen(".text") + 1 + strlen(".strtab") + 1 + strlen(".symtab") + 1 + strlen(".bss") + 1;
	rela_shdr.sh_type = SHT_RELA;
	rela_shdr.sh_flags = 0;
	rela_shdr.sh_addr = 0;
	rela_shdr.sh_offset = sizeof(Elf64_Ehdr) + 7*sizeof(Elf64_Shdr) + sizeof(shstrtab_padded) + sizeof(text) + sizeof(strtab) + sizeof(syms);
	rela_shdr.sh_size = (rela_ptr - relas) * sizeof(Elf64_Rela);
	rela_shdr.sh_link = 4;
	rela_shdr.sh_info = 2;
	rela_shdr.sh_addralign = 0;
	rela_shdr.sh_entsize = sizeof(Elf64_Rela);
	mywrite(outfd, &rela_shdr, sizeof(Elf64_Shdr));
	
	mywrite(outfd, shstrtab_padded, sizeof(shstrtab_padded));
	mywrite(outfd, text, sizeof(text));
	mywrite(outfd, strtab, sizeof(strtab));
	mywrite(outfd, syms, sizeof(syms));
	mywrite(outfd, relas, (rela_ptr - relas) * sizeof(Elf64_Rela));
}
