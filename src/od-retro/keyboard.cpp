#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "uae.h"
#include "memory.h"
#include "xwin.h"
#include "custom.h"
#include "drawing.h"
#include "keyboard.h"
#include "keybuf.h"
#include "gui.h"

#include "libretro.h"
extern int SHIFTON;

/* Decode KeySyms. This function knows about all keys that are common
 * between different keyboard languages. */
static int kc_decode (int prKeySym)
{
#ifdef PANDORA
  // Special handling of Pandora keyboard:
  // Some keys requires shift on Amiga, so we simulate shift...

  switch (prKeySym)
  {
    case RETROK_QUESTION:
      return SIMULATE_SHIFT | AK_SLASH;
    case RETROK_HASH:
      return SIMULATE_SHIFT | AK_3;
    case RETROK_DOLLAR:
      return SIMULATE_SHIFT | AK_4;
    case RETROK_QUOTEDBL:
      return SIMULATE_SHIFT | AK_QUOTE;
    case RETROK_PLUS:
      return SIMULATE_SHIFT | AK_EQUAL;
    case RETROK_AT:
      return SIMULATE_SHIFT | AK_2;
    case RETROK_LEFTPAREN:
      return SIMULATE_SHIFT | AK_9;
    case RETROK_RIGHTPAREN:
      return SIMULATE_SHIFT | AK_0;
    case RETROK_EXCLAIM:
      return SIMULATE_SHIFT | AK_1;
    case RETROK_UNDERSCORE:
      return SIMULATE_SHIFT | AK_MINUS;
    case RETROK_2:
      if(SHIFTON)
        return SIMULATE_SHIFT | AK_LBRACKET;
      break;
    case RETROK_3:
      if(SHIFTON)
        return SIMULATE_SHIFT | AK_RBRACKET;
      break;
    case RETROK_4:
      if(SHIFTON)
        return SIMULATE_SHIFT | AK_BACKQUOTE;
      break;
    case RETROK_9:
      if(SHIFTON)
        return SIMULATE_RELEASED_SHIFT | AK_LBRACKET;
      break;
    case RETROK_0:
      if(SHIFTON)
        return SIMULATE_RELEASED_SHIFT | AK_RBRACKET;
      break;
    case 124: // code for '|'
      return SIMULATE_SHIFT | AK_BACKSLASH;
  }
#endif
    switch (prKeySym)
    {
    case RETROK_b: return AK_B;
    case RETROK_c: return AK_C;
    case RETROK_d: return AK_D;
    case RETROK_e: return AK_E;
    case RETROK_f: return AK_F;
    case RETROK_g: return AK_G;
    case RETROK_h: return AK_H;
    case RETROK_i: return AK_I;
    case RETROK_j: return AK_J;
    case RETROK_k: return AK_K;
    case RETROK_l: return AK_L;
    case RETROK_n: return AK_N;
    case RETROK_o: return AK_O;
    case RETROK_p: return AK_P;
    case RETROK_r: return AK_R;
    case RETROK_s: return AK_S;
    case RETROK_t: return AK_T;
    case RETROK_u: return AK_U;
    case RETROK_v: return AK_V;
    case RETROK_x: return AK_X;

    case RETROK_0: return AK_0;
    case RETROK_1: return AK_1;
    case RETROK_2: return AK_2;
    case RETROK_3: return AK_3;
    case RETROK_4: return AK_4;
    case RETROK_5: return AK_5;
    case RETROK_6: return AK_6;
    case RETROK_7: return AK_7;
    case RETROK_8: return AK_8;
    case RETROK_9: return AK_9;

    case RETROK_KP0: return AK_NP0;
    case RETROK_KP1: return AK_NP1;
    case RETROK_KP2: return AK_NP2;
    case RETROK_KP3: return AK_NP3;
    case RETROK_KP4: return AK_NP4;
    case RETROK_KP5: return AK_NP5;
    case RETROK_KP6: return AK_NP6;
    case RETROK_KP7: return AK_NP7;
    case RETROK_KP8: return AK_NP8;
    case RETROK_KP9: return AK_NP9;
    case RETROK_KP_DIVIDE: return AK_NPDIV;
    case RETROK_KP_MULTIPLY: return AK_NPMUL;
    case RETROK_KP_MINUS: return AK_NPSUB;
    case RETROK_KP_PLUS: return AK_NPADD;
    case RETROK_KP_PERIOD: return AK_NPDEL;
    case RETROK_KP_ENTER: return AK_ENT;

    case RETROK_F1: return AK_F1;
    case RETROK_F2: return AK_F2;
    case RETROK_F3: return AK_F3;
    case RETROK_F4: return AK_F4;
    case RETROK_F5: return AK_F5;
    case RETROK_F6: return AK_F6;
    case RETROK_F7: return AK_F7;
    case RETROK_F8: return AK_F8;
    case RETROK_F9: return AK_F9;
    case RETROK_F10: return AK_F10;

    case RETROK_BACKSPACE: return AK_BS;
    case RETROK_DELETE: return AK_DEL;
    case RETROK_LCTRL: return AK_CTRL;
    case RETROK_RCTRL: return AK_RCTRL;
    case RETROK_TAB: return AK_TAB;
    case RETROK_LALT: return AK_LALT;
    case RETROK_RALT: return AK_RALT;
    case RETROK_RMETA: return AK_RAMI;
    case RETROK_LMETA: return AK_LAMI;
    case RETROK_RETURN: return AK_RET;
    case RETROK_SPACE: return AK_SPC;
    case RETROK_LSHIFT: return AK_LSH;
    case RETROK_RSHIFT: return AK_RSH;
    case RETROK_ESCAPE: return AK_ESC;

    case RETROK_INSERT: return AK_HELP;
    case RETROK_HOME: return AK_NPLPAREN;
    case RETROK_END: return AK_NPRPAREN;
    case RETROK_CAPSLOCK: return AK_CAPSLOCK;

    case RETROK_UP: return AK_UP;
    case RETROK_DOWN: return AK_DN;
    case RETROK_LEFT: return AK_LF;
    case RETROK_RIGHT: return AK_RT;

    case RETROK_PAGEUP: return AK_RAMI;          /* PgUp mapped to right amiga */
    case RETROK_PAGEDOWN: return AK_LAMI;        /* PgDn mapped to left amiga */

    default: return -1;
    }
}

/*
 * Handle keys specific to French (and Belgian) keymaps.
 *
 * Number keys are broken
 */
static int decode_fr (int prKeySym)
{
    switch(prKeySym) {
	case RETROK_a:		return AK_Q;
	case RETROK_m:		return AK_SEMICOLON;
	case RETROK_q:		return AK_A;
	case RETROK_y:		return AK_Y;
	case RETROK_w:		return AK_Z;
	case RETROK_z:		return AK_W;
	case RETROK_LEFTBRACKET:	return AK_LBRACKET;
	case RETROK_RIGHTBRACKET: return AK_RBRACKET;
	case RETROK_COMMA:	return AK_M;
	case RETROK_LESS:
	case RETROK_GREATER:	return AK_LTGT;
	case RETROK_PERIOD:
	case RETROK_SEMICOLON:	return AK_COMMA;
	case RETROK_RIGHTPAREN:	return AK_MINUS;
	case RETROK_EQUALS:	return AK_SLASH;
	case RETROK_HASH:		return AK_NUMBERSIGN;
	case RETROK_SLASH:	return AK_PERIOD;      // Is it true ?
	case RETROK_MINUS:	return AK_EQUAL;
	case RETROK_BACKSLASH:	return AK_SLASH;
	case RETROK_COLON:	return AK_PERIOD;
	case RETROK_EXCLAIM:	return AK_BACKSLASH;   // Not really...
	default: printf("Unknown key: %i\n",prKeySym); return -1;
    }
}

/*
 * Handle keys specific to German keymaps.
 */
static int decode_de (int prKeySym)
{
    switch(prKeySym) {
	case RETROK_a:		return AK_A;
	case RETROK_m:		return AK_M;
	case RETROK_q:		return AK_Q;
	case RETROK_w:		return AK_W;
	case RETROK_y:		return AK_Z;
	case RETROK_z:		return AK_Y;
	case RETROK_COLON:	return SIMULATE_SHIFT | AK_SEMICOLON;
#if 0
	/* German umlaut oe */
	case RETROK_WORLD_86:	return AK_SEMICOLON;
	/* German umlaut ae */
	case RETROK_WORLD_68:	return AK_QUOTE;
	/* German umlaut ue */
	case RETROK_WORLD_92:	return AK_LBRACKET;
#endif
	case RETROK_PLUS:
	case RETROK_ASTERISK:	return AK_RBRACKET;
	case RETROK_COMMA:	return AK_COMMA;
	case RETROK_PERIOD:	return AK_PERIOD;
	case RETROK_LESS:
	case RETROK_GREATER:	return AK_LTGT;
	case RETROK_HASH:		return AK_NUMBERSIGN;
#if 0
	/* German sharp s */
	case RETROK_WORLD_63:	return AK_MINUS;
#endif
	case RETROK_QUOTE:	return AK_EQUAL;
	case RETROK_CARET:	return AK_BACKQUOTE;
	case RETROK_MINUS:	return AK_SLASH;
	default: return -1;
    }
}


static int decode_us (int prKeySym)
{
    switch(prKeySym)
    {
	/* US specific */
    case RETROK_a: return AK_A;
    case RETROK_m: return AK_M;
    case RETROK_q: return AK_Q;
    case RETROK_y: return AK_Y;
    case RETROK_w: return AK_W;
    case RETROK_z: return AK_Z;
    case RETROK_COLON:   return SIMULATE_SHIFT | AK_SEMICOLON;
    case RETROK_LEFTBRACKET: return AK_LBRACKET;
    case RETROK_RIGHTBRACKET: return AK_RBRACKET;
    case RETROK_COMMA: return AK_COMMA;
    case RETROK_PERIOD: return AK_PERIOD;
    case RETROK_SLASH: return AK_SLASH;
    case RETROK_SEMICOLON: return AK_SEMICOLON;
    case RETROK_MINUS: return AK_MINUS;
    case RETROK_EQUALS: return AK_EQUAL;
	/* this doesn't work: */
    case RETROK_BACKQUOTE: return AK_QUOTE;
    case RETROK_QUOTE: return AK_BACKQUOTE;
    case RETROK_BACKSLASH: return AK_BACKSLASH;
    default: return -1;
    }
}

int keycode2amiga(int prKeySym)
{
  int iAmigaKeycode = kc_decode(prKeySym);
  if (iAmigaKeycode == -1)
    return decode_us(prKeySym);
  return iAmigaKeycode;
}

int getcapslockstate (void)
{
    return 0;
}

void setcapslockstate (int state)
{
}
