 /* 
  * UAE - The Un*x Amiga Emulator
  *
  * AutoConfig (tm) Expansions (ZorroII/III)
  *
  * Copyright 1996,1997 Stefan Reinauer <stepan@linux.de>
  * Copyright 1997 Brian King <Brian_King@Mitel.com>
  *   - added gfxcard code
  *
  */

#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "uae.h"
#include "traps.h"
#include "memory.h"
#include "rommgr.h"
#include "custom.h"
#include "newcpu.h"
#include "savestate.h"
#include "zfile.h"
#include "td-sdl/thread.h"
#include "gfxboard.h"
#include "cd32_fmv.h"
#include "autoconf.h"
#include "filesys.h"

// More information in first revision HRM Appendix_G
#define BOARD_PROTOAUTOCONFIG 1

#define BOARD_AUTOCONFIG_Z2 2
#define BOARD_AUTOCONFIG_Z3 3
#define BOARD_NONAUTOCONFIG_BEFORE 4
#define BOARD_NONAUTOCONFIG_AFTER_Z2 5
#define BOARD_NONAUTOCONFIG_AFTER_Z3 6
#define BOARD_IGNORE 7

#define KS12_BOOT_HACK 1

#define MAX_EXPANSION_BOARD_SPACE 8

/* ********************************************************** */
/* 00 / 02 */
/* er_Type */

#define Z2_MEM_8MB	0x00 /* Size of Memory Block */
#define Z2_MEM_4MB	0x07
#define Z2_MEM_2MB	0x06
#define Z2_MEM_1MB	0x05
#define Z2_MEM_512KB	0x04
#define Z2_MEM_256KB	0x03
#define Z2_MEM_128KB	0x02
#define Z2_MEM_64KB	0x01
/* extended definitions */
#define Z3_MEM_16MB		0x00
#define Z3_MEM_32MB		0x01
#define Z3_MEM_64MB		0x02
#define Z3_MEM_128MB	0x03
#define Z3_MEM_256MB	0x04
#define Z3_MEM_512MB	0x05
#define Z3_MEM_1GB		0x06

#define chainedconfig	0x08 /* Next config is part of the same card */
#define rom_card	0x10 /* ROM vector is valid */
#define add_memory	0x20 /* Link RAM into free memory list */

/* Type of Expansion Card */
#define protoautoconfig 0x40
#define zorroII		0xc0
#define zorroIII	0x80

/* ********************************************************** */
/* 04 - 06 & 10-16 */

/* Manufacturer */
#define commodore_g	 513 /* Commodore Braunschweig (Germany) */
#define commodore	 514 /* Commodore West Chester */
#define gvp		2017 /* GVP */
#define ass		2102 /* Advanced Systems & Software */
#define hackers_id	2011 /* Special ID for test cards */

/* Card Type */
#define commodore_a2091	     3 /* A2091 / A590 Card from C= */
#define commodore_a2091_ram 10 /* A2091 / A590 Ram on HD-Card */
#define commodore_a2232	    70 /* A2232 Multiport Expansion */
#define ass_nexus_scsi	     1 /* Nexus SCSI Controller */

#define gvp_series_2_scsi   11
#define gvp_iv_24_gfx	    32

/* ********************************************************** */
/* 08 - 0A  */
/* er_Flags */
#define Z3_SS_MEM_SAME		0x00
#define Z3_SS_MEM_AUTO		0x01
#define Z3_SS_MEM_64KB		0x02
#define Z3_SS_MEM_128KB		0x03
#define Z3_SS_MEM_256KB		0x04
#define Z3_SS_MEM_512KB		0x05
#define Z3_SS_MEM_1MB		0x06 /* Zorro III card subsize */
#define Z3_SS_MEM_2MB		0x07
#define Z3_SS_MEM_4MB		0x08
#define Z3_SS_MEM_6MB		0x09
#define Z3_SS_MEM_8MB		0x0a
#define Z3_SS_MEM_10MB		0x0b
#define Z3_SS_MEM_12MB		0x0c
#define Z3_SS_MEM_14MB		0x0d
#define Z3_SS_MEM_defunct1	0x0e
#define Z3_SS_MEM_defunct2	0x0f

#define force_z3	0x10 /* *MUST* be set if card is Z3 */
#define ext_size	0x20 /* Use extended size table for bits 0-2 of er_Type */
#define no_shutup	0x40 /* Card cannot receive Shut_up_forever */
#define care_addr	0x80 /* Z2=Adress HAS to be $200000-$9fffff Z3=1->mem,0=io */

/* ********************************************************** */
/* 40-42 */
/* ec_interrupt (unused) */

#define enable_irq	0x01 /* enable Interrupt */
#define reset_card	0x04 /* Reset of Expansion Card - must be 0 */
#define card_int2	0x10 /* READ ONLY: IRQ 2 active */
#define card_irq6	0x20 /* READ ONLY: IRQ 6 active */
#define card_irq7	0x40 /* READ ONLY: IRQ 7 active */
#define does_irq	0x80 /* READ ONLY: Card currently throws IRQ */

/* ********************************************************** */

/* ROM defines (DiagVec) */

#define rom_4bit	(0x00<<14) /* ROM width */
#define rom_8bit	(0x01<<14)
#define rom_16bit	(0x02<<14)

#define rom_never	(0x00<<12) /* Never run Boot Code */
#define rom_install	(0x01<<12) /* run code at install time */
#define rom_binddrv	(0x02<<12) /* run code with binddrivers */

uaecptr ROM_filesys_resname, ROM_filesys_resid;
uaecptr ROM_filesys_diagentry;
uaecptr ROM_hardfile_resname, ROM_hardfile_resid;
uaecptr ROM_hardfile_init;
int uae_boot_rom_type;
int uae_boot_rom_size; /* size = code size only */
static bool chipdone;

#define FILESYS_DIAGPOINT 0x01e0
#define FILESYS_BOOTPOINT 0x01e6
#define FILESYS_DIAGAREA 0x2000

/* ********************************************************** */

struct card_data
{
	addrbank *(*initrc)(void);
	addrbank *(*initnum)(int);
	addrbank *(*map)(void);
	const TCHAR *name;
	int flags;
	int zorro;
};

static struct card_data cards[MAX_EXPANSION_BOARD_SPACE];

static int ecard, cardno;
static addrbank *expamem_bank_current;

static uae_u16 uae_id;

static bool isnonautoconfig(int v)
{
	return v == BOARD_NONAUTOCONFIG_AFTER_Z2 ||
		v == BOARD_NONAUTOCONFIG_AFTER_Z3 ||
		v == BOARD_NONAUTOCONFIG_BEFORE;
}

static bool ks12orolder(void)
{
	/* check if Kickstart version is below 1.3 */
	return kickstart_version && kickstart_version < 34;
}
static bool ks11orolder(void)
{
	return kickstart_version && kickstart_version < 33;
}

/* ********************************************************** */

/* Please note: ZorroIII implementation seems to work different
 * than described in the HRM. This claims that ZorroIII config
 * address is 0xff000000 while the ZorroII config space starts
 * at 0x00e80000. In reality, both, Z2 and Z3 cards are 
 * configured in the ZorroII config space. Kickstart 3.1 doesn't
 * even do a single read or write access to the ZorroIII space.
 * The original Amiga include files tell the same as the HRM.
 * ZorroIII: If you set ext_size in er_Flags and give a Z2-size
 * in er_Type you can very likely add some ZorroII address space
 * to a ZorroIII card on a real Amiga. This is not implemented
 * yet.
 *  -- Stefan
 * 
 * Surprising that 0xFF000000 isn't used. Maybe it depends on the
 * ROM. Anyway, the HRM says that Z3 cards may appear in Z2 config
 * space, so what we are doing here is correct.
 *  -- Bernd
 */

/* Autoconfig address space at 0xE80000 */
static uae_u8 expamem[65536];

static uae_u8 expamem_lo;
static uae_u16 expamem_hi;
static uaecptr expamem_z3_sum;
uaecptr expamem_z3_pointer;
uaecptr expamem_z2_pointer;
uae_u32 expamem_z3_size;
uae_u32 expamem_z2_size;
static uae_u32 expamem_board_size;
static uae_u32 expamem_board_pointer;
static bool z3hack_override;

void set_expamem_z3_hack_override(bool overridenoz3hack)
{
	z3hack_override = overridenoz3hack;
}

bool expamem_z3hack(struct uae_prefs *p)
{
	if (z3hack_override)
		return false;
	return p->z3_mapping_mode == Z3MAPPING_AUTO || p->z3_mapping_mode == Z3MAPPING_UAE;
}

/* Ugly hack for >2M chip RAM in single pool
 * We can't add it any later or early boot menu
 * stops working because it sets kicktag at the end
 * of chip ram...
 */
static void addextrachip (uae_u32 sysbase)
{
	if (currprefs.chipmem_size <= 0x00200000)
		return;
	if (sysbase & 0x80000001)
		return;
	if (!valid_address (sysbase, 1000))
		return;
	uae_u32 ml = get_long (sysbase + 322);
	if (!valid_address (ml, 32))
		return;
	uae_u32 next;
	while ((next = get_long (ml))) {
		if (!valid_address (ml, 32))
			return;
		uae_u32 upper = get_long (ml + 24);
		uae_u32 lower = get_long (ml + 20);
		if (lower & ~0xffff) {
			ml = next;
			continue;
		}
		uae_u16 attr = get_word (ml + 14);
		if ((attr & 0x8002) != 2) {
			ml = next;
			continue;
		}
		if (upper >= currprefs.chipmem_size)
			return;
		uae_u32 added = currprefs.chipmem_size - upper;
		uae_u32 first = get_long (ml + 16);
		put_long (ml + 24, currprefs.chipmem_size); // mh_Upper
		put_long (ml + 28, get_long (ml + 28) + added); // mh_Free
		uae_u32 next;
		while (first) {
			next = first;
			first = get_long (next);
		}
		uae_u32 bytes = get_long (next + 4);
		if (next + bytes == 0x00200000) {
			put_long (next + 4, currprefs.chipmem_size - next);
		} else {
			put_long (0x00200000 + 0, 0);
			put_long (0x00200000 + 4, added);
			put_long (next, 0x00200000);
		}
		return;
	}
}

addrbank expamem_null;

DECLARE_MEMORY_FUNCTIONS(expamem);
addrbank expamem_bank = {
  expamem_lget, expamem_wget, expamem_bget,
  expamem_lput, expamem_wput, expamem_bput,
	default_xlate, default_check, NULL, NULL, _T("Autoconfig Z2"),
	dummy_lgeti, dummy_wgeti,
	ABFLAG_IO | ABFLAG_SAFE | ABFLAG_PPCIOSPACE, S_READ, S_WRITE
};
DECLARE_MEMORY_FUNCTIONS(expamemz3);
static addrbank expamemz3_bank = {
	expamemz3_lget, expamemz3_wget, expamemz3_bget,
	expamemz3_lput, expamemz3_wput, expamemz3_bput,
	default_xlate, default_check, NULL, NULL, _T("Autoconfig Z3"),
	dummy_lgeti, dummy_wgeti,
	ABFLAG_IO | ABFLAG_SAFE | ABFLAG_PPCIOSPACE, S_READ, S_WRITE
};

static addrbank *expamem_map_clear (void)
{
	write_log (_T("expamem_map_clear() got called. Shouldn't happen.\n"));
	return NULL;
}

static void expamem_init_clear (void)
{
  memset (expamem, 0xff, sizeof expamem);
}
/* autoconfig area is "non-existing" after last device */
static void expamem_init_clear_zero (void)
{
  map_banks (&dummy_bank, 0xe8, 1, 0);
	if (!currprefs.address_space_24)
		map_banks (&dummy_bank, 0xff000000 >> 16, 1, 0);
}

static void expamem_init_clear2 (void)
{
	expamem_bank.name = _T("Autoconfig Z2");
	expamemz3_bank.name = _T("Autoconfig Z3");
  expamem_init_clear_zero();
  ecard = cardno;
}

static addrbank *expamem_init_last (void)
{
  expamem_init_clear2();
	return NULL;
}

static uae_u8 REGPARAM2 expamem_read(int addr)
{
	uae_u8 b = (expamem[addr] & 0xf0) | (expamem[addr + 2] >> 4);
	if (addr == 0 || addr == 2 || addr == 0x40 || addr == 0x42)
		return b;
	b = ~b;
	return b;
}

static void REGPARAM2 expamem_write (uaecptr addr, uae_u32 value)
{
  addr &= 0xffff;
  if (addr == 00 || addr == 02 || addr == 0x40 || addr == 0x42) {
		expamem[addr] = (value & 0xf0);
		expamem[addr + 2] = (value & 0x0f) << 4;
  } else {
		expamem[addr] = ~(value & 0xf0);
		expamem[addr + 2] = ~((value & 0x0f) << 4);
  }
}

static int REGPARAM2 expamem_type (void)
{
	return expamem_read(0) & 0xc0;
}

static void call_card_init(int index)
{	
	addrbank *ab, *abe;
	uae_u8 code;
	uae_u32 expamem_z3_pointer_old;
	card_data *cd = &cards[ecard];

	expamem_bank.name = cd->name ? cd->name : _T("None");
	if (cd->initnum)
	  ab = cd->initnum(0);
	else
		ab = cd->initrc();
	expamem_z3_size = 0;
	if (ab == &expamem_null) {
		expamem_next(NULL, NULL);
		return;
	}

	abe = ab;
	if (!abe)
		abe = &expamem_bank;
	if (abe != &expamem_bank) {
		for (int i = 0; i < 16 * 4; i++)
			expamem[i] = abe->bget(i);
	}

	code = expamem_read(0);
	if ((code & 0xc0) == zorroII) {
		// Z2
		code &= 7;
		if (code == 0)
			expamem_z2_size = 8 * 1024 * 1024;
		else
			expamem_z2_size = 32768 << code;

		expamem_board_size = expamem_z2_size;
		expamem_board_pointer = expamem_z2_pointer;

	} else if ((code & 0xc0) == zorroIII) {
		// Z3
		if (expamem_z3_sum < Z3BASE_UAE) {
			expamem_z3_sum = currprefs.z3autoconfig_start;
			if (currprefs.mbresmem_high_size >= 128 * 1024 * 1024 && expamem_z3_sum == Z3BASE_UAE)
				expamem_z3_sum += (currprefs.mbresmem_high_size - 128 * 1024 * 1024) + 16 * 1024 * 1024;
			if (!expamem_z3hack(&currprefs))
				expamem_z3_sum = Z3BASE_REAL;
    }

		expamem_z3_pointer = expamem_z3_sum;

		code &= 7;
		if (expamem_read(8) & ext_size)
			expamem_z3_size = (16 * 1024 * 1024) << code;
		else
			expamem_z3_size = 16 * 1024 * 1024;
		expamem_z3_sum += expamem_z3_size;

		expamem_z3_pointer_old = expamem_z3_pointer;
		// align non-UAE 32M boards (FastLane is 32M and needs to be aligned)
		if (expamem_z3_size <= 32 * 1024 * 1024 && !(cd->flags & 16))
			expamem_z3_pointer = (expamem_z3_pointer + expamem_z3_size - 1) & ~(expamem_z3_size - 1);

		expamem_z3_sum += expamem_z3_pointer - expamem_z3_pointer_old;

		expamem_board_size = expamem_z3_size;
		expamem_board_pointer = expamem_z3_pointer;
	} else if ((code & 0xc0) == 0x40) {
		// 0x40 = "Box without init/diagnostic code"
		// proto autoconfig "box" size.
		//expamem_z2_size = (1 << ((code >> 3) & 7)) * 4096;
		// much easier this way, all old-style boards were made for
		// A1000 and didn't have passthrough connector.
		expamem_z2_size = 65536;
		expamem_board_size = expamem_z2_size;
		expamem_board_pointer = expamem_z2_pointer;
	}

	if (ab) {
		// non-NULL: not using expamem_bank
		expamem_bank_current = ab;
		if ((cd->flags & 1) && currprefs.cs_z3autoconfig && !currprefs.address_space_24) {
			map_banks(&expamemz3_bank, 0xff000000 >> 16, 1, 0);
			map_banks(&dummy_bank, 0xE8, 1, 0);
		} else {
		  map_banks(&expamem_bank, 0xE8, 1, 0);
		  if (!currprefs.address_space_24)
			  map_banks(&dummy_bank, 0xff000000 >> 16, 1, 0);
    }
	} else {
		if ((cd->flags & 1) && currprefs.cs_z3autoconfig && !currprefs.address_space_24) {
			map_banks(&expamemz3_bank, 0xff000000 >> 16, 1, 0);
			map_banks(&dummy_bank, 0xE8, 1, 0);
			expamem_bank_current = &expamem_bank;
	  } else {
		  map_banks(&expamem_bank, 0xE8, 1, 0);
		  if (!currprefs.address_space_24)
			  map_banks(&dummy_bank, 0xff000000 >> 16, 1, 0);
		  expamem_bank_current = NULL;
		}
  }
}

static void boardmessage(addrbank *mapped, bool success)
{
	uae_u8 type = expamem_read(0);
	int size = expamem_board_size;
	TCHAR sizemod = 'K';

	size /= 1024;
	if (size > 8 * 1024) {
		sizemod = 'M';
		size /= 1024;
	}
	write_log (_T("Card %d: Z%d 0x%08x %4d%c %s %s%s\n"),
		ecard + 1, (type & 0xc0) == zorroII ? 2 : ((type & 0xc0) == zorroIII ? 3 : 1),
		expamem_board_pointer, size, sizemod,
		type & rom_card ? _T("ROM") : (type & add_memory ? _T("RAM") : _T("IO ")),
		mapped->name,
		success ? _T("") : _T(" SHUT UP"));
}

void expamem_next(addrbank *mapped, addrbank *next)
{
	if (mapped)
		boardmessage(mapped, true);

  expamem_init_clear();
	expamem_init_clear_zero();
	for (;;) {
		++ecard;
		if (ecard >= cardno)
			break;
		struct card_data *ec = &cards[ecard];
		if (ec->initrc && isnonautoconfig(ec->zorro)) {
			ec->initrc();
		} else {
			call_card_init(ecard);
			break;
		}
	}
	if (ecard >= cardno) {
		expamem_init_clear2();
		expamem_init_last();
	}
}

static uae_u32 REGPARAM2 expamem_lget (uaecptr addr)
{
	if (expamem_bank_current && expamem_bank_current != &expamem_bank)
		return expamem_bank_current->lget(addr);
	write_log (_T("warning: Z2 READ.L from address $%08x PC=%x\n"), addr, M68K_GETPC);
  return (expamem_wget (addr) << 16) | expamem_wget (addr + 2);
}

static uae_u32 REGPARAM2 expamem_wget (uaecptr addr)
{
	if (expamem_bank_current && expamem_bank_current != &expamem_bank)
		return expamem_bank_current->wget(addr);
	if (expamem_type() != zorroIII) {
		if (expamem_bank_current && expamem_bank_current != &expamem_bank)
			return expamem_bank_current->bget(addr) << 8;
	}
  uae_u32 v = (expamem_bget (addr) << 8) | expamem_bget (addr + 1);
	write_log (_T("warning: READ.W from address $%08x=%04x PC=%x\n"), addr, v & 0xffff, M68K_GETPC);
  return v;
}

static uae_u32 REGPARAM2 expamem_bget (uaecptr addr)
{
  uae_u8 b;
	if (!chipdone) {
		chipdone = true;
		addextrachip (get_long (4));
	}
	if (expamem_bank_current && expamem_bank_current != &expamem_bank)
		return expamem_bank_current->bget(addr);
  addr &= 0xFFFF;
  b = expamem[addr];
  return b;
}

static void REGPARAM2 expamem_lput (uaecptr addr, uae_u32 value)
{
	if (expamem_bank_current && expamem_bank_current != &expamem_bank) {
		expamem_bank_current->lput(addr, value);
		return;
	}
	write_log (_T("warning: Z2 WRITE.L to address $%08x : value $%08x\n"), addr, value);
}

static void REGPARAM2 expamem_wput (uaecptr addr, uae_u32 value)
{
  value &= 0xffff;
  if (ecard >= cardno)
  	return;
	if (expamem_type () != zorroIII) {
		write_log (_T("warning: WRITE.W to address $%08x : value $%x PC=%08x\n"), addr, value, M68K_GETPC);
	}
	switch (addr & 0xff) {
	  case 0x48:
		  // A2630 boot rom writes WORDs to Z2 boards!
		  if (expamem_type() == zorroII) {
			  expamem_lo = 0;
			  expamem_hi = (value >> 8) & 0xff;
			  expamem_z2_pointer = (expamem_hi | (expamem_lo >> 4)) << 16; 
			  expamem_board_pointer = expamem_z2_pointer;
			  if (cards[ecard].map) {
				  expamem_next(cards[ecard].map(), NULL);
				  return;
			  }
			  if (expamem_bank_current && expamem_bank_current != &expamem_bank) {
				  expamem_bank_current->bput(addr, value >> 8);
				  return;
			  }
		  }
		  break;
	  case 0x44:
      if (expamem_type() == zorroIII) {
			  uaecptr addr;
			  expamem_hi = value & 0xff00;
			  addr = (expamem_hi | (expamem_lo >> 4)) << 16;
			  if (!expamem_z3hack(&currprefs)) {
				  expamem_z3_pointer = addr;
			  } else {
			    if (addr != expamem_z3_pointer) {
				    put_word (regs.regs[11] + 0x20, expamem_z3_pointer >> 16);
				    put_word (regs.regs[11] + 0x28, expamem_z3_pointer >> 16);
			    }
			  }
  			expamem_board_pointer = expamem_z3_pointer;
      }
		  if (cards[ecard].map) {
			  expamem_next(cards[ecard].map(), NULL);
			  return;
		  }
		  break;
	  case 0x4c:
		  if (cards[ecard].map) {
			  expamem_next (NULL, NULL);
			  return;
		  }
      break;
  }
	if (expamem_bank_current && expamem_bank_current != &expamem_bank)
		expamem_bank_current->wput(addr, value);
}

static void REGPARAM2 expamem_bput (uaecptr addr, uae_u32 value)
{
  value &= 0xff;
  if (ecard >= cardno)
  	return;
	if (expamem_type() == protoautoconfig) {
		switch (addr & 0xff) {
		case 0x22:
			expamem_hi = value & 0x7f;
			expamem_z2_pointer = 0xe80000 | (expamem_hi * 4096);
			expamem_board_pointer = expamem_z2_pointer;
			if (cards[ecard].map) {
				expamem_next(cards[ecard].map(), NULL);
				return;
			}
			break;
		}
	} else {
	  switch (addr & 0xff) {
		  case 0x48:
			  if (expamem_type () == zorroII) {
			    expamem_hi = value & 0xff;
			    expamem_z2_pointer = (expamem_hi | (expamem_lo >> 4)) << 16; 
			    expamem_board_pointer = expamem_z2_pointer;
			    if (cards[ecard].map) {
				    expamem_next(cards[ecard].map(), NULL);
				    return;
			    }
		    } else {
			    expamem_lo = value & 0xff;
		    }
			  break;

		  case 0x4a:
			  if (expamem_type () == zorroII)
			    expamem_lo = value & 0xff;
			  break;

		  case 0x4c:
		    if (cards[ecard].map) {
			    expamem_next(expamem_bank_current, NULL);
			    return;
		    }
			  break;
	  }
	}
	if (expamem_bank_current && expamem_bank_current != &expamem_bank)
		expamem_bank_current->bput(addr, value);
}

static uae_u32 REGPARAM2 expamemz3_bget (uaecptr addr)
{
	int reg = addr & 0xff;
	if (!expamem_bank_current)
		return 0;
	if (addr & 0x100)
		reg += 2;
	return expamem_bank_current->bget(reg + 0);
}

static uae_u32 REGPARAM2 expamemz3_wget (uaecptr addr)
{
	uae_u32 v = (expamemz3_bget (addr) << 8) | expamemz3_bget (addr + 1);
	write_log (_T("warning: Z3 READ.W from address $%08x=%04x PC=%x\n"), addr, v & 0xffff, M68K_GETPC);
	return v;
}

static uae_u32 REGPARAM2 expamemz3_lget (uaecptr addr)
{
	write_log (_T("warning: Z3 READ.L from address $%08x PC=%x\n"), addr, M68K_GETPC);
	return (expamemz3_wget (addr) << 16) | expamemz3_wget (addr + 2);
}

static void REGPARAM2 expamemz3_bput (uaecptr addr, uae_u32 value)
{
	int reg = addr & 0xff;
	if (!expamem_bank_current)
		return;
	if (addr & 0x100)
		reg += 2;
	if (reg == 0x48) {
		if (expamem_type() == zorroII) {
			expamem_hi = value & 0xff;
			expamem_z2_pointer = (expamem_hi | (expamem_lo >> 4)) << 16; 
			expamem_board_pointer = expamem_z2_pointer;
		} else {
			expamem_lo = value & 0xff;
		}
	} else if (reg == 0x4a) {
		if (expamem_type() == zorroII)
			expamem_lo = value & 0xff;
	}
	expamem_bank_current->bput(reg, value);
}

static void REGPARAM2 expamemz3_wput (uaecptr addr, uae_u32 value)
{
	int reg = addr & 0xff;
	if (!expamem_bank_current)
		return;
	if (addr & 0x100)
		reg += 2;
	if (reg == 0x44) {
		if (expamem_type() == zorroIII) {
			uaecptr addr;
			expamem_hi = value & 0xff00;
			addr = (expamem_hi | (expamem_lo >> 4)) << 16;;
			if (!expamem_z3hack(&currprefs)) {
				expamem_z3_pointer = addr;
			} else {
			  if (addr != expamem_z3_pointer) {
				  put_word (regs.regs[11] + 0x20, expamem_z3_pointer >> 16);
				  put_word (regs.regs[11] + 0x28, expamem_z3_pointer >> 16);
			  }
			}
			expamem_board_pointer = expamem_z3_pointer;
		}
	}
	expamem_bank_current->wput(reg, value);
}
static void REGPARAM2 expamemz3_lput (uaecptr addr, uae_u32 value)
{
	write_log (_T("warning: Z3 WRITE.L to address $%08x : value $%08x\n"), addr, value);
}

#ifdef CD32

static addrbank *expamem_map_cd32fmv (void)
{
	return cd32_fmv_init (expamem_z2_pointer);
}

static addrbank *expamem_init_cd32fmv (int devnum)
{
	int ids[] = { 23, -1 };
	struct romlist *rl = getromlistbyids (ids, NULL);
	struct romdata *rd;
	struct zfile *z;

	expamem_init_clear ();
	if (!rl)
		return NULL;
	write_log (_T("CD32 FMV ROM '%s' %d.%d\n"), rl->path, rl->rd->ver, rl->rd->rev);
	rd = rl->rd;
	z = read_rom (rd);
	if (z) {
		zfile_fread (expamem, 128, 1, z);
		zfile_fclose (z);
	}
	return NULL;
}

#endif

/* ********************************************************** */

/*
 *  Fast Memory
 */

MEMORY_FUNCTIONS(fastmem);
MEMORY_FUNCTIONS(fastmem_nojit);

addrbank fastmem_bank = {
  fastmem_lget, fastmem_wget, fastmem_bget,
  fastmem_lput, fastmem_wput, fastmem_bput,
	fastmem_xlate, fastmem_check, NULL, _T("fast"), _T("Fast memory"),
	fastmem_lget, fastmem_wget,
	ABFLAG_RAM | ABFLAG_THREADSAFE, 0, 0
};
addrbank fastmem_nojit_bank = {
	fastmem_nojit_lget, fastmem_nojit_wget, fastmem_bget,
	fastmem_nojit_lput, fastmem_nojit_wput, fastmem_bput,
	fastmem_nojit_xlate, fastmem_nojit_check, NULL, NULL, _T("Fast memory (nojit)"),
	fastmem_nojit_lget, fastmem_nojit_wget,
	ABFLAG_RAM | ABFLAG_THREADSAFE, S_READ, S_WRITE
};

static addrbank *fastbanks[] = 
{
	&fastmem_bank,
	&fastmem_nojit_bank,
};

#ifdef FILESYS

/*
* Filesystem device ROM/RAM space
 */

DECLARE_MEMORY_FUNCTIONS(filesys);
addrbank filesys_bank = {
	filesys_lget, filesys_wget, filesys_bget,
	filesys_lput, filesys_wput, filesys_bput,
	filesys_xlate, filesys_check, NULL, _T("filesys"), _T("Filesystem autoconfig"),
	filesys_lget, filesys_wget,
	ABFLAG_IO | ABFLAG_SAFE | ABFLAG_PPCIOSPACE, S_READ, S_WRITE
};

static uae_u32 filesys_start; /* Determined by the OS */

static bool filesys_write(uaecptr addr)
{
	return addr >= 0x4000;
}

static uae_u32 REGPARAM2 filesys_lget (uaecptr addr)
{
  uae_u8 *m;
  addr -= filesys_start;
  addr &= 65535;
  m = filesys_bank.baseaddr + addr;
  return do_get_mem_long ((uae_u32 *)m);
}

static uae_u32 REGPARAM2 filesys_wget (uaecptr addr)
{
  uae_u8 *m;
  addr -= filesys_start;
  addr &= 65535;
  m = filesys_bank.baseaddr + addr;
  return do_get_mem_word ((uae_u16 *)m);
}

static uae_u32 REGPARAM2 filesys_bget (uaecptr addr)
{
  addr -= filesys_start;
  addr &= 65535;
  return filesys_bank.baseaddr[addr];
}

static void REGPARAM2 filesys_bput(uaecptr addr, uae_u32 b)
{
	addr -= filesys_start;
	addr &= 65535;
	if (!filesys_write(addr))
		return;
	filesys_bank.baseaddr[addr] = b;
}

static void REGPARAM2 filesys_lput (uaecptr addr, uae_u32 l)
{
	addr -= filesys_start;
	addr &= 65535;
	if (!filesys_write(addr))
		return;
	filesys_bank.baseaddr[addr + 0] = l >> 24;
	filesys_bank.baseaddr[addr + 1] = l >> 16;
	filesys_bank.baseaddr[addr + 2] = l >> 8;
	filesys_bank.baseaddr[addr + 3] = l >> 0;
}

static void REGPARAM2 filesys_wput (uaecptr addr, uae_u32 w)
{
	addr -= filesys_start;
	addr &= 65535;
	if (!filesys_write(addr))
		return;
	filesys_bank.baseaddr[addr + 0] = w >> 8;
	filesys_bank.baseaddr[addr + 1] = w & 0xff;
}

static int REGPARAM2 filesys_check(uaecptr addr, uae_u32 size)
{
	addr -= filesys_bank.start;
	addr &= 65535;
	return (addr + size) <= filesys_bank.allocated;
}
static uae_u8 *REGPARAM2 filesys_xlate(uaecptr addr)
{
	addr -= filesys_bank.start;
	addr &= 65535;
	return filesys_bank.baseaddr + addr;
}

#endif /* FILESYS */

/*
 *  Z3fastmem Memory
 */

MEMORY_FUNCTIONS(z3fastmem);

addrbank z3fastmem_bank = {
  z3fastmem_lget, z3fastmem_wget, z3fastmem_bget,
  z3fastmem_lput, z3fastmem_wput, z3fastmem_bput,
  z3fastmem_xlate, z3fastmem_check, NULL, _T("z3"), _T("ZorroIII Fast RAM"),
	z3fastmem_lget, z3fastmem_wget,
	ABFLAG_RAM | ABFLAG_THREADSAFE, 0, 0
};

/* ********************************************************** */

/*
*     Expansion Card (ZORRO II) for 64/128/256/512KB 1/2/4/8MB of Fast Memory
 */

static addrbank *expamem_map_fastcard_2 (int boardnum)
{
	uae_u32 start = ((expamem_hi | (expamem_lo >> 4)) << 16);
	addrbank *ab = fastbanks[boardnum * 2 + ((start < 0x00A00000) ? 0 : 1)];
	uae_u32 size = ab->allocated;
	ab->start = start;
	if (ab->start) {
		map_banks_z2 (ab, ab->start >> 16, size >> 16);
  }
	return ab;
}

static void fastmem_autoconfig(int boardnum, int zorro, uae_u8 type, uae_u32 serial, int allocated)
{
  uae_u16 mid = 0;
  uae_u8 pid;
	uae_u8 flags = care_addr;
	uae_u8 ac[16] = { 0 };

	if (!mid) {
		if (zorro <= 2) {
			pid = 81;
		} else {
			int subsize = (allocated == 0x100000 ? Z3_SS_MEM_1MB
						   : allocated == 0x200000 ? Z3_SS_MEM_2MB
						   : allocated == 0x400000 ? Z3_SS_MEM_4MB
						   : allocated == 0x800000 ? Z3_SS_MEM_8MB
						   : Z3_SS_MEM_SAME);
			pid = 83;
			flags |= force_z3 | (allocated > 0x800000 ? ext_size : subsize);
		}
		mid = uae_id;
		serial = 1;
	}

	ac[0x00 / 4] = type;
	ac[0x04 / 4] = pid;
	ac[0x08 / 4] = flags;
	ac[0x10 / 4] = mid >> 8;
	ac[0x14 / 4] = (uae_u8)mid;
	ac[0x18 / 4] = serial >> 24;
	ac[0x1c / 4] = serial >> 16;
	ac[0x20 / 4] = serial >> 8;
	ac[0x24 / 4] = serial >> 0;

	expamem_write(0x00, ac[0x00 / 4]);
	expamem_write(0x04, ac[0x04 / 4]);
	expamem_write(0x08, ac[0x08 / 4]);
	expamem_write(0x10, ac[0x10 / 4]);
	expamem_write(0x14, ac[0x14 / 4]);

	expamem_write(0x18, ac[0x18 / 4]); /* ser.no. Byte 0 */
	expamem_write(0x1c, ac[0x1c / 4]); /* ser.no. Byte 1 */
	expamem_write(0x20, ac[0x20 / 4]); /* ser.no. Byte 2 */
	expamem_write(0x24, ac[0x24 / 4]); /* ser.no. Byte 3 */

  expamem_write (0x28, 0x00); /* ROM-Offset hi */
  expamem_write (0x2c, 0x00); /* ROM-Offset lo */

  expamem_write (0x40, 0x00); /* Ctrl/Statusreg.*/
}

static addrbank *expamem_init_fastcard_2(int boardnum)
{
	uae_u8 type = add_memory | zorroII;
	int allocated = fastmem_bank.allocated;

	if (allocated == 0)
		return &expamem_null;

	expamem_init_clear ();
	if (allocated == 65536)
		type |= Z2_MEM_64KB;
	else if (allocated == 131072)
		type |= Z2_MEM_128KB;
	else if (allocated == 262144)
		type |= Z2_MEM_256KB;
	else if (allocated == 524288)
		type |= Z2_MEM_512KB;
	else if (allocated == 0x100000)
		type |= Z2_MEM_1MB;
	else if (allocated == 0x200000)
		type |= Z2_MEM_2MB;
	else if (allocated == 0x400000)
		type |= Z2_MEM_4MB;
	else if (allocated == 0x800000)
		type |= Z2_MEM_8MB;

	fastmem_autoconfig(boardnum, BOARD_AUTOCONFIG_Z2, type, 1, allocated);

	return NULL;
}

static addrbank *expamem_init_fastcard(int boardnum)
{
	return expamem_init_fastcard_2(0);
}
static addrbank *expamem_map_fastcard (void)
{
	return expamem_map_fastcard_2 (0);
}

/* ********************************************************** */

#ifdef FILESYS

/* 
 * Filesystem device
 */

static void expamem_map_filesys_update(void)
{
	/* 68k code needs to know this. */
	uaecptr a = here();
	org(rtarea_base + RTAREA_FSBOARD);
	dl(filesys_start + 0x2000);
	org(a);
}

static addrbank *expamem_map_filesys (void)
{
	filesys_start = expamem_z2_pointer;
	map_banks_z2 (&filesys_bank, filesys_start >> 16, 1);
	expamem_map_filesys_update();
	return &filesys_bank;
}

#if KS12_BOOT_HACK
static void add_ks12_boot_hack(void)
{
	uaecptr name = ds(_T("UAE boot"));
	align(2);
	uaecptr code = here();
	// allocate fake diagarea
	dl(0x48e73f3e); // movem.l d2-d7/a2-a6,-(sp)
	dw(0x203c); // move.l #x,d0
	dl(0x0300);
	dw(0x7201); // moveq #1,d1
	dl(0x4eaeff3a); // jsr -0xc6(a6)
	dw(0x2440); // move.l d0,a2 ;diag area
	dw(0x9bcd); // sub.l a5,a5 ;expansionbase
	dw(0x97cb); // sub.l a3,a3 ;configdev
	dw(0x4eb9); // jsr
	dl(ROM_filesys_diagentry);
	dl(0x4cdf7cfc); // movem.l (sp)+,d2-d7/a2-a6
	dw(0x4e75);
	// struct Resident
	uaecptr addr = here();
	dw(0x4afc);
	dl(addr);
	dl(addr + 26);
	db(1); // RTF_COLDSTART
	db((uae_u8)kickstart_version); // version
	db(0); // NT_UNKNOWN
	db(1); // priority
	dl(name);
	dl(name);
	dl(code);
}
#endif

static addrbank* expamem_init_filesys (int devnum)
{
	bool ks12 = ks12orolder();
	bool hide = false;

  /* struct DiagArea - the size has to be large enough to store several device ROMTags */
	const uae_u8 diagarea[] = {
		0x90, 0x00, /* da_Config, da_Flags */
    0x02, 0x00, /* da_Size */
		FILESYS_DIAGPOINT >> 8, FILESYS_DIAGPOINT & 0xff,
		FILESYS_BOOTPOINT >> 8, FILESYS_BOOTPOINT & 0xff,
		0, (uae_u8)(hide ? 0 : 14), // Name offset
		0, 0, 0, 0,
		(uae_u8)(hide ? 0 : 'U'), (uae_u8)(hide ? 0 : 'A'), (uae_u8)(hide ? 0 : 'E'), 0
  };

  expamem_init_clear();
	expamem_write (0x00, Z2_MEM_64KB | zorroII | (ks12 ? 0 : rom_card));

  expamem_write (0x08, no_shutup);

  expamem_write (0x04, 82);
  expamem_write (0x10, uae_id >> 8);
  expamem_write (0x14, uae_id & 0xff);

  expamem_write (0x18, 0x00); /* ser.no. Byte 0 */
  expamem_write (0x1c, 0x00); /* ser.no. Byte 1 */
  expamem_write (0x20, 0x00); /* ser.no. Byte 2 */
  expamem_write (0x24, 0x01); /* ser.no. Byte 3 */

  /* er_InitDiagVec */
	expamem_write (0x28, 0x20); /* ROM-Offset hi */
  expamem_write (0x2c, 0x00); /* ROM-Offset lo */

  expamem_write (0x40, 0x00); /* Ctrl/Statusreg.*/

  /* Build a DiagArea */
	memcpy (expamem + FILESYS_DIAGAREA, diagarea, sizeof diagarea);

  /* Call DiagEntry */
	put_word_host(expamem + FILESYS_DIAGAREA + FILESYS_DIAGPOINT, 0x4EF9); /* JMP */
	put_long_host(expamem + FILESYS_DIAGAREA + FILESYS_DIAGPOINT + 2, ROM_filesys_diagentry);

  /* What comes next is a plain bootblock */
	put_word_host(expamem + FILESYS_DIAGAREA + FILESYS_BOOTPOINT, 0x4EF9); /* JMP */
	put_long_host(expamem + FILESYS_DIAGAREA + FILESYS_BOOTPOINT + 2, EXPANSION_bootcode);
  
	if (ks12)
		add_ks12_boot_hack();

	memcpy (filesys_bank.baseaddr, expamem, 0x3000);
	return NULL;
}

#endif

/*
 * Zorro III expansion memory
 */

static addrbank * expamem_map_z3fastmem_2 (addrbank *bank, uaecptr *startp, uae_u32 size, uae_u32 allocated, int chip)
{
	int z3fs = expamem_z3_pointer;
  int start = *startp;

	if (expamem_z3hack(&currprefs)) {
	  if (z3fs && start != z3fs) {
    	write_log(_T("WARNING: Z3MEM mapping changed from $%08x to $%08x\n"), start, z3fs);
	    map_banks(&dummy_bank, start >> 16, size >> 16,	allocated);
	    *startp = z3fs;
      map_banks_z3(bank, start >> 16, size >> 16);
    }
	} else {
		map_banks_z3(bank, z3fs >> 16, size >> 16);
		start = z3fs;
		*startp = z3fs;
	}
	return bank;
}

static addrbank *expamem_map_z3fastmem (void)
{
	return expamem_map_z3fastmem_2 (&z3fastmem_bank, &z3fastmem_bank.start, currprefs.z3fastmem_size, z3fastmem_bank.allocated, 0);
}

static addrbank *expamem_init_z3fastmem_2(int boardnum, addrbank *bank, uae_u32 start, uae_u32 size, uae_u32 allocated)
{
  int code = (allocated == 0x100000 ? Z2_MEM_1MB
		: allocated == 0x200000 ? Z2_MEM_2MB
		: allocated == 0x400000 ? Z2_MEM_4MB
		: allocated == 0x800000 ? Z2_MEM_8MB
		: allocated == 0x1000000 ? Z3_MEM_16MB
		: allocated == 0x2000000 ? Z3_MEM_32MB
		: allocated == 0x4000000 ? Z3_MEM_64MB
		: allocated == 0x8000000 ? Z3_MEM_128MB
		: allocated == 0x10000000 ? Z3_MEM_256MB
		: allocated == 0x20000000 ? Z3_MEM_512MB
		: Z3_MEM_1GB);

	if (allocated < 0x1000000)
		code = Z3_MEM_16MB; /* Z3 physical board size is always at least 16M */

  expamem_init_clear();
	fastmem_autoconfig(boardnum, BOARD_AUTOCONFIG_Z3, add_memory | zorroIII | code, 1, allocated);
	map_banks_z3(bank, start >> 16, size >> 16);
	return NULL;
}
static addrbank *expamem_init_z3fastmem (int devnum)
{
	return expamem_init_z3fastmem_2 (0, &z3fastmem_bank, z3fastmem_bank.start, currprefs.z3fastmem_size, z3fastmem_bank.allocated);
}

#ifdef PICASSO96
/*
 *  Fake Graphics Card (ZORRO III) - BDK
 */

static addrbank *expamem_map_gfxcard_z3 (void)
{
	gfxmem_bank.start = expamem_z3_pointer;
	map_banks_z3(&gfxmem_bank, gfxmem_bank.start >> 16, gfxmem_bank.allocated >> 16);
	return &gfxmem_bank;
}

static addrbank *expamem_map_gfxcard_z2 (void)
{
	gfxmem_bank.start = expamem_z2_pointer;
  map_banks_z2 (&gfxmem_bank, gfxmem_bank.start >> 16, gfxmem_bank.allocated >> 16);
	return &gfxmem_bank;
}

static addrbank *expamem_init_gfxcard (bool z3)
{
  int code = (gfxmem_bank.allocated == 0x100000 ? Z2_MEM_1MB
    : gfxmem_bank.allocated == 0x200000 ? Z2_MEM_2MB
    : gfxmem_bank.allocated == 0x400000 ? Z2_MEM_4MB
    : gfxmem_bank.allocated == 0x800000 ? Z2_MEM_8MB
    : gfxmem_bank.allocated == 0x1000000 ? Z3_MEM_16MB
    : gfxmem_bank.allocated == 0x2000000 ? Z3_MEM_32MB
    : gfxmem_bank.allocated == 0x4000000 ? Z3_MEM_64MB
  	: gfxmem_bank.allocated == 0x8000000 ? Z3_MEM_128MB
  	: gfxmem_bank.allocated == 0x10000000 ? Z3_MEM_256MB
  	: gfxmem_bank.allocated == 0x20000000 ? Z3_MEM_512MB
  	: Z3_MEM_1GB);
  int subsize = (gfxmem_bank.allocated == 0x100000 ? Z3_SS_MEM_1MB
  	: gfxmem_bank.allocated == 0x200000 ? Z3_SS_MEM_2MB
  	: gfxmem_bank.allocated == 0x400000 ? Z3_SS_MEM_4MB
  	: gfxmem_bank.allocated == 0x800000 ? Z3_SS_MEM_8MB
  	: Z3_SS_MEM_SAME);

	if (gfxmem_bank.allocated < 0x1000000 && z3)
		code = Z3_MEM_16MB; /* Z3 physical board size is always at least 16M */

  expamem_init_clear();
	expamem_write (0x00, (z3 ? zorroIII : zorroII) | code);

	expamem_write (0x08, care_addr | (z3 ? (force_z3 | (gfxmem_bank.allocated > 0x800000 ? ext_size: subsize)) : 0));
  expamem_write (0x04, 96);

  expamem_write (0x10, uae_id >> 8);
  expamem_write (0x14, uae_id & 0xff);

  expamem_write (0x18, 0x00); /* ser.no. Byte 0 */
  expamem_write (0x1c, 0x00); /* ser.no. Byte 1 */
  expamem_write (0x20, 0x00); /* ser.no. Byte 2 */
  expamem_write (0x24, 0x01); /* ser.no. Byte 3 */

  expamem_write (0x28, 0x00); /* ROM-Offset hi */
  expamem_write (0x2c, 0x00); /* ROM-Offset lo */

  expamem_write (0x40, 0x00); /* Ctrl/Statusreg.*/
	return NULL;
}
static addrbank *expamem_init_gfxcard_z3(int devnum)
{
	return expamem_init_gfxcard (true);
}
static addrbank *expamem_init_gfxcard_z2 (int devnum)
{
	return expamem_init_gfxcard (false);
}
#endif

#ifdef SAVESTATE
static size_t fast_filepos, z3_filepos, p96_filepos;
#endif

void free_fastmemory (int boardnum)
{
	mapped_free (&fastmem_bank);
}

static bool mapped_malloc_dynamic (uae_u32 *currpsize, uae_u32 *changedpsize, addrbank *bank, int max, const TCHAR *name)
{
	int alloc = *currpsize;

	bank->allocated = 0;
	bank->baseaddr = NULL;
	bank->mask = 0;

	if (!alloc)
		return false;

	while (alloc >= max * 1024 * 1024) {
		bank->mask = alloc - 1;
		bank->allocated = alloc;
		bank->label = name;
		if (mapped_malloc (bank)) {
			*currpsize = alloc;
			*changedpsize = alloc;
			return true;
		}
		write_log (_T("Out of memory for %s, %d bytes.\n"), name, alloc);
		alloc /= 2;
	}

	return false;
}

static void allocate_expamem (void)
{
  currprefs.fastmem_size = changed_prefs.fastmem_size;
  currprefs.z3fastmem_size = changed_prefs.z3fastmem_size;
	for (int i = 0; i < MAX_RTG_BOARDS; i++) {
		memcpy(&currprefs.rtgboards[i], &changed_prefs.rtgboards[i], sizeof(struct rtgboardconfig));
	}

	z3fastmem_bank.start = currprefs.z3autoconfig_start;
	if (!expamem_z3hack(&currprefs))
		z3fastmem_bank.start = Z3BASE_REAL;
	if (z3fastmem_bank.start == Z3BASE_UAE) {
		if (currprefs.mbresmem_high_size >= 128 * 1024 * 1024)
			z3fastmem_bank.start += (currprefs.mbresmem_high_size - 128 * 1024 * 1024) + 16 * 1024 * 1024;
	}

  if (fastmem_bank.allocated != currprefs.fastmem_size) {
    free_fastmemory (0);

  	fastmem_bank.allocated = currprefs.fastmem_size;
		fastmem_bank.mask = fastmem_bank.allocated - 1;

		fastmem_nojit_bank.allocated = fastmem_bank.allocated;
		fastmem_nojit_bank.mask = fastmem_bank.mask;

  	if (fastmem_bank.allocated) {
			mapped_malloc (&fastmem_bank);
			fastmem_nojit_bank.baseaddr = fastmem_bank.baseaddr;
  		if (fastmem_bank.baseaddr == 0) {
  			write_log (_T("Out of memory for fastmem card.\n"));
  			fastmem_bank.allocated = 0;
				fastmem_nojit_bank.allocated = 0;
  		}
  	}
    memory_hardreset(1);
  }

  if (z3fastmem_bank.allocated != currprefs.z3fastmem_size) {
		mapped_free (&z3fastmem_bank);
		mapped_malloc_dynamic (&currprefs.z3fastmem_size, &changed_prefs.z3fastmem_size, &z3fastmem_bank, 1, _T("z3"));
    memory_hardreset(1);
  }

#ifdef PICASSO96
	struct rtgboardconfig *rbc = &currprefs.rtgboards[0];
	if (gfxmem_bank.allocated != rbc->rtgmem_size) {
		mapped_free (&gfxmem_bank);
		mapped_malloc_dynamic (&rbc->rtgmem_size, &changed_prefs.rtgboards[0].rtgmem_size, &gfxmem_bank, 1, rbc->rtgmem_type ? _T("z3_gfx") : _T("z2_gfx"));
    memory_hardreset(1);
  }
#endif

#ifdef SAVESTATE
  if (savestate_state == STATE_RESTORE) {
  	if (fastmem_bank.allocated > 0) {
      restore_ram (fast_filepos, fastmem_bank.baseaddr);
			if (!fastmem_bank.start) {
				// old statefile compatibility support
				fastmem_bank.start = 0x00200000;
			}
  		map_banks (&fastmem_bank, fastmem_bank.start >> 16, currprefs.fastmem_size >> 16,
  			fastmem_bank.allocated);
  	}
  	if (z3fastmem_bank.allocated > 0) {
      restore_ram (z3_filepos, z3fastmem_bank.baseaddr);
  		map_banks (&z3fastmem_bank, z3fastmem_bank.start >> 16, currprefs.z3fastmem_size >> 16,
  			z3fastmem_bank.allocated);
  	}
#ifdef PICASSO96
    if (gfxmem_bank.allocated > 0 && gfxmem_bank.start > 0) {
      restore_ram (p96_filepos, gfxmem_bank.baseaddr);
			map_banks(&gfxmem_bank, gfxmem_bank.start >> 16, currprefs.rtgboards[0].rtgmem_size >> 16,
        gfxmem_bank.allocated);
  	}
#endif
  }
#endif /* SAVESTATE */
}

static uaecptr check_boot_rom (int *boot_rom_type)
{
  uaecptr b = RTAREA_DEFAULT;
  addrbank *ab;

	*boot_rom_type = 1;
  ab = &get_mem_bank (RTAREA_DEFAULT);
  if (ab) {
  	if (valid_address (RTAREA_DEFAULT, 65536))
	    b = RTAREA_BACKUP;
  }
  if (nr_directory_units (NULL))
  	return b;
  if (nr_directory_units (&currprefs))
  	return b;
	if (currprefs.socket_emu)
		return b;
  if (currprefs.input_tablet > 0)
    return b;
	if (currprefs.rtgboards[0].rtgmem_size)
		return b;
  if (currprefs.chipmem_size > 2 * 1024 * 1024)
    return b;
	*boot_rom_type = 0;
  return 0;
}

uaecptr need_uae_boot_rom (void)
{
  uaecptr v;

  uae_boot_rom_type = 0;
	v = check_boot_rom (&uae_boot_rom_type);
  if (!rtarea_base) {
	  uae_boot_rom_type = 0;
	  v = 0;
  }
  return v;
}

void expamem_reset (void)
{
  int do_mount = 1;

  ecard = 0;
  cardno = 0;
	chipdone = false;

  uae_id = hackers_id;

  allocate_expamem ();
	expamem_bank.name = _T("Autoconfig [reset]");

  if (need_uae_boot_rom() == 0)
    do_mount = 0;
	if (uae_boot_rom_type <= 0)
		do_mount = 0;

  /* check if Kickstart version is below 1.3 */
	if (ks12orolder() && do_mount) {
    /* warn user */
#if KS12_BOOT_HACK
		do_mount = -1;
		if (ks11orolder()) {
			filesys_start = 0xe90000;
			map_banks_z2(&filesys_bank, filesys_start >> 16, 1);
			expamem_init_filesys(0);
			expamem_map_filesys_update();
		}
#else
    write_log (_T("Kickstart version is below 1.3!  Disabling automount devices.\n"));
    do_mount = 0;
#endif
  }

  if (fastmem_bank.baseaddr != NULL && (fastmem_bank.allocated <= 262144 || currprefs.chipmem_size <= 2 * 1024 * 1024)) {
		cards[cardno].flags = 0;
		cards[cardno].name = _T("Z2Fast");
		cards[cardno].initnum = expamem_init_fastcard;
		cards[cardno++].map = expamem_map_fastcard;
  }

#ifdef CD32
	if (currprefs.cs_cd32cd && currprefs.fastmem_size == 0 && currprefs.chipmem_size <= 0x200000 && currprefs.cs_cd32fmv) {
		cards[cardno].flags = 0;
		cards[cardno].name = _T("CD32MPEG");
		cards[cardno].initnum = expamem_init_cd32fmv;
		cards[cardno++].map = expamem_map_cd32fmv;
	}
#endif
#ifdef FILESYS
  if (do_mount) {
		cards[cardno].flags = 0;
		cards[cardno].name = _T("UAEFS");
		cards[cardno].initnum = expamem_init_filesys;
		cards[cardno++].map = expamem_map_filesys;
  }
#endif
#ifdef PICASSO96
	for (int i = 0; i < MAX_RTG_BOARDS; i++) {
		struct rtgboardconfig *rbc = &currprefs.rtgboards[i];
		if (rbc->rtgmem_size && rbc->rtgmem_type == GFXBOARD_UAE_Z2 && gfxmem_bank.baseaddr != NULL) {
	    cards[cardno].flags = 4;
	    cards[cardno].name = _T("Z2RTG");
	    cards[cardno].initnum = expamem_init_gfxcard_z2;
	    cards[cardno++].map = expamem_map_gfxcard_z2;
		}
  }
#endif

	/* Z3 boards last */
	if (!currprefs.address_space_24) {

	  if (z3fastmem_bank.baseaddr != NULL) {
			bool alwaysmapz3 = currprefs.z3_mapping_mode != Z3MAPPING_REAL;
			cards[cardno].flags = 16 | 2 | 1;
			cards[cardno].name = _T("Z3Fast");
			cards[cardno].initnum = expamem_init_z3fastmem;
			cards[cardno++].map = expamem_map_z3fastmem;
			if (alwaysmapz3 || expamem_z3hack(&currprefs))
		    map_banks_z3(&z3fastmem_bank, z3fastmem_bank.start >> 16, currprefs.z3fastmem_size >> 16);
	  }
#ifdef PICASSO96
		if (currprefs.rtgboards[0].rtgmem_size && currprefs.rtgboards[0].rtgmem_type == GFXBOARD_UAE_Z3 && gfxmem_bank.baseaddr != NULL) {
			cards[cardno].flags = 16 | 4 | 1;
			cards[cardno].name = _T("Z3RTG");
			cards[cardno].initnum = expamem_init_gfxcard_z3;
			cards[cardno++].map = expamem_map_gfxcard_z3;
	  }
#endif
  }

	expamem_z3_pointer = 0;
	expamem_z3_sum = 0;
	expamem_z2_pointer = 0;
  if (cardno == 0 || savestate_state)
	  expamem_init_clear_zero ();
  else
		call_card_init(0);
}

void expansion_init (void)
{
	if (savestate_state != STATE_RESTORE) {

    fastmem_bank.allocated = 0;
		fastmem_bank.mask = fastmem_bank.start = 0;
    fastmem_bank.baseaddr = NULL;
		fastmem_nojit_bank.allocated = 0;
		fastmem_nojit_bank.mask = fastmem_nojit_bank.start = 0;
		fastmem_nojit_bank.baseaddr = NULL;

#ifdef PICASSO96
    gfxmem_bank.allocated = 0;
    gfxmem_bank.mask = gfxmem_bank.start = 0;
    gfxmem_bank.baseaddr = NULL;
#endif

    z3fastmem_bank.allocated = 0;
		z3fastmem_bank.mask = z3fastmem_bank.start = 0;
    z3fastmem_bank.baseaddr = NULL;
  }

#ifdef FILESYS
  filesys_start = 0;
#endif

  expamem_lo = 0;
  expamem_hi = 0;
  
  allocate_expamem ();

#ifdef FILESYS
	filesys_bank.allocated = 0x10000;
	if (!mapped_malloc (&filesys_bank)) {
	  write_log (_T("virtual memory exhausted (filesysory)!\n"));
	  abort();
  }
#endif
}

void expansion_cleanup (void)
{
	mapped_free (&fastmem_bank);
	fastmem_nojit_bank.baseaddr = NULL;
	mapped_free (&z3fastmem_bank);

#ifdef PICASSO96
	for (int i = 0; i < MAX_RTG_BOARDS; i++) {
	  mapped_free (&gfxmem_bank);
	}
#endif

#ifdef FILESYS
	mapped_free (&filesys_bank);
#endif
}

static void clear_bank (addrbank *ab)
{
	if (!ab->baseaddr || !ab->allocated)
		return;
	memset (ab->baseaddr, 0, ab->allocated > 0x800000 ? 0x800000 : ab->allocated);
}

void expansion_clear(void)
{
	clear_bank (&fastmem_bank);
	clear_bank (&z3fastmem_bank);
#ifdef PICASSO96
	clear_bank (&gfxmem_bank);
#endif
}

#ifdef SAVESTATE

/* State save/restore code.  */

uae_u8 *save_fram (int *len, int num)
{
  *len = fastmem_bank.allocated;
  return fastmem_bank.baseaddr;
}

uae_u8 *save_zram (int *len, int num)
{
  *len = z3fastmem_bank.allocated;
  return z3fastmem_bank.baseaddr;
}

#ifdef PICASSO96
uae_u8 *save_pram (int *len)
{
  *len = gfxmem_bank.allocated;
  return gfxmem_bank.baseaddr;
}
#endif

void restore_fram (int len, size_t filepos, int num)
{
  fast_filepos = filepos;
  changed_prefs.fastmem_size = len;
}

void restore_zram (int len, size_t filepos, int num)
{
  z3_filepos = filepos;
  changed_prefs.z3fastmem_size = len;
}

void restore_pram (int len, size_t filepos)
{
  p96_filepos = filepos;
	changed_prefs.rtgboards[0].rtgmem_size = len;
}

uae_u8 *save_expansion (int *len, uae_u8 *dstptr)
{
	uae_u8 *dst, *dstbak;
  if (dstptr)
  	dst = dstbak = dstptr;
  else
		dstbak = dst = xmalloc (uae_u8, 6 * 4);
  save_u32 (fastmem_bank.start);
  save_u32 (z3fastmem_bank.start);
#ifdef PICASSO96
  save_u32 (gfxmem_bank.start);
#endif
  save_u32 (rtarea_base);
	save_u32 (fastmem_bank.start);
  *len = dst - dstbak;
  return dstbak;
}

uae_u8 *restore_expansion (uae_u8 *src)
{
	fastmem_bank.start = restore_u32 ();
  z3fastmem_bank.start = restore_u32 ();
#ifdef PICASSO96
  gfxmem_bank.start = restore_u32 ();
#endif
  rtarea_base = restore_u32 ();
  restore_u32 (); /* ignore fastmem_bank.start */
  if (rtarea_base != 0 && rtarea_base != RTAREA_DEFAULT && rtarea_base != RTAREA_BACKUP && rtarea_base != RTAREA_BACKUP_2)
  	rtarea_base = 0;
  return src;
}

#endif /* SAVESTATE */
