#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

///////////////////////////////////
#define MOV_MASK 0b100010
#define MOV_D_MASK 0x2
#define MOV_W_MASK 0x1
#define MOV_RREG_MASK 0b111
#define MOV_LREG_MASK 0b111000


///////////////////////////////////
typedef uint8_t u8;
typedef uint8_t b8;
typedef enum { AL = 0, CL, DL, BL, AH, CH, DH, BH, Reg8_COUNT } Reg8;
typedef enum { AX = 0, CX, DX, BX, SP, BP, SI, DI, Reg16_COUNT } Reg16;

typedef struct {
 u8* current;
 u8* end;
 size_t size;
} Byte_Stream;


///////////////////////////////////
static const char* reg8_names[Reg8_COUNT] = { "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh" };
static const char* reg16_names[Reg16_COUNT] = { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di" };
static const char** reg_table[] = { &reg8_names[0], &reg16_names[0] };


///////////////////////////////////
b8
read_byte(Byte_Stream* stream, u8* out)
{
	if (stream->current == stream->end)
	{
	 return 0;
	}

	*out = *(stream->current);

	stream->current++;

	return 1;
}

Byte_Stream
read_file(const char* filepath)
{
	Byte_Stream result = {0};

	FILE* file = fopen(filepath, "rb");

	if ( file )
	{
		fseek(file, 0, SEEK_END);
		size_t file_size = ftell(file);
		fseek(file, 0, SEEK_SET);

		void* data = malloc(file_size);

		if ( data )
		{
			if ( fread(data, 1, file_size, file) == file_size )
			{
				result.current = (u8*)data;
				result.end = result.current + file_size;
				result.size = file_size;
			}
		}

		fclose(file);
	}

	return result;
}

void
destroy_byte_stream(Byte_Stream* stream)
{
	u8* memory = stream->end - stream->size;
	free(memory);
}

int
main(int argc, char* argv[])
{
	Byte_Stream stream = read_file(argv[1]);

	printf("; %s\n", argv[1]);
	puts("bits 16");

	u8 byte = 0;
	while ( read_byte(&stream, &byte) )
	{
		switch ( byte >> 2 )
		{
		case MOV_MASK: {
			b8 to_register_direction = (byte & MOV_D_MASK);
			u8 is_wide_op = (byte & MOV_W_MASK);

			if ( !read_byte(&stream, &byte) )
			{
				return -1;
			}

			const char* left_reg = reg_table[is_wide_op][(byte & MOV_LREG_MASK) >> 3];
			const char* right_reg = reg_table[is_wide_op][byte & MOV_RREG_MASK];

			if ( !to_register_direction )
			{
				const char *tmp = left_reg;
				left_reg = right_reg;
				right_reg = tmp;
			}

			printf("mov %s, %s\n", left_reg, right_reg);

		} break;
		default: {
			fprintf(stderr, "unsupported opcode: 0x%x", byte);
			return -1;
		}
		}

	}

	destroy_byte_stream(&stream);

	return 0;
}
