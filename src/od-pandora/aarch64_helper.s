// Some functions and tests to increase performance in drawing.cpp and custom.cpp

//.arm

.global save_host_fp_regs
.global restore_host_fp_regs
.global copy_screen_8bit
.global copy_screen_16bit_swap
.global copy_screen_32bit_to_16bit

.text

.align 8

//----------------------------------------------------------------
// save_host_fp_regs
//----------------------------------------------------------------
save_host_fp_regs:
  st4  {v7.2D-v10.2D}, [x0], #64
  st4  {v11.2D-v14.2D}, [x0], #64
  st1  {v25.2D}, [x0], #16
  ret
  
//----------------------------------------------------------------
// restore_host_fp_regs
//----------------------------------------------------------------
restore_host_fp_regs:
  ld4  {v7.2D-v10.2D}, [x0], #64
  ld4  {v11.2D-v14.2D}, [x0], #64
  ld1  {v25.2D}, [x0], #16
  ret


//----------------------------------------------------------------
// copy_screen_8bit
//
// r0: uae_u8   *dst
// r1: uae_u8   *src
// r2: int      bytes always a multiple of 64: even number of lines, number of pixel per line is multiple of 32 (320, 640, 800, 1024, 1152, 1280)
// r3: uae_u32  *clut
//
// void copy_screen_8bit(uae_u8 *dst, uae_u8 *src, int bytes, uae_u32 *clut);
//
//----------------------------------------------------------------
copy_screen_8bit:
copy_screen_8bit_loop:
  mov       x7, #64
copy_screen_8bit_loop_2:
  ldrsw     x4, [x1], #4
  and       x5, x4, #255
  ldrsw     x6, [x3, x5, lsl #2]
  lsr       x5, x4, #8
  and       x5, x5, #255
  strh      w6, [x0], #2
  ldrsw     x6, [x3, x5, lsl #2]
  lsr       x5, x4, #16
  and       x5, x5, #255
  strh      w6, [x0], #2
  ldrsw     x6, [x3, x5, lsl #2]
  lsr       x5, x4, #24
  strh      w6, [x0], #2
  ldrsw     x6, [x3, x5, lsl #2]
  subs      x7, x7, #4
  strh      w6, [x0], #2
  bgt       copy_screen_8bit_loop_2
  subs      x2, x2, #64
  bgt       copy_screen_8bit_loop
  ret
  

//----------------------------------------------------------------
// copy_screen_16bit_swap
//
// r0: uae_u8   *dst
// r1: uae_u8   *src
// r2: int      bytes always a multiple of 128: even number of lines, 2 bytes per pixel, number of pixel per line is multiple of 32 (320, 640, 800, 1024, 1152, 1280)
//
// void copy_screen_16bit_swap(uae_u8 *dst, uae_u8 *src, int bytes);
//
//----------------------------------------------------------------
copy_screen_16bit_swap:
  ldrsw     x3, [x1], #4
  rev16     w3, w3
  str       w3, [x0], #4
  subs      w2, w2, #4
  bne copy_screen_16bit_swap
  ret
  

//----------------------------------------------------------------
// copy_screen_32bit_to_16bit
//
// r0: uae_u8   *dst - Format (bits): rrrr rggg gggb bbbb
// r1: uae_u8   *src - Format (bytes) in memory rgba
// r2: int      bytes
//
// void copy_screen_32bit_to_16bit(uae_u8 *dst, uae_u8 *src, int bytes);
//
//----------------------------------------------------------------
copy_screen_32bit_to_16bit:
  ldrsw  x3, [x1], #4
  rev    w3, w3
  lsr    w4, w3, #27
  lsr    w5, w3, #18
  and    w5, w5, #63
  lsr    w6, w3, #11
  and    w6, w6, #31
  orr    w6, w6, w5, lsl #5
  orr    w6, w6, w4, lsl #11
  strh   w6, [x0], #2
  subs   w2, w2, #4
  ret
