/*
 *  compiler/gencomp_arm2.c - MC680x0 compilation generator (ARM Adaption JIT v1 & JIT v2)
 *
 *  Based on work Copyright 1995, 1996 Bernd Schmidt
 *  Changes for UAE-JIT Copyright 2000 Bernd Meyer
 *
 *  Adaptation for ARAnyM/ARM, copyright 2001-2015
 *    Milan Jurik, Jens Heitmann
 * 
 *  Basilisk II (C) 1997-2005 Christian Bauer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Notes
 *  =====
 *
 *  Advantages of JIT v2
 *  	- Processor independent style
 *  	- Reduced overhead
 *  	- Easier to understand / read
 *  	- Easier to optimize
 *  	- More precise flag handling
 *  	- Better optimization for different CPU version ARM, ARMv6 etc..
 *
 *  Disadvantages of JIT v2
 *  	- Less generated
 *  	- Requires more code implementation by hand (MidFunc)
 *  	- MIDFUNCS are more CPU minded (closer to raw)
 *  	- Separate code for each instruction (but this could be also an advantage, because you can concentrate on it)
 *
 * Additional note:
 *  	- current using jnf_xxx calls for non-flag operations and
 *  	  jff_xxx for flag operations
 *
 * Still todo:
 * 		- Optimize genamode, genastore, gen_writeXXX, gen_readXXX, genmovemXXX
 *
 */

#include "sysconfig.h"
#include "sysdeps.h"
#include <ctype.h>

#include "readcpu.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#undef abort

#define USE_JIT2

//#define DISABLE_I_OR_AND_EOR
//#define DISABLE_I_ORSR_EORSR_ANDSR
//#define DISABLE_I_SUB
//#define DISABLE_I_SUBA
//#define DISABLE_I_SUBX
//#define DISABLE_I_ADD
//#define DISABLE_I_ADDA
//#define DISABLE_I_ADDX
//#define DISABLE_I_NEG
//#define DISABLE_I_NEGX
//#define DISABLE_I_CLR
//#define DISABLE_I_NOT
//#define DISABLE_I_TST
//#define DISABLE_I_BCHG_BCLR_BSET_BTST
//#define DISABLE_I_CMPM_CMP
//#define DISABLE_I_CMPA
//#define DISABLE_I_MOVE
//#define DISABLE_I_MOVEA
//#define DISABLE_I_SWAP
//#define DISABLE_I_EXG
//#define DISABLE_I_EXT
//#define DISABLE_I_MVMEL
//#define DISABLE_I_MVMLE
//#define DISABLE_I_RTD
//#define DISABLE_I_LINK
//#define DISABLE_I_UNLK
//#define DISABLE_I_RTS
//#define DISABLE_I_JSR
//#define DISABLE_I_JMP
//#define DISABLE_I_BSR
//#define DISABLE_I_BCC
//#define DISABLE_I_LEA
//#define DISABLE_I_PEA
//#define DISABLE_I_DBCC
//#define DISABLE_I_SCC
//#define DISABLE_I_MULU
//#define DISABLE_I_MULS
//#define DISABLE_I_ASR
//#define DISABLE_I_ASL
//#define DISABLE_I_LSR
//#define DISABLE_I_LSL
//#define DISABLE_I_ROL
//#define DISABLE_I_ROR
//#define DISABLE_I_ROXL
//#define DISABLE_I_ROXR
//#define DISABLE_I_ASRW
//#define DISABLE_I_ASLW
//#define DISABLE_I_LSRW
//#define DISABLE_I_LSLW
//#define DISABLE_I_ROLW
//#define DISABLE_I_RORW
#define DISABLE_I_ROXLW
#define DISABLE_I_ROXRW
//#define DISABLE_I_MULL
//#define DISABLE_I_FPP
//#define DISABLE_I_FBCC
//#define DISABLE_I_FSCC
//#define DISABLE_I_MOVE16
//#define DISABLE_I_DIVU
//#define DISABLE_I_DIVS
//#define DISABLE_I_DIVL

//#define DISABLE_I_BFINS


#define RETURN "return 0;"
#define RETTYPE "uae_u32"
#define NEXT_CPU_LEVEL 5

#define BOOL_TYPE		"int"
#define failure			global_failure=1
#define FAILURE			global_failure=1
#define isjump			global_isjump=1
#define is_const_jump	global_iscjump=1;
#define isaddx			global_isaddx=1
#define uses_cmov		global_cmov=1
#define mayfail			global_mayfail=1
#define uses_fpu		global_fpu=1

int hack_opcode;

static int global_failure;
static int global_isjump;
static int global_iscjump;
static int global_isaddx;
static int global_cmov;
static int long_opcode;
static int global_mayfail;
static int global_fpu;

static char endstr[1000];
static char lines[100000];
static int comp_index = 0;

#include "flags_arm.h"

static int cond_codes[] = {
  NATIVE_CC_AL, -1,
	NATIVE_CC_HI, NATIVE_CC_LS,
	NATIVE_CC_CC, NATIVE_CC_CS,
	NATIVE_CC_NE, NATIVE_CC_EQ,
	NATIVE_CC_VC, NATIVE_CC_VS,
	NATIVE_CC_PL, NATIVE_CC_MI,
	NATIVE_CC_GE, NATIVE_CC_LT,
	NATIVE_CC_GT, NATIVE_CC_LE
};

static void comprintf(const char* format, ...) 
{
	va_list args;

	va_start(args, format);
	comp_index += vsprintf(lines + comp_index, format, args);
}

static void com_discard(void) 
{
	comp_index = 0;
}

static void com_flush(void) 
{
	int i;
	for (i = 0; i < comp_index; i++)
		putchar(lines[i]);
	com_discard();
}


static FILE *headerfile;
static FILE *stblfile;

static int using_prefetch;
static int using_exception_3;
static int cpu_level;
static int noflags;

/* For the current opcode, the next lower level that will have different code.
 * Initialized to -1 for each opcode. If it remains unchanged, indicates we
 * are done with that opcode.  */
static int next_cpu_level;

static int *opcode_map;
static int *opcode_next_clev;
static int *opcode_last_postfix;
static unsigned long *counts;

static void read_counts(void) 
{
	FILE *file;
	unsigned long opcode, count = 0, total;
	char name[20];
	int nr = 0;
	memset(counts, 0, 65536 * sizeof *counts);

	file = fopen("frequent.68k", "r");
	if (file) 
  {
		fscanf(file, "Total: %lu\n", &total);
		while (fscanf(file, "%lx: %lu %s\n", &opcode, &count, name) == 3) 
    {
			opcode_next_clev[nr] = NEXT_CPU_LEVEL;
			opcode_last_postfix[nr] = -1;
			opcode_map[nr++] = opcode;
			counts[opcode] = count;
		}
		fclose(file);
	}
	if (nr == nr_cpuop_funcs)
		return;
	for (opcode = 0; opcode < 0x10000; opcode++) 
  {
		if (table68k[opcode].handler == -1 && table68k[opcode].mnemo != i_ILLG
		&& counts[opcode] == 0) 
    {
			opcode_next_clev[nr] = NEXT_CPU_LEVEL;
			opcode_last_postfix[nr] = -1;
			opcode_map[nr++] = opcode;
			counts[opcode] = count;
		}
	}
	if (nr != nr_cpuop_funcs)
		abort();
}

static int n_braces = 0;
static int insn_n_cycles;

static void start_brace(void) 
{
	n_braces++;
	comprintf("{");
}

static void close_brace(void) 
{
	assert(n_braces > 0);
	n_braces--;
	comprintf("}");
}

static void finish_braces(void) 
{
	while (n_braces > 0)
		close_brace();
}

static inline void gen_update_next_handler(void) 
{
	return; /* Can anything clever be done here? */
}

static void gen_writebyte(const char* address, const char* source) 
{
	comprintf("\twritebyte(%s,%s);\n", address, source);
}

static void gen_writeword(const char* address, const char* source) 
{
	comprintf("\twriteword(%s,%s);\n", address, source);
}

static void gen_writelong(const char* address, const char* source) 
{
	comprintf("\twritelong(%s,%s);\n", address, source);
}

static void gen_readbyte(const char* address, const char* dest) 
{
	comprintf("\treadbyte(%s,%s);\n", address, dest);
}

static void gen_readword(const char* address, const char* dest) 
{
	comprintf("\treadword(%s,%s);\n", address, dest);
}

static void gen_readlong(const char* address, const char* dest) 
{
	comprintf("\treadlong(%s,%s);\n", address, dest);
}


static const char *gen_nextilong(void) 
{
	static char buffer[80];

	sprintf(buffer, "comp_get_ilong((m68k_pc_offset+=4)-4)");
	insn_n_cycles += 4;

	long_opcode = 1;
	return buffer;
}

static const char *gen_nextiword(void) 
{
	static char buffer[80];

	sprintf(buffer, "comp_get_iword((m68k_pc_offset+=2)-2)");
	insn_n_cycles += 2;

	long_opcode = 1;
	return buffer;
}

static const char *gen_nextibyte(void) 
{
	static char buffer[80];

	sprintf(buffer, "comp_get_ibyte((m68k_pc_offset+=2)-2)");
	insn_n_cycles += 2;

	long_opcode = 1;
	return buffer;
}


static void swap_opcode (void)
{
	/* no-op */
}

static void sync_m68k_pc(void) 
{
	comprintf("\t if (m68k_pc_offset>SYNC_PC_OFFSET) sync_m68k_pc();\n");
}


/* getv == 1: fetch data; getv != 0: check for odd address. If movem != 0,
 * the calling routine handles Apdi and Aipi modes.
 * gb-- movem == 2 means the same thing but for a MOVE16 instruction */
static void genamode(amodes mode, const char *reg, wordsizes size, const char *name, int getv, int movem) 
{
	switch (mode) 
  {
	case Dreg: /* Do we need to check dodgy here? */
		if (movem)
			abort();
		if (getv == 1 || getv == 2) {
			/* We generate the variable even for getv==2, so we can use
			 it as a destination for MOVE */
			comprintf("\tint %s=%s;\n", name, reg);
		}
		return;

	case Areg:
		if (movem)
			abort();
		if (getv == 1 || getv == 2) {
			/* see above */
			comprintf("\tint %s=dodgy?scratchie++:%s+8;\n", name, reg);
			if (getv == 1) {
				comprintf("\tif (dodgy) \n");
				comprintf("\t\tmov_l_rr(%s,%s+8);\n", name, reg);
			}
		}
		return;

	case Aind:
		comprintf("\tint %sa=dodgy?scratchie++:%s+8;\n", name, reg);
		comprintf("\tif (dodgy) \n");
		comprintf("\t\tmov_l_rr(%sa,%s+8);\n", name, reg);
		break;
	case Aipi:
		comprintf("\tint %sa=scratchie++;\n", name, reg);
		comprintf("\tmov_l_rr(%sa,%s+8);\n", name, reg);
		break;
	case Apdi:
		switch (size) 
    {
		case sz_byte:
			if (movem) {
				comprintf("\tint %sa=dodgy?scratchie++:%s+8;\n", name, reg);
				comprintf("\tif (dodgy) \n");
				comprintf("\tmov_l_rr(%sa,8+%s);\n", name, reg);
			} 
      else {
				start_brace();
				comprintf("\tint %sa=dodgy?scratchie++:%s+8;\n", name, reg);
				comprintf("\tarm_SUB_l_ri8(%s+8,areg_byteinc[%s]);\n", reg, reg);
				comprintf("\tif (dodgy) \n");
				comprintf("\tmov_l_rr(%sa,8+%s);\n", name, reg);
			}
			break;
		case sz_word:
			if (movem) {
				comprintf("\tint %sa=dodgy?scratchie++:%s+8;\n", name, reg);
				comprintf("\tif (dodgy) \n");
				comprintf("\tmov_l_rr(%sa,8+%s);\n", name, reg);
			} 
      else {
				start_brace();
				comprintf("\tint %sa=dodgy?scratchie++:%s+8;\n", name, reg);
				comprintf("\tarm_SUB_l_ri8(%s+8,2);\n", reg);
				comprintf("\tif (dodgy) \n");
				comprintf("\tmov_l_rr(%sa,8+%s);\n", name, reg);
			}
			break;
		case sz_long:
			if (movem) {
				comprintf("\tint %sa=dodgy?scratchie++:%s+8;\n", name, reg);
				comprintf("\tif (dodgy) \n");
				comprintf("\tmov_l_rr(%sa,8+%s);\n", name, reg);
			} 
      else {
				start_brace();
				comprintf("\tint %sa=dodgy?scratchie++:%s+8;\n", name, reg);
				comprintf("\tarm_SUB_l_ri8(%s+8,4);\n", reg);
				comprintf("\tif (dodgy) \n");
				comprintf("\tmov_l_rr(%sa,8+%s);\n", name, reg);
			}
			break;
		default:
			abort();
		}
		break;
	case Ad16:
		comprintf("\tint %sa=scratchie++;\n", name);
    comprintf("\tlea_l_brr(%sa,8+%s,(uae_s32)(uae_s16)%s);\n", name, reg, gen_nextiword());
		break;
	case Ad8r:
		comprintf("\tint %sa=scratchie++;\n", name);
		comprintf("\tcalc_disp_ea_020(%s+8,%s,%sa);\n", reg, gen_nextiword(), name);
		break;

	case PC16:
		comprintf("\tint %sa=scratchie++;\n", name);
		comprintf("\tuae_u32 address=start_pc+((char *)comp_pc_p-(char *)start_pc_p)+m68k_pc_offset;\n");
		comprintf("\tuae_s32 PC16off = (uae_s32)(uae_s16)%s;\n", gen_nextiword());
		comprintf("\tmov_l_ri(%sa,address+PC16off);\n", name);
		break;

	case PC8r:
		comprintf("\tint pctmp=scratchie++;\n");
		comprintf("\tint %sa=scratchie++;\n", name);
		comprintf("\tuae_u32 address=start_pc+((char *)comp_pc_p-(char *)start_pc_p)+m68k_pc_offset;\n");
		start_brace();
		comprintf("\tmov_l_ri(pctmp,address);\n");

		comprintf("\tcalc_disp_ea_020(pctmp,%s,%sa);\n", gen_nextiword(), name);
		break;
	case absw:
		comprintf("\tint %sa = scratchie++;\n", name);
		comprintf("\tmov_l_ri(%sa,(uae_s32)(uae_s16)%s);\n", name, gen_nextiword());
		break;
	case absl:
		comprintf("\tint %sa = scratchie++;\n", name);
		comprintf("\tmov_l_ri(%sa,%s); /* absl */\n", name, gen_nextilong());
		break;
	case imm:
		if (getv != 1)
			abort();
		switch (size) 
    {
		case sz_byte:
			comprintf("\tint %s = scratchie++;\n", name);
			comprintf("\tmov_l_ri(%s,(uae_s32)(uae_s8)%s);\n", name, gen_nextibyte());
			break;
		case sz_word:
			comprintf("\tint %s = scratchie++;\n", name);
			comprintf("\tmov_l_ri(%s,(uae_s32)(uae_s16)%s);\n", name, gen_nextiword());
			break;
		case sz_long:
			comprintf("\tint %s = scratchie++;\n", name);
			comprintf("\tmov_l_ri(%s,%s);\n", name, gen_nextilong());
			break;
		default:
			abort();
		}
		return;
	case imm0:
		if (getv != 1)
			abort();
		comprintf("\tint %s = scratchie++;\n", name);
		comprintf("\tmov_l_ri(%s,(uae_s32)(uae_s8)%s);\n", name, gen_nextibyte());
		return;
	case imm1:
		if (getv != 1)
			abort();
		comprintf("\tint %s = scratchie++;\n", name);
		comprintf("\tmov_l_ri(%s,(uae_s32)(uae_s16)%s);\n", name, gen_nextiword());
		return;
	case imm2:
		if (getv != 1)
			abort();
		comprintf("\tint %s = scratchie++;\n", name);
		comprintf("\tmov_l_ri(%s,%s);\n", name, gen_nextilong());
		return;
	case immi:
		if (getv != 1)
			abort();
		comprintf("\tint %s = scratchie++;\n", name);
		comprintf("\tmov_l_ri(%s,%s);\n", name, reg);
		return;
	default:
		abort();
	}

	/* We get here for all non-reg non-immediate addressing modes to
	 * actually fetch the value. */
	if (getv == 1) 
  {
		char astring[80];
		sprintf(astring, "%sa", name);
		switch (size) 
    {
		  case sz_byte:
			  insn_n_cycles += 2;
			  break;
		  case sz_word:
			  insn_n_cycles += 2;
			  break;
		  case sz_long:
			  insn_n_cycles += 4;
			  break;
		  default:
			  abort();
		}
		comprintf("\tint %s=scratchie++;\n", name);
		switch (size) 
    {
		  case sz_byte:
			  gen_readbyte(astring, name);
			  break;
		  case sz_word:
			  gen_readword(astring, name);
			  break;
		  case sz_long:
			  gen_readlong(astring, name);
			  break;
		  default:
			  abort();
		}
	}

	/* We now might have to fix up the register for pre-dec or post-inc
	 * addressing modes. */
	if (!movem) {
		switch (mode) 
    {
		case Aipi:
			switch (size) 
      {
			case sz_byte:
				comprintf("\tarm_ADD_l_ri8(%s+8,areg_byteinc[%s]);\n", reg, reg);
				break;
			case sz_word:
				comprintf("\tarm_ADD_l_ri8(%s+8,2);\n", reg);
				break;
			case sz_long:
				comprintf("\tarm_ADD_l_ri8(%s+8,4);\n", reg);
				break;
			default:
				abort();
			}
			break;
		case Apdi:
			break;
		default:
			break;
		}
	}
}

/* getv == 1: fetch data; getv != 0: check for odd address. If movem != 0,
* the calling routine handles Apdi and Aipi modes.
* gb-- movem == 2 means the same thing but for a MOVE16 instruction */
static void genamode_new(amodes mode, const char *reg, wordsizes size, const char *name, int getv, int movem)
{
  switch (mode)
  {
  case Dreg:
    if (movem)
      abort();
    if (getv == 1 || getv == 2) {
      /* We generate the variable even for getv==2, so we can use
      it as a destination for MOVE */
      if (name != "") {
        comprintf("\tint %s=%s;\n", name, reg);
      }
    }
    return;

  case Areg:
    if (movem)
      abort();
    if (getv == 1 || getv == 2) {
      /* see above */
      comprintf("\tint %s=%s+8;\n", name, reg);
    }
    return;

  case Aind:
    comprintf("\tint %sa=%s+8;\n", name, reg);
    break;
  case Aipi:
    comprintf("\tint %sa=%s+8;\n", name, reg);
    break;
  case Apdi:
    switch (size)
    {
    case sz_byte:
      if (movem) {
        comprintf("\tint %sa=%s+8;\n", name, reg);
      }
      else {
        comprintf("\tint %sa=%s+8;\n", name, reg);
        comprintf("\tarm_SUB_l_ri8(%s+8,areg_byteinc[%s]);\n", reg, reg);
      }
      break;
    case sz_word:
      if (movem) {
        comprintf("\tint %sa=%s+8;\n", name, reg);
      }
      else {
        comprintf("\tint %sa=%s+8;\n", name, reg);
        comprintf("\tarm_SUB_l_ri8(%s+8,2);\n", reg);
      }
      break;
    case sz_long:
      if (movem) {
        comprintf("\tint %sa=%s+8;\n", name, reg);
      }
      else {
        comprintf("\tint %sa=%s+8;\n", name, reg);
        comprintf("\tarm_SUB_l_ri8(%s+8,4);\n", reg);
      }
      break;
    default:
      abort();
    }
    break;
  case Ad16:
    comprintf("\tint %sa=scratchie++;\n", name);
    comprintf("\tlea_l_brr(%sa,8+%s,(uae_s32)(uae_s16)%s);\n", name, reg, gen_nextiword());
    break;
  case Ad8r:
    comprintf("\tint %sa=scratchie++;\n", name);
    comprintf("\tcalc_disp_ea_020(%s+8,%s,%sa);\n", reg, gen_nextiword(), name);
    break;

  case PC16:
    comprintf("\tint %sa=scratchie++;\n", name);
    comprintf("\tuae_u32 address=start_pc+((char *)comp_pc_p-(char *)start_pc_p)+m68k_pc_offset;\n");
    comprintf("\tuae_s32 PC16off = (uae_s32)(uae_s16)%s;\n", gen_nextiword());
    comprintf("\tmov_l_ri(%sa,address+PC16off);\n", name);
    break;

  case PC8r:
    comprintf("\tint pctmp=scratchie++;\n");
    comprintf("\tint %sa=scratchie++;\n", name);
    comprintf("\tuae_u32 address=start_pc+((char *)comp_pc_p-(char *)start_pc_p)+m68k_pc_offset;\n");
    comprintf("\tmov_l_ri(pctmp,address);\n");

    comprintf("\tcalc_disp_ea_020(pctmp,%s,%sa);\n", gen_nextiword(), name);
    break;
  case absw:
    comprintf("\tint %sa = scratchie++;\n", name);
    comprintf("\tmov_l_ri(%sa,(uae_s32)(uae_s16)%s);\n", name, gen_nextiword());
    break;
  case absl:
    comprintf("\tint %sa = scratchie++;\n", name);
    comprintf("\tmov_l_ri(%sa,%s); /* absl */\n", name, gen_nextilong());
    break;
  case imm:
    if (getv != 1)
      abort();
    switch (size)
    {
    case sz_byte:
      comprintf("\tint %s = scratchie++;\n", name);
      comprintf("\tmov_l_ri(%s,(uae_s32)(uae_s8)%s);\n", name, gen_nextibyte());
      break;
    case sz_word:
      comprintf("\tint %s = scratchie++;\n", name);
      comprintf("\tmov_l_ri(%s,(uae_s32)(uae_s16)%s);\n", name, gen_nextiword());
      break;
    case sz_long:
      comprintf("\tint %s = scratchie++;\n", name);
      comprintf("\tmov_l_ri(%s,%s);\n", name, gen_nextilong());
      break;
    default:
      abort();
    }
    return;
  case imm0:
    if (getv != 1)
      abort();
    comprintf("\tint %s = scratchie++;\n", name);
    comprintf("\tmov_l_ri(%s,(uae_s32)(uae_s8)%s);\n", name, gen_nextibyte());
    return;
  case imm1:
    if (getv != 1)
      abort();
    comprintf("\tint %s = scratchie++;\n", name);
    comprintf("\tmov_l_ri(%s,(uae_s32)(uae_s16)%s);\n", name, gen_nextiword());
    return;
  case imm2:
    if (getv != 1)
      abort();
    comprintf("\tint %s = scratchie++;\n", name);
    comprintf("\tmov_l_ri(%s,%s);\n", name, gen_nextilong());
    return;
  case immi:
    if (getv != 1)
      abort();
    if (name != "") {
      comprintf("\tint %s = scratchie++;\n", name);
      comprintf("\tmov_l_ri(%s,%s);\n", name, reg);
    }
    return;
  default:
    abort();
  }

  /* We get here for all non-reg non-immediate addressing modes to
  * actually fetch the value. */
  if (getv == 1)
  {
    char astring[80];
    sprintf(astring, "%sa", name);
    switch (size)
    {
    case sz_byte:
      insn_n_cycles += 2;
      break;
    case sz_word:
      insn_n_cycles += 2;
      break;
    case sz_long:
      insn_n_cycles += 4;
      break;
    default:
      abort();
    }
    comprintf("\tint %s=scratchie++;\n", name);
    switch (size)
    {
    case sz_byte:
      gen_readbyte(astring, name);
      break;
    case sz_word:
      gen_readword(astring, name);
      break;
    case sz_long:
      gen_readlong(astring, name);
      break;
    default:
      abort();
    }
  }
}

static void genamode_post(amodes mode, const char *reg, wordsizes size, const char *name, int getv, int movem)
{
  /* We now might have to fix up the register for pre-dec or post-inc
  * addressing modes. */
  if (!movem) {
    switch (mode)
    {
      case Aipi:
        switch (size)
        {
          case sz_byte:
            comprintf("\tarm_ADD_l_ri8(%s+8,areg_byteinc[%s]);\n", reg, reg);
            break;
          case sz_word:
            comprintf("\tarm_ADD_l_ri8(%s+8,2);\n", reg);
            break;
          case sz_long:
            comprintf("\tarm_ADD_l_ri8(%s+8,4);\n", reg);
            break;
          default:
            abort();
        }
        break;
    }
  }
}

static void genastore(const char *from, amodes mode, const char *reg, wordsizes size, const char *to)
{
	switch (mode) 
  {
	case Dreg:
		switch (size) 
    {
		  case sz_byte:
			  comprintf("\tif(%s!=%s)\n", reg, from);
			  comprintf("\t\tmov_b_rr(%s,%s);\n", reg, from);
			  break;
		  case sz_word:
			  comprintf("\tif(%s!=%s)\n", reg, from);
			  comprintf("\t\tmov_w_rr(%s,%s);\n", reg, from);
			  break;
		  case sz_long:
			  comprintf("\tif(%s!=%s)\n", reg, from);
			  comprintf("\t\tmov_l_rr(%s,%s);\n", reg, from);
			  break;
		  default:
			  abort();
		}
		break;
	case Areg:
		switch (size) 
    {
		  case sz_word:
			  comprintf("\tif(%s+8!=%s)\n", reg, from);
			  comprintf("\t\tmov_w_rr(%s+8,%s);\n", reg, from);
			  break;
		  case sz_long:
			  comprintf("\tif(%s+8!=%s)\n", reg, from);
			  comprintf("\t\tmov_l_rr(%s+8,%s);\n", reg, from);
			  break;
		  default:
			  abort();
		}
		break;

	case Apdi:
	case absw:
	case PC16:
	case PC8r:
	case Ad16:
	case Ad8r:
	case Aipi:
	case Aind:
	case absl: 
  {
		char astring[80];
		sprintf(astring, "%sa", to);

		switch (size) 
    {
		  case sz_byte:
			  insn_n_cycles += 2;
			  gen_writebyte(astring, from);
			  break;
		  case sz_word:
			  insn_n_cycles += 2;
			  gen_writeword(astring, from);
			  break;
		  case sz_long:
			  insn_n_cycles += 4;
			  gen_writelong(astring, from);
			  break;
		  default:
			  abort();
		}
	}
		break;
	case imm:
	case imm0:
	case imm1:
	case imm2:
	case immi:
		abort();
		break;
	default:
		abort();
	}
}

static void genastore_new(const char *from, amodes mode, wordsizes size, const char *to) 
{
	switch (mode) 
  {
  	case Dreg:
  		switch (size) 
      {
  		  case sz_byte:
  			  break;
  		  case sz_word:
  			  break;
  		  case sz_long:
  			  break;
  		  default:
  			  abort();
  		}
  		break;
  	case Areg:
  		switch (size) 
      {
  		  case sz_word:
  			  break;
  		  case sz_long:
  			  break;
  		  default:
  			  abort();
  		}
  		break;
  
  	case Apdi:
  	case absw:
  	case PC16:
  	case PC8r:
  	case Ad16:
  	case Ad8r:
  	case Aipi:
  	case Aind:
  	case absl: 
    {
  		char astring[80];
  		sprintf(astring, "%sa", to);
  
  		switch (size) 
      {
  		  case sz_byte:
  			  insn_n_cycles += 2;
  			  gen_writebyte(astring, from);
  			  break;
  		  case sz_word:
  			  insn_n_cycles += 2;
  			  gen_writeword(astring, from);
  			  break;
  		  case sz_long:
  			  insn_n_cycles += 4;
  			  gen_writelong(astring, from);
  			  break;
  		  default:
  			  abort();
  		}
  	}
  		break;
  	case imm:
  	case imm0:
  	case imm1:
  	case imm2:
  	case immi:
  		abort();
  		break;
  	default:
  		abort();
	}
}

static void gen_move16(uae_u32 opcode, struct instr *curi) 
{
	mayfail;
	comprintf("if (special_mem) {\n");
	comprintf("  FAIL(1);\n");
  comprintf("  " RETURN "\n");
	comprintf("} \n");

	uae_u32 masked_op = (opcode & 0xfff8);
	if (masked_op == 0xf620) {
		// POSTINCREMENT SOURCE AND DESTINATION version: MOVE16 (Ax)+,(Ay)+
    start_brace();
		comprintf("\t uae_u16 dstreg = ((%s)>>12) & 0x07;\n", gen_nextiword());
    comprintf("\tint srca=srcreg + 8;\n");
    comprintf("\tint dsta=dstreg + 8;\n");
	} else {
		/* Other variants */
		genamode_new(curi->smode, "srcreg", curi->size, "src", 0, 2);
		genamode_new(curi->dmode, "dstreg", curi->size, "dst", 0, 2);
		switch (masked_op) {
			case 0xf600:
			  comprintf("\t jnf_ADD_im8(srcreg + 8, srcreg + 8, 16);\n");
			  break;
			case 0xf608:
			  comprintf("\t jnf_ADD_im8(dstreg + 8, dstreg + 8, 16);\n");
			  break;
		}
	}
	comprintf("\tjnf_MOVE16(dsta, srca);\n");
	if (masked_op == 0xf620) {
		comprintf("\t if (srcreg != dstreg)\n");
		comprintf("\t   jnf_ADD_im8(srcreg + 8, srcreg + 8, 16);\n");
		comprintf("\t jnf_ADD_im8(dstreg + 8, dstreg + 8, 16);\n");
	} else {
		genamode_post(curi->smode, "srcreg", curi->size, "src", 0, 2);
		genamode_post(curi->dmode, "dstreg", curi->size, "dst", 0, 2);
  }
}

static void genmovemel(uae_u16 opcode) 
{
	comprintf("\tuae_u16 mask = %s;\n", gen_nextiword());
	comprintf("\tint native=scratchie++;\n");
	comprintf("\tint i;\n");
	comprintf("\tsigned char offset=0;\n");
	genamode_new(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "src", 2,	1);
  comprintf("\tif (!special_mem) {\n");

  /* Fast but unsafe...  */
	comprintf("\tget_n_addr(srca,native);\n");

	comprintf("\tfor (i=0;i<16;i++) {\n"
			"\t\tif ((mask>>i)&1) {\n");
	switch (table68k[opcode].size) {
	case sz_long:
    if (table68k[opcode].dmode == Aipi)
      comprintf("\t\t\tif(srca != i)\n");
    comprintf("\t\t\tjnf_MVMEL_l(i,native,offset);\n");
    comprintf("\t\t\toffset+=4;\n");
		break;
	case sz_word:
    if (table68k[opcode].dmode == Aipi)
      comprintf("\t\t\tif(srca != i)\n");
    comprintf("\t\t\tjnf_MVMEL_w(i,native,offset);\n");
    comprintf("\t\t\toffset+=2;\n");
		break;
	default: abort();
	}
	comprintf("\t\t}\n"
			"\t}");
	if (table68k[opcode].dmode == Aipi) {
		comprintf("\t\t\tjnf_ADD_im8(8+dstreg,srca,offset);\n");
	}
  /* End fast but unsafe.   */

  comprintf("\t} else {\n");

  comprintf ("\t\tint tmp=scratchie++;\n");

  comprintf("\t\tmov_l_rr(tmp,srca);\n");
  comprintf("\t\tfor (i=0;i<16;i++) {\n"
	      "\t\t\tif ((mask>>i)&1) {\n");
  switch(table68k[opcode].size) {
    case sz_long:
	    comprintf("\t\t\t\treadlong(tmp,i);\n"
		      "\t\t\t\tarm_ADD_l_ri8(tmp,4);\n");
	    break;
    case sz_word:
	    comprintf("\t\t\t\treadword(tmp,i);\n"
		      "\t\t\t\tarm_ADD_l_ri8(tmp,2);\n");
	    break;
    default: abort();
	}

  comprintf("\t\t\t}\n"
	      "\t\t}\n");
  if (table68k[opcode].dmode == Aipi) {
  	comprintf("\t\tmov_l_rr(8+dstreg,tmp);\n");
  }
  comprintf("\t}\n");
}


static void genmovemle(uae_u16 opcode) 
{
	comprintf("\tuae_u16 mask = %s;\n", gen_nextiword());
	comprintf("\tint native=scratchie++;\n");
	comprintf("\tint i;\n");
	comprintf("\tint tmp=scratchie++;\n");
	comprintf("\tsigned char offset=0;\n");
	genamode_new(table68k[opcode].dmode, "dstreg", table68k[opcode].size, "src", 2, 1);

  /* *Sigh* Some clever geek realized that the fastest way to copy a
      buffer from main memory to the gfx card is by using movmle. Good
      on her, but unfortunately, gfx mem isn't "real" mem, and thus that
      act of cleverness means that movmle must pay attention to special_mem,
      or Genetic Species is a rather boring-looking game ;-) */
  comprintf("\tif (!special_mem) {\n");
	comprintf("\tget_n_addr(srca,native);\n");

	if (table68k[opcode].dmode != Apdi) {
		comprintf("\tfor (i=0;i<16;i++) {\n"
				"\t\tif ((mask>>i)&1) {\n");
		switch (table68k[opcode].size) {
		case sz_long:
			comprintf("\t\t\tjnf_MVMLE_l(native,i,offset);\n"
					"\t\t\toffset+=4;\n");
			break;
		case sz_word:
			comprintf("\t\t\tjnf_MVMLE_w(native,i,offset);\n"
					"\t\t\toffset+=2;\n");
			break;
		default: abort();
		}
	} 
  else { /* Pre-decrement */
		comprintf("\tfor (i=0;i<16;i++) {\n"
				"\t\tif ((mask>>i)&1) {\n");
		switch (table68k[opcode].size) {
		case sz_long:
			comprintf("\t\t\toffset-=4;\n"
					"\t\t\tjnf_MVMLE_l(native,15-i,offset);\n"
          );
			break;
		case sz_word:
			comprintf("\t\t\toffset-=2;\n"
					"\t\t\tjnf_MVMLE_w(native,15-i,offset);\n"
          );
			break;
		default: abort();
		}
	}


	comprintf("\t\t}\n"
			"\t}\n");
	if (table68k[opcode].dmode == Apdi) {
		comprintf("\t\t\tlea_l_brr(8+dstreg,srca,(uae_s32)offset);\n");
	}
  comprintf("\t} else {\n");

  if (table68k[opcode].dmode!=Apdi) {
	  comprintf("\tmov_l_rr(tmp,srca);\n");
	  comprintf("\tfor (i=0;i<16;i++) {\n"
  		  "\t\tif ((mask>>i)&1) {\n");
	  switch(table68k[opcode].size) {
	    case sz_long:
	      comprintf("\t\t\twritelong(tmp,i);\n"
		        "\t\t\tarm_ADD_l_ri8(tmp,4);\n");
	      break;
	    case sz_word:
	      comprintf("\t\t\twriteword(tmp,i);\n"
		        "\t\t\tarm_ADD_l_ri8(tmp,2);\n");
	      break;
	    default: abort();
		}
  }
  else {  /* Pre-decrement */
	  comprintf("\tfor (i=0;i<16;i++) {\n"
		    "\t\tif ((mask>>i)&1) {\n");
	  switch(table68k[opcode].size) {
	    case sz_long:
	      comprintf("\t\t\tarm_SUB_l_ri8(srca,4);\n"
		        "\t\t\twritelong(srca,15-i);\n");
	      break;
	    case sz_word:
	      comprintf("\t\t\tarm_SUB_l_ri8(srca,2);\n"
		        "\t\t\twriteword(srca,15-i);\n");
	      break;
	    default: abort();
	  }
  }


  comprintf("\t\t}\n"
	      "\t}");
  if (table68k[opcode].dmode == Apdi) {
  	comprintf("\t\t\tmov_l_rr(8+dstreg,srca);\n");
  }
  comprintf("\t}\n");
}


typedef enum 
{
	flag_logical_noclobber, flag_logical, flag_add, flag_sub, flag_cmp,
	flag_addx, flag_subx, flag_zn, flag_av, flag_sv, flag_and, flag_or,
	flag_eor, flag_mov
} 
flagtypes;

static void gen_add(uae_u32 opcode, struct instr *curi, char* ssize) 
{
	(void) opcode;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_new(curi->dmode, "dstreg", curi->size, "dst", 1, 0);

	comprintf("\t dont_care_flags();\n");
	// Use tmp register to avoid destroying upper part in .B., .W cases
	if (!noflags) {
    comprintf("\t jff_ADD_%s(dst,src);\n", ssize);
		comprintf("\t live_flags();\n");
		comprintf(
				"\t if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
	} else {
    comprintf("\t jnf_ADD_%s(dst,src);\n", ssize);
  }
  genastore_new("dst", curi->dmode, curi->size, "dst");
  genamode_post(curi->dmode, "dstreg", curi->size, "dst", 1, 0);
}

static void gen_adda(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_new(curi->dmode, "dstreg", sz_long, "dst", 1, 0);
	comprintf("\t jnf_ADDA_%s(dst, src);\n", ssize);
  genastore_new("dst", curi->dmode, sz_long, "dst");
}

static void gen_addx(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	isaddx;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_new(curi->dmode, "dstreg", curi->size, "dst", 1, 0);

	// Use tmp register to avoid destroying upper part in .B., .W cases
	comprintf("\t dont_care_flags();\n");
	if (!noflags) {
		comprintf("\t make_flags_live();\n");
    comprintf("\t jff_ADDX_%s(dst,src);\n", ssize);
    comprintf("\t live_flags();\n");
		comprintf("\t if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
	} else {
    comprintf("\t jnf_ADDX_%s(dst,src);\n", ssize);
  }
  genastore_new("dst", curi->dmode, curi->size, "dst");
  genamode_post(curi->dmode, "dstreg", curi->size, "dst", 1, 0);
}

static void gen_and(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_new(curi->dmode, "dstreg", curi->size, "dst", 1, 0);

	comprintf("\t dont_care_flags();\n");
	if (!noflags) {
    comprintf("\t jff_AND_%s(dst,src);\n", ssize);
    comprintf("\t live_flags();\n");
	} else {
    comprintf("\t jnf_AND_%s(dst,src);\n", ssize);
  }
  genastore_new("dst", curi->dmode, curi->size, "dst");
  genamode_post(curi->dmode, "dstreg", curi->size, "dst", 1, 0);
}

static void gen_andsr(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	(void) ssize;
	comprintf("\tint src = (uae_s32)(uae_s16)%s;\n", gen_nextiword());
	if (!noflags) {
		comprintf("\t make_flags_live();\n");
		comprintf("\t jff_ANDSR(ARM_CCR_MAP[src & 0xF], (src & 0x10));\n");
		comprintf("\t live_flags();\n");
	}
}

static void gen_asl(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	comprintf("\t dont_care_flags();\n");

	genamode_new(curi->smode, "srcreg", curi->size, "", 1, 0);
	genamode_new(curi->dmode, "dstreg", curi->size, "data", 1, 0);

	if (curi->smode != immi) {
		if (!noflags) {
      comprintf("\t jff_ASL_%s_reg(data,srcreg);\n", ssize);
      comprintf("\t live_flags();\n");
			comprintf(
					"\t if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
		} else {
      comprintf("\t jnf_LSL_%s_reg(data,srcreg);\n", ssize);
    }
	} else {
		if (!noflags) {
      comprintf("\t jff_ASL_%s_imm(data,srcreg);\n", ssize);
      comprintf("\t live_flags();\n");
			comprintf(
					"\t if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
		} else {
      comprintf("\t jnf_LSL_%s_imm(data,srcreg);\n", ssize);
    }
	}
  genastore_new("data", curi->dmode, curi->size, "data");
}

static void gen_aslw(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	(void) ssize;
	comprintf("\t dont_care_flags();\n");
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
	if (!noflags) {
		comprintf("\t jff_ASLW(src);\n");
		comprintf("\t live_flags();\n");
	} else {
		comprintf("\t jnf_ASLW(src);\n");
	}
	genastore_new("src", curi->smode, curi->size, "src");
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
}

static void gen_asr(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void)opcode;
	comprintf("\t dont_care_flags();\n");

	genamode_new(curi->smode, "srcreg", curi->size, "", 1, 0);
	genamode_new(curi->dmode, "dstreg", curi->size, "data", 1, 0);

	if (curi->smode != immi) {
		if (!noflags) {
			comprintf("\t jff_ASR_%s_reg(data,srcreg);\n", ssize);
			comprintf("\t live_flags();\n");
			comprintf(
					"if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
		} else {
			comprintf("\t jnf_ASR_%s_reg(data,srcreg);\n", ssize);
		}
	} else {
		char *op;
		if (!noflags) {
			op = "ff";
		} else
		  op = "nf";

		comprintf("\t j%s_ASR_%s_imm(data,srcreg);\n", op, ssize);
		if (!noflags) {
			comprintf("\t live_flags();\n");
			comprintf(
					"\t if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
		}
	}
  genastore_new("data", curi->dmode, curi->size, "data");
}

static void gen_asrw(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	(void) ssize;
	comprintf("\t dont_care_flags();\n");
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);

	if (!noflags) {
		comprintf("\t jff_ASRW(src);\n");
		comprintf("\t live_flags();\n");
	} else {
		comprintf("\t jnf_ASRW(src);\n");
	}
	genastore_new("src", curi->smode, curi->size, "src");
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
}

static void gen_bchg(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
	genamode_new(curi->dmode, "dstreg", curi->size, "dst", 1, 0);

	if (!noflags) {
		comprintf("\t make_flags_live();\n");
		comprintf("\t jff_BCHG_%s(dst,src);\n", ssize);
		comprintf("\t live_flags();\n");
	} else {
		comprintf("\t jnf_BCHG_%s(dst,src);\n", ssize);
		comprintf("\t dont_care_flags();\n");
	}
  genastore_new("dst", curi->dmode, curi->size, "dst");
  genamode_post(curi->dmode, "dstreg", curi->size, "dst", 1, 0);
}

static void gen_bclr(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
	genamode_new(curi->dmode, "dstreg", curi->size, "dst", 1, 0);

	if (!noflags) {
		comprintf("\t make_flags_live();\n");
		comprintf("\t jff_BCLR_%s(dst,src);\n", ssize);
		comprintf("\t live_flags();\n");
	} else {
		comprintf("\t jnf_BCLR_%s(dst,src);\n", ssize);
		comprintf("\t dont_care_flags();\n");
	}
  genastore_new("dst", curi->dmode, curi->size, "dst");
  genamode_post(curi->dmode, "dstreg", curi->size, "dst", 1, 0);
}

static void gen_bset(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
	genamode_new(curi->dmode, "dstreg", curi->size, "dst", 1, 0);

	if (!noflags) {
		comprintf("\t make_flags_live();\n");
		comprintf("\t jff_BSET_%s(dst,src);\n", ssize);
		comprintf("\t live_flags();\n");
	} else {
		comprintf("\t jnf_BSET_%s(dst,src);\n", ssize);
		comprintf("\t dont_care_flags();\n");
	}
  genastore_new("dst", curi->dmode, curi->size, "dst");
  genamode_post(curi->dmode, "dstreg", curi->size, "dst", 1, 0);
}

static void gen_btst(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
	genamode_new(curi->dmode, "dstreg", curi->size, "dst", 1, 0);

	// If we are not interested in flags it is not necessary to do
	// anything with the data
	if (!noflags) {
		comprintf("\t make_flags_live();\n");
		comprintf("\t jff_BTST_%s(dst,src);\n", ssize);
		comprintf("\t live_flags();\n");
	} else {
		comprintf("\t dont_care_flags();\n");
	}
  genamode_post(curi->dmode, "dstreg", curi->size, "dst", 1, 0);
}

static void gen_clr(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	(void) ssize;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 2, 0);
	if((curi->smode != Dreg) && (curi->smode != Areg))
    comprintf("\t int src=scratchie++;\n");
  comprintf("\t dont_care_flags();\n");
	if (!noflags) {
    comprintf("\t jff_CLR_%s(src);\n", ssize);
    comprintf("\t live_flags();\n");
	} else {
    comprintf("\t jnf_CLR_%s(src);\n", ssize);
  }
	genastore_new("src", curi->smode, curi->size, "src");
  genamode_post(curi->smode, "srcreg", curi->size, "src", 2, 0);
}

static void gen_cmp(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_new(curi->dmode, "dstreg", curi->size, "dst", 1, 0);
	comprintf("\t dont_care_flags();\n");
	if (!noflags) {
		comprintf("\t jff_CMP_%s(dst,src);\n", ssize);
		comprintf("\t live_flags();\n");
		comprintf("\t if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
	} else {
		comprintf("/* Weird --- CMP with noflags ;-) */\n");
	}
  genamode_post(curi->dmode, "dstreg", curi->size, "dst", 1, 0);
}

static void gen_cmpa(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_new(curi->dmode, "dstreg", sz_long, "dst", 1, 0);
	if (!noflags) {
		comprintf("\t dont_care_flags();\n");
		comprintf("\t jff_CMPA_%s(dst,src);\n", ssize);
		comprintf("\t live_flags();\n");
		comprintf("\t if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
	} else {
		comprintf("\tdont_care_flags();\n");
		comprintf("/* Weird --- CMP with noflags ;-) */\n");
	}
  genamode_post(curi->dmode, "dstreg", sz_long, "dst", 1, 0);
}

static void gen_dbcc(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	(void) ssize;
	isjump;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
	genamode_new(curi->dmode, "dstreg", curi->size, "offs", 1, 0);

	if (curi->size != sz_word)
		abort();

	/* That offs is an immediate, so we can clobber it with abandon */
	comprintf("\tsub_l_ri(offs, m68k_pc_offset - m68k_pc_offset_thisinst - 2);\n");
	comprintf("\tarm_ADD_l_ri(offs, (uintptr)comp_pc_p);\n"); 
  /* New PC, once the offset_68k is also added */
	/* Let's fold in the m68k_pc_offset at this point */
	comprintf("\tarm_ADD_l_ri(offs, m68k_pc_offset);\n");
	comprintf("\tarm_ADD_l_ri(PC_P, m68k_pc_offset);\n");
	comprintf("\tm68k_pc_offset=0;\n");

	if (curi->cc >= 2) {
		comprintf("\tmake_flags_live();\n"); /* Load the flags */
	}

	switch (curi->cc) {
	  case 0: /* This is an elaborate nop? */
		  break;
	  case 1:
		  comprintf("\tsub_w_ri(src, 1);\n");
		  comprintf("\tuae_u32 v2;\n");
		  comprintf("\tuae_u32 v1=get_const(PC_P);\n");
		  comprintf("\tv2=get_const(offs);\n");
		  comprintf("\tregister_branch(v1, v2, %d);\n", NATIVE_CC_CS);
		  break;

	  case 8:
	  case 9:
    case 2:
	  case 3:
	  case 4:
	  case 5:
	  case 6:
	  case 7:
	  case 10:
	  case 11:
	  case 12:
	  case 13:
	  case 14:
	  case 15:
		  comprintf("\tuae_u32 v1=get_const(PC_P);\n");
		  comprintf("\tuae_u32 v2=get_const(offs);\n");
      comprintf("\tjff_DBCC(src, %d);\n", cond_codes[curi->cc]);
      comprintf("\tregister_branch(v1, v2, %d);\n", NATIVE_CC_CS);
		  break;
	  default: abort();
	}
	gen_update_next_handler();
}

static void gen_divu(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	(void) ssize;
	comprintf("\t dont_care_flags();\n");
	genamode_new(curi->smode, "srcreg", sz_word, "src", 1, 0);
  genamode_post(curi->smode, "srcreg", sz_word, "src", 1, 0);
  genamode_new(curi->dmode, "dstreg", sz_word, "dst", 1, 0);

  comprintf("\tregister_possible_exception();\n");
  if (!noflags) {
		comprintf("\tjff_DIVU(dst,src);\n");
		comprintf("\tlive_flags();\n");
	} else {
		comprintf("\tjnf_DIVU(dst,src);\n");
	}

	genastore_new("dst", curi->dmode, sz_long /*curi->size*/, "dst");
}

static void gen_divs(uae_u32 opcode, struct instr *curi, char* ssize) {
  (void)opcode;
  (void)ssize;
  comprintf("\t dont_care_flags();\n");
  genamode_new(curi->smode, "srcreg", sz_word, "src", 1, 0);
  genamode_post(curi->smode, "srcreg", sz_word, "src", 1, 0);
  genamode_new(curi->dmode, "dstreg", sz_word, "dst", 1, 0);

  comprintf("\tregister_possible_exception();\n");
  if (!noflags) {
    comprintf("\tjff_DIVS(dst,src);\n");
    comprintf("\tlive_flags();\n");
  }
  else {
    comprintf("\tjnf_DIVS(dst,src);\n");
	}

	genastore_new("dst", curi->dmode, sz_long /*curi->size*/, "dst");
}

static void gen_divl(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	(void) ssize;
  comprintf("\t dont_care_flags();\n");
	comprintf("\t uae_u16 extra=%s;\n", gen_nextiword());
  comprintf("\t int r2=(extra>>12)&7;\n");
	comprintf("\t int r3=extra&7;\n");
	genamode_new(curi->dmode, "dstreg", curi->size, "dst", 1, 0);
  genamode_post(curi->dmode, "dstreg", curi->size, "dst", 1, 0);
  comprintf("\tregister_possible_exception();\n");

	if (!noflags) {
		comprintf("\t if (extra & 0x0400) {\n"); /* Need full 64 bit divisor */
		comprintf("\t   FAIL(1);\n");
    comprintf("\t   m68k_pc_offset=m68k_pc_offset_thisinst;\n");
		comprintf("\t  " RETURN "\n");
		comprintf("\t	} else { \n");
		/* operands in src and r2, result goes into r2, remainder into r3 */
		/* s2 = s2 / src */
		/* s3 = s2 % src */
		comprintf("\t   if (extra & 0x0800) { \n"); /* signed */
		comprintf("\t     jff_DIVLS32(r2,dst,r3);\n");
		comprintf("\t	} else { \n");
		comprintf("\t\t	  jff_DIVLU32(r2,dst,r3);\n");
		comprintf("\t	} \n");
		comprintf("\t }\n");
		comprintf("\t live_flags();\n");
	} else {
		comprintf("\t if (extra & 0x0400) {\n"); /* Need full 64 bit divisor */
		comprintf("\t   FAIL(1);\n");
    comprintf("\t   m68k_pc_offset=m68k_pc_offset_thisinst;\n");
		comprintf("\t  " RETURN "\n");
		comprintf("\t } else {\n");
		comprintf("\t   if (extra & 0x0800) { \n"); /* signed */
		comprintf("\t     jnf_DIVLS32(r2,dst,r3);\n");
		comprintf("\t	} else { \n");
		comprintf("\t\t	  jnf_DIVLU32(r2,dst,r3);\n");
		comprintf("\t	} \n");
		comprintf("\t }\n");
	}
}

static void gen_bfins(uae_u32 opcode, struct instr *curi, char* ssize)
{
  comprintf("\tdont_care_flags();\n");
  comprintf("\tuae_u16 extra=%s;\n", gen_nextiword());
  comprintf("\tint srcreg = (extra >> 12) & 7;\n");
  comprintf("\tint offs, width;\n");
  comprintf("\tif ((extra & 0x0800) == 0x0000)\n"); /* offset imm */
  comprintf("\t  offs = (extra >> 6) & 0x1f;\n");
  comprintf("\telse\n");
  comprintf("\t  offs = (extra >> 6) & 0x07;\n");
  comprintf("\tif ((extra & 0x0020) == 0x0000)\n"); /* width imm */
  comprintf("\t  width = ((extra - 1) & 0x1f) + 1;\n");
  comprintf("\telse\n");
  comprintf("\t  width = (extra & 0x07);\n");
  
  if (curi->dmode == Aind) {
    comprintf("\tint dsta=scratchie++;\n");
    comprintf("\tmov_l_rr(dsta,dstreg+8);\n");
  } else {
    genamode_new(curi->dmode, "dstreg", curi->size, "dst", 2, 0);
  }
  if (curi->dmode != Dreg) {
    comprintf("\tif ((extra & 0x0800) == 0x0800) {\n"); /* offset is reg */
    comprintf("\t  arm_ADD_ldiv8(dsta,offs);\n");
    comprintf("\t}\n");
    comprintf("\tint dst=scratchie++;\n");
    comprintf("\treadlong(dsta,dst);\n");

    comprintf("\tint dsta2=scratchie++;\n");
    comprintf("\tlea_l_brr(dsta2, dsta, 4);\n");
    comprintf("\tint dst2=scratchie++;\n");
    comprintf("\treadlong(dsta2,dst2);\n");
  }

  comprintf("\tif ((extra & 0x0820) == 0x0000) {\n"); /* offset and width imm */
  if (curi->dmode == Dreg) {
    if (!noflags) {
      comprintf("\t  jff_BFINS_ii(dst, srcreg, offs, width);\n");
    } else {
      comprintf("\t  jnf_BFINS_ii(dst, srcreg, offs, width);\n");
    }
  } else {
    comprintf("\t  if(32 - offs - width >= 0) {\n");
    if (!noflags) {
      comprintf("\t    jff_BFINS_ii(dst, srcreg, offs, width);\n");
    } else {
      comprintf("\t    jnf_BFINS_ii(dst, srcreg, offs, width);\n");
    }
    comprintf("\t  } else {\n");
    if (!noflags) {
      comprintf("\t    jff_BFINS2_ii(dst, dst2, srcreg, offs, width);\n");
    } else {
      comprintf("\t    jnf_BFINS2_ii(dst, dst2, srcreg, offs, width);\n");
    }
    comprintf("\t  }\n");
  }

  if (curi->dmode == Dreg) {
    comprintf("\t} else if ((extra & 0x0820) == 0x0800) { \n"); /* offset in reg, width imm */
    if (!noflags) {
      comprintf("\t  jff_BFINS_di(dst, srcreg, offs, width);\n");
    } else {
      comprintf("\t  jnf_BFINS_di(dst, srcreg, offs, width);\n");
    }

    comprintf("\t} else if ((extra & 0x0820) == 0x0020) { \n"); /* offset imm, width in register */
    if (!noflags) {
      comprintf("\t  jff_BFINS_id(dst, srcreg, offs, width);\n");
    } else {
      comprintf("\t  jnf_BFINS_id(dst, srcreg, offs, width);\n");
    }

    comprintf("\t} else { \n"); /* offset and width in register */
    if (!noflags) {
      comprintf("\t  jff_BFINS_dd(dst, srcreg, offs, width);\n");
    } else {
      comprintf("\t  jnf_BFINS_dd(dst, srcreg, offs, width);\n");
    }

  } else { /* curi->dmode is <ea> */
    comprintf("\t} else if ((extra & 0x0820) == 0x0800) { \n"); /* offset in reg, width imm */
    if (!noflags) {
      comprintf("\t    jff_BFINS2_di(dst, dst2, srcreg, offs, width);\n");
    } else {
      comprintf("\t    jnf_BFINS2_di(dst, dst2, srcreg, offs, width);\n");
    }

    comprintf("\t} else if ((extra & 0x0820) == 0x0020) { \n");/* offset imm, width in register */
    if (!noflags) {
      comprintf("\t    jff_BFINS2_id(dst, dst2, srcreg, offs, width);\n");
    } else {
      comprintf("\t    jnf_BFINS2_id(dst, dst2, srcreg, offs, width);\n");
    }

    comprintf("\t} else { \n"); /* offset and width in register */
    if (!noflags) {
      comprintf("\t    jff_BFINS2_dd(dst, dst2, srcreg, offs, width);\n");
    } else {
      comprintf("\t    jnf_BFINS2_dd(dst, dst2, srcreg, offs, width);\n");
    }
  }
  comprintf("\t}\n");
  if (!noflags)
    comprintf("\tlive_flags();\n");

  if (curi->dmode != Dreg)
    comprintf("\twritelong(dsta2,dst2);\n");
  genastore_new("dst", curi->dmode, curi->size, "dst");
  genamode_post(curi->dmode, "dstreg", curi->size, "dst", 1, 0);
}

static void gen_eor(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_new(curi->dmode, "dstreg", curi->size, "dst", 1, 0);

	comprintf("\t dont_care_flags();\n");
	if (!noflags) {
    comprintf("\t jff_EOR_%s(dst,src);\n", ssize);
    comprintf("\t live_flags();\n");
	} else {
    comprintf("\t jnf_EOR_%s(dst,src);\n", ssize);
  }
  genastore_new("dst", curi->dmode, curi->size, "dst");
  genamode_post(curi->dmode, "dstreg", curi->size, "dst", 1, 0);
}

static void gen_eorsr(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	(void) ssize;
	comprintf("\tint src = (uae_s32)(uae_s16)%s;\n", gen_nextiword());
	if (!noflags) {
		comprintf("\t make_flags_live();\n");
		comprintf("\t jff_EORSR(ARM_CCR_MAP[src & 0xF], ((src & 0x10) >> 4));\n");
		comprintf("\t live_flags();\n");
	}
}

static void gen_exg(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	(void) ssize;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
	genamode_new(curi->dmode, "dstreg", curi->size, "dst", 1, 0);
	comprintf("\tint tmp=scratchie++;\n"
			"\tmov_l_rr(tmp,src);\n");
	genastore("dst", curi->smode, "srcreg", curi->size, "src");
	genastore("tmp", curi->dmode, "dstreg", curi->size, "dst");
}

static void gen_ext(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	genamode_new(curi->smode, "srcreg", sz_long, "src", 1, 0);
	comprintf("\t dont_care_flags();\n");
	if (!noflags) {
		comprintf("\t jff_EXT_%s(src);\n", ssize);
		comprintf("\t live_flags();\n");
	} else {
		comprintf("\t jnf_EXT_%s(src);\n", ssize);
	}
	genastore_new("src", curi->smode, curi->size == sz_word ? sz_word : sz_long, "src");
}

static void gen_lsl(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	comprintf("\tdont_care_flags();\n");

	genamode_new(curi->smode, "srcreg", curi->size, "", 1, 0);
	genamode_new(curi->dmode, "dstreg", curi->size, "data", 1, 0);
	if (curi->smode != immi) {
		if (!noflags) {
      comprintf("\t jff_LSL_%s_reg(data,srcreg);\n", ssize);
      comprintf("\t live_flags();\n");
			comprintf(
					"\t if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
		} else {
      comprintf("\t jnf_LSL_%s_reg(data,srcreg);\n", ssize);
    }
	} else {
		if (!noflags) {
      comprintf("\t jff_LSL_%s_imm(data,srcreg);\n", ssize);
      comprintf("\t live_flags();\n");
			comprintf(
					"\t if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
		} else {
      comprintf("\t jnf_LSL_%s_imm(data,srcreg);\n", ssize);
    }
	}
  genastore_new("data", curi->dmode, curi->size, "data");
}

static void gen_lslw(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	(void) ssize;
	comprintf("\t dont_care_flags();\n");
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
	if (!noflags) {
		comprintf("\t jff_LSLW(src);\n");
		comprintf("\t live_flags();\n");
	} else {
		comprintf("\t jnf_LSLW(src);\n");
	}
	genastore_new("src", curi->smode, curi->size, "src");
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
}

static void gen_lsr(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	comprintf("\t dont_care_flags();\n");

	genamode_new(curi->smode, "srcreg", curi->size, "", 1, 0);
	genamode_new(curi->dmode, "dstreg", curi->size, "data", 1, 0);
	if (curi->smode != immi) {
		if (!noflags) {
			comprintf("\t jff_LSR_%s_reg(data,srcreg);\n", ssize);
			comprintf("\t live_flags();\n");
			comprintf("if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
		} else {
			comprintf("\t jnf_LSR_%s_reg(data,srcreg);\n", ssize);
		}
	} else {
		char *op;
		if (!noflags) {
			op = "ff";
		} else
		op = "nf";

		comprintf("\t j%s_LSR_%s_imm(data,srcreg);\n", op, ssize);

		if (!noflags) {
			comprintf("\t live_flags();\n");
			comprintf(
					"\t if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
		}
	}
  genastore_new("data", curi->dmode, curi->size, "data");
}

static void gen_lsrw(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	(void) ssize;
	comprintf("\t dont_care_flags();\n");
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);

	if (!noflags) {
		comprintf("\t jff_LSRW(src);\n");
		comprintf("\t live_flags();\n");
	} else {
		comprintf("\t jnf_LSRW(src);\n");
	}
	genastore_new("src", curi->smode, curi->size, "src");
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
}

static void gen_move(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	switch (curi->dmode) {
		case Dreg:
		case Areg:
		  genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
      genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
      genamode_new(curi->dmode, "dstreg", curi->size, "dst", 2, 0);
		  comprintf("\t dont_care_flags();\n");
		  if (!noflags && curi->dmode == Dreg) {
			  comprintf("\t jff_MOVE_%s(dst, src);\n", ssize);
			  comprintf("\t live_flags();\n");
		  } else {
        comprintf("\t jnf_MOVE_%s(dst, src);\n", ssize);
      }
      genastore_new("dst", curi->dmode, curi->size, "dst");
      break;

		default: /* It goes to memory, not a register */
		  genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
      if (curi->smode == Areg && (curi->dmode == Apdi || curi->dmode == Aipi)) {
        comprintf("\tif(srcreg==(uae_s32)dstreg){\n");
        comprintf("\t\tsrc=scratchie++;\n");
        comprintf("\t\tmov_l_rr(src,srcreg+8);\n");
        comprintf("\t}\n");
      }
      genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
      genamode_new(curi->dmode, "dstreg", curi->size, "dst", 2, 0);
		  comprintf("\t dont_care_flags();\n");
		  if (!noflags) {
			  comprintf("\t jff_TST_%s(src);\n", ssize);
			  comprintf("\t live_flags();\n");
		  }
      genastore_new("src", curi->dmode, curi->size, "dst");
      genamode_post(curi->dmode, "dstreg", curi->size, "dst", 2, 0);
      break;
	}
}

static void gen_movea(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_new(curi->dmode, "dstreg", curi->size, "dst", 2, 0);

	comprintf("\t jnf_MOVEA_%s(dst, src);\n", ssize);
  genastore_new("dst", curi->dmode, sz_long, "dst");
}

static void gen_mull(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	(void) ssize;
	comprintf("\t uae_u16 extra=%s;\n", gen_nextiword());
	comprintf("\t int r2=(extra>>12)&7;\n");
	genamode_new(curi->dmode, "dstreg", curi->size, "dst", 1, 0);
  genamode_post(curi->dmode, "dstreg", curi->size, "dst", 1, 0);
	/* The two operands are in dst and r2 */
	if (!noflags) {
		comprintf("\t if (extra & 0x0400) {\n"); /* Need full 64 bit result */
		comprintf("\t   int r3=(extra & 7);\n");
		comprintf("\t   mov_l_rr(r3,dst);\n"); /* operands now in r3 and r2 */
		comprintf("\t   if (extra & 0x0800) { \n"); /* signed */
		comprintf("\t\t	  jff_MULS64(r2,r3);\n");
		comprintf("\t	} else { \n");
		comprintf("\t\t	  jff_MULU64(r2,r3);\n");
		comprintf("\t	} \n"); /* The result is in r2/r3, with r2 holding the lower 32 bits */
		comprintf("\t } else {\n"); /* Only want 32 bit result */
		/* operands in dst and r2, result goes into r2 */
		comprintf("\t   if (extra & 0x0800) { \n"); /* signed */
		comprintf("\t     jff_MULS32(r2,dst);\n");
		comprintf("\t	} else { \n");
		comprintf("\t\t	  jff_MULU32(r2,dst);\n");
		comprintf("\t	} \n"); /* The result is in r2, with r2 holding the lower 32 bits */
		comprintf("\t }\n");
		comprintf("\t live_flags();\n");
	} else {
		comprintf("\t if (extra & 0x0400) {\n"); /* Need full 64 bit result */
		comprintf("\t   int r3=(extra & 7);\n");
		comprintf("\t   mov_l_rr(r3,dst);\n"); /* operands now in r3 and r2 */
		comprintf("\t   if (extra & 0x0800) { \n"); /* signed */
		comprintf("\t\t	  jnf_MULS64(r2,r3);\n");
		comprintf("\t	} else { \n");
		comprintf("\t\t	  jnf_MULU64(r2,r3);\n");
		comprintf("\t	} \n"); /* The result is in r2/r3, with r2 holding the lower 32 bits */
		comprintf("\t } else {\n"); /* Only want 32 bit result */
		/* operands in dst and r2, result foes into r2 */
		comprintf("\t   if (extra & 0x0800) { \n"); /* signed */
		comprintf("\t     jnf_MULS32(r2,dst);\n");
		comprintf("\t	} else { \n");
		comprintf("\t\t	  jnf_MULU32(r2,dst);\n");
		comprintf("\t	} \n"); /* The result is in r2, with r2 holding the lower 32 bits */
		comprintf("\t }\n");
	}
}

static void gen_muls(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	(void) ssize;
	comprintf("\t dont_care_flags();\n");
	genamode_new(curi->smode, "srcreg", sz_word, "src", 1, 0);
  genamode_post(curi->smode, "srcreg", sz_word, "src", 1, 0);
  genamode_new(curi->dmode, "dstreg", sz_word, "dst", 1, 0);
	if (!noflags) {
		comprintf("\t jff_MULS(dst,src);\n");
		comprintf("\t live_flags();\n");
	} else {
		comprintf("\t jnf_MULS(dst,src);\n");
	}
	genastore_new("dst", curi->dmode, sz_long, "dst");
}

static void gen_mulu(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	(void) ssize;
	comprintf("\t dont_care_flags();\n");
	genamode_new(curi->smode, "srcreg", sz_word, "src", 1, 0);
  genamode_post(curi->smode, "srcreg", sz_word, "src", 1, 0);
  genamode_new(curi->dmode, "dstreg", sz_word, "dst", 1, 0);
	if (!noflags) {
		comprintf("\t jff_MULU(dst,src);\n");
		comprintf("\t live_flags();\n");
	} else {
		comprintf("\t jnf_MULU(dst,src);\n");
	}
	genastore_new("dst", curi->dmode, sz_long, "dst");
}

static void gen_neg(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
	if (!noflags) {
    comprintf("\t jff_NEG_%s(src);\n", ssize);
    comprintf("\t live_flags();\n");
		comprintf("\t if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
	} else {
    comprintf("\t jnf_NEG_%s(src);\n", ssize);
  }
	genastore_new("src", curi->smode, curi->size, "src");
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
}

static void gen_negx(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	isaddx;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
	if (!noflags) {
		comprintf("\t make_flags_live();\n");
    comprintf("\t jff_NEGX_%s(src);\n", ssize);
    comprintf("\t live_flags();\n");
		comprintf("\t if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
	} else {
    comprintf("\t jnf_NEGX_%s(src);\n", ssize);
  }
	genastore_new("src", curi->smode, curi->size, "src");
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
}

static void gen_not(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
	comprintf("\t dont_care_flags();\n");
	if (!noflags) {
    comprintf("\t jff_NOT_%s(src);\n", ssize);
    comprintf("\t live_flags();\n");
	} else {
    comprintf("\t jnf_NOT_%s(src);\n", ssize);
  }
	genastore_new("src", curi->smode, curi->size, "src");
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
}

static void gen_or(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_new(curi->dmode, "dstreg", curi->size, "dst", 1, 0);

	comprintf("\t dont_care_flags();\n");
	if (!noflags) {
    comprintf("\t jff_OR_%s(dst,src);\n", ssize);
    comprintf("\t live_flags();\n");
	} else {
    comprintf("\t jnf_OR_%s(dst,src);\n", ssize);
  }
  genastore_new("dst", curi->dmode, curi->size, "dst");
  genamode_post(curi->dmode, "dstreg", curi->size, "dst", 1, 0);
}

static void gen_orsr(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	(void) ssize;
	comprintf("\tint src = (uae_s32)(uae_s16)%s;\n", gen_nextiword());
	if (!noflags) {
		comprintf("\t make_flags_live();\n");
		comprintf("\t jff_ORSR(ARM_CCR_MAP[src & 0xF], ((src & 0x10) >> 4));\n");
		comprintf("\t live_flags();\n");
	}
}

static void gen_rol(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	comprintf("\t dont_care_flags();\n");
	genamode_new(curi->smode, "srcreg", curi->size, "cnt", 1, 0);
	genamode_new(curi->dmode, "dstreg", curi->size, "data", 1, 0);

	if (!noflags) {
    comprintf("\t jff_ROL_%s(data,cnt);\n", ssize);
    comprintf("\t live_flags();\n");
	} else {
    comprintf("\t jnf_ROL_%s(data,cnt);\n", ssize);
  }
  genastore_new("data", curi->dmode, curi->size, "data");
}

static void gen_rolw(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	(void) ssize;
	comprintf("\t dont_care_flags();\n");
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);

	if (!noflags) {
		comprintf("\t jff_ROLW(src);\n");
		comprintf("\t live_flags();\n");
	} else {
		comprintf("\t jnf_ROLW(src);\n");
	}
	genastore_new("src", curi->smode, curi->size, "src");
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
}

static void gen_ror(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	comprintf("\t dont_care_flags();\n");
	genamode_new(curi->smode, "srcreg", curi->size, "cnt", 1, 0);
	genamode_new(curi->dmode, "dstreg", curi->size, "data", 1, 0);

	if (!noflags) {
		comprintf("\t jff_ROR_%s(data,cnt);\n", ssize);
		comprintf("\t live_flags();\n");
	} else {
		comprintf("\t jnf_ROR_%s(data,cnt);\n", ssize);
	}
  genastore_new("data", curi->dmode, curi->size, "data");
}

static void gen_rorw(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	(void) ssize;
	comprintf("\t dont_care_flags();\n");
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);

	if (!noflags) {
		comprintf("\t jff_RORW(src);\n");
		comprintf("\t live_flags();\n");
	} else {
		comprintf("\t jnf_RORW(src);\n");
	}
	genastore_new("src", curi->smode, curi->size, "src");
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
}

static void gen_roxl(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	isaddx;
	comprintf("\t dont_care_flags();\n");
	genamode_new(curi->smode, "srcreg", curi->size, "cnt", 1, 0);
	genamode_new(curi->dmode, "dstreg", curi->size, "data", 1, 0);

	if (!noflags) {
		comprintf("\t make_flags_live();\n");
		comprintf("\t jff_ROXL_%s(data,cnt);\n", ssize);
		comprintf("\t live_flags();\n");
	} else {
		comprintf("\t jnf_ROXL_%s(data,cnt);\n", ssize);
	}
  genastore_new("data", curi->dmode, curi->size, "data");
}

static void gen_roxlw(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	(void) ssize;
	isaddx;
	comprintf("\t dont_care_flags();\n");
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);

	if (!noflags) {
		comprintf("\t make_flags_live();\n");
		comprintf("\t jff_ROXLW(src);\n");
		comprintf("\t live_flags();\n");
	} else {
		comprintf("\t jnf_ROXLW(src);\n");
	}
	genastore_new("src", curi->smode, curi->size, "src");
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
}

static void gen_roxr(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	isaddx;
	comprintf("\t dont_care_flags();\n");
	genamode_new(curi->smode, "srcreg", curi->size, "cnt", 1, 0);
	genamode_new(curi->dmode, "dstreg", curi->size, "data", 1, 0);

	if (!noflags) {
		comprintf("\t make_flags_live();\n");
		comprintf("\t jff_ROXR_%s(data,cnt);\n", ssize);
		comprintf("\t live_flags();\n");
	} else {
		comprintf("\t jnf_ROXR_%s(data,cnt);\n", ssize);
	}
  genastore_new("data", curi->dmode, curi->size, "data");
}

static void gen_roxrw(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	(void) ssize;
	isaddx;
	comprintf("\t dont_care_flags();\n");
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);

	if (!noflags) {
		comprintf("\t make_flags_live();\n");
		comprintf("\t jff_ROXRW(src);\n");
		comprintf("\t live_flags();\n");
	} else {
		comprintf("\t jnf_ROXRW(src);\n");
	}
	genastore_new("src", curi->smode, curi->size, "src");
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
}

static void gen_scc(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	(void) ssize;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 2, 0);
  if (curi->smode != Dreg)
    comprintf("\tint val = scratchie++;\n");

	switch (curi->cc) {
	  case 0: /* Unconditional set */
      if (curi->smode == Dreg)
        comprintf("\tmov_b_ri(srcreg, 0xff);\n");
      else
  		  comprintf("\tmov_b_ri(val, 0xff);\n");
		  break;
	  case 1:
		  /* Unconditional not-set */
      if (curi->smode == Dreg)
        comprintf("\tmov_b_ri(srcreg, 0);\n");
      else
        comprintf("\tmov_b_ri(val, 0);\n");
		  break;
	  case 2:
	  case 3:
	  case 4:
	  case 5:
	  case 6:
	  case 7:
	  case 8:
	  case 9:
	  case 10:
	  case 11:
	  case 12:
	  case 13:
	  case 14:
	  case 15:
		  comprintf("\tmake_flags_live();\n"); /* Load the flags */
      if (curi->smode == Dreg)
        comprintf("\tjnf_SCC(srcreg, %d);\n", cond_codes[curi->cc]);
      else
        comprintf("\tjnf_SCC(val, %d);\n", cond_codes[curi->cc]);
		  break;
	  default:
		  abort();
	}
  if (curi->smode != Dreg)
    genastore_new("val", curi->smode, curi->size, "src");
  genamode_post(curi->smode, "srcreg", curi->size, "src", 2, 0);
}

static void gen_sub(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_new(curi->dmode, "dstreg", curi->size, "dst", 1, 0);

	comprintf("\t dont_care_flags();\n");
	if (!noflags) {
    comprintf("\t jff_SUB_%s(dst,src);\n", ssize);
    comprintf("\t live_flags();\n");
		comprintf(
				"\t if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
	} else {
    comprintf("\t jnf_SUB_%s(dst,src);\n", ssize);
  }
  genastore_new("dst", curi->dmode, curi->size, "dst");
  genamode_post(curi->dmode, "dstreg", curi->size, "dst", 1, 0);
}

static void gen_suba(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_new(curi->dmode, "dstreg", sz_long, "dst", 1, 0);
	comprintf("\t jnf_SUBA_%s(dst, src);\n", ssize);
  genastore_new("dst", curi->dmode, sz_long, "dst");
}

static void gen_subx(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	isaddx;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
  genamode_new(curi->dmode, "dstreg", curi->size, "dst", 1, 0);
	comprintf("\tdont_care_flags();\n");
	if (!noflags) {
		comprintf("\t make_flags_live();\n");
    comprintf("\t jff_SUBX_%s(dst,src);\n", ssize);
    comprintf("\t live_flags();\n");
		comprintf("if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
	} else {
    comprintf("\t jnf_SUBX_%s(dst,src);\n", ssize);
  }
  genastore_new("dst", curi->dmode, curi->size, "dst");
  genamode_post(curi->dmode, "dstreg", curi->size, "dst", 1, 0);
}

static void gen_swap(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	(void) ssize;
	genamode_new(curi->smode, "srcreg", sz_long, "src", 1, 0);
	comprintf("\t dont_care_flags();\n");

	if (!noflags) {
		comprintf("\t jff_SWAP(src);\n");
		comprintf("\t live_flags();\n");
		comprintf("if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
	} else {
		comprintf("\t jnf_SWAP(src);\n");
	}
	genastore_new("src", curi->smode, sz_long, "src");
}

static void gen_tst(uae_u32 opcode, struct instr *curi, char* ssize) {
	(void) opcode;
	genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
	comprintf("\t dont_care_flags();\n");
	if (!noflags) {
		comprintf("\t jff_TST_%s(src);\n", ssize);
		comprintf("\t live_flags();\n");
	}
  genamode_post(curi->smode, "srcreg", curi->size, "src", 1, 0);
}

/* returns zero for success, non-zero for failure */
static int gen_opcode(unsigned int opcode) 
{
	struct instr *curi = table68k + opcode;
	char* ssize = NULL;

	insn_n_cycles = 2;
	global_failure = 0;
	long_opcode = 0;
	global_isjump = 0;
	global_iscjump = 0;
	global_isaddx = 0;
	global_cmov = 0;
	global_fpu = 0;
	global_mayfail = 0;
	hack_opcode = opcode;
	endstr[0] = 0;

	comprintf("\tuae_u8 scratchie=S1;\n");
	switch (curi->plev) {
	case 0: /* not privileged */
		break;
	case 1: /* unprivileged only on 68000 */
		if (cpu_level == 0)
			break;
		if (next_cpu_level < 0)
			next_cpu_level = 0;

		/* fall through */
	case 2: /* priviledged */
		failure; /* Easy ones first */
		break;
	case 3: /* privileged if size == word */
		if (curi->size == sz_byte)
			break;
		failure;
		break;
	}
	switch (curi->size) {
	case sz_byte:
		ssize = "b";
		break;
	case sz_word:
		ssize = "w";
		break;
	case sz_long:
		ssize = "l";
		break;
	default:
		abort();
	}
	(void) ssize;

	switch (curi->mnemo) {
	case i_AND:
#ifdef DISABLE_I_OR_AND_EOR
    failure;
#endif
		gen_and(opcode, curi, ssize);
		break;

	case i_OR:
#ifdef DISABLE_I_OR_AND_EOR
    failure;
#endif
		gen_or(opcode, curi, ssize);
		break;

	case i_EOR:
#ifdef DISABLE_I_OR_AND_EOR
    failure;
#endif
		gen_eor(opcode, curi, ssize);
		break;

	case i_ORSR:
#ifdef DISABLE_I_ORSR_EORSR_ANDSR
    failure;
#endif
		gen_orsr(opcode, curi, ssize);
		break;

	case i_EORSR:
#ifdef DISABLE_I_ORSR_EORSR_ANDSR
    failure;
#endif
		gen_eorsr(opcode, curi, ssize);
		break;

	case i_ANDSR:
#ifdef DISABLE_I_ORSR_EORSR_ANDSR
    failure;
#endif
		gen_andsr(opcode, curi, ssize);
		break;

	case i_SUB:
#ifdef DISABLE_I_SUB
    failure;
#endif
		gen_sub(opcode, curi, ssize);
		break;

	case i_SUBA:
#ifdef DISABLE_I_SUBA
    failure;
#endif
		gen_suba(opcode, curi, ssize);
		break;

	case i_SUBX:
#ifdef DISABLE_I_SUBX
    failure;
#endif
		gen_subx(opcode, curi, ssize);
		break;

	case i_SBCD:
    failure;
		break;

	case i_ADD:
#ifdef DISABLE_I_ADD
    failure;
#endif
		gen_add(opcode, curi, ssize);
		break;

	case i_ADDA:
#ifdef DISABLE_I_ADDA
    failure;
#endif
		gen_adda(opcode, curi, ssize);
		break;

	case i_ADDX:
#ifdef DISABLE_I_ADDX
    failure;
#endif
		gen_addx(opcode, curi, ssize);
		break;

	case i_ABCD:
    failure;
		break;

	case i_NEG:
#ifdef DISABLE_I_NEG
    failure;
#endif
		gen_neg(opcode, curi, ssize);
		break;

	case i_NEGX:
#ifdef DISABLE_I_NEGX
    failure;
#endif
		gen_negx(opcode, curi, ssize);
		break;

	case i_NBCD:
    failure;
		break;

	case i_CLR:
#ifdef DISABLE_I_CLR
    failure;
#endif
		gen_clr(opcode, curi, ssize);
		break;

	case i_NOT:
#ifdef DISABLE_I_NOT
    failure;
#endif
		gen_not(opcode, curi, ssize);
		break;

	case i_TST:
#ifdef DISABLE_I_TST
    failure;
#endif
		gen_tst(opcode, curi, ssize);
		break;

	case i_BCHG:
#ifdef DISABLE_I_BCHG_BCLR_BSET_BTST
    failure;
#endif
		gen_bchg(opcode, curi, ssize);
		break;

	case i_BCLR:
#ifdef DISABLE_I_BCHG_BCLR_BSET_BTST
    failure;
#endif
		gen_bclr(opcode, curi, ssize);
		break;

	case i_BSET:
#ifdef DISABLE_I_BCHG_BCLR_BSET_BTST
    failure;
#endif
		gen_bset(opcode, curi, ssize);
		break;

	case i_BTST:
#ifdef DISABLE_I_BCHG_BCLR_BSET_BTST
    failure;
#endif
		gen_btst(opcode, curi, ssize);
		break;

	case i_CMPM:
	case i_CMP:
#ifdef DISABLE_I_CMPM_CMP
    failure;
#endif
		gen_cmp(opcode, curi, ssize);
		break;

	case i_CMPA:
#ifdef DISABLE_I_CMPA
    failure;
#endif
		gen_cmpa(opcode, curi, ssize);
		break;

		/* The next two are coded a little unconventional, but they are doing
		 * weird things... */
	case i_MVPRM:
		isjump;
		failure;
		break;

	case i_MVPMR:
		isjump;
		failure;
		break;

	case i_MOVE:
#ifdef DISABLE_I_MOVE
    failure;
#endif
		gen_move(opcode, curi, ssize);
		break;

	case i_MOVEA:
#ifdef DISABLE_I_MOVEA
    failure;
#endif
		gen_movea(opcode, curi, ssize);
		break;

	case i_MVSR2:
		isjump;
		failure;
		break;

	case i_MV2SR:
		isjump;
		failure;
		break;

	case i_SWAP:
#ifdef DISABLE_I_SWAP
    failure;
#endif
		gen_swap(opcode, curi, ssize);
		break;

	case i_EXG:
#ifdef DISABLE_I_EXG
    failure;
#endif
		gen_exg(opcode, curi, ssize);
		break;

	case i_EXT:
#ifdef DISABLE_I_EXT
    failure;
#endif
		gen_ext(opcode, curi, ssize);
		break;

	case i_MVMEL:
#ifdef DISABLE_I_MVMEL
    failure;
#endif
		genmovemel(opcode);
		break;

	case i_MVMLE:
#ifdef DISABLE_I_MVMLE
    failure;
#endif
		genmovemle(opcode);
		break;

	case i_TRAP:
		isjump;
		failure;
		break;

	case i_MVR2USP:
		isjump;
		failure;
		break;

	case i_MVUSP2R:
		isjump;
		failure;
		break;

	case i_RESET:
		isjump;
		failure;
		break;

	case i_NOP:
		break;

	case i_STOP:
		isjump;
		failure;
		break;

	case i_RTE:
		isjump;
		failure;
		break;

	case i_RTD:
#ifdef DISABLE_I_RTD
    failure;
#endif
		genamode_new(curi->smode, "srcreg", curi->size, "offs", 1, 0);
		/* offs is constant */
		comprintf("\tarm_ADD_l_ri8(offs,4);\n");
		start_brace();
		comprintf("\tint newad=scratchie++;\n"
				"\treadlong(15,newad);\n"
				"\tmov_l_mr((uintptr)&regs.pc,newad);\n"
				"\tget_n_addr_jmp(newad,PC_P);\n"
				"\tmov_l_mr((uintptr)&regs.pc_oldp,PC_P);\n"
				"\tm68k_pc_offset=0;\n"
				"\tarm_ADD_l(15,offs);\n");
		gen_update_next_handler();
		isjump;
		break;

	case i_LINK:
#ifdef DISABLE_I_LINK
    failure;
#endif
    comprintf("\tint dodgy=0;\n");
    comprintf("\tif (srcreg==7) dodgy=1;\n");
    genamode(curi->smode, "srcreg", sz_long, "src", 1, 0);
		genamode_new(curi->dmode, "dstreg", curi->size, "offs", 1, 0);
    comprintf("\tsub_l_ri(15,4);\n"
				"\twritelong_clobber(15,src);\n");
    comprintf("\tif(!dodgy)\n");
    comprintf("\t\tmov_l_rr(src,15);\n");
    comprintf("\tarm_ADD_l(15,offs);\n");
    genastore("src", curi->smode, "srcreg", sz_long, "src");
		break;

	case i_UNLK:
#ifdef DISABLE_I_UNLK
    failure;
#endif
		genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
    comprintf("\tif(src==15){\n");
    comprintf("\t\treadlong(15,src);\n");
    comprintf("\t} else {\n");
    comprintf("\t\tmov_l_rr(15,src);\n");
    comprintf("\t\treadlong(15,src);\n");
    comprintf("\t\tarm_ADD_l_ri8(15,4);\n");
    comprintf("\t}\n");
		genastore("src", curi->smode, "srcreg", curi->size, "src");
		break;

	case i_RTS:
#ifdef DISABLE_I_RTS
    failure;
#endif
		comprintf("\tint newad=scratchie++;\n"
				"\treadlong(15,newad);\n"
				"\tmov_l_mr((uintptr)&regs.pc,newad);\n"
				"\tget_n_addr_jmp(newad,PC_P);\n"
				"\tmov_l_mr((uintptr)&regs.pc_oldp,PC_P);\n"
				"\tm68k_pc_offset=0;\n"
				"\tarm_ADD_l_ri8(15,4);\n");
		gen_update_next_handler();
		isjump;
		break;

	case i_TRAPV:
		isjump;
		failure;
		break;

	case i_RTR:
		isjump;
		failure;
		break;

	case i_JSR:
#ifdef DISABLE_I_JSR
    failure;
#endif
		isjump;
		genamode_new(curi->smode, "srcreg", curi->size, "src", 0, 0);
		start_brace();
		comprintf("\tuae_u32 retadd=start_pc+((char *)comp_pc_p-(char *)start_pc_p)+m68k_pc_offset;\n");
		comprintf("\tint ret=scratchie++;\n"
				"\tmov_l_ri(ret,retadd);\n"
				"\tsub_l_ri(15,4);\n"
				"\twritelong_clobber(15,ret);\n");
		comprintf("\tmov_l_mr((uintptr)&regs.pc,srca);\n"
				"\tget_n_addr_jmp(srca,PC_P);\n"
				"\tmov_l_mr((uintptr)&regs.pc_oldp,PC_P);\n"
				"\tm68k_pc_offset=0;\n");
		gen_update_next_handler();
		break;

	case i_JMP:
#ifdef DISABLE_I_JMP
    failure;
#endif
		isjump;
		genamode_new(curi->smode, "srcreg", curi->size, "src", 0, 0);
		comprintf("\tmov_l_mr((uintptr)&regs.pc,srca);\n"
				"\tget_n_addr_jmp(srca,PC_P);\n"
				"\tmov_l_mr((uintptr)&regs.pc_oldp,PC_P);\n"
				"\tm68k_pc_offset=0;\n");
		gen_update_next_handler();
		break;

	case i_BSR:
#ifdef DISABLE_I_BSR
    failure;
#endif
		is_const_jump;
		genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
		start_brace();
		comprintf("\tuae_u32 retadd=start_pc+((char *)comp_pc_p-(char *)start_pc_p)+m68k_pc_offset;\n");
		comprintf("\tint ret=scratchie++;\n"
				"\tmov_l_ri(ret,retadd);\n"
				"\tsub_l_ri(15,4);\n"
				"\twritelong_clobber(15,ret);\n");
		comprintf("\tarm_ADD_l_ri(src,m68k_pc_offset_thisinst+2);\n");
		comprintf("\tm68k_pc_offset=0;\n");
		comprintf("\tarm_ADD_l(PC_P,src);\n");
		comprintf("\tcomp_pc_p=(uae_u8*)(uintptr)get_const(PC_P);\n");
		break;

	case i_Bcc:
#ifdef DISABLE_I_BCC
    failure;
#endif
		comprintf("\tuae_u32 v1, v2;\n");
		genamode_new(curi->smode, "srcreg", curi->size, "src", 1, 0);
		/* That source is an immediate, so we can clobber it with abandon */
		comprintf("\tsub_l_ri(src, m68k_pc_offset - m68k_pc_offset_thisinst - 2);\n");
		/* Leave the following as "add" --- it will allow it to be optimized
		 away due to src being a constant ;-) */
		comprintf("\tarm_ADD_l_ri(src, (uintptr)comp_pc_p);\n");
		comprintf("\tmov_l_ri(PC_P, (uintptr)comp_pc_p);\n");
		/* Now they are both constant. Might as well fold in m68k_pc_offset */
		comprintf("\tarm_ADD_l_ri(src, m68k_pc_offset);\n");
		comprintf("\tarm_ADD_l_ri(PC_P, m68k_pc_offset);\n");
    comprintf("\tm68k_pc_offset = 0;\n");

		if (curi->cc >= 2) {
			comprintf("\tv1 = get_const(PC_P);\n");
			comprintf("\tv2 = get_const(src);\n");
			comprintf("\tregister_branch(v1, v2, %d);\n", cond_codes[curi->cc]);
			comprintf("\tmake_flags_live();\n"); /* Load the flags */
      isjump;
		} else {
			is_const_jump;
		}

		switch (curi->cc) {
		case 0: /* Unconditional jump */
			comprintf("\tmov_l_rr(PC_P, src);\n");
			comprintf("\tcomp_pc_p = (uae_u8*)(uintptr)get_const(PC_P);\n");
			break;
		case 1:
			break; /* This is silly! */
		case 8:
      //failure;
			break; /* Work out details! FIXME */
		case 9:
			//failure;
			break; /* Not critical, though! */

		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
			break;
		default:
			abort();
		}
		break;

	case i_LEA:
#ifdef DISABLE_I_LEA
    failure;
#endif
		genamode_new(curi->smode, "srcreg", curi->size, "src", 0, 0);
		genamode_new(curi->dmode, "dstreg", curi->size, "dst", 2, 0);
    genamode_post(curi->smode, "srcreg", curi->size, "src", 0, 0);
    genastore("srca", curi->dmode, "dstreg", curi->size, "dst");
    break;

	case i_PEA:
#ifdef DISABLE_I_PEA
    failure;
#endif
    comprintf("\tint dodgy=0;\n");
		if (table68k[opcode].smode == Areg 
        || table68k[opcode].smode == Aind
				|| table68k[opcode].smode == Aipi
				|| table68k[opcode].smode == Apdi
				|| table68k[opcode].smode == Ad16
				|| table68k[opcode].smode == Ad8r)
			comprintf("if (srcreg==7) dodgy=1;\n");

		genamode(curi->smode, "srcreg", curi->size, "src", 0, 0);
		genamode(Apdi, "7", sz_long, "dst", 2, 0);
		genastore("srca", Apdi, "7", sz_long, "dst");
		break;

	case i_DBcc:
#ifdef DISABLE_I_DBCC
    failure;
#endif
		gen_dbcc(opcode, curi, ssize);
		break;

	case i_Scc:
#ifdef DISABLE_I_SCC
    failure;
#endif
		gen_scc(opcode, curi, ssize);
		break;

	case i_DIVU:
#ifdef DISABLE_I_DIVU
		isjump;
    failure;
#endif
		gen_divu(opcode, curi, ssize);
		break;

	case i_DIVS:
#ifdef DISABLE_I_DIVS
		isjump;
		failure;
#endif
    gen_divs(opcode, curi, ssize);
		break;

	case i_MULU:
#ifdef DISABLE_I_MULU
    failure;
#endif
		gen_mulu(opcode, curi, ssize);
		break;

	case i_MULS:
#ifdef DISABLE_I_MULS
    failure;
#endif
		gen_muls(opcode, curi, ssize);
		break;

	case i_CHK:
		isjump;
		failure;
		break;

	case i_CHK2:
		isjump;
		failure;
		break;

	case i_ASR:
#ifdef DISABLE_I_ASR
    failure;
#endif
		gen_asr(opcode, curi, ssize);
		break;

	case i_ASL:
#ifdef DISABLE_I_ASL
    failure;
#endif
		gen_asl(opcode, curi, ssize);
		break;

	case i_LSR:
#ifdef DISABLE_I_LSR
    failure;
#endif
		gen_lsr(opcode, curi, ssize);
		break;

	case i_LSL:
#ifdef DISABLE_I_LSL
    failure;
#endif
		gen_lsl(opcode, curi, ssize);
		break;

	case i_ROL:
#ifdef DISABLE_I_ROL
    failure;
#endif
		gen_rol(opcode, curi, ssize);
		break;

	case i_ROR:
#ifdef DISABLE_I_ROR
    failure;
#endif
		gen_ror(opcode, curi, ssize);
		break;

	case i_ROXL:
#ifdef DISABLE_I_ROXL
    failure;
#endif
		gen_roxl(opcode, curi, ssize);
		break;

	case i_ROXR:
#ifdef DISABLE_I_ROXR
    failure;
#endif
		gen_roxr(opcode, curi, ssize);
		break;

	case i_ASRW:
#ifdef DISABLE_I_ASRW
    failure;
#endif
		gen_asrw(opcode, curi, ssize);
		break;

	case i_ASLW:
#ifdef DISABLE_I_ASLW
    failure;
#endif
		gen_aslw(opcode, curi, ssize);
		break;

	case i_LSRW:
#ifdef DISABLE_I_LSRW
    failure;
#endif
		gen_lsrw(opcode, curi, ssize);
		break;

	case i_LSLW:
#ifdef DISABLE_I_LSLW
    failure;
#endif
		gen_lslw(opcode, curi, ssize);
		break;

	case i_ROLW:
#ifdef DISABLE_I_ROLW
    failure;
#endif
		gen_rolw(opcode, curi, ssize);
		break;

	case i_RORW:
#ifdef DISABLE_I_RORW
    failure;
#endif
		gen_rorw(opcode, curi, ssize);
		break;

	case i_ROXLW:
#ifdef DISABLE_I_ROXLW
    failure;
#endif
		gen_roxlw(opcode, curi, ssize);
		break;

	case i_ROXRW:
#ifdef DISABLE_I_ROXRW
    failure;
#endif
		gen_roxrw(opcode, curi, ssize);
		break;

	case i_MOVEC2:
		isjump;
		failure;
		break;

	case i_MOVE2C:
		isjump;
		failure;
		break;

	case i_CAS:
		failure;
		break;

	case i_CAS2:
		failure;
		break;

	case i_MOVES:		/* ignore DFC and SFC because we have no MMU */
		isjump;
		failure;
		break;

	case i_BKPT:		/* only needed for hardware emulators */
		isjump;
		failure;
		break;

	case i_CALLM:		/* not present in 68030 */
		isjump;
		failure;
		break;

	case i_RTM:		/* not present in 68030 */
		isjump;
		failure;
		break;

	case i_TRAPcc:
		isjump;
		failure;
		break;

	case i_DIVL:
#ifdef DISABLE_I_DIVL
		failure;
#endif
		gen_divl(opcode, curi, ssize);
		break;

	case i_MULL:
#ifdef DISABLE_I_MULL
    failure;
#endif
		gen_mull(opcode, curi, ssize);
		break;

	case i_BFTST:
	case i_BFEXTU:
	case i_BFCHG:
	case i_BFEXTS:
	case i_BFCLR:
	case i_BFFFO:
	case i_BFSET:
		failure;
		break;

  case i_BFINS:
#ifdef DISABLE_I_BFINS
    failure;
#endif
    gen_bfins(opcode, curi, ssize);
    break;

	case i_PACK:
		failure;
		break;

	case i_UNPK:
		failure;
		break;

	case i_TAS:
		failure;
		break;

	case i_FPP:
#ifdef DISABLE_I_FPP
      failure;
#endif
		uses_fpu;
		mayfail;
	    comprintf("#ifdef USE_JIT_FPU\n");
		comprintf("\tuae_u16 extra=%s;\n",gen_nextiword());
		swap_opcode();
		comprintf("\tcomp_fpp_opp(opcode,extra);\n");
	    comprintf("#else\n");
	    comprintf("\tfailure = 1;\n");
	    comprintf("#endif\n");
	    break;

    case i_FBcc:
#ifdef DISABLE_I_FBCC
		failure;
#endif
		isjump;
		uses_cmov;
		mayfail;
	    comprintf("#ifdef USE_JIT_FPU\n");
		swap_opcode();
		comprintf("\tcomp_fbcc_opp(opcode);\n");
	    comprintf("#else\n");
	    comprintf("\tfailure = 1;\n");
	    comprintf("#endif\n");
		break;

	case i_FDBcc:
		uses_fpu;
		isjump;
		failure;
		break;

	case i_FScc:
#ifdef DISABLE_I_FSCC
      failure;
#endif
		mayfail;
		uses_cmov;
	    comprintf("#ifdef USE_JIT_FPU\n");
		comprintf("\tuae_u16 extra=%s;\n",gen_nextiword());
		swap_opcode();
		comprintf("\tcomp_fscc_opp(opcode,extra);\n");
	    comprintf("#else\n");
	    comprintf("\tfailure = 1;\n");
	    comprintf("#endif\n");
		break;

	case i_FTRAPcc:
		uses_fpu;
		isjump;
		failure;
		break;

	case i_FSAVE:
		uses_fpu;
		failure;
		break;

	case i_FRESTORE:
		uses_fpu;
		failure;
		break;

	case i_CINVL:
	case i_CINVP:
	case i_CINVA:
		isjump; /* Not really, but it's probably a good idea to stop
		 translating at this point */
		failure;
		comprintf("\tflush_icache(0, 3);\n"); /* Differentiate a bit more? */
		break;

	case i_CPUSHL:
	case i_CPUSHP:
	case i_CPUSHA:
		isjump; /* Not really, but it's probably a good idea to stop
		 translating at this point */
		failure;
		break;

	case i_MOVE16:
#ifdef DISABLE_I_MOVE16
      failure;
#endif
		gen_move16(opcode, curi);
		break;

    case i_MMUOP030:
    case i_PFLUSHN:
    case i_PFLUSH:
    case i_PFLUSHAN:
    case i_PFLUSHA:
    case i_PLPAR:
    case i_PLPAW:
    case i_PTESTR:
    case i_PTESTW:
    case i_LPSTOP:
		isjump;
		failure;
		break;

	default:
		abort();
		break;
	}
	comprintf("%s", endstr);
	finish_braces();
	sync_m68k_pc();
	if (global_mayfail)
		comprintf("\tif (failure)  m68k_pc_offset=m68k_pc_offset_thisinst;\n");
	return global_failure;
}

static void 
generate_includes(FILE * f) 
{
  fprintf (f, "#include \"sysconfig.h\"\n");
  fprintf (f, "#if defined(JIT)\n");
	fprintf(f, "#include \"sysdeps.h\"\n");
	fprintf (f, "#include \"options.h\"\n");
	fprintf(f, "#include \"memory.h\"\n");
	fprintf(f, "#include \"newcpu.h\"\n");
	fprintf (f, "#include \"custom.h\"\n");
	fprintf(f, "#include \"comptbl.h\"\n");
}

static int postfix;


static char *decodeEA (amodes mode, wordsizes size)
{
	static char buffer[80];

	buffer[0] = 0;
	switch (mode){
	case Dreg:
		strcpy (buffer,"Dn");
		break;
	case Areg:
		strcpy (buffer,"An");
		break;
	case Aind:
		strcpy (buffer,"(An)");
		break;
	case Aipi:
		strcpy (buffer,"(An)+");
		break;
	case Apdi:
		strcpy (buffer,"-(An)");
		break;
	case Ad16:
		strcpy (buffer,"(d16,An)");
		break;
	case Ad8r:
		strcpy (buffer,"(d8,An,Xn)");
		break;
	case PC16:
		strcpy (buffer,"(d16,PC)");
		break;
	case PC8r:
		strcpy (buffer,"(d8,PC,Xn)");
		break;
	case absw:
		strcpy (buffer,"(xxx).W");
		break;
	case absl:
		strcpy (buffer,"(xxx).L");
		break;
	case imm:
		switch (size){
		case sz_byte:
			strcpy (buffer,"#<data>.B");
			break;
		case sz_word:
			strcpy (buffer,"#<data>.W");
			break;
		case sz_long:
			strcpy (buffer,"#<data>.L");
			break;
		default:
			break;
		}
		break;
	case imm0:
		strcpy (buffer,"#<data>.B");
		break;
	case imm1:
		strcpy (buffer,"#<data>.W");
		break;
	case imm2:
		strcpy (buffer,"#<data>.L");
		break;
	case immi:
		strcpy (buffer,"#<data>");
		break;

	default:
		break;
	}
	return buffer;
}

static char *outopcode (int opcode)
{
	static char out[100];
	struct instr *ins;
	int i;

	ins = &table68k[opcode];
	for (i = 0; lookuptab[i].name[0]; i++) {
		if (ins->mnemo == lookuptab[i].mnemo)
			break;
	}
	{
		strcpy (out, lookuptab[i].name);
	}
	if (ins->smode == immi)
		strcat (out, "Q");
	if (ins->size == sz_byte)
		strcat (out,".B");
	if (ins->size == sz_word)
		strcat (out,".W");
	if (ins->size == sz_long)
		strcat (out,".L");
	strcat (out," ");
	if (ins->suse)
		strcat (out, decodeEA (ins->smode, ins->size));
	if (ins->duse) {
		if (ins->suse) strcat (out,",");
		strcat (out, decodeEA (ins->dmode, ins->size));
	}
	return out;
}

static void generate_one_opcode(int rp, int noflags) 
{
	int i;
	uae_u16 smsk, dmsk;
	unsigned int opcode = opcode_map[rp];
	int aborted = 0;
	int have_srcreg = 0;
	int have_dstreg = 0;

	if (table68k[opcode].mnemo == i_ILLG 
  || table68k[opcode].clev > cpu_level)
		return;

	for (i = 0; lookuptab[i].name[0]; i++) 
  {
		if (table68k[opcode].mnemo == lookuptab[i].mnemo)
			break;
	}

	if (table68k[opcode].handler != -1)
		return;

	switch (table68k[opcode].stype) 
  {
	  case 0:
		  smsk = 7;
		  break;
	  case 1:
		  smsk = 255;
		  break;
	  case 2:
		  smsk = 15;
		  break;
	  case 3:
		  smsk = 7;
		  break;
	  case 4:
		  smsk = 7;
		  break;
	  case 5:
		  smsk = 63;
		  break;
	  case 6:
		  smsk = 255;
		  break;
	  case 7:
		  smsk = 3;
		  break;
	  default:
		  abort();
	}
	dmsk = 7;

	next_cpu_level = -1;
  if (table68k[opcode].mnemo == i_DIVU || table68k[opcode].mnemo == i_DIVS || table68k[opcode].mnemo == i_DIVL
  || table68k[opcode].mnemo == i_BFINS) {
    comprintf("#if !defined(ARMV6T2) && !defined(CPU_AARCH64)\n");
    comprintf("  FAIL(1);\n");
    comprintf("  " RETURN "\n");
    comprintf("#else\n");
  }

	if (table68k[opcode].suse 
      && table68k[opcode].smode != imm && table68k[opcode].smode != imm0 
      && table68k[opcode].smode != imm1 && table68k[opcode].smode != imm2 
      && table68k[opcode].smode != absw && table68k[opcode].smode != absl 
      && table68k[opcode].smode != PC8r && table68k[opcode].smode != PC16) 
  {
		have_srcreg = 1;
		if (table68k[opcode].spos == -1) 
    {
			if (((int) table68k[opcode].sreg) >= 128)
				comprintf("\tuae_s32 srcreg = (uae_s32)(uae_s8)%d;\n", (int) table68k[opcode].sreg);
			else
				comprintf("\tuae_s32 srcreg = %d;\n", (int) table68k[opcode].sreg);
		} 
    else 
    {
			char source[100];
			int pos = table68k[opcode].spos;

			if (pos)
				sprintf(source, "((opcode >> %d) & %d)", pos, smsk);
			else
				sprintf(source, "(opcode & %d)", smsk);

			if (table68k[opcode].stype == 3)
				comprintf("\tuae_s32 srcreg = imm8_table[%s];\n", source);
			else if (table68k[opcode].stype == 1)
				comprintf("\tuae_s32 srcreg = (uae_s32)(uae_s8)%s;\n", source);
			else
				comprintf("\tuae_s32 srcreg = %s;\n", source);
		}
	}
	if (table68k[opcode].duse
	/* Yes, the dmode can be imm, in case of LINK or DBcc */
	&& table68k[opcode].dmode != imm && table68k[opcode].dmode != imm0
			&& table68k[opcode].dmode != imm1 && table68k[opcode].dmode != imm2
			&& table68k[opcode].dmode != absw && table68k[opcode].dmode != absl) 
  {
		have_dstreg = 1;
		if (table68k[opcode].dpos == -1) 
    {
			if (((int) table68k[opcode].dreg) >= 128)
				comprintf("\tuae_s32 dstreg = (uae_s32)(uae_s8)%d;\n", (int) table68k[opcode].dreg);
			else
				comprintf("\tuae_s32 dstreg = %d;\n", (int) table68k[opcode].dreg);
		} 
    else 
    {
			int pos = table68k[opcode].dpos;

			if (pos)
				comprintf("\tuae_u32 dstreg = (opcode >> %d) & %d;\n", 
          pos, dmsk);
			else
				comprintf("\tuae_u32 dstreg = opcode & %d;\n", dmsk);
		}
	}

	comprintf("\tuae_u32 m68k_pc_offset_thisinst=m68k_pc_offset;\n");
	comprintf("\tm68k_pc_offset+=2;\n");

	aborted = gen_opcode(opcode);
	{
		int flags = 0;
		if (global_isjump)	flags |= 1;
		if (long_opcode)	flags |= 2;
		if (global_cmov)	flags |= 4;
		if (global_isaddx)	flags |= 8;
		if (global_iscjump)	flags |= 16;
		if (global_fpu)	flags |= 32;

	  comprintf ("return 0;\n");
    if (table68k[opcode].mnemo == i_DIVU || table68k[opcode].mnemo == i_DIVS || table68k[opcode].mnemo == i_DIVL
    || table68k[opcode].mnemo == i_BFINS) {
      comprintf("#endif\n");
    }

		comprintf("}\n");

	  char name[100] = { 0 };
	  for (int k = 0; lookuptab[i].name[k]; k++)
		  name[k] = lookuptab[i].name[k];

		if (aborted) {
			fprintf(stblfile, "{ NULL, 0x%08x, %ld }, /* %s */\n", flags,	opcode, name);
			com_discard();
		} else {
			printf ("/* %s */\n", outopcode (opcode));
			if (noflags) {
				fprintf(stblfile,	"{ op_%lx_%d_comp_nf, 0x%08x, %ld }, /* %s */\n",	opcode, postfix, flags, opcode, name);
				fprintf(headerfile, "extern compop_func op_%lx_%d_comp_nf;\n", opcode, postfix);
				printf("uae_u32 REGPARAM2 op_%lx_%d_comp_nf(uae_u32 opcode)\n{\n",	opcode, postfix);
			} else {
				fprintf(stblfile,	"{ op_%lx_%d_comp_ff, 0x%08x, %ld }, /* %s */\n",	opcode, postfix, flags, opcode, name);
				fprintf(headerfile, "extern compop_func op_%lx_%d_comp_ff;\n",	opcode, postfix);
				printf("uae_u32 REGPARAM2 op_%lx_%d_comp_ff(uae_u32 opcode)\n{\n",	opcode, postfix);
			}
			com_flush();
		}
	}
  
	opcode_next_clev[rp] = next_cpu_level;
	opcode_last_postfix[rp] = postfix;
}

static void generate_func(int noflags) 
{
	int i, j, rp;
	const char *tbl = noflags ? "nf" : "ff";

	using_prefetch = 0;
	using_exception_3 = 0;
	for (i = 0; i < 1; i++) /* We only do one level! */
	{
		cpu_level = NEXT_CPU_LEVEL - i;
		postfix = i;

		fprintf(stblfile, "extern const struct comptbl op_smalltbl_%d_comp_%s[] = {\n", postfix, tbl);

		/* sam: this is for people with low memory (eg. me :)) */
		printf("\n"
				"#if !defined(PART_1) && !defined(PART_2) && "
				"!defined(PART_3) && !defined(PART_4) && "
				"!defined(PART_5) && !defined(PART_6) && "
				"!defined(PART_7) && !defined(PART_8)"
				"\n"
				"#define PART_1 1\n"
				"#define PART_2 1\n"
				"#define PART_3 1\n"
				"#define PART_4 1\n"
				"#define PART_5 1\n"
				"#define PART_6 1\n"
				"#define PART_7 1\n"
				"#define PART_8 1\n"
				"#endif\n\n");
	  printf ("extern void comp_fpp_opp();\n"
		   "extern void comp_fscc_opp();\n"
		   "extern void comp_fbcc_opp();\n\n");

		rp = 0;
		for (j = 1; j <= 8; ++j) 
    {
			int k = (j * nr_cpuop_funcs) / 8;
			printf("#ifdef PART_%d\n", j);
			for (; rp < k; rp++)
				generate_one_opcode(rp, noflags);
			printf("#endif\n\n");
		}

		fprintf(stblfile, "{ 0, 0,65536 }};\n");
	}

}

int main(int argc, char *argv[])
{
	read_table68k();
	do_merges();

  opcode_map = xmalloc (int, nr_cpuop_funcs);
  opcode_last_postfix = xmalloc (int, nr_cpuop_funcs);
  opcode_next_clev = xmalloc (int, nr_cpuop_funcs);
  counts = xmalloc (unsigned long, 65536);
	read_counts();

	/* It would be a lot nicer to put all in one file (we'd also get rid of
	 * cputbl.h that way), but cpuopti can't cope.  That could be fixed, but
	 * I don't dare to touch the 68k version.  */

  headerfile = fopen("jit/comptbl.h", "wb");

	fprintf (headerfile, "" \
		"extern const struct comptbl op_smalltbl_0_comp_nf[];\n" \
		"extern const struct comptbl op_smalltbl_0_comp_ff[];\n" \
		"");

	stblfile = fopen("jit/compstbl.cpp", "wb");
	freopen("jit/compemu.cpp", "wb", stdout);

	generate_includes(stdout);
	generate_includes(stblfile);

	printf("#include \"compemu.h\"\n");

	noflags = 0;
	generate_func(noflags);

	free(opcode_map);
	free(opcode_last_postfix);
	free(opcode_next_clev);
	free(counts);

  opcode_map = xmalloc (int, nr_cpuop_funcs);
  opcode_last_postfix = xmalloc (int, nr_cpuop_funcs);
  opcode_next_clev = xmalloc (int, nr_cpuop_funcs);
  counts = xmalloc (unsigned long, 65536);
	read_counts();
	noflags = 1;
	generate_func(noflags);

  printf ("#endif\n");
  fprintf (stblfile, "#endif\n");

	free(opcode_map);
	free(opcode_last_postfix);
	free(opcode_next_clev);
	free(counts);

	free(table68k);
	fclose(stblfile);
	fclose(headerfile);
	return 0;
}
