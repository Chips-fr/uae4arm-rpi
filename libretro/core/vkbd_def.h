#ifndef VKBD_DEF_H
#define VKBD_DEF_H 1

#include "keyboard.h"

typedef struct {
	char norml[NLETT];
	char shift[NLETT];
	int val;	
} Mvk;

Mvk MVk[NPLGN*NLIGN*2]={

	{ "ESC" ,"ESC"  ,AK_ESC },//0
	{ " 1" ," !" , AK_1 },//0
	{ " 2" ," \"" ,AK_2 },
	{ " 3" ," #"  ,AK_3 },
	{ " 4" ," $"  ,AK_4 },
	{ " 5" ," %"  ,AK_5 },
	{ " 6" ," &"  ,AK_6 },
	{ " 7" ," \'"  ,AK_7 },
	{ " 8" ," ("  ,AK_8 },
	{ " 9" ," )"  ,AK_9 },
	{ " 0" ," _"  ,AK_0 },
	{ " ^" ,"Pnd"  ,AK_NUMBERSIGN },

	{ " q" ," Q"  ,AK_Q}, //10+2
	{ " w" ," W"  ,AK_W},
	{ " e" ," E"  ,AK_E},
	{ " r" ," R"  ,AK_R},
	{ " t" ," T"  ,AK_T},
	{ " y" ," Y"  ,AK_Y},
	{ " u" ," U"  ,AK_U},
	{ " i" ," I"  ,AK_I},
	{ " o" ," O"  ,AK_O},
	{ " p" ," P"  ,AK_P},
	{ " /" ," |"  ,AK_NPDIV},
	{ " *" ," *"  ,AK_NPMUL},

	{ " a" ," A"  ,AK_A}, //20+4
	{ " s" ," S"  ,AK_S},
	{ " d" ," D"  ,AK_D},
	{ " f" ," F"  ,AK_F},
	{ " g" ," G"  ,AK_G},
	{ " h" ," H"  ,AK_H},
	{ " j" ," J"  ,AK_J},
	{ " k" ," K"  ,AK_K},	
	{ " l" ," L"  ,AK_L},
	{ " :" ," *"  ,AK_A},
	{ " ;" ," +"  ,AK_SEMICOLON},
	{ " ]" ," ]"  ,AK_RBRACKET},

	{ " z" ," Z"  ,AK_Z},//30+6
	{ " x" ," X"  ,AK_X},
	{ " c" ," C"  ,AK_C},
	{ " v" ," V"  ,AK_V},
	{ " b" ," B"  ,AK_B},
	{ " n" ," N"  ,AK_N},
	{ " m"," M"   ,AK_M},
	{ " ,"," <"   ,AK_COMMA},
	{ " ."," >"   ,AK_PERIOD},
	{ " /" ," ?"  ,AK_SLASH},
	{ " \\"," \\"   ,AK_BACKSLASH},
	{ "SHFT" ,"SHFT"  ,AK_LSH},

	{ "PG2","PG2" ,-2}, //40+8
	{ "TAB","TAB" ,AK_TAB},
	{ "CPSL" ,"CPSL"  ,AK_CAPSLOCK},
	{ "RET" ,"RET"  ,AK_RET},
	{ "DEL" ,"DEL"  ,AK_DEL},
	{ "CTRL" ,"CTRL"  ,AK_CTRL},
	{ "HLP" ,"HLP" , AK_HELP},
	{ "Spc" ,"Spc",AK_SPC},
	{ "RAMI" ,"RAMI",AK_RAMI},
	{ "LAMI" ,"LAMI",AK_LAMI},
	{ "F0" ,"F0"  ,AK_F10},
	{ "Ent" ,"Ent",AK_ENT},


	{ "ESC" ,"ESC"  ,AK_ESC },//50+10
	{ " 1" ," !" , AK_1 },
	{ " 2" ," \"" ,AK_2 },
	{ " 3" ," #"  ,AK_3 },
	{ " 4" ," $"  ,AK_4 },
	{ " 5" ," %"  ,AK_5 },
	{ " 6" ," &"  ,AK_6 },
	{ " 7" ," \'"  ,AK_7 },
	{ " 8" ," ("  ,AK_8 },
	{ " 9" ," )"  ,AK_9 },
	{ " 0" ," _"  ,AK_0 },
	{ " (" ," ("  ,AK_NPLPAREN },

	{ " F7" ," F7"  ,AK_F7}, //60+12
	{ " F8" ," F8"  ,AK_F8},
	{ " F9" ," F9"  ,AK_F9},
	{ " F0" ," F0"  ,AK_F10},
	{ " t" ," T"  ,AK_T},
	{ " /\\" ," /\\"  ,AK_UP},
	{ " u" ," U"  ,AK_U},
	{ " i" ," I"  ,AK_I},
	{ " o" ," O"  ,AK_O},
	{ " p" ," P"  ,AK_P},
	{ " )" ," )"  ,AK_NPRPAREN},
	{ " [" ," ["  ,AK_LBRACKET},

	{ " F4" ," F4"  ,AK_F4}, //70+14
	{ " F5" ," F5"  ,AK_F5},
	{ " F6" ," F6"  ,AK_F6},
	{ " \"" ," \""  ,AK_QUOTE},
	{ " <-" ," <-"  ,AK_LF},
	{ "LALT" ,"LALT"  ,AK_LALT},
	{ " ->" ," ->"  ,AK_RT},
	{ " k" ," K"  ,AK_K},	
	{ "np-" ,"np-"  ,AK_NPSUB},
	{ "RALT" ,"RALT"  ,AK_RALT},
	{ " -" ," +"  ,AK_MINUS},
	{ " =" ," ="  ,AK_EQUAL},

	{ " F1" ," F1"  ,AK_F1},//80+16
	{ " F2" ," F2"  ,AK_F2},
	{ " F3" ," F3"  ,AK_F3},
	{ "Ent" ,"Ent"  ,AK_ENT},
	{ "ndel" ,"ndel"  ,AK_NPDEL},
	{ " \\/" ," \\/"  ,AK_DN},
	{ "np+","np+"   ,AK_NPADD},
	{ " ,"," <"   ,AK_COMMA},
	{ " ."," >"   ,AK_PERIOD},
	{ "TAPE" ,"TAPE"  ,-8},
	{ "EXIT","EXIT"   ,-6},
	{ "SNA" ,"SNA"  ,-7},


	{ "PG1","PG1"  ,-2},//90+18
	{ "DSK","DSK"  ,-5},
	{ "GUI","GUI"  ,-13},
	{ "COL" ,"COL",-3},
	{ "CTRL" ,"CTRL" ,AK_CTRL},
	{ "SPC" ,"SPC" ,AK_SPC},
	{ "RSH" ,"RSH" ,AK_RSH},
	{ "ESC","ESC",AK_ESC},
	{ "LTGT" ,"LTGT",AK_LTGT},
	{ "DEL" ,"DEL",AK_DEL},
	{ "Ret" ,"Ret",AK_RET},
	{ "KBD" ,"KBD",-4},

} ;


#endif
