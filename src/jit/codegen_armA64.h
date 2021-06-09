/*
 * compiler/codegen_armA64.h - AARCH64 code generator
 *
 * Copyright (c) 2019 TomB
 * 
 * This file is part of the UAE4ARM project.
 *
 * JIT compiler m68k -> ARMv8.1
 *
 * Original 68040 JIT compiler for UAE, copyright 2000-2002 Bernd Meyer
 * This file is derived from CCG, copyright 1999-2003 Ian Piumarta
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef ARM_ASMA64_H
#define ARM_ASMA64_H


/* CPSR flags */

#define ARM_N_FLAG        0x80000000
#define ARM_Z_FLAG        0x40000000
#define ARM_C_FLAG        0x20000000
#define ARM_V_FLAG        0x10000000
#define ARM_CV_FLAGS      (ARM_C_FLAG|ARM_V_FLAG)


#define _W(c) emit_long(c)


#define MIN_EL0           0b011

#define EX_UXTB   0b000
#define EX_UXTH   0b001
#define EX_UXTW   0b010
#define EX_UXTX   0b011
#define EX_SXTB   0b100
#define EX_SXTH   0b101
#define EX_SXTW   0b110
#define EX_SXTX   0b111


/* read/write flags */
// move from sysreg
#define MRS_NZCV_x(Xt)    _W((0b1101010100111 << 19) | (MIN_EL0 << 16) | (0b0100 << 12) | (0b0010 << 8) | (0b000 << 5) | (Xt))
// move to sysreg
#define MSR_NZCV_x(Xt)		_W((0b1101010100011 << 19) | (MIN_EL0 << 16) | (0b0100 << 12) | (0b0010 << 8) | (0b000 << 5) | (Xt))


/*----------------------------------------
 * branch instructions 
 *----------------------------------------*/
// i is the number of instructions, i.e. 4 bytes
#define B_i(i)            _W((0b000101 << 26) | ((i) & 0x03ffffff))
#define BL_i(i)           _W((0b100101 << 26) | ((i) & 0x03ffffff))
#define BLR_x(Xn)         _W((0b11010110001 << 21) | (0b11111 << 16) | (0 << 10) | ((Xn) << 5) | 0)
#define BR_x(Xn)          _W((0b11010110000 << 21) | (0b11111 << 16) | (0 << 10) | ((Xn) << 5) | 0)

#define CC_B_i(cc,i)      _W((0b01010100 << 24) | (((i) & 0x0007ffff) << 5) | (cc))
#define BEQ_i(i)          CC_B_i(NATIVE_CC_EQ,i)
#define BNE_i(i)          CC_B_i(NATIVE_CC_NE,i)
#define BCS_i(i)          CC_B_i(NATIVE_CC_CS,i)
#define BCC_i(i)          CC_B_i(NATIVE_CC_CC,i)
#define BMI_i(i)          CC_B_i(NATIVE_CC_MI,i)
#define BPL_i(i)          CC_B_i(NATIVE_CC_PL,i)
#define BVS_i(i)          CC_B_i(NATIVE_CC_VS,i)
#define BVC_i(i)          CC_B_i(NATIVE_CC_VC,i)
#define BHI_i(i)          CC_B_i(NATIVE_CC_HI,i)
#define BLS_i(i)          CC_B_i(NATIVE_CC_LS,i)
#define BGE_i(i)          CC_B_i(NATIVE_CC_GE,i)
#define BLT_i(i)          CC_B_i(NATIVE_CC_LT,i)
#define BGT_i(i)          CC_B_i(NATIVE_CC_GT,i)
#define BLE_i(i)          CC_B_i(NATIVE_CC_LE,i)

#define RET               _W((0b11010110010 << 21) | (0b11111 << 16) | (0 << 10) | ((30) << 5) | (0))

/* compare and branch */
// these instructions do not affect condition flags
#define CBNZ_wi(Wt,i)       _W((0b00110101 << 24) | (((i) & 0x0007ffff) << 5) | (Wt))
#define CBNZ_xi(Xt,i)       _W((0b10110101 << 24) | (((i) & 0x0007ffff) << 5) | (Xt))
#define CBZ_wi(Wt,i)        _W((0b00110100 << 24) | (((i) & 0x0007ffff) << 5) | (Wt))
#define CBZ_xi(Xt,i)        _W((0b10110100 << 24) | (((i) & 0x0007ffff) << 5) | (Xt))

/* test bit and branch */
// these instructions do not affect condition flags
#define TBNZ_xii(Xt,bit,i)  _W(((((bit) & 0x20) >> 5) << 31) | (0b0110111 << 24) | (((bit) & 0x1f) << 19) | ((((i)/4) % 0x3fff) << 5) | (Xt))
#define TBNZ_wii(Wt,bit,i)  _W((0 << 31) | (0b0110111 << 24) | (((bit) & 0x1f) << 19) | ((((i)/4) % 0x3fff) << 5) | (Wt))
#define TBZ_xii(Xt,bit,i)   _W(((((bit) & 0x20) >> 5) << 31) | (0b0110110 << 24) | (((bit) & 0x1f) << 19) | ((((i)/4) % 0x3fff) << 5) | (Xt))
#define TBZ_wii(Wt,bit,i)   _W((0 << 31) | (0b0110110 << 24) | (((bit) & 0x1f) << 19) | ((((i)/4) % 0x3fff) << 5) | (Wt))


/*----------------------------------------
 * load/store registers
 *---------------------------------------- */
// i is number of bytes
#define LDP_wwXi(Wt1,Wt2,Xn,i)    _W((0b0010100101 << 22) | ((((i)/4) & 0x7f) << 15) | ((Wt2) << 10) | ((Xn) << 5) | (Wt1))
#define LDP_xxXi(Xt1,Xt2,Xn,i)    _W((0b1010100101 << 22) | ((((i)/8) & 0x7f) << 15) | ((Xt2) << 10) | ((Xn) << 5) | (Xt1))
#define LDPSW_xxXi(Xt1,Xt2,Xn,i)  _W((0b0110100101 << 22) | ((((i)/4) & 0x7f) << 15) | ((Xt2) << 10) | ((Xn) << 5) | (Xt1))
#define LDR_wXi(Wt,Xn,i)          _W((0b1011100101 << 22) | ((((i)/4) & 0xfff) << 10) | ((Xn) << 5) | (Wt))
#define LDR_xXi(Xt,Xn,i)          _W((0b1111100101 << 22) | ((((i)/8) & 0xfff) << 10) | ((Xn) << 5) | (Xt))
#define LDR_wPCi(Wt,i)            _W((0b00011000 << 24) | ((((i)/4) & 0x7ffff) << 5) | (Wt))
#define LDR_xPCi(Xt,i)            _W((0b01011000 << 24) | ((((i)/4) & 0x7ffff) << 5) | (Xt))
#define LDRSW_xPCi(Xt,i)          _W((0b10011000 << 24) | ((((i)/4) & 0x7ffff) << 5) | (Xt))
#define LDR_wXx(Wt,Xn,Xm)         _W((0b10111000011 << 21) | ((Xm) << 16) | (0b011 << 13) | (0 << 12) | (0b10 << 10) | ((Xn) << 5) | (Wt))
#define LDR_xXx(Xt,Xn,Xm)         _W((0b11111000011 << 21) | ((Xm) << 16) | (0b011 << 13) | (0 << 12) | (0b10 << 10) | ((Xn) << 5) | (Xt))
// i=0 no shift, i=1 LSL 2 (W) or LSL 3 (X)
#define LDR_wXxLSLi(Wt,Xn,Xm,i)   _W((0b10111000011 << 21) | ((Xm) << 16) | (0b011 << 13) | (((i)&1) << 12) | (0b10 << 10) | ((Xn) << 5) | (Wt))
#define LDR_xXxLSLi(Xt,Xn,Xm,i)   _W((0b11111000011 << 21) | ((Xm) << 16) | (0b011 << 13) | (((i)&1) << 12) | (0b10 << 10) | ((Xn) << 5) | (Xt))
#define LDR_xXwUXTW(Xt,Xn,Wm)     _W((0b11111000011 << 21) | ((Wm) << 16) | (0b010 << 13) | (0 << 12) | (0b10 << 10) | ((Xn) << 5) | (Xt))
#define LDRB_wXi(Wt,Xn,i)         _W((0b0011100101 << 22) | (((i) & 0xfff) << 10) | ((Xn) << 5) | (Wt))
#define LDRB_wXx(Wt,Xn,Xm)        _W((0b00111000011 << 21) | ((Xm) << 16) | (0b011 << 13) | (0 << 12) | (0b10 << 10) | ((Xn) << 5) | (Wt))
#define LDRH_wXi(Wt,Xn,i)         _W((0b0111100101 << 22) | ((((i)/2) &0xfff) << 10) | ((Xn) << 5) | (Wt))
#define LDRH_wXx(Wt,Xn,Xm)        _W((0b01111000011 << 21) | ((Xm) << 16) | (0b011 << 13) | (0 << 12) | (0b10 << 10) | ((Xn) << 5) | (Wt))
#define LDRSB_wXi(Wt,Xn,i)        _W((0b0011100111 << 22) | (((i) & 0xfff) << 10) | ((Xn) << 5) | (Wt))
#define LDRSB_xXi(Xt,Xn,i)        _W((0b0011100110 << 22) | (((i) & 0xfff) << 10) | ((Xn) << 5) | (Xt))
#define LDRSB_wXx(Wt,Xn,Xm)       _W((0b00111000111 << 21) | ((Xm) << 16) | (0b011 << 13) | (0 << 12) | (0b10 << 10) | ((Xn) << 5) | (Wt))
#define LDRSB_xXx(Xt,Xn,Xm)       _W((0b00111000101 << 21) | ((Xm) << 16) | (0b011 << 13) | (0 << 12) | (0b10 << 10) | ((Xn) << 5) | (Xt))
#define LDRSH_wXi(Wt,Xn,i)        _W((0b0111100111 << 22) | ((((i)/2) & 0xfff) << 10) | ((Xn) << 5) | (Wt))
#define LDRSH_xXi(Xt,Xn,i)        _W((0b0111100110 << 22) | ((((i)/2) & 0xfff) << 10) | ((Xn) << 5) | (Xt))
#define LDRSH_wXx(Wt,Xn,Xm)       _W((0b01111000111 << 21) | ((Xm) << 16) | (0b011 << 13) | (0 << 12) | (0b10 << 10) | ((Xn) << 5) | (Wt))
#define LDRSH_xXx(Xt,Xn,Xm)       _W((0b01111000101 << 21) | ((Xm) << 16) | (0b011 << 13) | (0 << 12) | (0b10 << 10) | ((Xn) << 5) | (Xt))
#define LDRSW_xXi(Xt,Xn,i)        _W((0b1011100110 << 22) | ((((i)/4) & 0xfff) << 10) | ((Xn) << 5) | (Xt))
#define LDRSW_xXx(Xt,Xn,Xm)       _W((0b10111000101 << 21) | ((Xm) << 16) | (0b011 << 13) | (0 << 12) | (0b10 << 10) | ((Xn) << 5) | (Xt))

#define ADR_xPCi(Xd,i21)          _W((0b0 << 31) | ((i21 & 3) << 29) | (0b10000 << 24) | ((((i21) >> 2) & 0x7ffff) << 5) | (Xd))

#define STP_wwXi(Wt1,Wt2,Xn,i)    _W((0b0010100100 << 22) | ((((i)/4) & 0x7f) << 15) | ((Wt2) << 10) | ((Xn) << 5) | (Wt1))
#define STP_xxXi(Xt1,Xt2,Xn,i)    _W((0b1010100100 << 22) | ((((i)/8) & 0x7f) << 15) | ((Xt2) << 10) | ((Xn) << 5) | (Xt1))
#define STR_wXi(Wt,Xn,i)          _W((0b1011100100 << 22) | ((((i)/4) & 0xfff) << 10) | ((Xn) << 5) | (Wt))
#define STR_xXi(Xt,Xn,i)          _W((0b1111100100 << 22) | ((((i)/8) & 0xfff) << 10) | ((Xn) << 5) | (Xt))
#define STR_wXx(Wt,Xn,Xm)         _W((0b10111000001 << 21) | ((Xm) << 16) | (0b011 << 13) | (0 << 12) | (0b10 << 10) | ((Xn) << 5) | (Wt))
#define STR_xXx(Xt,Xn,Xm)         _W((0b11111000001 << 21) | ((Xm) << 16) | (0b011 << 13) | (0 << 12) | (0b10 << 10) | ((Xn) << 5) | (Xt))
// i=0 no shift, i=1 LSL 2 (W) or LSL 3 (X)
#define STR_wXxLSLi(Wt,Xn,Xm,i)   _W((0b10111000001 << 21) | ((Xm) << 16) | (0b011 << 13) | (((i)&1) << 12) | (0b10 << 10) | ((Xn) << 5) | (Wt))
#define STR_xXxLSLi(Xt,Xn,Xm,i)   _W((0b11111000001 << 21) | ((Xm) << 16) | (0b011 << 13) | (((i)&1) << 12) | (0b10 << 10) | ((Xn) << 5) | (Xt))
#define STR_xXwUXTW(Xt,Xn,Wm)     _W((0b11111000001 << 21) | ((Wm) << 16) | (0b010 << 13) | (0 << 12) | (0b10 << 10) | ((Xn) << 5) | (Xt))
#define STRB_wXi(Wt,Xn,i)         _W((0b0011100100 << 22) | (((i) & 0xfff) << 10) | ((Xn) << 5) | (Wt))
#define STRB_wXx(Wt,Xn,Xm)        _W((0b00111000001 << 21) | ((Xm) << 16) | (0b011 << 13) | (0 << 12) | (0b10 << 10) | ((Xn) << 5) | (Wt))
#define STRH_wXi(Wt,Xn,i)         _W((0b0111100100 << 22) | ((((i)/2) &0xfff) << 10) | ((Xn) << 5) | (Wt))
#define STRH_wXx(Wt,Xn,Xm)        _W((0b01111000001 << 21) | ((Xm) << 16) | (0b011 << 13) | (0 << 12) | (0b10 << 10) | ((Xn) << 5) | (Wt))


/*----------------------------------------
 * move immediate/register 
 *----------------------------------------*/
#define MOV_wi(Wd,i16)            _W((0b01010010100 << 21) | (((i16) & 0xffff) << 5) | (Wd))
#define MOV_xi(Xd,i16)            _W((0b11010010100 << 21) | (((i16) & 0xffff) << 5) | (Xd))
// sh in bits (multiple of 16)
#define MOV_wish(Wd,i16,sh)       _W((0b010100101 << 23) | (((sh)/16) << 21) | (((i16) & 0xffff) << 5) | (Wd))
#define MOV_xish(Xd,i16,sh)       _W((0b110100101 << 23) | (((sh)/16) << 21) | (((i16) & 0xffff) << 5) | (Xd))
#define MOV_ww(Wd,Wm)             _W((0b00101010000 << 21) | ((Wm) << 16) | (0 << 10) | (0b11111 << 5) | (Wd))
#define MOV_xx(Xd,Xm)             _W((0b10101010000 << 21) | ((Xm) << 16) | (0 << 10) | (0b11111 << 5) | (Xd))
#define MOVK_wi(Wd,i16)           _W((0b01110010100 << 21) | (((i16) & 0xffff) << 5) | (Wd))
#define MOVK_xi(Xd,i16)           _W((0b11110010100 << 21) | (((i16) & 0xffff) << 5) | (Xd))
#define MOVK_wish(Wd,i16,sh)      _W((0b011100101 << 23) | (((sh)/16) << 21) | (((i16) & 0xffff) << 5) | (Wd))
#define MOVK_xish(Xd,i16,sh)      _W((0b111100101 << 23) | (((sh)/16) << 21) | (((i16) & 0xffff) << 5) | (Xd))
#define MOVN_wi(Wd,i16)           _W((0b00010010100 << 21) | (((i16) & 0xffff) << 5) | (Wd))
#define MOVN_xi(Xd,i16)           _W((0b10010010100 << 21) | (((i16) & 0xffff) << 5) | (Xd))
#define MOVN_wish(Wd,i16,sh)      _W((0b000100101 << 23) | (((sh)/16) << 21) | (((i16) & 0xffff) << 5) | (Wd))
#define MOVN_xish(Xd,i16,sh)      _W((0b100100101 << 23) | (((sh)/16) << 21) | (((i16) & 0xffff) << 5) | (Xd))
#define MVN_ww(Wd,Wm)             _W((0b00101010001 << 21) | ((Wm) << 16) | (0 << 10) | (0b11111 << 5) | (Wd))
#define MVN_xx(Xd,Xm)             _W((0b10101010001 << 21) | ((Xm) << 16) | (0 << 10) | (0b11111 << 5) | (Xd))
#define MVN_wwLSLi(Wd,Wm,i)       _W((0b00101010001 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | (0b11111 << 5) | (Wd))
#define MVN_xxLSLi(Xd,Xm,i)       _W((0b10101010001 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | (0b11111 << 5) | (Xd))
#define MVN_wwLSRi(Wd,Wm,i)       _W((0b00101010011 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | (0b11111 << 5) | (Wd))
#define MVN_xxLSRi(Xd,Xm,i)       _W((0b10101010011 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | (0b11111 << 5) | (Xd))
#define MVN_wwASRi(Wd,Wm,i)       _W((0b00101010101 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | (0b11111 << 5) | (Wd))
#define MVN_xxASRi(Xd,Xm,i)       _W((0b10101010101 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | (0b11111 << 5) | (Xd))
#define MVN_wwRORi(Wd,Wm,i)       _W((0b00101010111 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | (0b11111 << 5) | (Wd))
#define MVN_xxRORi(Xd,Xm,i)       _W((0b10101010111 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | (0b11111 << 5) | (Xd))


/*----------------------------------------
 * arithmetic
 *----------------------------------------*/
/* ADD */
#define ADC_www(Wd,Wn,Wm)         _W((0b00011010000 << 21) | ((Wm) << 16) | (0 << 10) | ((Wn) << 5) | (Wd))
#define ADC_xxx(Xd,Xn,Xm)         _W((0b10011010000 << 21) | ((Xm) << 16) | (0 << 10) | ((Xn) << 5) | (Xd))
#define ADCS_www(Wd,Wn,Wm)        _W((0b00111010000 << 21) | ((Wm) << 16) | (0 << 10) | ((Wn) << 5) | (Wd))
#define ADCS_xxx(Xd,Xn,Xm)        _W((0b10111010000 << 21) | ((Xm) << 16) | (0 << 10) | ((Xn) << 5) | (Xd))
#define ADD_wwwEX(Wd,Wn,Wm,ex)    _W((0b00001011001 << 21) | ((Wm) << 16) | ((ex) << 13) | (0 << 10) | ((Wn) << 5) | (Wd))
#define ADD_xxwEX(Xd,Xn,Wm,ex)    _W((0b10001011001 << 21) | ((Wm) << 16) | ((ex) << 13) | (0 << 10) | ((Xn) << 5) | (Xd))
#define ADD_wwi(Wd,Wn,i12)        _W((0b0001000100 << 22) | (((i12) & 0xfff) << 10) | ((Wn) << 5) | (Wd))
#define ADD_xxi(Xd,Xn,i12)        _W((0b1001000100 << 22) | (((i12) & 0xfff) << 10) | ((Xn) << 5) | (Xd))
// sh: 0 - no shift, 1 - LSL 12
#define ADD_wwish(Wd,Wn,i12,sh)   _W((0b000100010 << 23) | (((sh) & 1) << 22) | (((i12) & 0xfff) << 10) | ((Wn) << 5) | (Wd))
#define ADD_xxish(Xd,Xn,i12,sh)   _W((0b100100010 << 23) | (((sh) & 1) << 22) | (((i12) & 0xfff) << 10) | ((Xn) << 5) | (Xd))
#define ADD_www(Wd,Wn,Wm)         _W((0b00001011000 << 21) | ((Wm) << 16) | (0 << 10) | ((Wn) << 5) | (Wd))
#define ADD_xxx(Xd,Xn,Xm)         _W((0b10001011000 << 21) | ((Xm) << 16) | (0 << 10) | ((Xn) << 5) | (Xd))
#define ADD_wwwLSLi(Wd,Wn,Wm,i)   _W((0b00001011000 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define ADD_xxxLSLi(Xd,Xn,Xm,i)   _W((0b10001011000 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define ADD_wwwLSRi(Wd,Wn,Wm,i)   _W((0b00001011010 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define ADD_xxxLSRi(Xd,Xn,Xm,i)   _W((0b10001011010 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define ADD_wwwASRi(Wd,Wn,Wm,i)   _W((0b00001011100 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define ADD_xxxASRi(Xd,Xn,Xm,i)   _W((0b10001011100 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define ADDS_wwwEX(Wd,Wn,Wm,ex)   _W((0b00101011001 << 21) | ((Wm) << 16) | ((ex) << 13) | (0 << 10) | ((Wn) << 5) | (Wd))
#define ADDS_xxwEX(Xd,Xn,Wm,ex)   _W((0b10101011001 << 21) | ((Wm) << 16) | ((ex) << 13) | (0 << 10) | ((Xn) << 5) | (Xd))
#define ADDS_wwi(Wd,Wn,i12)       _W((0b0011000100 << 22) | (((i12) & 0xfff) << 10) | ((Wn) << 5) | (Wd))
#define ADDS_xxi(Xd,Xn,i12)       _W((0b1011000100 << 22) | (((i12) & 0xfff) << 10) | ((Xn) << 5) | (Xd))
// sh: 0 - no shift, 1 - LSL 12
#define ADDS_wwish(Wd,Wn,i12,sh)  _W((0b001100010 << 23) | (((sh) & 1) << 22) | (((i12) & 0xfff) << 10) | ((Wn) << 5) | (Wd))
#define ADDS_xxish(Xd,Xn,i12,sh)  _W((0b101100010 << 23) | (((sh) & 1) << 22) | (((i12) & 0xfff) << 10) | ((Xn) << 5) | (Xd))
#define ADDS_www(Wd,Wn,Wm)        _W((0b00101011000 << 21) | ((Wm) << 16) | (0 << 10) | ((Wn) << 5) | (Wd))
#define ADDS_xxx(Xd,Xn,Xm)        _W((0b10101011000 << 21) | ((Xm) << 16) | (0 << 10) | ((Xn) << 5) | (Xd))
#define ADDS_wwwLSLi(Wd,Wn,Wm,i)  _W((0b00101011000 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define ADDS_xxxLSLi(Xd,Xn,Xm,i)  _W((0b10101011000 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define ADDS_wwwLSRi(Wd,Wn,Wm,i)  _W((0b00101011010 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define ADDS_xxxLSRi(Xd,Xn,Xm,i)  _W((0b10101011010 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define ADDS_wwwASRi(Wd,Wn,Wm,i)  _W((0b00101011100 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define ADDS_xxxASRi(Xd,Xn,Xm,i)  _W((0b10101011100 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))

/* compare */
#define CMN_wwEX(Wn,Wm,ex)        _W((0b00101011001 << 21) | ((Wm) << 16) | ((ex) << 13) | (0 << 10) | ((Wn) << 5) | (0b11111))
#define CMN_xwEX(Xn,Wm,ex)        _W((0b10101011001 << 21) | ((Wm) << 16) | ((ex) << 13) | (0 << 10) | ((Xn) << 5) | (0b11111))
#define CMN_wi(Wn,i12)            _W((0b0011000100 << 22) | (((i12) & 0xfff) << 10) | ((Wn) << 5) | (0b11111))
#define CMN_xi(Xn,i12)            _W((0b1011000100 << 22) | (((i12) & 0xfff) << 10) | ((Xn) << 5) | (0b11111))
// sh: 0 - no shift, 1 - LSL 12
#define CMN_wish(Wn,i12,sh)       _W((0b001100010 << 23) | (((sh) & 1) << 22) | (((i12) & 0xfff) << 10) | ((Wn) << 5) | (0b11111))
#define CMN_xish(Xn,i12,sh)       _W((0b101100010 << 23) | (((sh) & 1) << 22) | (((i12) & 0xfff) << 10) | ((Xn) << 5) | (0b11111))
#define CMN_ww(Wn,Wm)             _W((0b00101011000 << 21) | ((Wm) << 16) | (0 << 10) | ((Wn) << 5) | (0b11111))
#define CMN_xx(Xn,Xm)             _W((0b10101011000 << 21) | ((Xm) << 16) | (0 << 10) | ((Xn) << 5) | (0b11111))
#define CMN_wwLSLi(Wn,Wm,i)       _W((0b00101011000 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (0b11111))
#define CMN_xxLSLi(Xn,Xm,i)       _W((0b10101011000 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (0b11111))
#define CMN_wwLSRi(Wn,Wm,i)       _W((0b00101011010 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (0b11111))
#define CMN_xxLSRi(Xn,Xm,i)       _W((0b10101011010 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (0b11111))
#define CMN_wwASRi(Wn,Wm,i)       _W((0b00101011100 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (0b11111))
#define CMN_xxASRi(Xn,Xm,i)       _W((0b10101011100 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (0b11111))
#define CMP_wwEX(Wn,Wm,ex)        _W((0b01101011001 << 21) | ((Wm) << 16) | ((ex) << 13) | (0 << 10) | ((Wn) << 5) | (0b11111))
#define CMP_xwEX(Xn,Wm,ex)        _W((0b11101011001 << 21) | ((Wm) << 16) | ((ex) << 13) | (0 << 10) | ((Xn) << 5) | (0b11111))
#define CMP_wi(Wn,i12)            _W((0b0111000100 << 22) | (((i12) & 0xfff) << 10) | ((Wn) << 5) | (0b11111))
#define CMP_xi(Xn,i12)            _W((0b1111000100 << 22) | (((i12) & 0xfff) << 10) | ((Xn) << 5) | (0b11111))
// sh: 0 - no shift, 1 - LSL 12
#define CMP_wish(Wn,i12,sh)       _W((0b011100010 << 23) | (((sh) & 1) << 22) | (((i12) & 0xfff) << 10) | ((Wn) << 5) | (0b11111))
#define CMP_xish(Xn,i12,sh)       _W((0b111100010 << 23) | (((sh) & 1) << 22) | (((i12) & 0xfff) << 10) | ((Xn) << 5) | (0b11111))
#define CMP_ww(Wn,Wm)             _W((0b01101011000 << 21) | ((Wm) << 16) | (0 << 10) | ((Wn) << 5) | (0b11111))
#define CMP_xx(Xn,Xm)             _W((0b11101011000 << 21) | ((Xm) << 16) | (0 << 10) | ((Xn) << 5) | (0b11111))
#define CMP_wwLSLi(Wn,Wm,i)       _W((0b01101011000 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (0b11111))
#define CMP_xxLSLi(Xn,Xm,i)       _W((0b11101011000 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (0b11111))
#define CMP_wwLSRi(Wn,Wm,i)       _W((0b01101011010 << 21) | ((Wm) << 16) | (((i) & 0x3d) << 10) | ((Wn) << 5) | (0b11111))
#define CMP_xxLSRi(Xn,Xm,i)       _W((0b11101011010 << 21) | ((Xm) << 16) | (((i) & 0x3d) << 10) | ((Xn) << 5) | (0b11111))
#define CMP_wwASRi(Wn,Wm,i)       _W((0b01101011100 << 21) | ((Wm) << 16) | (((i) & 0x3d) << 10) | ((Wn) << 5) | (0b11111))
#define CMP_xxASRi(Xn,Xm,i)       _W((0b11101011100 << 21) | ((Xm) << 16) | (((i) & 0x3d) << 10) | ((Xn) << 5) | (0b11111))

/* MUL */
#define MUL_www(Wd,Wn,Wm)         _W((0b00011011000 << 21) | ((Wm) << 16) | (0b011111 << 10) | ((Wn) << 5) | (Wd))
#define MUL_xxx(Xd,Xn,Xm)         _W((0b10011011000 << 21) | ((Xm) << 16) | (0b011111 << 10) | ((Xn) << 5) | (Xd))
// Xd = (Xn*Xm)[127...64]
#define SMULH_xxx(Xd,Xn,Xm)       _W((0b10011011010 << 21) | ((Xm) << 16) | (0b011111 << 10) | ((Xn) << 5) | (Xd))
#define UMULH_xxx(Xd,Xn,Xm)       _W((0b10011011110 << 21) | ((Xm) << 16) | (0b011111 << 10) | ((Xn) << 5) | (Xd))
// Xd = (Xn*Xm)[63...0]
#define SMULL_xxx(Xd,Xn,Xm)       _W((0b10011011001 << 21) | ((Xm) << 16) | (0b011111 << 10) | ((Xn) << 5) | (Xd))
#define UMULL_xxx(Xd,Xn,Xm)       _W((0b10011011101 << 21) | ((Xm) << 16) | (0b011111 << 10) | ((Xn) << 5) | (Xd))

/* multiply add */
// Wd = Wa + (Wn * Wm)
#define MADD_wwww(Wd,Wn,Wm,Wa)    _W((0b00011011000 << 21) | ((Wm) << 16) | (0 << 15) | ((Wa) << 10) | ((Wn) << 5) | (Wd))
#define MADD_xxxx(Xd,Xn,Xm,Xa)    _W((0b10011011000 << 21) | ((Xm) << 16) | (0 << 15) | ((Xa) << 10) | ((Xn) << 5) | (Xd))
#define SMADDL_xwwx(Xd,Wn,Wm,Xa)  _W((0b10011011001 << 21) | ((Wm) << 16) | (0 << 15) | ((Xa) << 10) | ((Wn) << 5) | (Xd))
#define UMADDL_xwwx(Xd,Wn,Wm,Xa)  _W((0b10011011101 << 21) | ((Wm) << 16) | (0 << 15) | ((Xa) << 10) | ((Wn) << 5) | (Xd))

/* multiply sub */
// Wd = Wa - (Wn * Wm)
#define MSUB_wwww(Wd,Wn,Wm,Wa)    _W((0b00011011000 << 21) | ((Wm) << 16) | (1 << 15) | ((Wa) << 10) | ((Wn) << 5) | (Wd))
#define MSUB_xxxx(Xd,Xn,Xm,Xa)    _W((0b10011011000 << 21) | ((Xm) << 16) | (1 << 15) | ((Xa) << 10) | ((Xn) << 5) | (Xd))
#define SMSUBL_xwwx(Xd,Wn,Wm,Xa)  _W((0b10011011001 << 21) | ((Wm) << 16) | (1 << 15) | ((Xa) << 10) | ((Wn) << 5) | (Xd))
#define UMSUBL_xwwx(Xd,Wn,Wm,Xa)  _W((0b10011011101 << 21) | ((Wm) << 16) | (1 << 15) | ((Xa) << 10) | ((Wn) << 5) | (Xd))

/* DIV */
// Wd = Wn / Wm
#define SDIV_www(Wd,Wn,Wm)        _W((0b00011010110 << 21) | ((Wm) << 16) | (0b000011 << 10) | ((Wn) << 5) | (Wd))
#define SDIV_xxx(Xd,Xn,Xm)        _W((0b10011010110 << 21) | ((Xm) << 16) | (0b000011 << 10) | ((Xn) << 5) | (Xd))
#define UDIV_www(Wd,Wn,Wm)        _W((0b00011010110 << 21) | ((Wm) << 16) | (0b000010 << 10) | ((Wn) << 5) | (Wd))
#define UDIV_xxx(Xd,Xn,Xm)        _W((0b10011010110 << 21) | ((Xm) << 16) | (0b000010 << 10) | ((Xn) << 5) | (Xd))

/* NEG */
#define NEG_ww(Wd,Wm)             _W((0b01001011000 << 21) | ((Wm) << 16) | (0 << 10) | (0b11111 << 5) | (Wd))
#define NEG_xx(Xd,Xm)             _W((0b11001011000 << 21) | ((Xm) << 16) | (0 << 10) | (0b11111 << 5) | (Xd))
#define NEG_wwLSLi(Wd,Wm,i)       _W((0b01001011000 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | (0b11111 << 5) | (Wd))
#define NEG_xxLSLi(Xd,Xm,i)       _W((0b11001011000 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | (0b11111 << 5) | (Xd))
#define NEG_wwLSRi(Wd,Wm,i)       _W((0b01001011010 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | (0b11111 << 5) | (Wd))
#define NEG_xxLSRi(Xd,Xm,i)       _W((0b11001011010 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | (0b11111 << 5) | (Xd))
#define NEG_wwASRi(Wd,Wm,i)       _W((0b01001011100 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | (0b11111 << 5) | (Wd))
#define NEG_xxASRi(Xd,Xm,i)       _W((0b11001011100 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | (0b11111 << 5) | (Xd))
#define NEGS_ww(Wd,Wm)            _W((0b01101011000 << 21) | ((Wm) << 16) | (0 << 10) | (0b11111 << 5) | (Wd))
#define NEGS_xx(Xd,Xm)            _W((0b11101011000 << 21) | ((Xm) << 16) | (0 << 10) | (0b11111 << 5) | (Xd))
#define NEGS_wwLSLi(Wd,Wm,i)      _W((0b01101011000 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | (0b11111 << 5) | (Wd))
#define NEGS_xxLSLi(Xd,Xm,i)      _W((0b11101011000 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | (0b11111 << 5) | (Xd))
#define NEGS_wwLSRi(Wd,Wm,i)      _W((0b01101011010 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | (0b11111 << 5) | (Wd))
#define NEGS_xxLSRi(Xd,Xm,i)      _W((0b11101011010 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | (0b11111 << 5) | (Xd))
#define NEGS_wwASRi(Wd,Wm,i)      _W((0b01101011100 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | (0b11111 << 5) | (Wd))
#define NEGS_xxASRi(Xd,Xm,i)      _W((0b11101011100 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | (0b11111 << 5) | (Xd))
#define NGC_ww(Wd,Wm)             _W((0b01011010000 << 21) | ((Wm) << 16) | (0 << 10) | (0b11111 << 5) | (Wd))
#define NGC_xx(Xd,Xm)             _W((0b11011010000 << 21) | ((Xm) << 16) | (0 << 10) | (0b11111 << 5) | (Xd))
#define NGCS_ww(Wd,Wm)            _W((0b01111010000 << 21) | ((Wm) << 16) | (0 << 10) | (0b11111 << 5) | (Wd))
#define NGCS_xx(Xd,Xm)            _W((0b11111010000 << 21) | ((Xm) << 16) | (0 << 10) | (0b11111 << 5) | (Xd))

/* SUB */
// Wd = Wn - Wm
#define SBC_www(Wd,Wn,Wm)         _W((0b01011010000 << 21) | ((Wm) << 16) | (0 << 10) | ((Wn) << 5) | (Wd))
#define SBC_xxx(Xd,Xn,Xm)         _W((0b11011010000 << 21) | ((Xm) << 16) | (0 << 10) | ((Xn) << 5) | (Xd))
#define SBCS_www(Wd,Wn,Wm)        _W((0b01111010000 << 21) | ((Wm) << 16) | (0 << 10) | ((Wn) << 5) | (Wd))
#define SBCS_xxx(Xd,Xn,Xm)        _W((0b11111010000 << 21) | ((Xm) << 16) | (0 << 10) | ((Xn) << 5) | (Xd))
#define SUB_wwwEX(Wd,Wn,Wm,ex)    _W((0b01001011001 << 21) | ((Wm) << 16) | ((ex) << 13) | (0 << 10) | ((Wn) << 5) | (Wd))
#define SUB_xxwEX(Xd,Xn,Wm,ex)    _W((0b11001011001 << 21) | ((Wm) << 16) | ((ex) << 13) | (0 << 10) | ((Xn) << 5) | (Xd))
#define SUB_wwi(Wd,Wn,i12)        _W((0b0101000100 << 22) | (((i12) & 0xfff) << 10) | ((Wn) << 5) | (Wd))
#define SUB_xxi(Xd,Xn,i12)        _W((0b1101000100 << 22) | (((i12) & 0xfff) << 10) | ((Xn) << 5) | (Xd))
// sh: 0 - no shift, 1 - LSL 12
#define SUB_wwish(Wd,Wn,i12,sh)   _W((0b010100010 << 23) | (((sh) & 1) << 22) | (((i12) & 0xfff) << 10) | ((Wn) << 5) | (Wd))
#define SUB_xxish(Xd,Xn,i12,sh)   _W((0b110100010 << 23) | (((sh) & 1) << 22) | (((i12) & 0xfff) << 10) | ((Xn) << 5) | (Xd))
#define SUB_www(Wd,Wn,Wm)         _W((0b01001011000 << 21) | ((Wm) << 16) | (0 << 10) | ((Wn) << 5) | (Wd))
#define SUB_xxx(Xd,Xn,Xm)         _W((0b11001011000 << 21) | ((Xm) << 16) | (0 << 10) | ((Xn) << 5) | (Xd))
#define SUB_wwwLSLi(Wd,Wn,Wm,i)   _W((0b01001011000 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define SUB_xxxLSLi(Xd,Xn,Xm,i)   _W((0b11001011000 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define SUB_wwwLSRi(Wd,Wn,Wm,i)   _W((0b01001011010 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define SUB_xxxLSRi(Xd,Xn,Xm,i)   _W((0b11001011010 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define SUB_wwwASRi(Wd,Wn,Wm,i)   _W((0b01001011100 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define SUB_xxxASRi(Xd,Xn,Xm,i)   _W((0b11001011100 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define SUBS_wwwEX(Wd,Wn,Wm,ex)   _W((0b01101011001 << 21) | ((Wm) << 16) | ((ex) << 13) | (0 << 10) | ((Wn) << 5) | (Wd))
#define SUBS_xxwEX(Xd,Xn,Wm,ex)   _W((0b11101011001 << 21) | ((Wm) << 16) | ((ex) << 13) | (0 << 10) | ((Xn) << 5) | (Xd))
#define SUBS_wwi(Wd,Wn,i12)       _W((0b0111000100 << 22) | (((i12) & 0xfff) << 10) | ((Wn) << 5) | (Wd))
#define SUBS_xxi(Xd,Xn,i12)       _W((0b1111000100 << 22) | (((i12) & 0xfff) << 10) | ((Xn) << 5) | (Xd))
// sh: 0 - no shift, 1 - LSL 12
#define SUBS_wwish(Wd,Wn,i12,sh)  _W((0b011100010 << 23) | (((sh) & 1) << 22) | (((i12) & 0xfff) << 10) | ((Wn) << 5) | (Wd))
#define SUBS_xxish(Xd,Xn,i12,sh)  _W((0b111100010 << 23) | (((sh) & 1) << 22) | (((i12) & 0xfff) << 10) | ((Xn) << 5) | (Xd))
#define SUBS_www(Wd,Wn,Wm)        _W((0b01101011000 << 21) | ((Wm) << 16) | (0 << 10) | ((Wn) << 5) | (Wd))
#define SUBS_xxx(Xd,Xn,Xm)        _W((0b11101011000 << 21) | ((Xm) << 16) | (0 << 10) | ((Xn) << 5) | (Xd))
#define SUBS_wwwLSLi(Wd,Wn,Wm,i)  _W((0b01101011000 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define SUBS_xxxLSLi(Xd,Xn,Xm,i)  _W((0b11101011000 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define SUBS_wwwLSRi(Wd,Wn,Wm,i)  _W((0b01101011010 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define SUBS_xxxLSRi(Xd,Xn,Xm,i)  _W((0b11101011010 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define SUBS_wwwASRi(Wd,Wn,Wm,i)  _W((0b01101011100 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define SUBS_xxxASRi(Xd,Xn,Xm,i)  _W((0b11101011100 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))

/* signed extend */
#define SBFM_wwii(Wd,Wn,immr,imms)  _W((0b0001001100 << 22) | ((immr) << 16) | ((imms) << 10) | ((Wn) << 5) | (Wd))
#define SBFM_xxii(Xd,Xn,immr,imms)  _W((0b1001001101 << 22) | ((immr) << 16) | ((imms) << 10) | ((Xn) << 5) | (Xd))
#define SXTB_ww(Wd,Wn)              SBFM_wwii(Wd,Wn,0,7)
#define SXTB_xx(Xd,Xn)              SBFM_xxii(Xd,Xn,0,7)
#define SXTH_ww(Wd,Wn)              SBFM_wwii(Wd,Wn,0,15)
#define SXTH_xx(Xd,Xn)              SBFM_xxii(Xd,Xn,0,15)
#define SXTW_xw(Xd,Wn)              SBFM_xxii(Xd,Wn,0,31)

/* unsigned extend */
#define UBFM_wwii(Wd,Wn,immr,imms)  _W((0b0101001100 << 22) | ((immr) << 16) | ((imms) << 10) | ((Wn) << 5) | (Wd))
#define UBFM_xxii(Xd,Xn,immr,imms)  _W((0b1101001101 << 22) | ((immr) << 16) | ((imms) << 10) | ((Xn) << 5) | (Xd))
#define UXTB_ww(Wd,Wn)              UBFM_wwii(Wd,Wn,0,7)
#define UXTB_xx(Xd,Xn)              UBFM_xxii(Xd,Xn,0,7)
#define UXTH_ww(Wd,Wn)              UBFM_wwii(Wd,Wn,0,15)
#define UXTH_xx(Xd,Xn)              UBFM_xxii(Xd,Xn,0,15)


/*----------------------------------------
 * logical
 *----------------------------------------*/
/* AND */
#define AND_www(Wd,Wn,Wm)         _W((0b00001010000 << 21) | ((Wm) << 16) | (0 << 10) | ((Wn) << 5) | (Wd))
#define AND_xxx(Xd,Xn,Xm)         _W((0b10001010000 << 21) | ((Xm) << 16) | (0 << 10) | ((Xn) << 5) | (Xd))
#define AND_wwwLSLi(Wd,Wn,Wm,i)   _W((0b00001010000 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define AND_xxxLSLi(Xd,Xn,Xm,i)   _W((0b10001010000 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define AND_wwwLSRi(Wd,Wn,Wm,i)   _W((0b00001010010 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define AND_xxxLSRi(Xd,Xn,Xm,i)   _W((0b10001010010 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define AND_wwwASRi(Wd,Wn,Wm,i)   _W((0b00001010100 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define AND_xxxASRi(Xd,Xn,Xm,i)   _W((0b10001010100 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define AND_wwwRORi(Wd,Wn,Wm,i)   _W((0b00001010110 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define AND_xxxRORi(Xd,Xn,Xm,i)   _W((0b10001010110 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define ANDS_www(Wd,Wn,Wm)        _W((0b01101010000 << 21) | ((Wm) << 16) | (0 << 10) | ((Wn) << 5) | (Wd))
#define ANDS_xxx(Xd,Xn,Xm)        _W((0b11101010000 << 21) | ((Xm) << 16) | (0 << 10) | ((Xn) << 5) | (Xd))
#define ANDS_wwwLSLi(Wd,Wn,Wm,i)  _W((0b01101010000 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define ANDS_xxxLSLi(Xd,Xn,Xm,i)  _W((0b11101010000 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define ANDS_wwwLSRi(Wd,Wn,Wm,i)  _W((0b01101010010 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define ANDS_xxxLSRi(Xd,Xn,Xm,i)  _W((0b11101010010 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define ANDS_wwwASRi(Wd,Wn,Wm,i)  _W((0b01101010100 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define ANDS_xxxASRi(Xd,Xn,Xm,i)  _W((0b11101010100 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define ANDS_wwwRORi(Wd,Wn,Wm,i)  _W((0b01101010110 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define ANDS_xxxRORi(Xd,Xn,Xm,i)  _W((0b11101010110 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))

/* EOR */
#define EON_www(Wd,Wn,Wm)         _W((0b01001010001 << 21) | ((Wm) << 16) | (0 << 10) | ((Wn) << 5) | (Wd))
#define EON_xxx(Xd,Xn,Xm)         _W((0b11001010001 << 21) | ((Xm) << 16) | (0 << 10) | ((Xn) << 5) | (Xd))
#define EON_wwwLSLi(Wd,Wn,Wm,i)   _W((0b01001010001 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define EON_xxxLSLi(Xd,Xn,Xm,i)   _W((0b11001010001 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define EON_wwwLSRi(Wd,Wn,Wm,i)   _W((0b01001010011 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define EON_xxxLSRi(Xd,Xn,Xm,i)   _W((0b11001010011 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define EON_wwwASRi(Wd,Wn,Wm,i)   _W((0b01001010101 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define EON_xxxASRi(Xd,Xn,Xm,i)   _W((0b11001010101 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define EON_wwwRORi(Wd,Wn,Wm,i)   _W((0b01001010111 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define EON_xxxRORi(Xd,Xn,Xm,i)   _W((0b11001010111 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define EOR_www(Wd,Wn,Wm)         _W((0b01001010000 << 21) | ((Wm) << 16) | (0 << 10) | ((Wn) << 5) | (Wd))
#define EOR_xxx(Xd,Xn,Xm)         _W((0b11001010000 << 21) | ((Xm) << 16) | (0 << 10) | ((Xn) << 5) | (Xd))
#define EOR_wwwLSLi(Wd,Wn,Wm,i)   _W((0b01001010000 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define EOR_xxxLSLi(Xd,Xn,Xm,i)   _W((0b11001010000 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define EOR_wwwLSRi(Wd,Wn,Wm,i)   _W((0b01001010010 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define EOR_xxxLSRi(Xd,Xn,Xm,i)   _W((0b11001010010 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define EOR_wwwASRi(Wd,Wn,Wm,i)   _W((0b01001010100 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define EOR_xxxASRi(Xd,Xn,Xm,i)   _W((0b11001010100 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define EOR_wwwRORi(Wd,Wn,Wm,i)   _W((0b01001010110 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define EOR_xxxRORi(Xd,Xn,Xm,i)   _W((0b11001010110 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))

/* ORR */
#define ORN_www(Wd,Wn,Wm)         _W((0b00101010001 << 21) | ((Wm) << 16) | (0 << 10) | ((Wn) << 5) | (Wd))
#define ORN_xxx(Xd,Xn,Xm)         _W((0b10101010001 << 21) | ((Xm) << 16) | (0 << 10) | ((Xn) << 5) | (Xd))
#define ORN_wwwLSLi(Wd,Wn,Wm,i)   _W((0b00101010001 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define ORN_xxxLSLi(Xd,Xn,Xm,i)   _W((0b10101010001 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define ORN_wwwLSRi(Wd,Wn,Wm,i)   _W((0b00101010011 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define ORN_xxxLSRi(Xd,Xn,Xm,i)   _W((0b10101010011 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define ORN_wwwASRi(Wd,Wn,Wm,i)   _W((0b00101010101 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define ORN_xxxASRi(Xd,Xn,Xm,i)   _W((0b10101010101 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define ORN_wwwRORi(Wd,Wn,Wm,i)   _W((0b00101010111 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define ORN_xxxRORi(Xd,Xn,Xm,i)   _W((0b10101010111 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define ORR_www(Wd,Wn,Wm)         _W((0b00101010000 << 21) | ((Wm) << 16) | (0 << 10) | ((Wn) << 5) | (Wd))
#define ORR_xxx(Xd,Xn,Xm)         _W((0b10101010000 << 21) | ((Xm) << 16) | (0 << 10) | ((Xn) << 5) | (Xd))
#define ORR_wwwLSLi(Wd,Wn,Wm,i)   _W((0b00101010000 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define ORR_xxxLSLi(Xd,Xn,Xm,i)   _W((0b10101010000 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define ORR_wwwLSRi(Wd,Wn,Wm,i)   _W((0b00101010010 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define ORR_xxxLSRi(Xd,Xn,Xm,i)   _W((0b10101010010 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define ORR_wwwASRi(Wd,Wn,Wm,i)   _W((0b00101010100 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define ORR_xxxASRi(Xd,Xn,Xm,i)   _W((0b10101010100 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define ORR_wwwRORi(Wd,Wn,Wm,i)   _W((0b00101010110 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (Wd))
#define ORR_xxxRORi(Xd,Xn,Xm,i)   _W((0b10101010110 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))

/* TST */
#define TST_ww(Wn,Wm)             _W((0b01101010000 << 21) | ((Wm) << 16) | (0 << 10) | ((Wn) << 5) | (0b11111))
#define TST_xx(Xn,Xm)             _W((0b11101010000 << 21) | ((Xm) << 16) | (0 << 10) | ((Xn) << 5) | (0b11111))
#define TST_wwLSLi(Wn,Wm,i)       _W((0b01101010000 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (0b11111))
#define TST_xxLSLi(Xn,Xm,i)       _W((0b11101010000 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (0b11111))
#define TST_wwLSRi(Wn,Wm,i)       _W((0b01101010010 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (0b11111))
#define TST_xxLSRi(Xn,Xm,i)       _W((0b11101010010 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (0b11111))
#define TST_wwASRi(Wn,Wm,i)       _W((0b01101010100 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (0b11111))
#define TST_xxASRi(Xn,Xm,i)       _W((0b11101010100 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (0b11111))
#define TST_wwRORi(Wn,Wm,i)       _W((0b01101010110 << 21) | ((Wm) << 16) | (((i) & 0x3f) << 10) | ((Wn) << 5) | (0b11111))
#define TST_xxRORi(Xn,Xm,i)       _W((0b11101010110 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (0b11111))


/*----------------------------------------
 * shift
 *----------------------------------------*/
#define ASR_www(Wd,Wn,Wm)         _W((0b00011010110 << 21) | ((Wm) << 16) | (0b001010 << 10) | ((Wn) << 5) | (Wd))
#define ASR_xxx(Xd,Xn,Xm)         _W((0b10011010110 << 21) | ((Xm) << 16) | (0b001010 << 10) | ((Xn) << 5) | (Xd))
#define ASR_wwi(Wd,Wn,i)          _W((0b0001001100 << 22) | (((i) & 0x1f) << 16) | (0b011111 << 10) | ((Wn) << 5) | (Wd))
#define ASR_xxi(Xd,Xn,i)          _W((0b1001001101 << 22) | (((i) & 0x3f) << 16) | (0b111111 << 10) | ((Xn) << 5) | (Xd))
#define LSL_www(Wd,Wn,Wm)         _W((0b00011010110 << 21) | ((Wm) << 16) | (0b001000 << 10) | ((Wn) << 5) | (Wd))
#define LSL_xxx(Xd,Xn,Xm)         _W((0b10011010110 << 21) | ((Xm) << 16) | (0b001000 << 10) | ((Xn) << 5) | (Xd))
#define LSL_wwi(Wd,Wn,i)          _W((0b0101001100 << 22) | (((32-(i)) & 0x1f) << 16) | (((31-(i)) & 0x1f) << 10) | ((Wn) << 5) | (Wd))
#define LSL_xxi(Xd,Xn,i)          _W((0b1101001101 << 22) | (((64-(i)) & 0x3f) << 16) | (((63-(i)) & 0x3f) << 10) | ((Xn) << 5) | (Xd))
#define LSR_www(Wd,Wn,Wm)         _W((0b00011010110 << 21) | ((Wm) << 16) | (0b001001 << 10) | ((Wn) << 5) | (Wd))
#define LSR_xxx(Xd,Xn,Xm)         _W((0b10011010110 << 21) | ((Xm) << 16) | (0b001001 << 10) | ((Xn) << 5) | (Xd))
#define LSR_wwi(Wd,Wn,i)          _W((0b0101001100 << 22) | (((i) & 0x1f) << 16) | (0b011111 << 10) | ((Wn) << 5) | (Wd))
#define LSR_xxi(Xd,Xn,i)          _W((0b1101001101 << 22) | (((i) & 0x3f) << 16) | (0b111111 << 10) | ((Xn) << 5) | (Xd))
#define ROR_wwi(Wd,Ws,i)          _W((0b00010011100 << 21) | ((Ws) << 16) | (((i) & 0x1f) << 10) | ((Ws) << 5) | (Wd))
#define ROR_xxi(Xd,Xs,i)          _W((0b10010011110 << 21) | ((Xs) << 16) | (((i) & 0x3f) << 10) | ((Xs) << 5) | (Xd))
#define ROR_www(Wd,Wn,Wm)         _W((0b00011010110 << 21) | ((Wm) << 16) | (0b001011 << 10) | ((Wn) << 5) | (Wd))
#define ROR_xxx(Xd,Xn,Xm)         _W((0b10011010110 << 21) | ((Xm) << 16) | (0b001011 << 10) | ((Xn) << 5) | (Xd))


/*----------------------------------------
 * bit ops
 *----------------------------------------*/
/* extract */
// Wd[31...0] = Wn[31-lsb...0]Wm[31...lsb]
// Xd[63...0] = Xn[63-lsb...0]Xm[63...lsb]
#define EXTR_wwwi(Wd,Wn,Wm,lsb)   _W((0b00010011100 << 21) | ((Wm) << 16) | (((lsb) & 0x1f) << 10) | ((Wn) << 5) | (Wd))
#define EXTR_xxxi(Xd,Xn,Xm,lsb)   _W((0b10010011110 << 21) | ((Xm) << 16) | (((lsb) & 0x3f) << 10) | ((Xn) << 5) | (Xd))

/* bitfield */
// BFC requires ARMv8.2
//#define BFC_wii(Wd,lsb,width)     _W((0b0011001100 << 22) | (((32-lsb)&0x1f) << 16) | (((width-1)&0x1f) << 10) | (0b11111 << 5) | (Wd))
//#define BFC_xii(Xd,lsb,width)     _W((0b1011001101 << 22) | (((64-lsb)&0x3f) << 16) | (((width-1)&0x3f) << 10) | (0b11111 << 5) | (Xd))

// dst[lsb+width-1...lsb] = src[width-1...0]
#define BFI_wwii(Wd,Wn,lsb,width)     _W((0b0011001100 << 22) | (((32-(lsb))&0x1f) << 16) | ((((width)-1)&0x1f) << 10) | ((Wn) << 5) | (Wd))
#define BFI_xxii(Xd,Xn,lsb,width)     _W((0b1011001101 << 22) | (((64-(lsb))&0x3f) << 16) | ((((width)-1)&0x3f) << 10) | ((Xn) << 5) | (Xd))
#define SBFIZ_wwii(Wd,Wn,lsb,width)   _W((0b0001001100 << 22) | (((32-(lsb))&0x1f) << 16) | ((((width)-1)&0x1f) << 10) | ((Wn) << 5) | (Wd))
#define SBFIZ_xxii(Xd,Xn,lsb,width)   _W((0b1001001101 << 22) | (((64-(lsb))&0x3f) << 16) | ((((width)-1)&0x3f) << 10) | ((Xn) << 5) | (Xd))
#define UBFIZ_wwii(Wd,Wn,lsb,width)   _W((0b0101001100 << 22) | (((32-(lsb))&0x1f) << 16) | ((((width)-1)&0x1f) << 10) | ((Wn) << 5) | (Wd))
#define UBFIZ_xxii(Xd,Xn,lsb,width)   _W((0b1101001101 << 22) | (((64-(lsb))&0x3f) << 16) | ((((width)-1)&0x3f) << 10) | ((Xn) << 5) | (Xd))

// dst[width-1...0] = src[lsb+width-1...lsb]
#define BFXIL_wwii(Wd,Wn,lsb,width)   _W((0b0011001100 << 22) | (((lsb)&0x1f) << 16) | ((((lsb)+(width)-1)&0x3f) << 10) | ((Wn) << 5) | (Wd))
#define BFXIL_xxii(Xd,Xn,lsb,width)   _W((0b1011001101 << 22) | (((lsb)&0x3f) << 16) | ((((lsb)+(width)-1)&0x3f) << 10) | ((Xn) << 5) | (Xd))
#define SBFX_wwii(Wd,Wn,lsb,width)    _W((0b0001001100 << 22) | (((lsb)&0x1f) << 16) | ((((lsb)+(width)-1)&0x3f) << 10) | ((Wn) << 5) | (Wd))
#define SBFX_xxii(Xd,Xn,lsb,width)    _W((0b1001001101 << 22) | (((lsb)&0x3f) << 16) | ((((lsb)+(width)-1)&0x3f) << 10) | ((Xn) << 5) | (Xd))
#define UBFX_wwii(Wd,Wn,lsb,width)    _W((0b0101001100 << 22) | (((lsb)&0x1f) << 16) | ((((lsb)+(width)-1)&0x3f) << 10) | ((Wn) << 5) | (Wd))
#define UBFX_xxii(Xd,Xn,lsb,width)    _W((0b1101001101 << 22) | (((lsb)&0x3f) << 16) | ((((lsb)+(width)-1)&0x3f) << 10) | ((Xn) << 5) | (Xd))

#define BIC_www(Wd,Wn,Wm)         _W((0b00001010001 << 21) | ((Wm) << 16) | (0 << 10) | ((Wn) << 5) | (Wd))
#define BIC_xxx(Xd,Xn,Xm)         _W((0b10001010001 << 21) | ((Xm) << 16) | (0 << 10) | ((Xn) << 5) | (Xd))
#define BIC_wwwLSLi(Wd,Wn,Wm,i)   _W((0b00001010001 << 21) | ((Wm) << 16) | ((((i)&0x3f) << 10) | ((Wn) << 5) | (Wd))
#define BIC_xxxLSLi(Xd,Xn,Xm,i)   _W((0b10001010001 << 21) | ((Xm) << 16) | ((((i)&0x3f) << 10) | ((Xn) << 5) | (Xd))
#define BIC_wwwLSRi(Wd,Wn,Wm,i)   _W((0b00001010011 << 21) | ((Wm) << 16) | ((((i)&0x3f) << 10) | ((Wn) << 5) | (Wd))
#define BIC_xxxLSRi(Xd,Xn,Xm,i)   _W((0b10001010011 << 21) | ((Xm) << 16) | ((((i)&0x3f) << 10) | ((Xn) << 5) | (Xd))
#define BIC_wwwASRi(Wd,Wn,Wm,i)   _W((0b00001010101 << 21) | ((Wm) << 16) | ((((i)&0x3f) << 10) | ((Wn) << 5) | (Wd))
#define BIC_xxxASRi(Xd,Xn,Xm,i)   _W((0b10001010101 << 21) | ((Xm) << 16) | ((((i)&0x3f) << 10) | ((Xn) << 5) | (Xd))
#define BIC_wwwRORi(Wd,Wn,Wm,i)   _W((0b00001010111 << 21) | ((Wm) << 16) | ((((i)&0x3f) << 10) | ((Wn) << 5) | (Wd))
#define BIC_xxxRORi(Xd,Xn,Xm,i)   _W((0b10001010111 << 21) | ((Xm) << 16) | ((((i)&0x3f) << 10) | ((Xn) << 5) | (Xd))
#define BICS_www(Wd,Wn,Wm)        _W((0b01101010001 << 21) | ((Wm) << 16) | (0 << 10) | ((Wn) << 5) | (Wd))
#define BICS_xxx(Xd,Xn,Xm)        _W((0b11101010001 << 21) | ((Xm) << 16) | (0 << 10) | ((Xn) << 5) | (Xd))
#define BICS_wwwLSLi(Wd,Wn,Wm,i)  _W((0b01101010001 << 21) | ((Wm) << 16) | ((((i)&0x3f) << 10) | ((Wn) << 5) | (Wd))
#define BICS_xxxLSLi(Xd,Xn,Xm,i)  _W((0b11101010001 << 21) | ((Xm) << 16) | ((((i)&0x3f) << 10) | ((Xn) << 5) | (Xd))
#define BICS_wwwLSRi(Wd,Wn,Wm,i)  _W((0b01101010011 << 21) | ((Wm) << 16) | ((((i)&0x3f) << 10) | ((Wn) << 5) | (Wd))
#define BICS_xxxLSRi(Xd,Xn,Xm,i)  _W((0b11101010011 << 21) | ((Xm) << 16) | ((((i)&0x3f) << 10) | ((Xn) << 5) | (Xd))
#define BICS_wwwASRi(Wd,Wn,Wm,i)  _W((0b01101010101 << 21) | ((Wm) << 16) | ((((i)&0x3f) << 10) | ((Wn) << 5) | (Wd))
#define BICS_xxxASRi(Xd,Xn,Xm,i)  _W((0b11101010101 << 21) | ((Xm) << 16) | ((((i)&0x3f) << 10) | ((Xn) << 5) | (Xd))
#define BICS_wwwRORi(Wd,Wn,Wm,i)  _W((0b01101010111 << 21) | ((Wm) << 16) | ((((i)&0x3f) << 10) | ((Wn) << 5) | (Wd))
#define BICS_xxxRORi(Xd,Xn,Xm,i)  _W((0b11101010111 << 21) | ((Xm) << 16) | ((((i)&0x3f) << 10) | ((Xn) << 5) | (Xd))

/* reverse */
#define RBIT_ww(Wd,Wn)            _W((0b01011010110 << 21) | (0b00000 << 16) | (0b000000 << 10) | ((Wn) << 5) | (Wd))
#define RBIT_xx(Xd,Xn)            _W((0b11011010110 << 21) | (0b00000 << 16) | (0b000000 << 10) | ((Xn) << 5) | (Xd))
#define REV_ww(Wd,Wn)             _W((0b01011010110 << 21) | (0b00000 << 16) | (0b000010 << 10) | ((Wn) << 5) | (Wd))
#define REV_xx(Xd,Xn)             _W((0b11011010110 << 21) | (0b00000 << 16) | (0b000011 << 10) | ((Xn) << 5) | (Xd))
#define REV16_ww(Wd,Wn)           _W((0b01011010110 << 21) | (0b00000 << 16) | (0b000001 << 10) | ((Wn) << 5) | (Wd))
#define REV16_xx(Xd,Xn)           _W((0b11011010110 << 21) | (0b00000 << 16) | (0b000001 << 10) | ((Xn) << 5) | (Xd))
#define REV32_xx(Xd,Xn)           _W((0b11011010110 << 21) | (0b00000 << 16) | (0b000010 << 10) | ((Xn) << 5) | (Xd))


/*----------------------------------------
 * conditional ops
 *----------------------------------------*/
/* conditional compare */
#define CCMN_wifc(Wn,i5,nzcv,cond)  _W((0b00111010010 << 21) | (((i5) & 0x1f) << 16) | ((cond) << 12) | (0b10 << 10) | ((Wn) << 5) | (nzcv))
#define CCMN_xifc(Xn,i5,nzcv,cond)  _W((0b10111010010 << 21) | (((i5) & 0x1f) << 16) | ((cond) << 12) | (0b10 << 10) | ((Xn) << 5) | (nzcv))
#define CCMN_wwfc(Wn,Wm,nzcv,cond)  _W((0b00111010010 << 21) | ((Wm) << 16) | ((cond) << 12) | (0b00 << 10) | ((Wn) << 5) | (nzcv))
#define CCMN_xxfc(Xn,Xm,nzcv,cond)  _W((0b10111010010 << 21) | ((Xm) << 16) | ((cond) << 12) | (0b00 << 10) | ((Xn) << 5) | (nzcv))
#define CCMP_wifc(Wn,i5,nzcv,cond)  _W((0b01111010010 << 21) | (((i5) & 0x1f) << 16) | ((cond) << 12) | (0b10 << 10) | ((Wn) << 5) | (nzcv))
#define CCMP_xifc(Xn,i5,nzcv,cond)  _W((0b11111010010 << 21) | (((i5) & 0x1f) << 16) | ((cond) << 12) | (0b10 << 10) | ((Xn) << 5) | (nzcv))
#define CCMP_wwfc(Wn,Wm,nzcv,cond)  _W((0b01111010010 << 21) | ((Wm) << 16) | ((cond) << 12) | (0b00 << 10) | ((Wn) << 5) | (nzcv))
#define CCMP_xxfc(Xn,Xm,nzcv,cond)  _W((0b11111010010 << 21) | ((Xm) << 16) | ((cond) << 12) | (0b00 << 10) | ((Xn) << 5) | (nzcv))

/* conditionl increment */
#define CINC_wwc(Wd,Wn,cond)      _W((0b00011010100 << 21) | ((Wn) << 16) | ((cond) << 12) | (0b01 << 10) | ((Wn) << 5) | (Wd))
#define CINC_xxc(Xd,Xn,cond)      _W((0b10011010100 << 21) | ((Xn) << 16) | ((cond) << 12) | (0b01 << 10) | ((Xn) << 5) | (Xd))

/* conditional invert */
#define CINV_wwc(Wd,Wn,cond)      _W((0b01011010100 << 21) | ((Wn) << 16) | ((cond) << 12) | (0b00 << 10) | ((Wn) << 5) | (Wd))
#define CINV_xxc(Xd,Xn,cond)      _W((0b11011010100 << 21) | ((Xn) << 16) | ((cond) << 12) | (0b00 << 10) | ((Xn) << 5) | (Xd))

/* conditional negate */
#define CNEG_wwc(Wd,Wn,cond)      _W((0b01011010100 << 21) | ((Wn) << 16) | ((cond) << 12) | (0b01 << 10) | ((Wn) << 5) | (Wd))
#define CNEG_xxc(Xd,Xn,cond)      _W((0b11011010100 << 21) | ((Xn) << 16) | ((cond) << 12) | (0b01 << 10) | ((Xn) << 5) | (Xd))

/* conditional select */
#define CSEL_wwwc(Wd,Wn,Wm,cond)  _W((0b00011010100 << 21) | ((Wm) << 16) | ((cond) << 12) | (0b00 << 10) | ((Wn) << 5) | (Wd))
#define CSEL_xxxc(Xd,Xn,Xm,cond)  _W((0b10011010100 << 21) | ((Xm) << 16) | ((cond) << 12) | (0b00 << 10) | ((Xn) << 5) | (Xd))

/* conditional set */
#define CSET_wc(Wd,cond)          _W((0b00011010100 << 21) | (0b11111 << 16) | ((cond) << 12) | (0b01 << 10) | (0b11111 << 5) | (Wd))
#define CSET_xc(Xd,cond)          _W((0b10011010100 << 21) | (0b11111 << 16) | ((cond) << 12) | (0b01 << 10) | (0b11111 << 5) | (Xd))

/* conditional set mask */
#define CSETM_wc(Wd,cond)         _W((0b01011010100 << 21) | (0b11111 << 16) | ((cond) << 12) | (0b00 << 10) | (0b11111 << 5) | (Wd))
#define CSETM_xc(Xd,cond)         _W((0b11011010100 << 21) | (0b11111 << 16) | ((cond) << 12) | (0b00 << 10) | (0b11111 << 5) | (Xd))

/* conditional select inc */
#define CSINC_wwwc(Wd,Wn,Wm,cond) _W((0b00011010100 << 21) | ((Wm) << 16) | ((cond) << 12) | (0b01 << 10) | ((Wn) << 5) | (Wd))
#define CSINC_xxxc(Xd,Xn,Xm,cond) _W((0b10011010100 << 21) | ((Xm) << 16) | ((cond) << 12) | (0b01 << 10) | ((Xn) << 5) | (Xd))

/* coditional select invert */
#define CSINV_wwwc(Wd,Wn,Wm,cond) _W((0b01011010100 << 21) | ((Wm) << 16) | ((cond) << 12) | (0b00 << 10) | ((Wn) << 5) | (Wd))
#define CSINV_xxxc(Xd,Xn,Xm,cond) _W((0b11011010100 << 21) | ((Xm) << 16) | ((cond) << 12) | (0b00 << 10) | ((Xn) << 5) | (Xd))

/* conditional select neg */
#define CSNEG_wwwc(Wd,Wn,Wm,cond) _W((0b01011010100 << 21) | ((Wm) << 16) | ((cond) << 12) | (0b01 << 10) | ((Wn) << 5) | (Wd))
#define CSNEG_xxxc(Xd,Xn,Xm,cond) _W((0b11011010100 << 21) | ((Xm) << 16) | ((cond) << 12) | (0b01 << 10) | ((Xn) << 5) | (Xd))


/*----------------------------------------
 * some special defines
 *----------------------------------------*/
// immediate encoding for flags:
//   N  N=1 immr=100001 imms=000000
//   Z  N=1 immr=100010 imms=000000
//   C  N=1 immr=100011 imms=000000
//   V  N=1 immr=100100 imms=000000
//  ~N  N=1 immr=100000 imms=111110
//  ~Z  N=1 immr=100001 imms=111110
//  ~C  N=1 immr=100010 imms=111110
//  ~V  N=1 immr=100011 imms=111110
#define immEncode(N,immr,imms)    (N << 22) | (immr << 16) | (imms << 10)
#define immNflag                  immEncode(1, 0b100001, 0b000000)
#define immZflag                  immEncode(1, 0b100010, 0b000000)
#define immCflag                  immEncode(1, 0b100011, 0b000000)
#define immVflag                  immEncode(1, 0b100100, 0b000000)

#define immNflagInv               immEncode(1, 0b100000, 0b111110)
#define immZflagInv               immEncode(1, 0b100001, 0b111110)
#define immCflagInv               immEncode(1, 0b100010, 0b111110)
#define immVflagInv               immEncode(1, 0b100011, 0b111110)

#define immOP_EOR                 (0b110100100 << 23)
#define immOP_AND                 (0b100100100 << 23)
#define immOP_ORR                 (0b101100100 << 23)

// R_MEMSTART is in 32bit address space and never 0, so CCMN(R_MEMSTART, #0, #0, #<any cond>) will always clear NZCV
#define CLEAR_NZCV()              _W((0b10111010010 << 21) | (0 << 16) | (NATIVE_CC_EQ << 12) | (0b10 << 10) | (R_MEMSTART << 5) | (0))

#define EOR_xxCflag(Xd,Xn)        _W(immCflag | immOP_EOR | ((Xn) << 5) | (Xd))
#define CLEAR_xxZflag(Xd,Xn)      _W(immZflagInv | immOP_AND | ((Xn) << 5) | (Xd))
#define SET_xxZflag(Xd,Xn)        _W(immZflag | immOP_ORR | ((Xn) << 5) | (Xd))

#define EOR_xxbit(Xd,Xn,bit)      _W(immOP_EOR | immEncode(1, ((-(bit)) & 0x3f), 0b000000) | ((Xn) << 5) | (Xd))
#define CLEAR_xxbit(Xd,Xn,bit)    _W(immOP_AND | immEncode(1, ((-((bit)+1)) & 0x3f), 0b111110) | ((Xn) << 5) | (Xd))
#define SET_xxbit(Xd,Xn,bit)      _W(immOP_ORR | immEncode(1, ((-(bit)) & 0x3f), 0b000000) | ((Xn) << 5) | (Xd))

#define CLEAR_LOW8_xx(Xd,Xn)      _W(immOP_AND | immEncode(1, 0b111000, 0b110111) | ((Xn) << 5) | (Xd))
#define CLEAR_LOW16_xx(Xd,Xn)     _W(immOP_AND | immEncode(1, 0b110000, 0b101111) | ((Xn) << 5) | (Xd))

#endif /* ARM_ASMA64_H */
