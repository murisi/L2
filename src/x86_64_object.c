#include "elf.h"

/*
 * Some functions for loading up object files, querying the addresses of its
 * symbols in memory, and mutating those of its symbols that are mutable.
 * The object files must conform to the ELF-64 Object File Format and the AMD64
 * ILP32 ABI and must not be position independent, that is, it must not use the
 * global offset table or the procedure linkage table.
 */
typedef struct {
	Elf64_Ehdr *ehdr;
	Elf64_Shdr *shdrs;
	Elf64_Sym **syms;
	Elf64_Rela **relas;
	Elf64_Sxword **addends;
	void **segs;
} Object;

/*
 * Update the segments of the loaded object with the current symbol values under
 * the direction of the relocation entries.
 */
void do_relocations(Object *obj) {
	int sec;
	for(sec = 0; sec < obj->ehdr->e_shnum; sec++) {
		if(obj->shdrs[sec].sh_type == SHT_REL || obj->shdrs[sec].sh_type == SHT_RELA) {
			int relanum = obj->shdrs[sec].sh_size / obj->shdrs[sec].sh_entsize;
			int rela;
			for(rela = 0; rela < relanum; rela++) {
				Elf64_Sym *sym = &obj->syms[obj->shdrs[sec].sh_link][ELF64_R_SYM(obj->relas[sec][rela].r_info)];
				unsigned char *location = (unsigned char *) obj->relas[sec][rela].r_offset;
				Elf64_Addr temp;
				switch(ELF64_R_TYPE(obj->relas[sec][rela].r_info)) {
					case R_X86_64_NONE:
						break;
					case R_X86_64_64:
						memcpy(location, (temp = sym->st_value + obj->addends[sec][rela], &temp), 8);
						break;
					case R_X86_64_PC32:
						memcpy(location, (temp = sym->st_value + obj->addends[sec][rela] - obj->relas[sec][rela].r_offset, &temp), 4);
						break;
					case R_X86_64_GLOB_DAT://LP64
						memcpy(location, (temp = sym->st_value, &temp), 8);
						break;
					case R_X86_64_JUMP_SLOT://LP64
						memcpy(location, (temp = sym->st_value, &temp), 8);
						break;
					case R_X86_64_32:
						memcpy(location, (temp = sym->st_value + obj->addends[sec][rela], &temp), 4);
						break;
					case R_X86_64_32S:
						memcpy(location, (temp = sym->st_value + obj->addends[sec][rela], &temp), 4);
						break;
					case R_X86_64_16:
						memcpy(location, (temp = sym->st_value + obj->addends[sec][rela], &temp), 2);
						break;
					case R_X86_64_PC16:
						memcpy(location, (temp = sym->st_value + obj->addends[sec][rela] - obj->relas[sec][rela].r_offset, &temp), 2);
						break;
					case R_X86_64_8:
						memcpy(location, (temp = sym->st_value + obj->addends[sec][rela], &temp), 1);
						break;
					case R_X86_64_PC8:
						memcpy(location, (temp = sym->st_value + obj->addends[sec][rela] - obj->relas[sec][rela].r_offset, &temp), 1);
						break;
					case R_X86_64_SIZE32:
						memcpy(location, (temp = sym->st_size + obj->addends[sec][rela], &temp), 4);
						break;
					case R_X86_64_SIZE64:
						memcpy(location, (temp = sym->st_size + obj->addends[sec][rela], &temp), 8);
						break;
				}
			}
		}
	}
}

/*
 * Store the relocation addends because we may want to do several relocations on
 * the object files.
 */
void store_addends(Object *obj, region reg, jumpbuf *handler) {
	int sec;
	for(sec = 0; sec < obj->ehdr->e_shnum; sec++) {
		if(obj->shdrs[sec].sh_type == SHT_RELA) {
			int relanum = obj->shdrs[sec].sh_size / obj->shdrs[sec].sh_entsize;
			obj->addends[sec] = region_alloc(reg, relanum * sizeof(Elf64_Sxword));
			int rela;
			for(rela = 0; rela < relanum; rela++) {
				obj->addends[sec][rela] = obj->relas[sec][rela].r_addend;
			}
		} else if(obj->shdrs[sec].sh_type == SHT_REL) {
			int relnum = obj->shdrs[sec].sh_size / obj->shdrs[sec].sh_entsize;
			obj->addends[sec] = region_alloc(reg, relnum * sizeof(Elf64_Sxword));
			int rel;
			for(rel = 0; rel < relnum; rel++) {
				switch(ELF64_R_TYPE(obj->relas[sec][rel].r_info)) {
					case R_X86_64_8: case R_X86_64_PC8:
						obj->addends[sec][rel] = *((unsigned char *) obj->relas[sec][rel].r_offset);
						break;
					case R_X86_64_16: case R_X86_64_PC16:
						obj->addends[sec][rel] = *((Elf64_Half *) obj->relas[sec][rel].r_offset);
						break;
					case R_X86_64_PC32: case R_X86_64_32: case R_X86_64_32S:
					case R_X86_64_SIZE32:
						obj->addends[sec][rel] = *((Elf64_Word *) obj->relas[sec][rel].r_offset);
						break;
					case R_X86_64_64: case R_X86_64_SIZE64: case R_X86_64_GLOB_DAT: case R_X86_64_JUMP_SLOT:
						obj->addends[sec][rel] = *((Elf64_Xword *) obj->relas[sec][rel].r_offset);
						break;
					default:
						throw_object(handler);
						break;
				}
			}
		}
	}
}

/*
 * Convert all file offsets in the symbol table(s) and relocation entries to
 * virtual addresses, for the relocations are predicated on these kinds of
 * inputs.
 */
void offsets_to_addresses(Object *obj) {
	int sec;
	for(sec = 0; sec < obj->ehdr->e_shnum; sec++) {
		if(obj->shdrs[sec].sh_type == SHT_RELA || obj->shdrs[sec].sh_type == SHT_REL) {
			int relanum = obj->shdrs[sec].sh_size / obj->shdrs[sec].sh_entsize;
			int rela;
			for(rela = 0; rela < relanum; rela++) {
				obj->relas[sec][rela].r_offset = (Elf64_Addr) obj->segs[obj->shdrs[sec].sh_info] + obj->relas[sec][rela].r_offset;
			}
		} else if(obj->shdrs[sec].sh_type == SHT_SYMTAB) {
			int symnum = obj->shdrs[sec].sh_size / obj->shdrs[sec].sh_entsize;
			int sym;
			for(sym = 0; sym < symnum; sym++) {
				switch(obj->syms[sec][sym].st_shndx) {
					case SHN_ABS:
						obj->syms[sec][sym].st_value = obj->syms[sec][sym].st_value;
						break;
					case SHN_COMMON: case SHN_UNDEF:
						obj->syms[sec][sym].st_value = 0;
						break;
					default:
						obj->syms[sec][sym].st_value = (Elf64_Addr) obj->segs[obj->syms[sec][sym].st_shndx] +
							obj->syms[sec][sym].st_value;
						break;
				}
			}
		}
	}
}

/*
 * Checks the integrity of the ELF file at obj_file and loads it into memory if
 * all the checks are passed. Specifically, the segments are loaded into memory
 * at address mem and require the amount of memory stated by
 * object_required_memory. Returns a handle to manipulate the loaded object.
 */
Object *read_object(unsigned char *objsrc, int objsrc_sz, region reg, jumpbuf *handler) {
	Object *obj = region_alloc(reg, sizeof(Object));
	obj->ehdr = region_alloc(reg, sizeof(Elf64_Ehdr));
	if(objsrc_sz < sizeof(Elf64_Ehdr)) throw_object(handler);
	memcpy(obj->ehdr, objsrc, sizeof(Elf64_Ehdr));
	if(!(obj->ehdr->e_ident[EI_MAG0] == ELFMAG0 && obj->ehdr->e_ident[EI_MAG1] == ELFMAG1 &&
		obj->ehdr->e_ident[EI_MAG2] == ELFMAG2 && obj->ehdr->e_ident[EI_MAG3] == ELFMAG3 &&
		obj->ehdr->e_ident[EI_CLASS] == ELFCLASS64 && obj->ehdr->e_ident[EI_DATA] == ELFDATA2LSB))
			throw_object(handler);
	
	obj->shdrs = region_alloc(reg, obj->ehdr->e_shnum * sizeof(Elf64_Shdr));
	obj->syms = region_alloc(reg, obj->ehdr->e_shnum * sizeof(Elf64_Sym *));
	obj->relas = region_alloc(reg, obj->ehdr->e_shnum * sizeof(Elf64_Rela *));
	obj->addends = region_alloc(reg, obj->ehdr->e_shnum * sizeof(Elf64_Sxword *));
	obj->segs = region_alloc(reg, obj->ehdr->e_shnum * sizeof(void *));
	
	int sec;
	for(sec = 0; sec < obj->ehdr->e_shnum; sec++) {
		memcpy(&obj->shdrs[sec], objsrc + obj->ehdr->e_shoff + (obj->ehdr->e_shentsize * sec), sizeof(Elf64_Shdr));
		obj->segs[sec] = region_alloc(reg, obj->shdrs[sec].sh_size);
		if(obj->shdrs[sec].sh_type != SHT_NOBITS) {
			memcpy(obj->segs[sec], objsrc + obj->shdrs[sec].sh_offset, obj->shdrs[sec].sh_size);
		}
		
		if(obj->shdrs[sec].sh_type == SHT_SYMTAB) {
			int symnum = obj->shdrs[sec].sh_size / obj->shdrs[sec].sh_entsize;
			obj->syms[sec] = region_alloc(reg, symnum * sizeof(Elf64_Sym));
			int sym;
			for(sym = 0; sym < symnum; sym++) {
				memcpy(&obj->syms[sec][sym], obj->segs[sec] + (obj->shdrs[sec].sh_entsize * sym), sizeof(Elf64_Sym));
			}
		} else if(obj->shdrs[sec].sh_type == SHT_RELA || obj->shdrs[sec].sh_type == SHT_REL) {
			int relanum = obj->shdrs[sec].sh_size / obj->shdrs[sec].sh_entsize;
			obj->relas[sec] = region_alloc(reg, relanum * sizeof(Elf64_Rela));
			int rela;
			for(rela = 0; rela < relanum; rela++) {
				memcpy(&obj->relas[sec][rela], obj->segs[sec] + (obj->shdrs[sec].sh_entsize * rela),
					obj->shdrs[sec].sh_type == SHT_RELA ? sizeof(Elf64_Rela) : sizeof(Elf64_Rel));
			}
		}
	}
	return obj;
}

/*
 * Checks the integrity of the ELF file at obj_file and loads it into memory if
 * all the checks are passed. More precisely, the segments are loaded into
 * memory at address mem and require the amount of memory stated by
 * object_required_memory. Relocation addends are then stored to facilitate
 * future relocations. Relocations are then done in light of the fact that the
 * object code now has a concrete address in memory. Returns a handle to
 * manipulate the loaded object.
 */
Object *load(unsigned char *objsrc, int objsrc_sz, region reg, jumpbuf *handler) {
	Object *obj = read_object(objsrc, objsrc_sz, reg, handler);
	offsets_to_addresses(obj);
	store_addends(obj, reg, handler);
	do_relocations(obj);
	return obj;
}

char *name_of(Object *obj, Elf64_Shdr *shdr, Elf64_Sym *sym) {
	return obj->segs[shdr->sh_link] + sym->st_name;
}

typedef struct {
	char *name;
	void *address;
} object_symbol;

object_symbol *make_object_symbol(char *nm, void *addr, region r) {
	object_symbol *sym = region_alloc(r, sizeof(object_symbol));
	sym->name = nm;
	sym->address = addr;
	return sym;
}

/*
 * Goes through the loaded object obj and modifies all occurences of symbols
 * with the same name as update to point to the same address as update.
 */
void mutate_symbols(Object *obj, list updates) {
	object_symbol *update;
	foreach(update, updates) {
		int sec;
		for(sec = 0; sec < obj->ehdr->e_shnum; sec++) {
			if(obj->shdrs[sec].sh_type == SHT_SYMTAB) {
				int symnum = obj->shdrs[sec].sh_size / obj->shdrs[sec].sh_entsize;
				int sym;
				for(sym = 1; sym < symnum; sym++) {
					if(!strcmp(name_of(obj, &obj->shdrs[sec], &obj->syms[sec][sym]), update->name) &&
						(obj->syms[sec][sym].st_shndx == SHN_UNDEF || obj->syms[sec][sym].st_shndx == SHN_COMMON) &&
						(ELF64_ST_BIND(obj->syms[sec][sym].st_info) == STB_GLOBAL ||
						ELF64_ST_BIND(obj->syms[sec][sym].st_info) == STB_WEAK)) {
							obj->syms[sec][sym].st_value = (Elf64_Addr) update->address;
					}
				}
			}
		}
	}
	do_relocations(obj);
}

list symbols(int flag, Object *obj, region reg) {
	list syms = nil;
	int sec;
	for(sec = 0; sec < obj->ehdr->e_shnum; sec++) {
		if(obj->shdrs[sec].sh_type == SHT_SYMTAB) {
			int symnum = obj->shdrs[sec].sh_size / obj->shdrs[sec].sh_entsize;
			int sym;
			for(sym = 1; sym < symnum; sym++) {
				if(((obj->syms[sec][sym].st_shndx == SHN_UNDEF || obj->syms[sec][sym].st_shndx == SHN_COMMON) == flag) &&
					(ELF64_ST_BIND(obj->syms[sec][sym].st_info) == STB_GLOBAL ||
					ELF64_ST_BIND(obj->syms[sec][sym].st_info) == STB_WEAK)) {
						object_symbol *symbol = region_alloc(reg, sizeof(object_symbol));
						symbol->name = name_of(obj, &obj->shdrs[sec], &obj->syms[sec][sym]);
						symbol->address = (void *) obj->syms[sec][sym].st_value;
						prepend(symbol, &syms, reg);
				}
			}
		}
	}
	return syms;
}

/*
 * See the analogous function for mutable symbols.
 */
list mutable_symbols(Object *obj, region reg) {
	return symbols(1, obj, reg);
}

/*
 * See the analogous function for mutable symbols.
 */
list immutable_symbols(Object *obj, region reg) {
	return symbols(0, obj, reg);
}

/*
 * Runs the object obj, that is, calls each .text section of obj in order.
 * Intended for use with programming languages that execute top-down without
 * an entry point function. Not to be used for programming langauges like C that
 * have a main function; for those cases one should get the address of the
 * symbol "main" and call it.
 */
void *segment(Object *obj, char *name) {
	if(obj->ehdr->e_shstrndx != SHN_UNDEF) {
		int sec;
		for(sec = 0; sec < obj->ehdr->e_shnum; sec++) {
			if(!strcmp(obj->segs[obj->ehdr->e_shstrndx] + obj->shdrs[sec].sh_name, name)) {
				return (void (*)()) obj->segs[sec];
			}
		}
	}
}
