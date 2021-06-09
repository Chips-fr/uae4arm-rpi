/*
 * compiler/compemu_midfunc_armA64.cpp - Native MIDFUNCS for AARCH64
 *
 * Copyright (c) 2019 TomB
 *
 * Inspired by Christian Bauer's Basilisk II
 *
 *  Original 68040 JIT compiler for UAE, copyright 2000-2002 Bernd Meyer
 *
 *  Adaptation for Basilisk II and improvements, copyright 2000-2002
 *    Gwenole Beauchesne
 *
 *  Basilisk II (C) 1997-2002 Christian Bauer
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
 *  Note:
 *  	File is included by compemu_support.cpp
 *
 */

/********************************************************************
 * CPU functions exposed to gencomp. Both CREATE and EMIT time      *
 ********************************************************************/

/*
 *  RULES FOR HANDLING REGISTERS:
 *
 *  * In the function headers, order the parameters
 *     - 1st registers written to
 *     - 2nd read/modify/write registers
 *     - 3rd registers read from
 *  * Before calling raw_*, you must call readreg, writereg or rmw for
 *    each register
 *  * The order for this is
 *     - 1nd call readreg for all registers read without offset
 *     - 2rd call rmw for all rmw registers
 *     - 3th call writereg for all written-to registers
 *     - 4th call raw_*
 *     - 5th unlock2 all registers that were locked
 */

#define INIT_REGS_b(d,s) 					\
	int targetIsReg = (d < 16); 		\
	int s_is_d = (s == d);  				\
	if(!s_is_d) 										\
		s = readreg(s); 							\
	d = rmw(d);											\
	if(s_is_d)											\
		s = d;

#define INIT_RREGS_b(d,s) 				\
	int targetIsReg = (d < 16); 		\
	int s_is_d = (s == d);  				\
	if(!s_is_d) 										\
		s = readreg(s); 							\
	d = readreg(d);									\
	if(s_is_d)											\
		s = d;

#define INIT_REG_b(d) 						\
	int targetIsReg = (d < 16); 		\
	d = rmw(d);

#define INIT_RREG_b(d) 						\
	int targetIsReg = (d < 16); 		\
	d = readreg(d);

#define INIT_WREG_b(d) 						\
	int targetIsReg = (d < 16); 		\
	if(targetIsReg)									\
		d = rmw(d); 									\
	else														\
		d = writereg(d);

#define INIT_REGS_w(d,s) 					\
	int targetIsReg = (d < 16); 		\
	int s_is_d = (s == d);  				\
	if(!s_is_d) 										\
		s = readreg(s); 							\
	d = rmw(d);											\
	if(s_is_d)											\
		s = d;

#define INIT_RREGS_w(d,s)					\
	int targetIsReg = (d < 16); 		\
	int s_is_d = (s == d);  				\
	if(!s_is_d) 										\
		s = readreg(s); 							\
	d = readreg(d);									\
	if(s_is_d)											\
		s = d;

#define INIT_REG_w(d) 						\
	int targetIsReg = (d < 16); 		\
	d = rmw(d);

#define INIT_RREG_w(d) 						\
	int targetIsReg = (d < 16); 		\
	d = readreg(d);

#define INIT_WREG_w(d) 						\
	int targetIsReg = (d < 16); 		\
	if(targetIsReg)									\
		d = rmw(d);										\
	else														\
		d = writereg(d);

#define INIT_REGS_l(d,s) 					\
	int targetIsReg = (d < 16); 		\
	int s_is_d = (s == d);  				\
	if(!s_is_d) 										\
		s = readreg(s); 							\
	d = rmw(d);											\
	if(s_is_d)											\
		s = d;

#define INIT_RREGS_l(d,s) 				\
	int targetIsReg = (d < 16); 		\
	int s_is_d = (s == d);  				\
	if(!s_is_d) 										\
		s = readreg(s); 							\
	d = readreg(d);									\
	if(s_is_d)											\
		s = d;

#define EXIT_REGS(d,s)						\
	unlock2(d);											\
	if(!s_is_d)											\
		unlock2(s);
	

MIDFUNC(0,live_flags,(void))
{
	live.flags_on_stack = TRASH;
	live.flags_in_flags = VALID;
	live.flags_are_important = 1;
}
MENDFUNC(0,live_flags,(void))

MIDFUNC(0,dont_care_flags,(void))
{
	live.flags_are_important = 0;
}
MENDFUNC(0,dont_care_flags,(void))

MIDFUNC(0,make_flags_live,(void))
{
	make_flags_live_internal();
}
MENDFUNC(0,make_flags_live,(void))

MIDFUNC(2,mov_l_mi,(IMPTR d, IM32 s))
{
  /* d points always to memory in regs struct */
  LOAD_U32(REG_WORK2, s);
  uintptr idx = d - (uintptr) &regs;
  if(d == (uintptr) &(regs.pc_p) || d == (uintptr) &(regs.pc_oldp))
    STR_xXi(REG_WORK2, R_REGSTRUCT, idx);
  else
    STR_wXi(REG_WORK2, R_REGSTRUCT, idx);
}
MENDFUNC(2,mov_l_mi,(IMPTR d, IM32 s))

MIDFUNC(4,disp_ea20_target_add,(RW4 target, RR4 reg, IM8 shift, IM8 extend))
{
	if(isconst(target) && isconst(reg)) {
		if(extend)
			set_const(target, live.state[target].val + (((uae_s32)(uae_s16)live.state[reg].val) << (shift & 0x1f)));
		else
			set_const(target, live.state[target].val + (live.state[reg].val << (shift & 0x1f)));
		return;
	}

	reg = readreg(reg);
	target = rmw(target);
	
	if(extend) {
		SIGNED16_REG_2_REG(REG_WORK1, reg);
		ADD_wwwLSLi(target, target, REG_WORK1, shift & 0x1f);
	} else {
		ADD_wwwLSLi(target, target, reg, shift & 0x1f);
	}
	
	unlock2(target);
	unlock2(reg);
}
MENDFUNC(4,disp_ea20_target_add,(RW4 target, RR4 reg, IM8 shift, IM8 extend))

MIDFUNC(4,disp_ea20_target_mov,(W4 target, RR4 reg, IM8 shift, IM8 extend))
{
	if(isconst(reg)) {
		if(extend)
			set_const(target, ((uae_s32)(uae_s16)live.state[reg].val) << (shift & 0x1f));
		else
			set_const(target, live.state[reg].val << (shift & 0x1f));
		return;
	}

	reg = readreg(reg);
	target = writereg(target);
	
	if(extend) {
		SIGNED16_REG_2_REG(REG_WORK1, reg);
		LSL_wwi(target, REG_WORK1, shift & 0x1f);
	} else {
		LSL_wwi(target, reg, shift & 0x1f);
	}
	
	unlock2(target);
	unlock2(reg);
}
MENDFUNC(4,disp_ea20_target_mov,(W4 target, RR4 reg, IM8 shift, IM8 extend))

MIDFUNC(2,sign_extend_16_rr,(W4 d, RR2 s))
{
  // Only used in calc_disp_ea_020() -> flags not relevant and never modified
	if (isconst(s)) {
		set_const(d, (uae_s32)(uae_s16)live.state[s].val);
		return;
	}

	int s_is_d = (s == d);
	if (!s_is_d) {
		s = readreg(s);
		d = writereg(d);
	}	else {
		s = d = rmw(s);
	}
	SIGNED16_REG_2_REG(d, s);
	if (!s_is_d)
		unlock2(d);
	unlock2(s);
}
MENDFUNC(2,sign_extend_16_rr,(W4 d, RR2 s))

MIDFUNC(3,lea_l_brr,(W4 d, RR4 s, IM32 offset))
{
	if (isconst(s)) {
		COMPCALL(mov_l_ri)(d, live.state[s].val+offset);
		return;
	}

	int s_is_d = (s == d);
	if(s_is_d) {
		s = d = rmw(d);
	} else {
		s = readreg(s);
		d = writereg(d);
	}
	
  if((offset & ~0xfff) == 0) {
    ADD_wwi(d, s, offset);
  } else {
    LOAD_U32(REG_WORK1, offset);
  	ADD_www(d, s, REG_WORK1);
  }

	EXIT_REGS(d,s);
}
MENDFUNC(3,lea_l_brr,(W4 d, RR4 s, IM32 offset))

MIDFUNC(5,lea_l_brr_indexed,(W4 d, RR4 s, RR4 index, IM8 factor, IM8 offset))
{
	if (!offset) {
		COMPCALL(lea_l_rr_indexed)(d, s, index, factor);
		return;
	}
	
	s = readreg(s);
	index = readreg(index);
	d = writereg(d);

	int shft;
	switch(factor) {
  	case 1: shft=0; break;
  	case 2: shft=1; break;
  	case 4: shft=2; break;
  	case 8: shft=3; break;
  	default: abort();
	}

  SIGNED8_IMM_2_REG(REG_WORK1, offset);
  ADD_www(REG_WORK1, s, REG_WORK1);
  LSL_wwi(REG_WORK2, index, shft);
	ADD_www(d, REG_WORK1, REG_WORK2);

	unlock2(d);
	unlock2(index);
	unlock2(s);
}
MENDFUNC(5,lea_l_brr_indexed,(W4 d, RR4 s, RR4 index, IM8 factor, IM8 offset))

MIDFUNC(4,lea_l_rr_indexed,(W4 d, RR4 s, RR4 index, IM8 factor))
{
	s = readreg(s);
	index = readreg(index);
	d = writereg(d);

	int shft;
	switch(factor) {
  	case 1: shft=0; break;
  	case 2: shft=1; break;
  	case 4: shft=2; break;
  	case 8: shft=3; break;
  	default: abort();
	}

	ADD_wwwLSLi(d, s, index, shft);

	unlock2(d);
	unlock2(index);
	unlock2(s);
}
MENDFUNC(4,lea_l_rr_indexed,(W4 d, RR4 s, RR4 index, IM8 factor))

MIDFUNC(2,mov_l_rr,(W4 d, RR4 s))
{
	int olds;

	if (d == s) { /* How pointless! */
		return;
	}
	if (isconst(s)) {
		COMPCALL(mov_l_ri)(d, live.state[s].val);
		return;
	}
	olds = s;
	disassociate(d);
	s = readreg(s);
	live.state[d].realreg = s;
	live.state[d].realind = live.nat[s].nholds;
	live.state[d].val = live.state[olds].val;
	live.state[d].validsize = 4;
	set_status(d, DIRTY);

	live.nat[s].holds[live.nat[s].nholds] = d;
	live.nat[s].nholds++;
	unlock2(s);
}
MENDFUNC(2,mov_l_rr,(W4 d, RR4 s))

MIDFUNC(2,mov_l_mr,(IMPTR d, RR4 s))
{
  /* d points always to memory in regs struct */
	if (isconst(s)) {
		COMPCALL(mov_l_mi)(d, live.state[s].val);
		return;
	}
	
	s = readreg(s);

  uintptr idx = d - (uintptr) &regs;
  if(d == (uintptr)&regs.pc_oldp || d == (uintptr)&regs.pc_p)
    STR_xXi(s, R_REGSTRUCT, idx);
  else
    STR_wXi(s, R_REGSTRUCT, idx);

	unlock2(s);
}
MENDFUNC(2,mov_l_mr,(IMPTR d, RR4 s))

MIDFUNC(2,mov_l_rm,(W4 d, IMPTR s))
{
  /* s points always to memory in regs struct */
	d = writereg(d);

  uintptr idx = s - (uintptr) & regs;
  LDR_wXi(d, R_REGSTRUCT, idx);

	unlock2(d);
}
MENDFUNC(2,mov_l_rm,(W4 d, IMPTR s))

MIDFUNC(2,mov_l_ri,(W4 d, IM32 s))
{
	set_const(d, s);
}
MENDFUNC(2,mov_l_ri,(W4 d, IM32 s))

MIDFUNC(2,mov_b_ri,(W1 d, IM8 s))
{
  if(d < 16) {
	  if (isconst(d)) {
		  set_const(d, (live.state[d].val & 0xffffff00) | (s & 0x000000ff));
		  return;
	  }
	  d = rmw(d);

		MOV_xi(REG_WORK1, (s & 0xff));
		BFI_xxii(d, REG_WORK1, 0, 8);
		
	  unlock2(d);
  } else {
    set_const(d, s & 0xff);
  }
}
MENDFUNC(2,mov_b_ri,(W1 d, IM8 s))

MIDFUNC(2,sub_l_ri,(RW4 d, IM8 i))
{
	if (!i)
	  return;
	if (isconst(d)) {
		live.state[d].val -= i;
		return;
	}

	d = rmw(d);

  SUB_wwi(d, d, i);

	unlock2(d);
}
MENDFUNC(2,sub_l_ri,(RW4 d, IM8 i))

MIDFUNC(2,sub_w_ri,(RW2 d, IM8 i))
{
  // This function is only called with i = 1
	// Caller needs flags...
	clobber_flags();

	d = rmw(d);

	LSL_wwi(REG_WORK2, d, 16);

	SUBS_wwish(REG_WORK2, REG_WORK2, (i & 0xff) << 4, 1);
	BFXIL_xxii(d, REG_WORK2, 16, 16);

	MRS_NZCV_x(REG_WORK1);
	EOR_xxCflag(REG_WORK1, REG_WORK1);
	MSR_NZCV_x(REG_WORK1);

	unlock2(d);
}
MENDFUNC(2,sub_w_ri,(RW2 d, IM8 i))

/* forget_about() takes a mid-layer register */
MIDFUNC(1,forget_about,(W4 r))
{
	if (isinreg(r))
	  disassociate(r);
	live.state[r].val = 0;
	set_status(r, UNDEF);
}
MENDFUNC(1,forget_about,(W4 r))


MIDFUNC(2,arm_ADD_l,(RW4 d, RR4 s))
{
	if (isconst(s)) {
		COMPCALL(arm_ADD_l_ri)(d,live.state[s].val);
		return;
	}

	s = readreg(s);
	d = rmw(d);
	ADD_www(d, d, s);
	unlock2(d);
	unlock2(s);
}
MENDFUNC(2,arm_ADD_l,(RW4 d, RR4 s))

MIDFUNC(2,arm_ADD_l_ri,(RW4 d, IM32 i))
{
	if (!i) 
	  return;
	if (isconst(d)) {
		live.state[d].val += i;
		return;
	}

	d = rmw(d);

  if((i & ~0xfff) == 0) {
		ADD_wwi(d, d, i);
	} else {
	  LOAD_U32(REG_WORK1, i);
		ADD_www(d, d, REG_WORK1);
  }
	
	unlock2(d);
}
MENDFUNC(2,arm_ADD_l_ri,(RW4 d, IM32 i))

MIDFUNC(2,arm_ADD_l_ri8,(RW4 d, IM8 i))
{
	if (!i) 
	  return;
	if (isconst(d)) {
		live.state[d].val += i;
		return;
	}

	d = rmw(d);
	ADD_wwi(d, d, i);
	unlock2(d);
}
MENDFUNC(2,arm_ADD_l_ri8,(RW4 d, IM8 i))

MIDFUNC(2,arm_SUB_l_ri8,(RW4 d, IM8 i))
{
	if (!i) 
	  return;
	if (isconst(d)) {
		live.state[d].val -= i;
		return;
	}

	d = rmw(d);
	SUB_wwi(d, d, i);
	unlock2(d);
}
MENDFUNC(2,arm_SUB_l_ri8,(RW4 d, IM8 i))


#ifdef DEBUG
#include "aarch64.h"
#endif

STATIC_INLINE void flush_cpu_icache(void *start, void *stop)
{
#ifdef DEBUG
  if((uae_u64)stop - (uae_u64)start > 4) {
    char disbuf[256];
    uint64_t disptr = (uint64_t)start;
    while(disptr < (uint64_t)stop) {
      disasm(disptr, disbuf);
      write_log("%016llx  %s\n", disptr, disbuf);
      disptr += 4;
    }
  }
#endif

  //__builtin___clear_cache(start, stop);
  __clear_cache(start, stop);
}


STATIC_INLINE void write_jmp_target(uae_u32* jmpaddr, uintptr a) 
{
	uintptr off = ((uintptr)a - (uintptr)jmpaddr) >> 2;
  if((*(jmpaddr) & 0xfc000000) == 0x14000000)
    /* branch always */
    *(jmpaddr) = (*(jmpaddr) & 0xfc000000) | (off & 0x03ffffff);
  else
    /* conditional branch */
    *(jmpaddr) = (*(jmpaddr) & 0xff00001f) | ((off << 5) & 0x00ffffe0);

  flush_cpu_icache((void *)jmpaddr, (void *)&jmpaddr[1]);
}
