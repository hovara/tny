#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "object.h"
#include "vm.h"

#define VERSION_MAJOR 0
#define VERSION_MINOR 0
#define VERSION_PATCH 1

static 
char *read_file(const char *filename);

static
void print_version(void) {
	printf("Pepper v%d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
}

static 
int repl(void) {
	print_version();
	printf("Press CTRL+c to exit\n\n");

	struct program *program;
	struct symbol_table *symbol_table = symbol_table_new();
	struct object_list *constants = make_object_list(64);
	struct object globals[GLOBALS_SIZE];
	char input[BUFSIZ] = { '\0' };
	while (1)
	{
		printf(">> ");
		if (fgets(input, BUFSIZ, stdin) == NULL) {
			continue;
		}

		struct lexer lexer = new_lexer(input);
		struct parser parser = new_parser(&lexer);
		program = parse_program(&parser);

		if (parser.errors > 0) {
			printf("Parsing error:\n");
			for (unsigned i = 0; i < parser.errors; i++) {
				printf("- %s\n", parser.error_messages[i]);
			}

			continue;
		}

		struct compiler *compiler = compiler_new_with_state(symbol_table, constants);
		int err = compile_program(compiler, program);
		if (err) {
			puts(compiler_error_str(err));
			continue;
		}

		struct bytecode *code = get_bytecode(compiler);
		struct vm *machine = vm_new_with_globals(code, globals);
		err = vm_run(machine);
		if (err) {
			printf("Error executing bytecode: %d\n", err);
			continue;
		}

		struct object obj = vm_stack_last_popped(machine);
		if (obj.type != OBJ_COMPILED_FUNCTION && obj.type != OBJ_BUILTIN) {
			print_object(obj);
			puts("");
		}

		// copy globals out of VM so we can re-use them in next iteration
		for (unsigned i=0; i < GLOBALS_SIZE; i++) {
			globals[i] = machine->globals[i];
		}

		//free_parser(&parser);
		//free_program(program);
		//compiler_free(compiler);
		//vm_free(machine);
		free(code);
	}

	return 0;
}

static 
int run_script(const char *filename) {
	char *input = read_file(filename);
	struct lexer lexer = new_lexer(input);
	struct parser parser = new_parser(&lexer);
	struct program *program = parse_program(&parser);

	if (parser.errors > 0) {
		for (unsigned i = 0; i < parser.errors; i++) {
			puts(parser.error_messages[i]);
		}

		return EXIT_FAILURE;
	}

	struct compiler *compiler = compiler_new();
	int err = compile_program(compiler, program);
	if (err) {
		printf("SyntaxError: %s\n", compiler_error_str(err));
		return EXIT_FAILURE;
	}

	struct bytecode *code = get_bytecode(compiler);
	struct vm *machine = vm_new(code);
	err = vm_run(machine);
	if (err) {
		printf("Error executing bytecode: %d\n", err);
		return EXIT_FAILURE;
	}

	free_program(program);
	compiler_free(compiler);
	free(code);
	vm_free(machine);
	free(input);
	return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		return repl();
	}

	if (strcmp(argv[1], "--version") == 0) {
		print_version();
		return 0;
	}

	return run_script(argv[1]);
}

char *read_file(const char *filename) {
	char *input = (char *) calloc(BUFSIZ, sizeof(char));
	assert(input != NULL);
	uint32_t size = 0;

	FILE *f = fopen(filename, "r");
	if (!f) {
		printf("Could not open \"%s\" for reading", filename);
		exit(1);
	}

	uint32_t read = 0;
	while ( (read = fread(input, sizeof(char), BUFSIZ, f)) > 0) {
		size += read;

		if (read >= BUFSIZ) {
			input = (char*) realloc(input, size + BUFSIZ);
			assert(input != NULL);
		}
	}
	input[size] = '\0';

	fclose(f);
	return input;
}
