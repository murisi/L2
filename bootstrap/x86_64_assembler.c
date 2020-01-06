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
  int end = *idx + cnt;
  for(; *idx != end; (*idx)++, bytes++) {
    mem[*idx] = *((unsigned char *) bytes);
  }
}

void write_mr_instr(char *bin, int *pos, char opcode, int reg, int rm, bool m, bool rexw) {
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
  unsigned long int val = 0;
  if(expr->base.type == literal) {
    val = expr->literal.value;
  } else if(expr->base.type == symbol && bytes == 8) {
    (*relas)->r_offset = *pos;
    (*relas)->r_info = ELF64_R_INFO((Elf64_Sym *) expr->symbol.binding_aug->context - symtab, R_X86_64_64);
    (*relas)->r_addend = 0;
    (*relas)++;
  } else if(expr->base.type == assembly && expr->assembly.opcode == LNKR_ADD_OFF_TO_REF && bytes == 8) {
    union expression *ref = expr->assembly.arguments->fst;
    union expression *offset = expr->assembly.arguments->frst;
    (*relas)->r_offset = *pos;
    (*relas)->r_info = ELF64_R_INFO((Elf64_Sym *) ref->symbol.binding_aug->context - symtab, R_X86_64_64);
    (*relas)->r_addend = offset->literal.value;
    (*relas)++;
  } else if(expr->base.type == assembly && expr->assembly.opcode == LNKR_SUB_RIP_TO_REF && bytes == 4) {
    union expression *ref = expr->assembly.arguments->fst;
    (*relas)->r_offset = *pos;
    (*relas)->r_info = ELF64_R_INFO((Elf64_Sym *) ref->symbol.binding_aug->context - symtab, R_X86_64_PC32);
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
    switch(n->assembly.opcode) {
      case LABEL: {
        union expression *label_ref = (union expression *) n->assembly.arguments->fst;
        Elf64_Sym *sym = label_ref->symbol.binding_aug->context;
        label_ref->symbol.binding_aug->offset = (unsigned long) *pos;
        sym->st_value = *pos;
        break;
      } case LEAQ_MDB_TO_REG: {
        unsigned char opcode = 0x8D;
        int reg = ((union expression *) n->invoke.arguments->frrst)->assembly.opcode; //Dest
        int rm = ((union expression *) n->invoke.arguments->frst)->assembly.opcode; //Src
        write_mr_instr(bin, pos, opcode, reg, rm, true, true);
        write_static_value(bin, pos, n->invoke.arguments->fst, 4, symtab, relas);
        break;
      } case MOVQ_REG_TO_MDB: {
        unsigned char opcode = 0x89;
        int reg = ((union expression *) n->invoke.arguments->fst)->assembly.opcode; //Src
        int rm = ((union expression *) n->invoke.arguments->frrst)->assembly.opcode; //Dest
        write_mr_instr(bin, pos, opcode, reg, rm, true, true);
        write_static_value(bin, pos, n->invoke.arguments->frst, 4, symtab, relas);
        break;
      } case JMP_REL: {
        unsigned char opcode = 0xE9;
        mem_write(bin, pos, &opcode, 1);
        write_static_value(bin, pos, n->invoke.arguments->fst, 4, symtab, relas);
        break;
      } case MOVQ_MDB_TO_REG: {
        unsigned char opcode = 0x8B;
        int reg = ((union expression *) n->invoke.arguments->frrst)->assembly.opcode; //Dest
        int rm = ((union expression *) n->invoke.arguments->frst)->assembly.opcode; //Src
        write_mr_instr(bin, pos, opcode, reg, rm, true, true);
        write_static_value(bin, pos, n->invoke.arguments->fst, 4, symtab, relas);
        break;
      } case PUSHQ_REG: {
        unsigned char opcode = 0x50;
        int reg = ((union expression *) n->invoke.arguments->fst)->assembly.opcode; //Dest
        write_o_instr(bin, pos, opcode, reg);
        break;
      } case MOVQ_REG_TO_REG: {
        unsigned char opcode = 0x8B;
        int reg = ((union expression *) n->invoke.arguments->frst)->assembly.opcode; //Dest
        int rm = ((union expression *) n->invoke.arguments->fst)->assembly.opcode; //Src
        write_mr_instr(bin, pos, opcode, reg, rm, false, true);
        break;
      } case SUBQ_IMM_TO_REG: {
        unsigned char opcode = 0x81;
        unsigned char reg = 5;
        int rm = ((union expression *) n->invoke.arguments->frst)->assembly.opcode; //Dest
        write_mr_instr(bin, pos, opcode, reg, rm, false, true);
        mem_write(bin, pos, &((union expression *) n->invoke.arguments->fst)->literal.value, 4);
        break;
      } case ADDQ_IMM_TO_REG: {
        unsigned char opcode = 0x81;
        unsigned char reg = 0;
        int rm = ((union expression *) n->invoke.arguments->frst)->assembly.opcode; //Dest
        write_mr_instr(bin, pos, opcode, reg, rm, false, true);
        mem_write(bin, pos, &((union expression *) n->invoke.arguments->fst)->literal.value, 4);
        break;
      } case POPQ_REG: {
        unsigned char opcode = 0x58;
        int reg = ((union expression *) n->invoke.arguments->fst)->assembly.opcode; //Dest
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
        int rm = ((union expression *) n->invoke.arguments->fst)->assembly.opcode;
        write_mr_instr(bin, pos, opcode, reg, rm, false, false);
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
        int reg = ((union expression *) n->invoke.arguments->frst)->assembly.opcode; //Dest
        int rm = ((union expression *) n->invoke.arguments->fst)->assembly.opcode; //Src
        write_mr_instr(bin, pos, opcode, reg, rm, false, true);
        break;
      } case MOVQ_IMM_TO_REG: {
        unsigned char opcode = 0xB8;
        union expression *imm_expr = ((union expression *) n->invoke.arguments->fst);
        int reg = ((union expression *) n->invoke.arguments->frst)->assembly.opcode; //Dest
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
        int rm = ((union expression *) n->invoke.arguments->fst)->assembly.opcode;
        write_mr_instr(bin, pos, opcode, reg, rm, false, false);
        break;
      }
    }
  }
}

int measure_strtab(list generated_expressions, list undefined_bindings, list static_bindings) {
  struct binding_aug *bndg;
  int strtab_len = 1;
  {foreach(bndg, undefined_bindings) {
    if(bndg->name) {
      strtab_len += strlen(bndg->name) + 1;
    }
  }}
  {foreach(bndg, static_bindings) {
    if(bndg->name) {
      strtab_len += strlen(bndg->name) + 1;
    }
  }}
  union expression *e;
  {foreach(e, generated_expressions) {
    if(e->assembly.opcode == LABEL) {
      char *label_str = ((union expression *) e->assembly.arguments->fst)->symbol.name;
      if(label_str) {
        strtab_len += strlen(label_str) + 1;
      }
    }
  }}
  return strtab_len;
}

int measure_symtab(list generated_expressions, list undefined_bindings, list static_bindings) {
  int sym_count = 1;
  sym_count += length(undefined_bindings) + length(static_bindings);
  union expression *e;
  {foreach(e, generated_expressions) {
    if(e->assembly.opcode == LABEL) {
      sym_count++;
    }
  }}
  return sym_count;
}

#define SHSTRTAB "\0.shstrtab\0.text\0.strtab\0.symtab\0.bss\0.rela.text\0"

#define SH_COUNT 7

int max_elf_size(list generated_expressions, list undefined_bindings, list static_bindings) {
  return sizeof(Elf64_Ehdr) + (sizeof(Elf64_Shdr) * SH_COUNT) + pad_size(strvlen(SHSTRTAB), ALIGNMENT) +
    pad_size(MAX_INSTR_LEN * length(generated_expressions), ALIGNMENT) +
    pad_size(measure_strtab(generated_expressions, undefined_bindings, static_bindings), ALIGNMENT) +
    (sizeof(Elf64_Sym) * measure_symtab(generated_expressions, undefined_bindings, static_bindings)) +
    (sizeof(Elf64_Rela) * MAX_INSTR_FIELDS * length(generated_expressions));
}

void write_elf(list generated_expressions, list undefined_bindings, list static_bindings, unsigned char **bin, int *pos, region elfreg) {
  *pos = 0;
  *bin = region_alloc(elfreg, max_elf_size(generated_expressions, undefined_bindings, static_bindings));
  
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
  ehdr.e_flags = 0;
  ehdr.e_ehsize = sizeof(Elf64_Ehdr);
  ehdr.e_phentsize = 0;
  ehdr.e_phnum = 0;
  ehdr.e_shentsize = sizeof(Elf64_Shdr);
  ehdr.e_shnum = SH_COUNT;
  ehdr.e_shstrndx = 1;
  mem_write(*bin, pos, &ehdr, sizeof(Elf64_Ehdr));
  
  int strtab_len = measure_strtab(generated_expressions, undefined_bindings, static_bindings),
    sym_count = measure_symtab(generated_expressions, undefined_bindings, static_bindings);
  
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
  
  char strtab[pad_size(strtab_len, ALIGNMENT)];
  char *strtabptr = strtab;
  *(strtabptr++) = '\0';
  
  unsigned long bss_ptr = 0;
  
  struct binding_aug *bndg;
  {foreach(bndg, static_bindings) {
    if(bndg->scope == local_scope) {
      if(bndg->name) {
        strcpy(strtabptr, bndg->name);
        sym_ptr->st_name = strtabptr - strtab;
        strtabptr += strlen(bndg->name) + 1;
      } else {
        sym_ptr->st_name = 0;
      }
      sym_ptr->st_value = bss_ptr;
      bndg->offset = sym_ptr->st_value;
      sym_ptr->st_size = bndg->size;
      sym_ptr->st_info = ELF64_ST_INFO(STB_LOCAL, STT_NOTYPE);
      sym_ptr->st_other = 0;
      sym_ptr->st_shndx = 5;
      bndg->context = sym_ptr;
      sym_ptr++;
      bss_ptr += pad_size(bndg->size, WORD_SIZE);
    }
  }}
  union expression *e;
  {foreach(e, generated_expressions) {
    if(e->assembly.opcode == LABEL && ((union expression *) e->assembly.arguments->fst)->symbol.binding_aug->scope == local_scope) {
      union expression *ref = (union expression *) e->assembly.arguments->fst;
      if(ref->symbol.name) {
        strcpy(strtabptr, ref->symbol.name);
        sym_ptr->st_name = strtabptr - strtab;
        strtabptr += strlen(ref->symbol.name) + 1;
      } else {
        sym_ptr->st_name = 0;
      }
      sym_ptr->st_value = 0;
      sym_ptr->st_size = 0;
      sym_ptr->st_info = ELF64_ST_INFO(STB_LOCAL, STT_NOTYPE);
      sym_ptr->st_other = 0;
      sym_ptr->st_shndx = 2;
      ref->symbol.binding_aug->context = sym_ptr;
      sym_ptr++;
    }
  }}
  int local_symbol_count = sym_ptr - syms;
  {foreach(bndg, static_bindings) {
    if(bndg->scope == global_scope && bndg->state == defined_state) {
      if(bndg->name) {
        strcpy(strtabptr, bndg->name);
        sym_ptr->st_name = strtabptr - strtab;
        strtabptr += strlen(bndg->name) + 1;
      } else {
        sym_ptr->st_name = 0;
      }
      sym_ptr->st_value = bss_ptr;
      bndg->offset = sym_ptr->st_value;
      sym_ptr->st_size = bndg->size;
      sym_ptr->st_info = ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE);
      sym_ptr->st_other = 0;
      sym_ptr->st_shndx = 5;
      bndg->context = sym_ptr;
      sym_ptr++;
      bss_ptr += pad_size(bndg->size, WORD_SIZE);
    }
  }}
  {foreach(bndg, undefined_bindings) {
    if(bndg->scope == global_scope && bndg->state == undefined_state) {
      if(bndg->name) {
        strcpy(strtabptr, bndg->name);
        sym_ptr->st_name = strtabptr - strtab;
        strtabptr += strlen(bndg->name) + 1;
      } else {
        sym_ptr->st_name = 0;
      }
      sym_ptr->st_value = 0;
      sym_ptr->st_size = 0;
      sym_ptr->st_info = ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE);
      sym_ptr->st_other = 0;
      sym_ptr->st_shndx = SHN_UNDEF;
      bndg->context = sym_ptr;
      sym_ptr++;
    }
  }}
  {foreach(e, generated_expressions) {
    if(e->assembly.opcode == LABEL && ((union expression *) e->assembly.arguments->fst)->symbol.binding_aug->scope == global_scope) {
      union expression *ref = (union expression *) e->assembly.arguments->fst;
      if(ref->symbol.name) {
        strcpy(strtabptr, ref->symbol.name);
        sym_ptr->st_name = strtabptr - strtab;
        strtabptr += strlen(ref->symbol.name) + 1;
      } else {
        sym_ptr->st_name = 0;
      }
      sym_ptr->st_value = 0;
      sym_ptr->st_size = 0;
      sym_ptr->st_info = ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE);
      sym_ptr->st_other = 0;
      sym_ptr->st_shndx = 2;
      ref->symbol.binding_aug->context = sym_ptr;
      sym_ptr++;
    }
  }}
  
  int text_len, max_text_sec_len = pad_size(MAX_INSTR_LEN * length(generated_expressions), ALIGNMENT),
    max_rela_sec_len = MAX_INSTR_FIELDS * length(generated_expressions) * sizeof(Elf64_Rela);
  
  region temp_reg = create_region(0);
  unsigned char *text = region_alloc(temp_reg, max_text_sec_len);
  Elf64_Rela *relas = region_alloc(temp_reg, max_rela_sec_len);
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
  
  char shstrtab_padded[pad_size(strvlen(SHSTRTAB), ALIGNMENT)];
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
  strtab_shdr.sh_offset = text_shdr.sh_offset + max_text_sec_len;
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
  bss_shdr.sh_offset = symtab_shdr.sh_offset + symtab_shdr.sh_size;
  bss_shdr.sh_size = bss_ptr;
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
  rela_shdr.sh_offset = bss_shdr.sh_offset + 0;
  rela_shdr.sh_size = (rela_ptr - relas) * sizeof(Elf64_Rela);
  rela_shdr.sh_link = 4;
  rela_shdr.sh_info = 2;
  rela_shdr.sh_addralign = 0;
  rela_shdr.sh_entsize = sizeof(Elf64_Rela);
  mem_write(*bin, pos, &rela_shdr, sizeof(Elf64_Shdr));
  
  mem_write(*bin, pos, shstrtab_padded, sizeof(shstrtab_padded));
  mem_write(*bin, pos, text, max_text_sec_len);
  mem_write(*bin, pos, strtab, sizeof(strtab));
  mem_write(*bin, pos, syms, sizeof(syms));
  mem_write(*bin, pos, relas, (rela_ptr - relas) * sizeof(Elf64_Rela));
  destroy_region(temp_reg);
}

void binding_aug_offsets_to_addresses(list asms, list static_bindings, Object *obj) {
  struct binding_aug *bndg;
  {foreach(bndg, static_bindings) {
    if(bndg->type == absolute_storage && bndg->state == defined_state) {
      bndg->offset += (unsigned long) segment(obj, ".bss");
    }
  }}
  union expression *l;
  {foreach(l, asms) {
    if(l->assembly.opcode == LABEL) {
      union expression *label_ref = l->assembly.arguments->fst;
      label_ref->symbol.binding_aug->offset += (unsigned long) segment(obj, ".text");
    }
  }}
}
