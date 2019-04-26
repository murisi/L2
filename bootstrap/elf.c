//ELF-64 Object File Format

//Data representation: ELF64 Data Types

typedef unsigned long Elf64_Addr;
typedef unsigned long Elf64_Off;
typedef unsigned short Elf64_Half;
typedef unsigned int Elf64_Word;
typedef signed int Elf64_Sword;
typedef unsigned long int Elf64_Xword;
typedef signed long int Elf64_Sxword;

//File header: ELF-64 Header

#define ELF64_EHDR 64

typedef struct
{
  unsigned char e_ident[16];
  Elf64_Half e_type;
  Elf64_Half e_machine;
  Elf64_Word e_version;
  Elf64_Addr e_entry;
  Elf64_Off e_phoff;
  Elf64_Off e_shoff;
  Elf64_Word e_flags;
  Elf64_Half e_ehsize;
  Elf64_Half e_phentsize;
  Elf64_Half e_phnum;
  Elf64_Half e_shentsize;
  Elf64_Half e_shnum;
  Elf64_Half e_shstrndx;
} Elf64_Ehdr;

//File header: ELF Identification, e-ident

#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6
#define EI_OSABI 7
#define EI_ABIVERSION 8

#define EI_PAD 9
#define EI_NIDENT 16

#define ELFMAG0 0x7F
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

//File header: Object File Classes, e-ident > ei-class

#define ELFCLASS32 1
#define ELFCLASS64 2

//File header: Data Encodings, e-ident > ei-data

#define ELFDATA2LSB 1
#define ELFDATA3MSB 2

//File header: Operating System and ABI Identifiers, e-ident > ei-osabi

#define ELFOSABI_SYSV 0
#define ELFOSABI_HPUX 1
#define ELFOSABI_STANDALONE 255

//File header: Object File Types, e-type

#define ET_NONE 0
#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3
#define ET_CORE 4
#define ET_LOOS 0xFE00
#define ET_HIOS 0xFEFF
#define ET_LOPROC 0xFF00
#define ET_HIPROC 0xFFFF

//Sections: Section indicies

#define SHN_UNDEF 0
#define SHN_LOPROC 0xFF00
#define SHN_HIPROC 0xFF1F
#define SHN_LOOS 0xFF20
#define SHN_HIOS 0xFF3F
#define SHN_ABS 0xFFF1
#define SHN_COMMON 0xFFF2

//Sections: Section header entries

#define ELF64_SHDR 64

typedef struct
{
  Elf64_Word sh_name;
  Elf64_Word sh_type;
  Elf64_Xword sh_flags;
  Elf64_Addr sh_addr;
  Elf64_Off sh_offset;
  Elf64_Xword sh_size;
  Elf64_Word sh_link;
  Elf64_Word sh_info;
  Elf64_Xword sh_addralign;
  Elf64_Xword sh_entsize;
} Elf64_Shdr;

//Sections: Section Types

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_HASH 5
#define SHT_DYNAMIC 6
#define SHT_NOTE 7
#define SHT_NOBITS 8
#define SHT_REL 9
#define SHT_SHLIB 10
#define SHT_DYNSYM 11
#define SHT_LOOS 0x60000000
#define SHT_HIOS 0x6FFFFFFF
#define SHT_LOPROC 0x70000000
#define SHT_HIPROC 0x7FFFFFFF

//Sections: Section Attributes, sh-flags

#define SHF_WRITE 0x1
#define SHF_ALLOC 0x2
#define SHF_EXECINSTR 0x4
#define SHF_MASKOS 0x0F000000
#define SHF_MASKPROC 0xF0000000

//Symbol Table: ELF-64 Symbol Table Entry

#define ELF64_SYM 24

typedef struct
{
  Elf64_Word st_name;
  unsigned char st_info;
  unsigned char st_other;
  Elf64_Half st_shndx;
  Elf64_Addr st_value;
  Elf64_Xword st_size;
} Elf64_Sym;

//Symbol Table: Symbol Bindings

#define STB_LOCAL 0
#define STB_GLOBAL 1
#define STB_WEAK 2
#define STB_LOOS 10
#define STB_HIOS 12
#define STB_LOPROC 13
#define STB_HIPROC 15

#define ELF64_ST_BIND(i) ((i)>>4)
#define ELF64_ST_TYPE(i) ((i)&0xf)
#define ELF64_ST_INFO(b,t) (((b)<<4)+((t)&0xf))

//Symbol Table: Symbol Types

#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC 2
#define STT_SECTION 3
#define STT_FILE 4
#define STT_LOOS 10
#define STT_HIOS 12
#define STT_LOPROC 13
#define STT_HIPROC 15

//Relocations: ELF-64 Relocation Entries

typedef struct
{
  Elf64_Addr r_offset;
  Elf64_Xword r_info;
} Elf64_Rel;

typedef struct
{
  Elf64_Addr r_offset;
  Elf64_Xword r_info;
  Elf64_Sxword r_addend;
} Elf64_Rela;

#define ELF64_R_SYM(i)((i) >> 32)
#define ELF64_R_TYPE(i)((i) & 0xffffffffL)
#define ELF64_R_INFO(s, t)(((s) << 32) + ((t) & 0xffffffffL))

//System V Application Binary Interface: AMD64 Architecture Processor Supplement

//Machine Information: Processor Identification

#define EM_X86_64 62

//Sections: Section Flags

#define SHF_X86_64_LARGE 0x10000000

//Sections: Section Types

#define SHT_X86_64_UNWIND 0x70000001

//Relocation: Relocation Types

#define R_X86_64_NONE 0
#define R_X86_64_64 1
#define R_X86_64_PC32 2
#define R_X86_64_GOT32 3
#define R_X86_64_PLT32 4
#define R_X86_64_COPY 5
#define R_X86_64_GLOB_DAT 6
#define R_X86_64_JUMP_SLOT 7
#define R_X86_64_RELATIVE 8
#define R_X86_64_GOTPCREL 9
#define R_X86_64_32 10
#define R_X86_64_32S 11
#define R_X86_64_16 12
#define R_X86_64_PC16 13
#define R_X86_64_8 14
#define R_X86_64_PC8 15
#define R_X86_64_DTPMOD64 16
#define R_X86_64_DTPOFF64 17
#define R_X86_64_TPOSS64 18
#define R_X86_64_TLSGD 19
#define R_X86_64_TLSLD 20
#define R_X86_64_DTPOFF32 21
#define R_X86_64_GOTTPOFF 22
#define R_X86_64_TPOFF32 23
#define R_X86_64_PC64 24
#define R_X86_64_GOTOFF64 25
#define R_X86_64_GOTPC32 26
#define R_X86_64_SIZE32 32
#define R_X86_64_SIZE64 33
#define R_X86_64_GOTPC32_TLSDESC 34
#define R_X86_64_TLSDESC_CALL 35
#define R_X86_64_TLSDESC 36
#define R_X86_64_IRELATIVE 37
#define R_X86_64_RELATIVE64 38
#define R_X86_64_GOTPCRELX 41
#define R_X86_64_REX_GOTPCRELX 42
