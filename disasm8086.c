#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

///////////////////////////////////
#define MOV_MASK 0b10001000
#define MOV_D_MASK 0x2
#define MOV_W_MASK 0x1
#define MOV_MOD_MASK 0b11000000
#define MOV_RREG_MASK 0b111
#define MOV_LREG_MASK 0b111000

#define MOV_IMM_REG_MASK 0b10110000
#define MOV_IMM_REG_W_MASK 0b1000

///////////////////////////////////
typedef uint8_t u8;
typedef uint8_t b8;
typedef uint16_t u16;
typedef enum { AL = 0, CL, DL, BL, AH, CH, DH, BH, Reg8_COUNT } Reg8;
typedef enum { AX = 0, CX, DX, BX, SP, BP, SI, DI, Reg16_COUNT } Reg16;
typedef enum { Mov_Mod_00 = 0, Mov_Mod_01, Mov_Mod_10, Mov_Mod_11 } Mov_Mod;
typedef enum { Mov_RM_000 = 0, Mov_RM_001, Mov_RM_010, Mov_RM_011, Mov_RM_100, Mov_RM_101, Mov_RM_110, Mov_RM_111 } Mov_RM;

typedef struct {
  u8* current;
  u8* end;
  size_t size;
} Byte_Stream;

///////////////////////////////////
static const char* reg8_names[Reg8_COUNT] = { "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh" };
static const char* reg16_names[Reg16_COUNT] = { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di" };
static const char** reg_table[] = { &reg8_names[0], &reg16_names[0] };
static const char* mov_displacement_rm_00[] = {"bx + si", "bx + di", "bp + si", "bp + di", "si", "di", NULL, "bx"};
static const char* mov_displacement_rm[] = {"bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "bp", "bx"};


///////////////////////////////////
b8
read_u8(Byte_Stream* stream, void* out)
{
  if (stream->current == stream->end)
    {
      return 0;
    }

  *(u8*)out = *(stream->current);

  stream->current++;

  return 1;
}

b8
read_u16(Byte_Stream* stream, void* out)
{
  if ((stream->end - stream->current) < 2)
  {
    return 0;
  }

  *(u16*)out = *((u16*)stream->current);
  stream->current += 2;

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

void
print_usage(const char* executable_name)
{
  printf("Usage: %s <binary file path>", executable_name);
}

int
main(int argc, char* argv[])
{

  if (argc != 2)
  {
    print_usage(argv[0]);
    return 1;
  }
  
  Byte_Stream stream = read_file(argv[1]);

  printf("; %s\n", argv[1]);
  puts("bits 16");

  u8 byte = 0;
  while ( read_u8(&stream, &byte) )
  {
    if ((byte & MOV_IMM_REG_MASK) == MOV_IMM_REG_MASK)
    {
      b8 wide = (byte & MOV_IMM_REG_W_MASK) >> 3;
      const char* register_name = reg_table[wide][byte & 3];

      u16 immediate = 0;
      b8 (*read_func)(Byte_Stream *, void *) = wide ? read_u16 : read_u8;
      if (!read_func(&stream, &immediate))
      {
	return -1;
      }
      
      printf("mov %s, %u\n", register_name, immediate);
    }
    else if ((byte & MOV_MASK) == MOV_MASK)
    {
      b8 to_register_direction = (byte & MOV_D_MASK);
      u8 is_wide_op = (byte & MOV_W_MASK);

      if ( !read_u8(&stream, &byte) )
      {
	  return -1;
      }


      u8 mod = byte >> 6;
      u8 reg = (byte & 0x38) >> 3;
      u8 rm = (byte & 7);

      switch(mod) {
      case Mov_Mod_00: {
	switch (rm) {
	case Mov_RM_110: {
	  return -1;
	} break;
	  
	default: {
	  const char* register_name = reg_table[is_wide_op][reg];
	  const char* displacement = mov_displacement_rm_00[rm];

	  if (to_register_direction)
	  {
	      printf("mov %s, [%s]\n", register_name, displacement);
	  }
	  else
	  {
	    printf("mov [%s], %s\n", displacement, register_name);
	  }

	}
	}
	
      } break;
      case Mov_Mod_01:
      case Mov_Mod_10: {
	const char* register_name = reg_table[is_wide_op][reg];
	const char* displacement = mov_displacement_rm[rm];

	u16 immediate = 0;
	b8 (*read_func)(Byte_Stream *, void *) = (mod == Mov_Mod_01) ? read_u8 : read_u16;

	if (!read_func(&stream, &immediate))
	{
	  return -1;
	}

	if (to_register_direction)
	{
	  if (immediate)
	  {
	    printf("mov %s, [%s + %u]\n", register_name, displacement, immediate);
	  }
	  else
	  {
	    printf("mov %s, [%s]\n", register_name, displacement);
	  }
	}
	else
	{
	  if (immediate)
	  {
	    printf("mov [%s + %u], %s\n", displacement, immediate, register_name);	    
	  }
	  else
	  {
	    printf("mov [%s], %s\n", displacement, register_name);
	  }
	}
	
      } break;
      case Mov_Mod_11: {
	const char* left_reg = reg_table[is_wide_op][reg];
	const char* right_reg = reg_table[is_wide_op][rm];
      
	if ( !to_register_direction )
	  {
	    const char *tmp = left_reg;
	    left_reg = right_reg;
	    right_reg = tmp;
	  }
      
	printf("mov %s, %s\n", left_reg, right_reg);	
      } break;

      default: {
	printf("Unsupported mov mod: %u\n", mod);
	return -1;
      } 
      }
    }
    else
    {
      fprintf(stderr, "unsupported opcode: 0x%x", byte);
      return -1;      
    }
  }
  
  destroy_byte_stream(&stream);

  return 0;
}
