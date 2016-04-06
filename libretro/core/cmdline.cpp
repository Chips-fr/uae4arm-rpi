#include <ctype.h>

//Args for experimental_cmdline
static char ARGUV[64][1024];
static unsigned char ARGUC=0;

// Args for Core
static char XARGV[64][1024];
static const char* xargv_cmd[64];
int PARAMCOUNT=0;

extern int  skel_main(int argc, char *argv[]);
void parse_cmdline( const char *argv );

void Add_Option(const char* option)
{
   static int first=0;

   if(first==0)
   {
      PARAMCOUNT=0;	
      first++;
   }

   sprintf(XARGV[PARAMCOUNT++],"%s\0",option);
}

int pre_main(const char *argv)
{
   int i;
   bool Only1Arg;

   parse_cmdline(argv); 

   Only1Arg = (strcmp(ARGUV[0],"uaearm") == 0) ? 0 : 1;

   for (i = 0; i<64; i++)
      xargv_cmd[i] = NULL;


   if(Only1Arg)

   {  
	char tmpstr[512];

	  Add_Option("uaearm");
	//  Add_Option("-cpctype"); //FIXME:first not taken in account so some padding
	//  Add_Option("2");
/*
	  if (strlen(RPATH) >= strlen("crt")){

		if(!strcasecmp(&RPATH[strlen(RPATH)-strlen("crt")], "crt"))
			Add_Option("-cart");
		else if (!strcasecmp(&RPATH[strlen(RPATH)-strlen("cpr")], "cpr"))
			Add_Option("-cart");
		else if (!strcasecmp(&RPATH[strlen(RPATH)-strlen("cdt")], "cdt"))
			Add_Option("-tape");
		else if (!strcasecmp(&RPATH[strlen(RPATH)-strlen("wav")], "wav"))
			Add_Option("-tape");
		else if (!strcasecmp(&RPATH[strlen(RPATH)-strlen("tzx")], "tzx"))
			Add_Option("-tape");
		else if (!strcasecmp(&RPATH[strlen(RPATH)-strlen("voc")], "voc"))
			Add_Option("-tape");
		else if (!strcasecmp(&RPATH[strlen(RPATH)-strlen("czw")], "czw"))
			Add_Option("-tape");
		else if(!strcasecmp(&RPATH[strlen(RPATH)-strlen("sna")], "sna"))
			Add_Option("-snapshot");
		else if (!strcasecmp(&RPATH[strlen(RPATH)-strlen("drv")], "drv"))
			Add_Option("-drivea");

		else
	  		Add_Option("-drivea");
	  }
*/
	if (strlen(RPATH) >= strlen("uae")){
		if(!strcasecmp(&RPATH[strlen(RPATH)-strlen("uae")], "uae"))
		{
			Add_Option("-f"); 
			Add_Option(RPATH);
		}
		else if(!strcasecmp(&RPATH[strlen(RPATH)-strlen("adf")], "adf"))
		{
			Add_Option("-s");
			sprintf(tmpstr,"floppy0=%s\0",RPATH);
			Add_Option(tmpstr);
		}
		else if(!strcasecmp(&RPATH[strlen(RPATH)-strlen("hdf")], "hdf"))
		{
			Add_Option("-s");
			sprintf(tmpstr,"hardfile=rw,32,1,2,512,%s\0",RPATH);
			Add_Option(tmpstr);
		}
		else
		{
			Add_Option("-s");
			sprintf(tmpstr,"floppy0=%s\0",RPATH);
			Add_Option(tmpstr);
			//Add_Option(RPATH/*ARGUV[0]*/)

		}

	}

      //Add_Option(RPATH/*ARGUV[0]*/);

   }
   else
   { // Pass all cmdline args
      for(i = 0; i < ARGUC; i++)
         Add_Option(ARGUV[i]);
   }

   for (i = 0; i < PARAMCOUNT; i++)
   {
      xargv_cmd[i] = (char*)(XARGV[i]);
      LOGI("%2d  %s\n",i,XARGV[i]);
   }

   skel_main(PARAMCOUNT,( char **)xargv_cmd); 

   xargv_cmd[PARAMCOUNT - 2] = NULL;
}

void parse_cmdline(const char *argv)
{
	char *p,*p2,*start_of_word;
	int c,c2;
	static char buffer[512*4];
	enum states { DULL, IN_WORD, IN_STRING } state = DULL;
	
	strcpy(buffer,argv);
	strcat(buffer," \0");

	for (p = buffer; *p != '\0'; p++)
   {
      c = (unsigned char) *p; /* convert to unsigned char for is* functions */
      switch (state)
      {
         case DULL: /* not in a word, not in a double quoted string */
            if (isspace(c)) /* still not in a word, so ignore this char */
               continue;
            /* not a space -- if it's a double quote we go to IN_STRING, else to IN_WORD */
            if (c == '"')
            {
               state = IN_STRING;
               start_of_word = p + 1; /* word starts at *next* char, not this one */
               continue;
            }
            state = IN_WORD;
            start_of_word = p; /* word starts here */
            continue;
         case IN_STRING:
            /* we're in a double quoted string, so keep going until we hit a close " */
            if (c == '"')
            {
               /* word goes from start_of_word to p-1 */
               //... do something with the word ...
               for (c2 = 0,p2 = start_of_word; p2 < p; p2++, c2++)
                  ARGUV[ARGUC][c2] = (unsigned char) *p2;
               ARGUC++; 

               state = DULL; /* back to "not in word, not in string" state */
            }
            continue; /* either still IN_STRING or we handled the end above */
         case IN_WORD:
            /* we're in a word, so keep going until we get to a space */
            if (isspace(c))
            {
               /* word goes from start_of_word to p-1 */
               //... do something with the word ...
               for (c2 = 0,p2 = start_of_word; p2 <p; p2++,c2++)
                  ARGUV[ARGUC][c2] = (unsigned char) *p2;
               ARGUC++; 

               state = DULL; /* back to "not in word, not in string" state */
            }
            continue; /* either still IN_WORD or we handled the end above */
      }	
   }
}

