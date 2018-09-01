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
		union expression *ref = expr->instruction.arguments->fst;
		union expression *offset = expr->instruction.arguments->frst;
		(*relas)->r_offset = *pos;
		(*relas)->r_info = ELF64_R_INFO((Elf64_Sym *) ref->reference.referent->reference.context - symtab, R_X86_64_64);
		(*relas)->r_addend = offset->literal.value;
		(*relas)++;
	} else if(expr->base.type == instruction && expr->instruction.opcode == STVAL_SUB_RIP_FROM_REF && bytes == 4) {
		union expression *ref = expr->instruction.arguments->fst;
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

void assemble(list generated_expressions, unsigned char *bin, int *pos, Elf64_Sym *symtab, Elf64_Rela **relas) {
	*pos = 0;
	union expression *n;
	foreach(n, generated_expressions) {
		switch(n->instruction.opcode) {
			case LOCAL_LABEL: case GLOBAL_LABEL: {
				Elf64_Sym *sym = ((union expression *) n->instruction.arguments->fst)->reference.context;
				sym->st_value = *pos;
				break;
			} case LEAQ_OF_MDB_INTO_REG: {
				unsigned char opcode = 0x8D;
				int reg = ((union expression *) n->invoke.arguments->frrst)->instruction.opcode; //Dest
				int rm = ((union expression *) n->invoke.arguments->frst)->instruction.opcode; //Src
				write_mr_rm_instr(bin, pos, opcode, reg, rm, true, true);
				write_static_value(bin, pos, n->invoke.arguments->fst, 4, symtab, relas);
				break;
			} case MOVQ_FROM_REG_INTO_MDB: {
				unsigned char opcode = 0x89;
				int reg = ((union expression *) n->invoke.arguments->fst)->instruction.opcode; //Src
				int rm = ((union expression *) n->invoke.arguments->frrst)->instruction.opcode; //Dest
				write_mr_rm_instr(bin, pos, opcode, reg, rm, true, true);
				write_static_value(bin, pos, n->invoke.arguments->frst, 4, symtab, relas);
				break;
			} case JMP_REL: {
				unsigned char opcode = 0xE9;
				mem_write(bin, pos, &opcode, 1);
				write_static_value(bin, pos, n->invoke.arguments->fst, 4, symtab, relas);
				break;
			} case MOVQ_MDB_TO_REG: {
				unsigned char opcode = 0x8B;
				int reg = ((union expression *) n->invoke.arguments->frrst)->instruction.opcode; //Dest
				int rm = ((union expression *) n->invoke.arguments->frst)->instruction.opcode; //Src
				write_mr_rm_instr(bin, pos, opcode, reg, rm, true, true);
				write_static_value(bin, pos, n->invoke.arguments->fst, 4, symtab, relas);
				break;
			} case PUSHQ_REG: {
				unsigned char opcode = 0x50;
				int reg = ((union expression *) n->invoke.arguments->fst)->instruction.opcode; //Dest
				write_o_instr(bin, pos, opcode, reg);
				break;
			} case MOVQ_REG_TO_REG: {
				unsigned char opcode = 0x8B;
				int reg = ((union expression *) n->invoke.arguments->frst)->instruction.opcode; //Dest
				int rm = ((union expression *) n->invoke.arguments->fst)->instruction.opcode; //Src
				write_mr_rm_instr(bin, pos, opcode, reg, rm, false, true);
				break;
			} case SUBQ_IMM_FROM_REG: {
				unsigned char opcode = 0x81;
				unsigned char reg = 5;
				int rm = ((union expression *) n->invoke.arguments->frst)->instruction.opcode; //Dest
				write_mr_rm_instr(bin, pos, opcode, reg, rm, false, true);
				mem_write(bin, pos, &((union expression *) n->invoke.arguments->fst)->literal.value, 4);
				break;
			} case ADDQ_IMM_TO_REG: {
				unsigned char opcode = 0x81;
				unsigned char reg = 0;
				int rm = ((union expression *) n->invoke.arguments->frst)->instruction.opcode; //Dest
				write_mr_rm_instr(bin, pos, opcode, reg, rm, false, true);
				mem_write(bin, pos, &((union expression *) n->invoke.arguments->fst)->literal.value, 4);
				break;
			} case POPQ_REG: {
				unsigned char opcode = 0x58;
				int reg = ((union expression *) n->invoke.arguments->fst)->instruction.opcode; //Dest
				write_o_instr(bin, pos, opcode, reg);
				break;
			} case LEAVE: {
				unsigned char opcode = 0xC9;
				mem_write(bin, pos, &opcode, 1);
				break;
			} case RET: {
				unsigned char opcode = 0xC3;
				mem_write(bin, pos, &opcode, 1);
				break;
			} case JMP_TO_REG: {
				unsigned char opcode = 0xFF;
				unsigned char reg = 4;
				int rm = ((union expression *) n->invoke.arguments->fst)->instruction.opcode;
				write_mr_rm_instr(bin, pos, opcode, reg, rm, false, false);
				break;
			} case JE_REL: {
				unsigned char opcode1 = 0x0F;
				unsigned char opcode2 = 0x84;
				mem_write(bin, pos, &opcode1, 1);
				mem_write(bin, pos, &opcode2, 1);
				write_static_value(bin, pos, n->invoke.arguments->fst, 4, symtab, relas);
				break;
			} case ORQ_REG_TO_REG: {
				char opcode = 0x0B;
				int reg = ((union expression *) n->invoke.arguments->frst)->instruction.opcode; //Dest
				int rm = ((union expression *) n->invoke.arguments->fst)->instruction.opcode; //Src
				write_mr_rm_instr(bin, pos, opcode, reg, rm, false, true);
				break;
			} case MOVQ_IMM_TO_REG: {
				unsigned char opcode = 0xB8;
				union expression *imm_expr = ((union expression *) n->invoke.arguments->fst);
				int reg = ((union expression *) n->invoke.arguments->frst)->instruction.opcode; //Dest
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
				unsigned char opcode = 0xFF;
				unsigned char reg = 2;
				int rm = ((union expression *) n->invoke.arguments->fst)->instruction.opcode;
				write_mr_rm_instr(bin, pos, opcode, reg, rm, false, false);
				break;
			}
		}
	}
}

int measure_strtab(list generated_expressions, list locals, list globals) {
	union expression *e;
	int strtab_len = 1;
	{foreach(e, locals) {
		strtab_len += strlen(e->reference.name) + 1;
	}}
	{foreach(e, globals) {
		strtab_len += strlen(e->reference.name) + 1;
	}}
	{foreach(e, generated_expressions) {
		if(e->instruction.opcode == LOCAL_LABEL || e->instruction.opcode == GLOBAL_LABEL) {
			strtab_len += strlen(((union expression *) e->instruction.arguments->fst)->reference.name) + 1;
		}
	}}
	return strtab_len;
}

int measure_symtab(list generated_expressions, list locals, list globals) {
	int sym_count = 1;
	union expression *e;
	{foreach(e, locals) {
		sym_count++;
	}}
	{foreach(e, globals) {
		sym_count++;
	}}
	{foreach(e, generated_expressions) {
		if(e->instruction.opcode == LOCAL_LABEL || e->instruction.opcode == GLOBAL_LABEL) {
			sym_count++;
		}
	}}
	return sym_count;
}

int strvlen(char *strv) {
	int i;
	for(i = 1; strv[i - 1] || strv[i]; i++);
	return i;
}

#define SHSTRTAB "\0.shstrtab\0.text\0.strtab\0.symtab\0.bss\0.rela.text\0"

#define SH_COUNT 7

int max_elf_size(list generated_expressions, list locals, list globals) {
	return sizeof(Elf64_Ehdr) + (sizeof(Elf64_Shdr) * SH_COUNT) + round_size(strvlen(SHSTRTAB), ALIGNMENT) +
		round_size(MAX_INSTR_LEN * length(generated_expressions), ALIGNMENT) +
		round_size(measure_strtab(generated_expressions, locals, globals), ALIGNMENT) +
		(sizeof(Elf64_Sym) * measure_symtab(generated_expressions, locals, globals)) +
		(sizeof(Elf64_Rela) * MAX_INSTR_FIELDS * length(generated_expressions));
}

void write_elf(list generated_expressions, list locals, list globals, unsigned char **bin, int *pos) {
	*pos = 0;
	*bin = malloc(max_elf_size(generated_expressions, locals, globals));
	
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
	ehdr.e_shnum = SH_COUNT;
	ehdr.e_shstrndx = 1;
	mem_write(*bin, pos, &ehdr, sizeof(Elf64_Ehdr));
	
	int strtab_len = measure_strtab(generated_expressions, locals, globals),
		sym_count = measure_symtab(generated_expressions, locals, globals);
	
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
	
	union expression *e;
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
			union expression *ref = (union expression *) e->instruction.arguments->fst;
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
			union expression *ref = (union expression *) e->instruction.arguments->fst;
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
	mem_write(*bin, pos, &undef_shdr, sizeof(Elf64_Shdr));
	
	char shstrtab_padded[round_size(strvlen(SHSTRTAB), ALIGNMENT)];
	memcpy(shstrtab_padded, SHSTRTAB, strvlen(SHSTRTAB));
	
	Elf64_Shdr shstrtab_shdr;
	shstrtab_shdr.sh_name = 1;
	shstrtab_shdr.sh_type = SHT_STRTAB;
	shstrtab_shdr.sh_flags = 0;
	shstrtab_shdr.sh_addr = 0;
	shstrtab_shdr.sh_offset = sizeof(Elf64_Ehdr) + SH_COUNT*sizeof(Elf64_Shdr);
	shstrtab_shdr.sh_size = strvlen(SHSTRTAB);
	shstrtab_shdr.sh_link = SHN_UNDEF;
	shstrtab_shdr.sh_info = 0;
	shstrtab_shdr.sh_addralign = 0;
	shstrtab_shdr.sh_entsize = 0;
	mem_write(*bin, pos, &shstrtab_shdr, sizeof(Elf64_Shdr));
	
	Elf64_Shdr text_shdr;
	text_shdr.sh_name = shstrtab_shdr.sh_name + strlen(".shstrtab") + 1;
	text_shdr.sh_type = SHT_PROGBITS;
	text_shdr.sh_flags = SHF_ALLOC | SHF_EXECINSTR;
	text_shdr.sh_addr = 0;
	text_shdr.sh_offset = shstrtab_shdr.sh_offset + sizeof(shstrtab_padded);
	text_shdr.sh_size = text_len;
	text_shdr.sh_link = SHN_UNDEF;
	text_shdr.sh_info = 0;
	text_shdr.sh_addralign = 1;
	text_shdr.sh_entsize = 0;
	mem_write(*bin, pos, &text_shdr, sizeof(Elf64_Shdr));
	
	Elf64_Shdr strtab_shdr;
	strtab_shdr.sh_name = text_shdr.sh_name + strlen(".text") + 1;
	strtab_shdr.sh_type = SHT_STRTAB;
	strtab_shdr.sh_flags = 0;
	strtab_shdr.sh_addr = 0;
	strtab_shdr.sh_offset = text_shdr.sh_offset + sizeof(text);
	strtab_shdr.sh_size = strtab_len;
	strtab_shdr.sh_link = SHN_UNDEF;
	strtab_shdr.sh_info = 0;
	strtab_shdr.sh_addralign = 0;
	strtab_shdr.sh_entsize = 0;
	mem_write(*bin, pos, &strtab_shdr, sizeof(Elf64_Shdr));
	
	Elf64_Shdr symtab_shdr;
	symtab_shdr.sh_name = strtab_shdr.sh_name + strlen(".strtab") + 1;
	symtab_shdr.sh_type = SHT_SYMTAB;
	symtab_shdr.sh_flags = 0;
	symtab_shdr.sh_addr = 0;
	symtab_shdr.sh_offset = strtab_shdr.sh_offset + sizeof(strtab);
	symtab_shdr.sh_size = sizeof(syms);
	symtab_shdr.sh_link = 3;
	symtab_shdr.sh_info = local_symbol_count;
	symtab_shdr.sh_addralign = 0;
	symtab_shdr.sh_entsize = sizeof(Elf64_Sym);
	mem_write(*bin, pos, &symtab_shdr, sizeof(Elf64_Shdr));
	
	Elf64_Shdr bss_shdr;
	bss_shdr.sh_name = symtab_shdr.sh_name + strlen(".symtab") + 1;
	bss_shdr.sh_type = SHT_NOBITS;
	bss_shdr.sh_flags = SHF_WRITE | SHF_ALLOC;
	bss_shdr.sh_addr = 0;
	bss_shdr.sh_offset = symtab_shdr.sh_offset + sizeof(syms);
	bss_shdr.sh_size = length(locals) * WORD_SIZE;
	bss_shdr.sh_link = SHN_UNDEF;
	bss_shdr.sh_info = 0;
	bss_shdr.sh_addralign = WORD_SIZE;
	bss_shdr.sh_entsize = 0;
	mem_write(*bin, pos, &bss_shdr, sizeof(Elf64_Shdr));
	
	Elf64_Shdr rela_shdr;
	rela_shdr.sh_name = bss_shdr.sh_name + strlen(".bss") + 1;
	rela_shdr.sh_type = SHT_RELA;
	rela_shdr.sh_flags = 0;
	rela_shdr.sh_addr = 0;
	rela_shdr.sh_offset = bss_shdr.sh_offset;
	rela_shdr.sh_size = (rela_ptr - relas) * sizeof(Elf64_Rela);
	rela_shdr.sh_link = 4;
	rela_shdr.sh_info = 2;
	rela_shdr.sh_addralign = 0;
	rela_shdr.sh_entsize = sizeof(Elf64_Rela);
	mem_write(*bin, pos, &rela_shdr, sizeof(Elf64_Shdr));
	
	mem_write(*bin, pos, shstrtab_padded, sizeof(shstrtab_padded));
	mem_write(*bin, pos, text, sizeof(text));
	mem_write(*bin, pos, strtab, sizeof(strtab));
	mem_write(*bin, pos, syms, sizeof(syms));
	mem_write(*bin, pos, relas, (rela_ptr - relas) * sizeof(Elf64_Rela));
}
