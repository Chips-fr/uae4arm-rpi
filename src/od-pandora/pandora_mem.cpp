#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "uae.h"
#include "options.h"
#include "gui.h"
#include "memory.h"
#include "newcpu.h"
#include "custom.h"
#include "autoconf.h"
#include "akiko.h"
#include "ar.h"
#include <sys/mman.h>
#include <SDL.h>


uae_u8* natmem_offset = 0;
static uae_u32 natmem_size;
uae_u32 max_z3fastmem;

/* JIT can access few bytes outside of memory block of it executes code at the very end of memory block */
#define BARRIER 32

static uae_u8* additional_mem = (uae_u8*) MAP_FAILED;
#define ADDITIONAL_MEMSIZE (128 + 16) * 1024 * 1024

static uae_u8* a3000_mem = (uae_u8*) MAP_FAILED;
static int a3000_totalsize = 0;
#define A3000MEM_START 0x08000000


int z3base_adr = 0;


void free_AmigaMem(void)
{
  if(natmem_offset != 0)
  {
    free(natmem_offset);
    natmem_offset = 0;
  }
  if(additional_mem != MAP_FAILED)
  {
    munmap(additional_mem, ADDITIONAL_MEMSIZE + BARRIER);
    additional_mem = (uae_u8*) MAP_FAILED;
  }
  if(a3000_mem != MAP_FAILED)
  {
    munmap(a3000_mem, a3000_totalsize);
    a3000_mem = (uae_u8*) MAP_FAILED;
    a3000_totalsize = 0;
  }
}


void alloc_AmigaMem(void)
{
	int i;
	uae_u64 total;
	int max_allowed_mman;

  free_AmigaMem();
	set_expamem_z3_hack_override(false);

  // First attempt: allocate 16 MB for all memory in 24-bit area 
  // and additional mem for Z3 and RTG at correct offset
  natmem_size = 16 * 1024 * 1024;
  natmem_offset = (uae_u8*)valloc (natmem_size + BARRIER);
  max_z3fastmem = ADDITIONAL_MEMSIZE - (16 * 1024 * 1024);
	if (!natmem_offset) {
		write_log("Can't allocate 16M of virtual address space!?\n");
    abort();
	}
  additional_mem = (uae_u8*) mmap(natmem_offset + Z3BASE_REAL, ADDITIONAL_MEMSIZE + BARRIER,
    PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  if(additional_mem != MAP_FAILED)
  {
    // Allocation successful -> we can use natmem_offset for entire memory access at real address
    changed_prefs.z3autoconfig_start = currprefs.z3autoconfig_start = Z3BASE_REAL;
    z3base_adr = Z3BASE_REAL;
    write_log("Allocated 16 MB for 24-bit area and %d MB for Z3 and RTG at real address\n", ADDITIONAL_MEMSIZE / (1024 * 1024));
    set_expamem_z3_hack_override(true);
    return;
  }

  additional_mem = (uae_u8*) mmap(natmem_offset + Z3BASE_UAE, ADDITIONAL_MEMSIZE + BARRIER,
    PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  if(additional_mem != MAP_FAILED)
  {
    // Allocation successful -> we can use natmem_offset for entire memory access at fake address
    changed_prefs.z3autoconfig_start = currprefs.z3autoconfig_start = Z3BASE_UAE;
    z3base_adr = Z3BASE_UAE;
    write_log("Allocated 16 MB for 24-bit area and %d MB for Z3 and RTG at fake address\n", ADDITIONAL_MEMSIZE / (1024 * 1024));
    return;
  }
  free(natmem_offset);
  
  // Next attempt: allocate huge memory block for entire area
  natmem_size = ADDITIONAL_MEMSIZE + 256 * 1024 * 1024;
  natmem_offset = (uae_u8*)valloc (natmem_size + BARRIER);
  if(natmem_offset)
  {
    // Allocation successful
    changed_prefs.z3autoconfig_start = currprefs.z3autoconfig_start = Z3BASE_UAE;
    z3base_adr = Z3BASE_UAE;
    write_log("Allocated %d MB for entire memory\n", natmem_size / (1024 * 1024));
    return;
  }

  // No mem for Z3 or RTG at all
	natmem_size = 16 * 1024 * 1024;
	natmem_offset = (uae_u8*)valloc (natmem_size + BARRIER);

	if (!natmem_offset) {
		write_log("Can't allocate 16M of virtual address space!?\n");
    abort();
	}

  changed_prefs.z3autoconfig_start = currprefs.z3autoconfig_start = 0x00000000; // No mem for Z3
  z3base_adr = 0x00000000;
  max_z3fastmem = 0;

	write_log("Reserved: %p-%p (0x%08x %dM)\n", natmem_offset, (uae_u8*)natmem_offset + natmem_size,
		natmem_size, natmem_size >> 20);
}


bool HandleA3000Mem(int lowsize, int highsize)
{
  bool result = true;
  
  if(a3000_mem != MAP_FAILED)
  {
    munmap(a3000_mem, a3000_totalsize);
    a3000_mem = (uae_u8*) MAP_FAILED;
    a3000_totalsize = 0;
  }
  if(lowsize + highsize > 0)
  {
    // Try to get memory for A3000 motherboard
    write_log("Try to get A3000 memory at correct place (0x%08x). %d MB and %d MB.\n", A3000MEM_START, 
      lowsize / (1024 * 1024), highsize / (1024 * 1024));
    a3000_totalsize = lowsize + highsize;
    a3000_mem = (uae_u8*) mmap(natmem_offset + (A3000MEM_START - lowsize), a3000_totalsize,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if(a3000_mem != MAP_FAILED)
    {
      write_log("Succeeded!\n");
    }
    else
    {
      write_log("Failed.\n");
      a3000_totalsize = 0;
      result = false;
    }
  }

  return result;
}


bool A3000MemAvailable(void)
{
  return (a3000_mem != MAP_FAILED);
}


static uae_u32 getz2rtgaddr (int rtgsize)
{
	uae_u32 start;
	start = currprefs.fastmem_size;
	start += rtgsize - 1;
	start &= ~(rtgsize - 1);
	while (start & (currprefs.rtgboards[0].rtgmem_size - 1) && start < 4 * 1024 * 1024)
		start += 1024 * 1024;
	return start + 2 * 1024 * 1024;
}


bool mapped_malloc (addrbank *ab)
{
  ab->startmask = ab->start;
  ab->flags |= ABFLAG_DIRECTMAP;
  
  if(ab->label != NULL) {
    if(!strcmp(ab->label, "chip"))
      ab->baseaddr = natmem_offset + chipmem_start_addr;

    if(!strcmp(ab->label, "fast"))
      ab->baseaddr = natmem_offset + 0x200000;

    if(!strcmp(ab->label, "bogo"))
      ab->baseaddr = natmem_offset + bogomem_start_addr;

    if(!strcmp(ab->label, "custmem1"))
      ab->baseaddr = natmem_offset + currprefs.custom_memory_addrs[0];

    if(!strcmp(ab->label, "custmem2"))
      ab->baseaddr = natmem_offset + currprefs.custom_memory_addrs[1];

    if(!strcmp(ab->label, "rom_f0"))
      ab->baseaddr = natmem_offset + 0xf00000;
      
    if(!strcmp(ab->label, "rom_e0"))
      ab->baseaddr = natmem_offset + 0xe00000;

    if(!strcmp(ab->label, "rom_a8"))
      ab->baseaddr = natmem_offset + 0xa80000;

    if(!strcmp(ab->label, "kick"))
      ab->baseaddr = natmem_offset + kickmem_start_addr;

#ifdef PICASSO96
    if(!strcmp(ab->label, "z3"))
      ab->baseaddr = natmem_offset + z3fastmem_bank.start;

    if(!strcmp(ab->label, "z3_gfx")) {
      gfxmem_bank.start = z3base_adr + currprefs.z3fastmem_size;
      ab->baseaddr = natmem_offset + gfxmem_bank.start;
    }

    if(!strcmp(ab->label, "z2_gfx")) {
      gfxmem_bank.start = getz2rtgaddr(currprefs.rtgboards[0].rtgmem_size);
      ab->baseaddr = natmem_offset + gfxmem_bank.start;
    }
#endif

    if(!strcmp(ab->label, "rtarea"))
      ab->baseaddr = natmem_offset + rtarea_base;

    if(!strcmp(ab->label, "hrtmem") || !strcmp(ab->label, "xpower_e2") || !strcmp(ab->label, "nordic_f0") || !strcmp(ab->label, "superiv_d0"))
      ab->baseaddr = natmem_offset + hrtmem_start;

    if(!strcmp(ab->label, "xpower_f2") || !strcmp(ab->label, "nordic_f4") || !strcmp(ab->label, "superiv_b0"))
      ab->baseaddr = natmem_offset + hrtmem2_start;

    if(!strcmp(ab->label, "nordic_f6") || !strcmp(ab->label, "superiv_e0"))
      ab->baseaddr = natmem_offset + hrtmem3_start;

    if(!strcmp(ab->label, "filesys"))
      ab->baseaddr = (uae_u8 *) malloc (0x10000 + 4);
      
    if(!strcmp(ab->label, "ramsey_low") && A3000MemAvailable())
      ab->baseaddr = natmem_offset + ab->start;
    
    if(!strcmp(ab->label, "ramsey_high") && A3000MemAvailable())
      ab->baseaddr = natmem_offset + ab->start;
      
    if(!strcmp(ab->label, "fmv_rom"))
      ab->baseaddr = natmem_offset + 0x200000;
      
    if(!strcmp(ab->label, "fmv_ram"))
      ab->baseaddr = natmem_offset + 0x280000;
  }
  return (ab->baseaddr != NULL);
}


void mapped_free (addrbank *ab)
{
  if(ab->label != NULL && !strcmp(ab->label, "filesys") && ab->baseaddr != NULL)
    free(ab->baseaddr);
  ab->baseaddr = NULL;
}


void protect_roms (bool protect)
{
/*
  If this code is enabled, we can't switch back from JIT to nonJIT emulation...
  
	if (protect) {
		// protect only if JIT enabled, always allow unprotect
		if (!currprefs.cachesize)
			return;
	}

  // Protect all regions, which contains ROM
  if(extendedkickmem_bank.baseaddr != NULL)
    mprotect(extendedkickmem_bank.baseaddr, 0x80000, protect ? PROT_READ : PROT_READ | PROT_WRITE);
  if(extendedkickmem2_bank.baseaddr != NULL)
    mprotect(extendedkickmem2_bank.baseaddr, 0x80000, protect ? PROT_READ : PROT_READ | PROT_WRITE);
  if(kickmem_bank.baseaddr != NULL)
    mprotect(kickmem_bank.baseaddr, 0x80000, protect ? PROT_READ : PROT_READ | PROT_WRITE);
  if(rtarea != NULL)
    mprotect(rtarea, RTAREA_SIZE, protect ? PROT_READ : PROT_READ | PROT_WRITE);
  if(filesysory != NULL)
    mprotect(filesysory, 0x10000, protect ? PROT_READ : PROT_READ | PROT_WRITE);
*/
}
