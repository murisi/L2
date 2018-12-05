#define WORD_SIZE 8
#define NULL 0UL
typedef unsigned long int bool;
#define true 1
#define false 0

#include "x86_64_linux_interface.c"
#include "list.c"
#include "errors.c"
#include "lexer.c"
#include "expressions.c"
#include "elf.c"
#include "x86_64_object.c"
#include "preparer.c"
#include "x86_64_generator.c"
#include "x86_64_assembler.c"

list compile_program(union expression *program, list *bindings, buffer expr_buf, jumpbuf *handler) {
	visit_expressions(vfind_multiple_definitions, &program, handler);
	classify_program_binding_augs(program->function.expression);
	visit_expressions(vlink_symbols, &program->function.expression, (void* []) {handler, expr_buf});
	visit_expressions(vescape_analysis, &program, NULL);
	classify_program_binding_augs(program->function.expression);
	visit_expressions(vlayout_frames, &program->function.expression, expr_buf);
	return generate_program(program, bindings, expr_buf);
}

Object *load_program(union expression *program, buffer expr_buf, buffer obj_buf, jumpbuf *handler) {
	list bindings;
	list asms = compile_program(program, &bindings, expr_buf, handler);
	unsigned char *objdest; int objdest_sz;
	write_elf(asms, bindings, &objdest, &objdest_sz, obj_buf);
	Object *obj = load(objdest, objdest_sz, obj_buf, handler);
	binding_aug_offsets_to_addresses(asms, bindings, obj);
	return obj;
}

Object *load_program_and_mutate(union expression *program, list bindings, buffer expr_buf, buffer obj_buf, jumpbuf *handler) {
	Object *obj = load_program(program, expr_buf, obj_buf, handler);
	buffer temp_reg = create_buffer(0);
	list ms = mutable_bindings(obj, temp_reg);
	struct binding_aug *missing_bndg = not_subset((bool (*)(void *, void *)) binding_equals, ms, bindings);
	if(missing_bndg) {
		throw_undefined_symbol(missing_bndg->name, handler);
	}
	destroy_buffer(temp_reg);
	mutate_bindings(obj, bindings);
	return obj;
}

list read_expressions(unsigned char *src, buffer expr_buf, jumpbuf *handler) {
	int fd = open(src, handler);
	long int src_sz = size(fd);
	unsigned char *src_buf = buffer_alloc(expr_buf, src_sz);
	read(fd, src_buf, src_sz);
	close(fd);
	
	list expressions = nil;
	int pos = 0;
	while(after_leading_space(src_buf, src_sz, &pos)) {
		append(build_expression(build_fragment(src_buf, src_sz, &pos, expr_buf, handler), expr_buf, handler), &expressions, expr_buf);
	}
	return expressions;
}

void evaluate_files(list metaprograms, list *bindings, buffer expr_buf, buffer obj_buf, jumpbuf *handler) {
	list objects = nil;
	char *fn;
	foreach(fn, metaprograms) {
		Object *obj;
		char *dot = strrchr(fn, '.');
		
		if(dot && !strcmp(dot, ".l2")) {
			obj = load_program(generate_metaprogram(make_program(read_expressions(fn, expr_buf, handler), expr_buf),
				bindings, expr_buf, obj_buf, handler), expr_buf, obj_buf, handler);
		} else if(dot && !strcmp(dot, ".o")) {
			int obj_fd = open(fn, handler);
			long int obj_sz = size(obj_fd);
			unsigned char *buf = buffer_alloc(obj_buf, obj_sz);
			read(obj_fd, buf, obj_sz);
			close(obj_fd);
			
			obj = load(buf, obj_sz, obj_buf, handler);
		}
		append(obj, &objects, obj_buf);
		append_list(bindings, immutable_bindings(obj, obj_buf));
	}
	
	Object *obj;
	{foreach(obj, objects) {
		mutate_bindings(obj, *bindings);
	}}
	{foreach(obj, objects) {
		((void (*)()) segment(obj, ".text"))();
	}}
}

void compile_files(list programs, list bndgs, buffer expr_buf, buffer obj_buf, jumpbuf *handler) {
	char *infn;
	foreach(infn, programs) {
		char *dot = strrchr(infn, '.');
		if(dot && !strcmp(dot, ".l2")) {
			union expression *program = make_program(read_expressions(infn, expr_buf, handler), expr_buf);
			pre_visit_expressions(vgenerate_metas, &program, (void * []) {bndgs, expr_buf, handler});
			list bindings;
			list asms = compile_program(program, &bindings, expr_buf, handler);
			unsigned char *objdest; int objdest_sz;
			write_elf(asms, bindings, &objdest, &objdest_sz, obj_buf);
			char *outfn = buffer_alloc(obj_buf, strlen(infn) + 1);
			strcpy(outfn, infn);
			char *dot = strrchr(outfn, '.');
			strcpy(dot, ".o");
			int fd = create(outfn, handler);
			write(fd, objdest, objdest_sz);
			close(fd);
		}
	}
}

//The following functions form the interface provided to loaded L2 files

void *_fst_(list l) {
	return l->fst;
}

list _rst_(list l) {
	return l->rst;
}

int main(int argc, char *argv[]) {
	//Initialize the error handler
	jumpbuf evaluate_handler;
	buffer evaluate_buffer = evaluate_handler.ctx = create_buffer(0);
	setjump(&evaluate_handler);
	
	if(evaluate_handler.ctx != evaluate_buffer) {
		union evaluate_error *err = (union evaluate_error *) evaluate_handler.ctx;
		write_str(STDOUT, "Error found: ");
		switch(err->arguments.type) {
			case param_count_mismatch: {
				write_str(STDOUT, "The number of arguments in ");
				print_expression(err->param_count_mismatch.src_expression);
				write_str(STDOUT, " does not match the number of parameters in ");
				print_expression(err->param_count_mismatch.dest_expression);
				write_str(STDOUT, ".\n");
				break;
			} case special_form: {
				if(err->special_form.subexpression_list) {
					write_str(STDOUT, "The subexpression ");
					print_fragment(err->special_form.subexpression_list);
					write_str(STDOUT, " does not satisfy the requirements of the expression ");
					print_fragment(err->special_form.expression_list);
					write_str(STDOUT, ".\n");
				} else {
					write_str(STDOUT, "The expression ");
					print_fragment(err->special_form.expression_list);
					write_str(STDOUT, " has an incorrect number of subexpressions.\n");
				}
				break;
			} case unexpected_character: {
				write_str(STDOUT, "Unexpectedly read ");
				write_char(STDOUT, err->unexpected_character.character);
				write_str(STDOUT, " at ");
				write_ulong(STDOUT, err->unexpected_character.position);
				write_str(STDOUT, ".\n");
				break;
			} case multiple_definition: {
				write_str(STDOUT, "The definition of ");
				write_str(STDOUT, err->multiple_definition.reference_value);
				write_str(STDOUT, " erroneously occured multiple times.\n");
				break;
			} case object: {
				write_str(STDOUT, "Bad object file supplied.\n");
				break;
			} case missing_file: {
				write_str(STDOUT, "There is no file at the path ");
				write_str(STDOUT, err->missing_file.path);
				write_str(STDOUT, ".\n");
				break;
			} case arguments: {
				write_str(STDOUT, "Bad command line arguments.\nUsage: l2evaluate (src1.l2 | obj1.o) ... - src1.l2 ...\n"
					"Outcome: Compiles each L2 file before the dash into an object file, then links all the object files\n"
					"before the dash together, and then executes each object file before the dash in the order they were\n"
					"specified. L2 files after the dash are then compiled using the global functions defined before the\n"
					"dash as macros.\n");
				break;
			} case undefined_symbol: {
				write_str(STDOUT, "Undefined reference: ");
				write_str(STDOUT, err->undefined_symbol.symbol_value);
				write_str(STDOUT, ".\n");
				break;
			}
		}
		return err->arguments.type;
	}
	
	struct binding static_bindings_arr[] = {
		{.name = "-!-", .address = _char('!')},
		{.name = "-\"-", .address = _char('"')},
		{.name = "-#-", .address = _char('#')},
		{.name = "-$-", .address = _char('$')},
		{.name = "-%-", .address = _char('%')},
		{.name = "-&-", .address = _char('&')},
		{.name = "-'-", .address = _char('\'')},
		{.name = "-*-", .address = _char('*')},
		{.name = "-+-", .address = _char('+')},
		{.name = "-,-", .address = _char(',')},
		{.name = "---", .address = _char('-')},
		{.name = "-.-", .address = _char('.')},
		{.name = "-/-", .address = _char('/')},
		{.name = "-0-", .address = _char('0')},
		{.name = "-1-", .address = _char('1')},
		{.name = "-2-", .address = _char('2')},
		{.name = "-3-", .address = _char('3')},
		{.name = "-4-", .address = _char('4')},
		{.name = "-5-", .address = _char('5')},
		{.name = "-6-", .address = _char('6')},
		{.name = "-7-", .address = _char('7')},
		{.name = "-8-", .address = _char('8')},
		{.name = "-9-", .address = _char('9')},
		{.name = "-:-", .address = _char(':')},
		{.name = "-;-", .address = _char(';')},
		{.name = "-<-", .address = _char('<')},
		{.name = "-=-", .address = _char('=')},
		{.name = "->-", .address = _char('>')},
		{.name = "-?-", .address = _char('?')},
		{.name = "-@-", .address = _char('@')},
		{.name = "-A-", .address = _char('A')},
		{.name = "-B-", .address = _char('B')},
		{.name = "-C-", .address = _char('C')},
		{.name = "-D-", .address = _char('D')},
		{.name = "-E-", .address = _char('E')},
		{.name = "-F-", .address = _char('F')},
		{.name = "-G-", .address = _char('G')},
		{.name = "-H-", .address = _char('H')},
		{.name = "-I-", .address = _char('I')},
		{.name = "-J-", .address = _char('J')},
		{.name = "-K-", .address = _char('K')},
		{.name = "-L-", .address = _char('L')},
		{.name = "-M-", .address = _char('M')},
		{.name = "-N-", .address = _char('N')},
		{.name = "-O-", .address = _char('O')},
		{.name = "-P-", .address = _char('P')},
		{.name = "-Q-", .address = _char('Q')},
		{.name = "-R-", .address = _char('R')},
		{.name = "-S-", .address = _char('S')},
		{.name = "-T-", .address = _char('T')},
		{.name = "-U-", .address = _char('U')},
		{.name = "-V-", .address = _char('V')},
		{.name = "-W-", .address = _char('W')},
		{.name = "-X-", .address = _char('X')},
		{.name = "-Y-", .address = _char('Y')},
		{.name = "-Z-", .address = _char('Z')},
		{.name = "-\\-", .address = _char('\\')},
		{.name = "-^-", .address = _char('^')},
		{.name = "-_-", .address = _char('_')},
		{.name = "-`-", .address = _char('`')},
		{.name = "-a-", .address = _char('a')},
		{.name = "-b-", .address = _char('b')},
		{.name = "-c-", .address = _char('c')},
		{.name = "-d-", .address = _char('d')},
		{.name = "-e-", .address = _char('e')},
		{.name = "-f-", .address = _char('f')},
		{.name = "-g-", .address = _char('g')},
		{.name = "-h-", .address = _char('h')},
		{.name = "-i-", .address = _char('i')},
		{.name = "-j-", .address = _char('j')},
		{.name = "-k-", .address = _char('k')},
		{.name = "-l-", .address = _char('l')},
		{.name = "-m-", .address = _char('m')},
		{.name = "-n-", .address = _char('n')},
		{.name = "-o-", .address = _char('o')},
		{.name = "-p-", .address = _char('p')},
		{.name = "-q-", .address = _char('q')},
		{.name = "-r-", .address = _char('r')},
		{.name = "-s-", .address = _char('s')},
		{.name = "-t-", .address = _char('t')},
		{.name = "-u-", .address = _char('u')},
		{.name = "-v-", .address = _char('v')},
		{.name = "-w-", .address = _char('w')},
		{.name = "-x-", .address = _char('x')},
		{.name = "-y-", .address = _char('y')},
		{.name = "-z-", .address = _char('z')},
		{.name = "-|-", .address = _char('|')},
		{.name = "-~-", .address = _char('~')},
		{.name = "@fst", .address = _fst_},
		{.name = "@rst", .address = _rst_},
		{.name = "lst", .address = lst},
		{.name = "token?", .address = is_token},
		{.name = "emt?", .address = is_nil},
		{.name = "emt", .address = nil},
		{.name = "char=", .address = char_equals}
	};
	
	buffer obj_buf = create_buffer(0);
	buffer expr_buf = create_buffer(0);
	list bndgs = nil;
	
	int i;
	for(i = 0; i < sizeof(static_bindings_arr) / sizeof(struct binding); i++) {
		prepend(&static_bindings_arr[i], &bndgs, obj_buf);
	}
	for(i = 1; i < argc; i++) {
		if(!strcmp(argv[i], "-")) {
			break;
		}
	}
	if(i == argc) {
		throw_arguments(&evaluate_handler);
	}
	list metaprograms = nil;
	for(i = 1; strcmp(argv[i], "-"); i++) {
		append(argv[i], &metaprograms, evaluate_buffer);
	}
	
	evaluate_files(metaprograms, &bndgs, expr_buf, obj_buf, &evaluate_handler);
	
	list programs = nil;
	for(i++; i < argc; i++) {
		append(argv[i], &programs, evaluate_buffer);
	}
	compile_files(programs, bndgs, expr_buf, obj_buf, &evaluate_handler);
	
	destroy_buffer(expr_buf);
	destroy_buffer(obj_buf);
	return 0;
}
