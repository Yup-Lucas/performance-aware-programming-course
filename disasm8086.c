#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

///////////////////////////////////
#define MOV_MASK 0b10001000
#define MOV_D_MASK 0x2
#define MOV_W_MASK 0x1
#define MOV_MOD_MASK 0b11000000
#define MOV_RREG_MASK 0b111
#define MOV_LREG_MASK 0b111000

#define MOV_IMM_REG_MASK 0b10110000
#define MOV_IMM_REG_W_MASK 0b1000

#define MOV_IMM_RM_MASK 0b11000110

#define MOV_MEM_ACC_MASK 0b10100000

///////////////////////////////////
typedef uint8_t u8;
typedef uint8_t b8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef signed char i8;
typedef signed short i16;

typedef enum { AL = 0, CL, DL, BL, AH, CH, DH, BH, Reg8_COUNT } Reg8;
typedef enum { AX = 0, CX, DX, BX, SP, BP, SI, DI, Reg16_COUNT } Reg16;
typedef enum { Mov_Mod_00 = 0, Mov_Mod_01, Mov_Mod_10, Mov_Mod_11 } Mov_Mod;
typedef enum { Mov_RM_000 = 0, Mov_RM_001, Mov_RM_010, Mov_RM_011, Mov_RM_100, Mov_RM_101, Mov_RM_110, Mov_RM_111 } Mov_RM;

typedef struct {
  u8* memory;
  size_t size;
  size_t used;
} Arena;

#define arena_push_array(ARENA, TYPE, COUNT) (TYPE*)arena_push_array_(ARENA, sizeof(TYPE), COUNT, _Alignof(TYPE))

typedef struct {
  u8* current;
  u8* end;
  size_t size;
} Byte_Stream;

///////////////////////////////////
static const u8 accumulator_index = 0;
static const char* reg8_names[Reg8_COUNT] = { "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh" };
static const char* reg16_names[Reg16_COUNT] = { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di" };
static const char** reg_table[] = { &reg8_names[0], &reg16_names[0] };
static const char* mov_displacement_rm_00[] = {"bx + si", "bx + di", "bp + si", "bp + di", "si", "di", NULL, "bx"};
static const char* mov_displacement_rm[] = {"bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "bp", "bx"};
static u8 scratch_memory[1 << 20];

///////////////////////////////////
void
arena_reset(Arena* arena)
{
  arena->used = 0;
}

void*
arena_push_array_(Arena* arena, size_t type_size, u32 count, size_t alignment)
{
  assert(arena->memory);
  size_t alloc_size = (type_size * count + alignment) & (alignment - 1);
  size_t new_used = arena->used + alloc_size;
  new_used = (new_used + alignment) & (alignment - 1);
  if (new_used > arena->size) return NULL;

  void* allocated = arena->memory + arena->used;

  for (size_t i = 0; i < alloc_size; i++) {
    ((u8*)allocated)[i] = 0;
  }
  
  arena->used = new_used;
  return allocated;
}

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

const char*
format(Arena* arena, const char* fmt, ...)
{
  char* buf = arena_push_array(arena, char, 255);
  va_list args;
  va_start(args, fmt);


#pragma warning(disable: 4774)
  vsnprintf(buf, 255, fmt, args);
#pragma warning(default: 4774)
  
  va_end(args);
  return buf;
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

  Arena scratch_arena = {
    .memory = &scratch_memory[0],
    .size = sizeof(scratch_memory),
    .used = 0
  };
 
  Byte_Stream stream = read_file(argv[1]);

  printf("; %s\n", argv[1]);
  puts("bits 16");

  u8 byte = 0;
  while ( read_u8(&stream, &byte) )
  {

    arena_reset(&scratch_arena);

    if ((byte & MOV_IMM_RM_MASK) == MOV_IMM_RM_MASK)
    {

#pragma warning(disable:4189)
      b8 wide = byte & 1;

      if (!read_u8(&stream, &byte))
      {
	return -1;
      }

      u8 mod = byte >> 6;
      u8 rm = byte & 7;

      b8 (*read_func)(Byte_Stream *, void *) = wide ? read_u16 : read_u8;
      
      if (mod == Mov_Mod_00)
      {
	  const char* displacement = mov_displacement_rm_00[rm];

	  u16 data = 0;
	  if (!read_func(&stream, &data)) return -1;
	  printf("mov [%s], %s %u\n", displacement, wide ? "word" : "byte", data);
      }
      else
      {
	const char* displacement = mov_displacement_rm[rm];
	u16 immediate = 0;
	if (!read_func(&stream, &immediate)) return -1;
	u16 data = 0;
	if (!read_func(&stream, &data)) return -1;

	printf("mov [%s + %u], %s %u\n", displacement, immediate, wide ? "word" : "byte", data);	
      }
#pragma warning(default: 4189)
    }
    else if ((byte & MOV_IMM_REG_MASK) == MOV_IMM_REG_MASK)
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
      u8 wide = (byte & MOV_W_MASK);

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
	  b8 (*read_func)(Byte_Stream *, void *) = wide ? read_u16 : read_u8;

	  u16 address = 0;
	  if (!read_func(&stream, &address)) return -1;

	  printf("mov %s, [%u]\n", reg_table[wide][reg], address);
	} break;
	default: {
	  const char* register_name = reg_table[wide][reg];
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
	i16 immediate = 0;
	if (mod == Mov_Mod_01)
	{
	  // Hack to make sure the number gets sign-extended into 16-bits
	  i8 tmp = 0;
	  if (!read_u8(&stream, &tmp)) return -1;

	  immediate = tmp;
	}
	else
	{
	  if (!read_u16(&stream, &immediate)) return -1;
	}

	const char* rhs;
	if (immediate == 0) rhs = format(&scratch_arena, "[%s]", mov_displacement_rm[rm]);
	else if (immediate < 0) rhs = format(&scratch_arena, "[%s - %hd]", mov_displacement_rm[rm], -immediate);
	else rhs = format(&scratch_arena, "[%s + %hd]", mov_displacement_rm[rm], immediate);

	const char* lhs = reg_table[wide][reg];	
	if (!to_register_direction)
	{
	  const char* tmp = rhs;
	  rhs = lhs;
	  lhs = tmp;
	}

	printf("mov %s, %s\n", lhs, rhs);
      } break;
      case Mov_Mod_11: {
	const char* left_reg = reg_table[wide][reg];
	const char* right_reg = reg_table[wide][rm];
      
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

    else if ( (byte & MOV_MEM_ACC_MASK) == MOV_MEM_ACC_MASK)
    {
      b8 acc_to_mem = byte & 2;
      b8 wide = byte & 1;
      b8 (*read_func)(Byte_Stream *, void *) = wide ? read_u16 : read_u8;

      u16 address = 0;
      if (!read_func(&stream, &address)) return -1;

      if (!acc_to_mem)
      {
        printf("mov %s, [%u]\n", reg_table[wide][accumulator_index], address);
      }
      else
      {
        printf("mov [%u], %s\n", address, reg_table[wide][accumulator_index]);
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
