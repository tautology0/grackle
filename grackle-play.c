#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#define HEADER_END 0xA54F
#define DICTIONARY_START 0xa5d
#define ROOM_START 0x10d

#define TOKEN_HYPHENATE       0x20
#define TOKEN_LOWERCASE       0x40
#define TOKEN_UPPERCASE       0x80
#define TOKEN_PUNCTUATION     0xc0

#define SPEC_PUNCTUATION      0x6201
#define SPEC_START            0xA51F
#define SPEC_OFFSET           0x3FE5
#define SPEC_ROOMDATA         0xA54D

#define CPCAMSDOS_PUNCTUATION 0x214d
#define CPCAMSDOS_START       0x4000
#define CPCAMSDOS_OFFSET     -0x40
#define CPCAMSDOS_ROOMDATA    0x4018

#define CPCSNA_PUNCTUATION    0x220d
#define CPCSNA_START          0x4000
#define CPCSNA_OFFSET        -0x100
#define CPCSNA_ROOMDATA       0x4018

#define C64_PUNCTUATION       0x7ec0
#define C64_START             0x83a
#define C64_OFFSET            0
#define C64_ROOMDATA          0x850

#define BBC_PUNCTUATION       0
#define BBC_START            -0x14
#define BBC_OFFSET           -0x14
#define BBC_ROOMDATA          0

// Standard message numbers
#define ASK             240
#define CANTDO          241
#define NOTUNDERSTAND   242
#define RESTART         243
#define YOUSURE         244
#define ALREADYHAVE     245
#define DONTHAVE        246
#define CANTSEE         247
#define TOOMUCH         248
#define YOURSCORE       249
#define YOUTOOK         250
#define ITSDARK         251
#define CANTFIND        252
#define OBJHERE         253
#define OKAY            254
#define TURNS           255

#define TYPE_SPECTRUM   1
#define TYPE_CPCAMSDOS  2
#define TYPE_CPC        3
#define TYPE_BBC        4
#define TYPE_C64        5
#define TYPE_CPCSNA     6

// Globals - I'm doing this for laziness
char *dictionary[65535];
char *messages[1024];
char stack[64][1024];
int stackint[64];
int sp=0,spint=0;

// header
typedef struct
{
   unsigned char type;
   unsigned int datastart;
   unsigned int roomdata;
   unsigned int offset;
   int verbs, verbptr;
   int nouns, nounptr;
   int adverbs, adverbptr;
   int objects, objectptr;
   int rooms, roomptr;
   int hpc, hpcptr;
   int locc, loccptr;
   int lpc, lpcptr;
   int messages, messageptr;
   int dicts, dictptr;
   int startroom, startptr;
   int images, imageptr;
   int endptr;
} header_struct;

// no need for structures for dictionary, messages
// as they're just arrays
typedef struct
{
   char word[60];
   unsigned char number;
} word_struct;

typedef struct
{
   int object;
   char description[255];
   int weight;
   int start;
   int location;
} object_struct;

typedef struct
{
   int direction;
   int destination;
} exit_struct;

typedef struct
{
   int room;
   char description[512];
   int picture;
   int noexits;
   exit_struct *exits[16];
} room_struct;

typedef struct
{
   char name[6];
   int parameters;
   int returns;
} directive_struct;

typedef struct
{
   int room;
   unsigned char condition[255];
   int length;
} condition_struct;

typedef struct
{
   int room;
   unsigned char flags[255];
   unsigned char counters[255];
   int maxweight;
   int verb;
   int adverb;
   int noun1;
   int noun2;
   unsigned char moved;
   unsigned char finished;
   unsigned char done;
   unsigned char iftrue;
   unsigned char force_exits;
} run_struct;

directive_struct directives[64]=
{
   { "OP0",0,0 },  { "AND",2,1 },   { "OR",2,1 },    { "NOT",1,1 },
   { "XOR",2,1 },  { "HOLD",1,0 },  { "GET",1,0 },   { "DROP",1,0 },
   { "SWAP",2,0 }, { "TO",2,0 },    { "OBJ",1,0 },   { "SET",1,0 },
   { "RESE",1,0 }, { "SET?",1,1 },  { "RES?",1,1 },  { "CSET",2,0 },
   { "CTR",1,1 },  { "DECR",1,0 },  { "INCR",1,0 },  { "EQU?",2,1 },
   { "DESC",1,0 }, { "LOOK",0,0 },  { "MESS",1,0 },  { "PRIN",1,0 },
   { "RAND",1,1 }, { "<",2,1 },     { ">",2,1 },     { "=",2,1 },
   { "SAVE",0,0 }, { "LOAD",0,0 },  { "HERE",1,1 },  { "AVAI",1,1 },
   { "CARR",1,1 }, { "+",2,1 },     { "-",2,1 },     { "TURN",0,1 },
   { "AT",1,1 },   { "BRIN",1,0 },  { "FIND",1,0 },  { "IN",2,1 },
   { "OP28",1,0 }, { "OP29",0,0 },  { "OKAY",0,0 },  { "WAIT",0,0 },
   { "QUIT",0,0 }, { "EXIT",0,0 },  { "ROOM",0,1 },  { "NOUN",1,1 },
   { "VERB",1,1 }, { "ADVE",1,1 },  { "GOTO",1,0 },  { "NO1",0,1 },
   { "NO2",0,1 },  { "VBNO",0,1 },  { "LIST",1,0 },  { "PICT",0,0 },
   { "TEXT",0,0 }, { "CONN",1,1 },  { "WEIG",1,1 },  { "WITH",0,1 },
   { "STRE",1,0 }, { "LF",0,0 },    { "IF",1,0 },    { "END",0,0 }
};

unsigned char get8bit(infile)
FILE *infile;
{
   int low;

   low=fgetc(infile);

   return low;
}

int get16bit(infile)
FILE *infile;
{
   int low, high;

   low=fgetc(infile);
   high=fgetc(infile);

   return (high << 8) + low;
}

int readtokenised(infile)
FILE *infile;
{
   int low, high;

   low=fgetc(infile);
   high=fgetc(infile);

   return ((high & 0x7f) << 8) + low;
}

char punctuation[]="\0 .,-!?:";
char *exitdecode[]={"","North","South","East","West","Up","Down"};
header_struct *header;
word_struct *verbs[255];
word_struct *nouns[255];
word_struct *adverbs[255];
object_struct *objects[255];
room_struct *rooms[9999];
condition_struct *hpc[512];
condition_struct *lpc[512];
condition_struct *locc[512];

void push(w)
char *w;
{
   if (sp < 64) strcpy(stack[sp++],w);
}

char *pop()
{
   if (sp > 0) return stack[--sp];
   return "";
}
void pushint(w)
int w;
{
   if (spint < 64) stackint[spint++]=w;
}

int popint()
{
   if (spint > 0) return stackint[--spint];
   return 0;
}
void resetint()
{
   spint=0;
}

unsigned char detecttype(infile)
FILE *infile;
{
   int i, ptr, flag;

   // Check where the punc string is on each file, first Spectrum
   ptr = fseek(infile, SPEC_PUNCTUATION, SEEK_SET);
   if (!ptr)
   {
      flag=1;
      for (i=1;i<strlen(punctuation+1);i++)
      {
         if (punctuation[i] != fgetc(infile))
         {
            flag=0;
            break;
         }
      }
      if (flag)
      {
         header->datastart=SPEC_START;
         header->offset=SPEC_OFFSET;
         header->roomdata=SPEC_ROOMDATA;
         return TYPE_SPECTRUM;
      }
   }

   // CPC
   ptr = fseek(infile, CPCAMSDOS_PUNCTUATION, SEEK_SET);
   if (!ptr)
   {
      flag=1;
      for (i=1;i<strlen(punctuation+1);i++)
      {
         if (punctuation[i] != fgetc(infile))
         {
            flag=0;
            break;
         }
      }
      if (flag)
      {
         header->datastart=CPCAMSDOS_START;
         header->offset=CPCAMSDOS_OFFSET;
         header->roomdata=CPCAMSDOS_ROOMDATA;
         return TYPE_CPCAMSDOS;
      }
   }

   ptr = fseek(infile, CPCSNA_PUNCTUATION, SEEK_SET);
   if (!ptr)
   {
      flag=1;
      for (i=1;i<strlen(punctuation+1);i++)
      {
         if (punctuation[i] != fgetc(infile))
         {
            flag=0;
            break;
         }
      }
      if (flag)
      {
         header->datastart=CPCSNA_START;
         header->offset=CPCSNA_OFFSET;
         header->roomdata=CPCSNA_ROOMDATA;
         return TYPE_CPCSNA;
      }
   }
   
   // Check it's C64 - the VSF size can be variable - so check first
   ptr = fseek(infile, 0, SEEK_SET);
   if (fgetc(infile) == 'V' && fgetc(infile) == 'I')
   {
      int c64off = 0;
      ptr = fseek(infile, 0x37, SEEK_SET);

      c64off = 0x25 + fgetc(infile) + 0x1a;
      //printf("%x\n", c64off);
      
      //ptr = fseek(infile, C64_PUNCTUATION + c64off, SEEK_SET);
      if (!ptr)
      {
         flag=1;
         // Needs fixing: the punctuation string seems to not be constant.
         //for (i=1;i<strlen(punctuation+1);i++)
         //{
         //  if (punctuation[i] != fgetc(infile))
         //   {
         //      flag=0;
         //      break;
         //   }
         //}
         if (flag)
         {
            header->datastart=C64_START;
            header->offset=0-c64off;
            header->roomdata=C64_ROOMDATA;
            return TYPE_C64;
         }
      }
   }

   // else assume it's a BBC
   header->datastart=BBC_START;
   header->offset=BBC_OFFSET;
   header->roomdata=BBC_ROOMDATA;
   return TYPE_BBC;
}

void readheader(infile, header)
FILE *infile;
header_struct *header;
{
   int scrap;
   fseek(infile,header->datastart - header->offset,SEEK_SET);

   // read each of the header entries - skip the counts for now as we don't know them!
   // verbptr is always static (at the end of the header)
   header->nounptr = get16bit(infile) - header->offset;
   header->adverbptr = get16bit(infile) - header->offset;
   header->objectptr = get16bit(infile) - header->offset;
   header->roomptr = get16bit(infile) - header->offset;
   header->hpcptr = get16bit(infile) - header->offset;
   header->loccptr = get16bit(infile) - header->offset;
   header->lpcptr = get16bit(infile) - header->offset;
   header->messageptr = get16bit(infile) - header->offset;
   if (header->type == TYPE_CPCAMSDOS || header->type == TYPE_CPC || header->type==TYPE_CPCSNA)
   {
      // skip the next 16bit number
      scrap = get16bit(infile);
   }
   if (header->type != TYPE_BBC)
   {
      header->imageptr = get16bit(infile) - header->offset;
   }
   header->dictptr = get16bit(infile) - header->offset;
   if (header->type != TYPE_BBC)
   {
      header->endptr = get16bit(infile) - header->offset;
      header->startptr = header->roomdata - header->offset;
      header->verbptr = header->roomdata + 2 - header->offset;
      if (header->type == TYPE_C64) { header->verbptr += 4; }
   }
   else
   {
      header->startptr = get16bit(infile) - header->offset;
      header->verbptr = 0 - header->offset;
      header->endptr = header->startptr;
   }
   if (header->type == TYPE_CPCAMSDOS || header->type == TYPE_CPC || header->type==TYPE_CPCSNA)
   {
      // for some reason the verbptr is also 0x4100;
      header->verbptr = 0x4140;
      if (header->type==TYPE_CPCSNA)
      {
         header->verbptr += 0xc0;
      }
   }
}

int readdictionary(infile, header)
FILE *infile;
header_struct *header;
{
   int endptr; // signifies when we have reached the end - i.e. the location of the startptr
   unsigned char mess, mess2, outb; //temporary char variables
   unsigned char len, i, j; //length of the string
   int currenttoken=0; // the current token
   fpos_t pos;

   printf("Reading dictionary\n");
   // go to the start of the dictionary
   fseek(infile,header->dictptr, SEEK_SET);
   endptr = header->endptr;

   do
   {
      // blank the current token;
      memset(dictionary[currenttoken],'\0',254);
      j=0;
      len=get8bit(infile);
      i=0;
      do
      {
         mess=get8bit(infile);
         i++;
         // blank out any formatting characters to a space
         if (mess & 0x80 && header->type==TYPE_BBC)
         { // top bit set is compressed
            mess2=get8bit(infile);
            i++;

            outb=0;
            outb=(mess & 0x7f) >> 2;
            outb|=0x40;
            dictionary[currenttoken][j++]=outb;
            outb=0;
            outb=(mess & 0x03) << 3;
            outb|=(mess2 & 0xe0) >> 5;
            outb|=0x40;
            dictionary[currenttoken][j++]=outb;
            outb=0;
            outb=(mess2 & 0x1f);
            outb|=0x40;
            dictionary[currenttoken][j++]=outb;
         }
         else
         {
            if ((mess & 0x7f)<32) { mess=32; }
            if ((mess > 0x7f)) { mess &= 0x7f; }
            dictionary[currenttoken][j++]=mess;
         }
      } while (i<len);
      currenttoken++;
   } while (!feof(infile) && len!=0 && fgetpos(infile,&pos)<endptr);

   return currenttoken-1;
}

int readwords(infile, header, words, startptr, endptr)
FILE *infile;
header_struct *header;
word_struct **words;
int startptr, endptr;
{
   unsigned char count,temp; // keeps a count of the number of entries in the dictionary
   int dictentry, j; //temporary char variables
   int current=0; // the current token
   fpos_t pos;

   fseek(infile, startptr, SEEK_SET);
   j=startptr;
   temp=1;
   do
   {
      count=get8bit(infile);
      if (count==0)
      {
         fgetpos(infile, &pos);
         temp=get8bit(infile);
         fsetpos(infile, &pos);
      }
      if (count!=0 && temp!=0)
      {
         dictentry=readtokenised(infile);
         j+=3;
         strncpy(words[current]->word,dictionary[dictentry],60);
         words[current]->number=count;
         current++;
      }
   } while (!feof(infile) && j<endptr && count!=0 && temp!=0);

   return current;
}

char *readstring(infile, size)
FILE *infile;
int size;
{
   int low, high, end=0;
   int len, first, i, punc;
   char working[255],scrap[1];
   char *result;

   result=calloc(1,255);

   memset(result,'\0',254);
   memset(working,'\0',254);
   first=1;
   len=0;
   do
   {
      low=get8bit(infile);
      high=get8bit(infile);
      size-=2;
      memset(working,'\0',254);
      // First check whether we have reached the end of the string
      if ((high & TOKEN_PUNCTUATION) == 0xc0)
      { // is punctuation
         if (low == 0) end=1;
         for (i=0;i<low;i++)
         {
            punc=(high & 0x38) >> 3;
            if (punc)
            {
               scrap[0]=punctuation[punc];
               strncat(working,scrap,1);
            }
            if (punc==0) end=1;
         }
      }
      else
      {
         //if (!first) strncpy(working," ",1);
         strncat(working,dictionary[(high & 0x7)*256 + low],255);
         first=0;

         if (high & TOKEN_LOWERCASE)
         {  // token is lowercase
            for (i=0;i<strlen(working);i++)
            {
               working[i]=tolower(working[i]);
            }
         }
         else if ((high & 0xc0) == 0)
         { // first character is uppercase
            for (i=(working[0]==' '?2:1);i<strlen(working);i++)
            {
               working[i]=tolower(working[i]);
            }
         }
         // Add punctuation to the end
         punc=(high & 0x38) >> 3;
         if (punc)
         {
            scrap[0]=punctuation[punc];
            strncat(working,scrap,1);
         }
         if (punc==0) end=1;
      }
      strncat(result,working,255);
   } while (size>0 && end==0);

   return result;
}

int readobjects(infile, header, objects, startptr, endptr)
FILE *infile;
header_struct *header;
object_struct **objects;
int startptr, endptr;
{
   int object,size, weight, start, scrap; // keeps a count of the number of entries in the dictionary
   int j, len; //temporary char variables
   int current=0; // the current token
   int fileptr; // temporary save the start of the previous object

   fseek(infile, startptr, SEEK_SET);
   j=startptr;
   do
   {
      fileptr=ftell(infile);
      object=get8bit(infile);
      size=get8bit(infile);
      weight=get8bit(infile);
      start=get8bit(infile);
      scrap=get8bit(infile);
      start+=scrap<<8;
      size-=3;
      j+=5;
      if (object!=0 && size!=0)
      {
         objects[current]->object=object;
         objects[current]->weight=weight;
         objects[current]->start=start;
         objects[current]->location=start;
         len=0;
         strcat(objects[current]->description,readstring(infile, size));
         j+=size;
         j+=3;
         current++;
      }
      // move up to next object
      fseek(infile,fileptr+size+5,SEEK_SET);
   } while (!feof(infile) && ftell(infile)<endptr);

   return current-1;
}

int readrooms(infile, header, rooms, startptr, endptr)
FILE *infile;
header_struct *header;
room_struct **rooms;
int startptr, endptr;
{
   int room, curexit, scrap; // keeps a count of the number of entries in the dictionary
   int j, len; //temporary char variables
   int current=0; // the current token

   fseek(infile, startptr, SEEK_SET);
   j=startptr;
   do
   {
      room=get16bit(infile);
      if (room!=0)
      {
         rooms[current]->room=room;
         len=get16bit(infile);
         rooms[current]->picture=get16bit(infile);
         j+=6;
         len-=2;

         // exits
         curexit=0;
         do
         {
            scrap=get8bit(infile);
            len--;
            j++;
            if (scrap != 0)
            {
               rooms[current]->exits[curexit]=calloc(1,sizeof(exit_struct));
               rooms[current]->exits[curexit]->direction=scrap;
               rooms[current]->exits[curexit]->destination=get16bit(infile);
               j+=2;
               len-=2;
               curexit++;
            }
         } while (scrap != 0);
         rooms[current]->noexits=curexit;


         // get description
         j+=5;
         strcat(rooms[current]->description,readstring(infile, len));
         j+=len;
         j+=3;
         current++;
      }

   } while (!feof(infile) && ftell(infile)<endptr);

   return current-1;
}

int readmessages(infile, header, messages, startptr, endptr)
FILE *infile;
header_struct *header;
char **messages;
int startptr, endptr;
{
   int message; // keeps a count of the number of entries in the dictionary
   int i, j, len; //temporary char variables
   int fileptr;
   char *scrap;

   scrap=calloc(1,255);

   fseek(infile, startptr, SEEK_SET);
   j=startptr;
   i=0;
   do
   {
      message=get8bit(infile);
      len=get8bit(infile);
      fileptr=ftell(infile);
      if (message==0 && len==0) break;
      if (messages[message][0]==0)
      {
         strncpy(messages[message],readstring(infile,len),255);
      }
      else
      {
         scrap=readstring(infile,len);
      }
      i++;
      fseek(infile,fileptr+len,SEEK_SET);
   } while (!feof(infile) && ftell(infile)<endptr);

   return i-1;
}

int readconditions(infile, header, conditions, startptr, endptr)
FILE *infile;
header_struct *header;
condition_struct **conditions;
int startptr, endptr;
{
   int byte;
   int i, j; //temporary char variables
   unsigned char isnumber; // flag to ensure the number 0x3f doesn't get taken as the token END

   fseek(infile, startptr, SEEK_SET);
   j=1;
   i=0;
   isnumber=0;
   do
   {
      byte=get8bit(infile);
      conditions[j]->condition[i++]=byte;
      if (byte == 0x3f && !isnumber)
      {
         conditions[j]->length=i;
         j++;
         i=0;
      }
      isnumber=0;
      if (byte &0x80) isnumber=1;
   } while (!feof(infile) && ftell(infile)<endptr);

   return j;
}

int readroomconditions(infile, header, conditions, startptr, endptr)
FILE *infile;
header_struct *header;
condition_struct **conditions;
int startptr, endptr;
{
   int byte, obyte;
   int i, j, room; //temporary char variables
   unsigned char isnumber; // flag to ensure the number 0x3f doesn't get taken as the token END

   fseek(infile, startptr, SEEK_SET);
   j=1;
   i=0;
   isnumber=0;
   room=get16bit(infile);
   conditions[j]->room=room;
   //printf("Room: %d ",room);
   do
   {
      byte=get8bit(infile);
      conditions[j]->condition[i++]=byte;
      //printf("%02x ",byte);
      if ((byte == 0x3f || byte == 0) && !isnumber)
      {
         obyte=byte;
         conditions[j]->length=i;
         j++;
         i=0;
         byte=get8bit(infile);
         if (obyte == 0) room=byte+(get8bit(infile)*256);
         if (byte != 0)
         {
            conditions[j]->room=room;
            if (obyte != 0) conditions[j]->condition[i++]=byte;
         }
         else
         {
            if (room!=0)
            {
               room=get16bit(infile);
               conditions[j]->room=room;
            }
         }
         //printf("\nRoom: %d ",room);
      }
      isnumber=0;
      if (byte & 0x80) isnumber=1;
   } while (ftell(infile)<endptr && room!=0);
   return j;
}

int readstartroom(infile, header, startptr)
FILE *infile;
header_struct *header;
int startptr;
{
   int startroom;

   fseek(infile, startptr, SEEK_SET);
   startroom=get16bit(infile);
   return startroom;
}

char *find_word(word, words, max, result)
int word;
word_struct **words;
int max;
char *result;
{
   int i=0;

   while(word != words[i]->number && i<max)
   {
      i++;
   }

   if (i==max)
   {
      strcpy(result,"NONE");
   }
   else
   {
      strcpy(result, words[i]->word);
   }

   return result;
}

int find_object(object, objects, max)
int object;
object_struct **objects;
int max;
{
   int i=0;
   int found=254;

   for (i=0; i<=max;i++)
   {
      if (objects[i]->object == object)
      {
         found=i;
      }
   }

   return found;
}

int find_room(room, rooms, max)
int room;
room_struct **rooms;
int max;
{
   int i=0;
   int found=254;

   for (i=0; i<=max;i++)
   {
      if (rooms[i]->room == room)
      {
         found=i;
      }
   }

   return found;
}

int find_wordnumber(word, words, max)
char *word;
word_struct **words;
int max;
{
   int i=0, found=0, j=0;
   char sub_word[255], match_word[255];
   char punc[]="_'-";

   strcpy(match_word,word);

   for (j=0; j<strlen(punc);j++)
   {
      if (strchr(word,punc[j]))
      {
         match_word[strchr(word,punc[j])-word]='\0';
      }
   }

   for (i=0;i<max;i++)
   {
      if (!strcmp(match_word, words[i]->word)) found=words[i]->number;
      for (j=0; j<strlen(punc);j++)
      {
         if (strchr(words[i]->word,punc[j]))
         {
            strncpy(sub_word,words[i]->word,strchr(words[i]->word,punc[j])-words[i]->word);
            sub_word[strchr(words[i]->word,punc[j])-words[i]->word]='\0';
            if (!strcmp(match_word, sub_word)) found=words[i]->number;
         }
      }
      if (found!=0) break;
   }

   return found;
}


// Convert a condition string to text
char *condition_text(decoded,condition,length)
char *decoded;
char *condition;
int length;
{
   unsigned int i,number;
   unsigned char byte;
   char working[2048], working2[2048], working3[2048],working4[2048];

   memset(decoded,'\0',2048);

   for (i=0;i<length;i++)
   {
      byte=condition[i];
      if (byte & 0x80)
      {
         i++;
         number=(byte & 0x7F) * 256 + (unsigned char)condition[i];
         sprintf(working,"%u",number);
         push(working);
      }
      else //look up in the directives tables
      {
         sprintf(working,"%s ",directives[byte].name);
         if (directives[byte].parameters == 1)
         {
            strcpy(working2,pop());
            if (strcmp(directives[byte].name,"VERB") == 0)
            {
               find_word(atoi(working2),verbs,header->verbs,working3);
               sprintf(working4,"VERB %s:\"%s\"",working2,working3);
               strcpy(working2,working4);
            }
            if (strcmp(directives[byte].name,"NOUN") == 0)
            {
               find_word(atoi(working2),nouns,header->nouns,working3);
               sprintf(working4,"NOUN %s:\"%s\"",working2,working3);
               strcpy(working2,working4);
            }
            if (strcmp(directives[byte].name,"ADVE") == 0)
            {
               find_word(atoi(working2),adverbs,header->adverbs,working3);
               sprintf(working4,"ADVERB %s:\"%s\"",working2,working3);
               strcpy(working2,working4);
            }
            if (strcmp(directives[byte].name,"AT") == 0   ||
                strcmp(directives[byte].name,"GOTO") == 0)
            {
               sprintf(working3,"ROOM %s:\"%s\"",working2,rooms[find_room(atoi(working2),rooms,header->rooms)]->description);
               strcpy(working2,working3);
            }
            if (strcmp(directives[byte].name,"GET") == 0  ||
                strcmp(directives[byte].name,"DROP") == 0 ||
                strcmp(directives[byte].name,"CARR") == 0 ||
                strcmp(directives[byte].name,"AVAI") == 0 ||
                strcmp(directives[byte].name,"BRIN") == 0 ||
                strcmp(directives[byte].name,"HERE") == 0)
            {
               sprintf(working3,"OBJECT %s:\"%s\"",working2,objects[find_object(atoi(working2),objects,header->objects)]->description);
               strcpy(working2,working3);
            }
            if (strcmp(directives[byte].name,"MESS") == 0)
            {
               sprintf(working3,"MESSAGE %s:\"%s\"",working2,messages[atoi(working2)]);
               strcpy(working2,working3);
            }
            sprintf(working,"%s(%s)",working,working2);
         }
         else if (directives[byte].parameters == 2)
         {
            strcpy(working2,pop());
            strcpy(working3,pop());
            // working4 exists to stop memory corruption on working
            if (strcmp(directives[byte].name,"IN") == 0)
            {
               sprintf(working4,"(OBJECT: %s: \"%s\" %s ROOM:%s: \"%s\") ",
                  working3,
                  objects[find_object(atoi(working3),objects,header->objects)]->description,
                  working,
                  working2,
                  rooms[find_room(atoi(working2),rooms,header->rooms)]->description);
            }
            else
            {
               sprintf(working4,"(%s %s%s) ",working3,working,working2);
            }
            strcpy(working,working4);
         }
         if (directives[byte].returns)
         {
            push(working);
         }
         else
         {
            sprintf(decoded,"%s %s",decoded,working);
         }
      }
   }

   return decoded;
}

// more laziness
void assignmemory(chararray, size, number)
void **chararray;
int size, number;
{
   int i;
   for (i=0;i<number;i++)
   {
      chararray[i]=calloc(1,size);
   }
}

void display_room(room, adventure_status)
int room;
run_struct *adventure_status;
{
   int i, set=0, here=0;
   char objhere[255];

   // Check whether there's light
   if (adventure_status->flags[1] == 0 && adventure_status->flags[2] == 0)
   {
      // In a dark room
      printf("%s\n",messages[ITSDARK]);
   }
   else
   {
      here=find_room(room,rooms,header->rooms);
      // print description
      printf("%s",rooms[here]->description);

      strcpy(objhere,messages[OBJHERE]);
      // display any objects
      for (i=0;i<=header->objects;i++)
      {
         if (objects[i]->location == room)
         {
            if (set) strncat(objhere,",",255);
            strncat(objhere, objects[i]->description,255);
            set=1;
         }
      }
      if (set) printf("%s",objhere);
      if (header->type == TYPE_BBC)
      {
         printf("\n");
      }
      if (adventure_status->force_exits)
      {
         printf("\n");
         for (i=0;i<rooms[here]->noexits;i++)
         {
            printf("You can go %s\n",find_word(rooms[here]->exits[i]->direction,verbs,header->verbs,objhere));
         }
      }
   }
}

// Convert a condition string to text
int do_condition(condition,length, adventure_status, docmd)
char *condition;
int length;
run_struct *adventure_status;
int docmd;
{
   unsigned int i,number,j,param1,param2, playerweight, true;
   unsigned char byte, answer;
   //unsigned char decoded[1024];

   true=0;
   //printf("%s \n",condition_text(decoded,condition,length));
   for (i=0;i<length;i++)
   {
      byte=condition[i];
      if (byte & 0x80)
      {
         i++;
         number=(byte & 0x7F) * 256 + (unsigned char)condition[i];
         pushint(number);
      }
      else //look up in the directives tables
      {
         switch(byte)
         {
            case (0): // OP0
               break;

            case (1): // AND
               param1=popint();
               param2=popint();
               pushint(param1 & param2);
               break;

            case (2): // OR
               param1=popint();
               param2=popint();
               pushint(param1 | param2);
               break;

            case (3): // NOT
               param1=popint();
               pushint(!param1);
               break;

            case (4): // XOR
               param1=popint();
               param2=popint();
               pushint(param1 ^ param2);
               break;

            case (5): // HOLD
               if (docmd==0) break;
               param1=popint();
               //printf("Press any key to continue");
               break;

            case (6): //GET
               if (docmd==0) break;
               param1=popint();
               // First check object is present
               true=find_object(param1,objects,header->objects);
               if (objects[true]->location == adventure_status->room)
               {
                  // Check object weight
                  playerweight=0;
                  for (j=0;j<=header->objects;j++)
                  {
                     if (objects[j]->location==255) playerweight+=objects[j]->weight;
                  }
                  if (playerweight + objects[true]->weight >= adventure_status->maxweight)
                  {
                     printf("%s\n",messages[TOOMUCH]);
                     docmd=0;
                  }
                  else
                  {
                     objects[true]->location=255;
                  }
               }
               else
               {
                  printf("%s\n",messages[CANTSEE]);
                  docmd=0;
               }
               break;

            case (7): // DROP
               if (docmd==0) break;
               param1=popint();
               true=find_object(param1,objects,header->objects);
               // First check object is present
               if (objects[true]->location == 255)
               {
                  objects[true]->location=adventure_status->room;
               }
               else
               {
                  printf("%s\n",messages[DONTHAVE]);
                  docmd=0;
               }
               break;

            case (8): // SWAP
               if (docmd==0) break;
               param1=popint();
               param2=popint();
               true=find_object(param1,objects,header->objects);
               number=find_object(param2,objects,header->objects);
               j=objects[true]->location;
               objects[true]->location=objects[number]->location;
               objects[number]->location=j;
               break;

            case (9): // TO
               if (docmd==0) break;
               param1=popint();
               param2=popint();
               number=find_object(param2,objects,header->objects);
               objects[number]->location=param1;
               break;

            case (10): // OBJ
               if (docmd==0) break;
               param1=popint();
               true=find_object(param1,objects,header->objects);
               printf("%s\n",objects[true]->description);
               break;

            case (11): // SET
               if (docmd==0) break;
               param1=popint();
               adventure_status->flags[param1]=1;
               break;

            case (12): // RESE
               if (docmd==0) break;
               param1=popint();
               adventure_status->flags[param1]=0;
               break;

            case (13): // SET?
               param1=popint();
               if (adventure_status->flags[param1])
               {
                  pushint(1);
               }
               else
               {
                  pushint(0);
               }
               break;

            case (14): // RES?
               param1=popint();
               if (!adventure_status->flags[param1])
               {
                  pushint(1);
               }
               else
               {
                  pushint(0);
               }
               break;

            case (15): // CSET
               if (docmd==0) break;
               param1=popint();
               param2=popint();
               adventure_status->counters[param1]=param2;
               break;

            case (16): // CTR
               param1=popint();
               pushint(adventure_status->counters[param1]);
               break;

            case (17): // DECR
               if (docmd==0) break;
               param1=popint();
               if (adventure_status->counters[param1]>0) adventure_status->counters[param1]--;
               break;

            case (18): // INCR
               if (docmd==0) break;
               param1=popint();
               if (adventure_status->counters[param1]<255) adventure_status->counters[param1]++;
               break;

            case (19): // EQU?
               param1=popint();
               param2=popint();
               if (adventure_status->counters[param1]==param2)
               {
                  pushint(1);
               }
               else
               {
                  pushint(0);
               }
               break;

            case (20): // DESC
               if (docmd==0) break;
               param1=popint();
               display_room(param1,adventure_status);
               break;

            case (21): // LOOK
               if (docmd==0) break;
               display_room(adventure_status->room,adventure_status);
               break;

            case (22): // MESS
               if (docmd==0) break;
               param1=popint();
               printf("%s",messages[param1]);
               break;

            case (23): // PRIN
               if (docmd==0) break;
               param1=popint();
               printf("%d",param1);
               break;

            case (24): // RAND
               param1=popint();
               j=rand() % param1;
               pushint(j);
               break;

            case (25): // <
               param1=popint();
               param2=popint();
               if (param2 < param1)
               {
                  pushint(1);
               }
               else
               {
                  pushint(0);
               }
               break;

            case (26): // >
               param1=popint();
               param2=popint();
               if (param2 > param1)
               {
                  pushint(1);
               }
               else
               {
                  pushint(0);
               }
               break;

            case (27): // =
               param1=popint();
               param2=popint();
               if (param1 == param2)
               {
                  pushint(1);
               }
               else
               {
                  pushint(0);
               }
               break;

            case (28): // SAVE
            case (29): // LOAD
               if (docmd==0) break;
               printf("Not implemented (yet).\n");
               break;

            case (30): // HERE
               param1=popint();
               true=find_object(param1,objects,header->objects);
               if (objects[true]->location == adventure_status->room)
               {
                  pushint(1);
               }
               else
               {
                  pushint(0);
               }
               break;

            case (31): // AVAI
               param1=popint();
               true=find_object(param1,objects,header->objects);
               if (objects[true]->location == adventure_status->room ||
                   objects[true]->location == 255)
               {
                  pushint(1);
               }
               else
               {
                  pushint(0);
               }
               break;

            case (32): // CARR
               param1=popint();
               true=find_object(param1,objects,header->objects);
               if (objects[true]->location == 255)
               {
                  pushint(1);
               }
               else
               {
                  pushint(0);
               }
               break;

            case (33): // +
               param1=popint();
               param2=popint();
               pushint(param2 + param1);
               break;

            case (34): // -
               param1=popint();
               param2=popint();
               pushint(param2 - param1);
               break;

            case (35): // TURN
               pushint(adventure_status->counters[127]+adventure_status->counters[126]*256);
               break;

            case (36): // AT
               param1=popint();
               if (adventure_status->room == param1)
               {
                  pushint(1);
               }
               else
               {
                  pushint(0);
               }
               break;

            case (37): // BRIN
               if (docmd==0) break;
               param1=popint();
               true=find_object(param1,objects,header->objects);
               objects[true]->location=adventure_status->room;
               break;

            case (38): // FIND
               if (docmd==0) break;
               param1=popint();
               true=find_object(param1,objects,header->objects);
               adventure_status->room=objects[true]->location;
               break;

            case (39): // AT
               param1=popint();
               param2=popint();
               true=find_object(param2,objects,header->objects);
               if (objects[true]->location == param1)
               {
                  pushint(1);
               }
               else
               {
                  pushint(0);
               }
               break;

            case (40): // OP28
            case (41): // OP29
               if (docmd==0) break;
               printf("Illegal Operation.\n");
               break;

            case (42): // OKAY
               if (docmd==0) break;
               printf("%s\n",messages[OKAY]);
               adventure_status->done=1;
               break;

            case (43): // WAIT
               if (docmd==0) break;
               adventure_status->done=1;
               break;

            case (44): // QUIT
               if (docmd==0) break;
               printf("%s",messages[YOUSURE]);
               answer=tolower(getc(stdin));
               if (answer == 'n')
               {
                  break;
               }
            case (45): // EXIT
               if (docmd==0) break;
               adventure_status->finished=1;
               break;

            case (46): // ROOM
               pushint(adventure_status->room);
               break;

            case (47): // NOUN
               param1=popint();
               true=0;
               if (adventure_status->noun1==param1 || adventure_status->noun2==param1) true=1;
               pushint(true);
               break;

            case (48): // VERB
               param1=popint();
               true=0;
               if (adventure_status->verb==param1) true=1;
               pushint(true);
               break;

            case (49): // ADVERB
               param1=popint();
               true=0;
               if (adventure_status->adverb==param1) true=1;
               pushint(true);
               break;

            case (50): // GOTO
               if (docmd==0) break;
               param1=popint();
               adventure_status->room=param1;
               display_room(param1,adventure_status);
               break;

            case (51): // NO1
               pushint(adventure_status->noun1);
               break;

            case (52): // NO2
               pushint(adventure_status->noun2);
               break;

            case (53): // VBNO
               pushint(adventure_status->verb);
               break;

            case (54): // LIST
               if (docmd==0) break;
               param1=popint();
               for (j=0;j<=header->objects;j++)
               {
                  if (objects[j]->location==param1)
                  {
                     printf("%s\n",objects[j]->description);
                  }
               }
               break;

            case (55): // PICT
            case (56): // TEXT
               if (docmd==0) break;
               printf("Not implemented (yet).\n");
               break;

            case (57): // CONN
               param1=popint();
               true=0;
               number=find_room(adventure_status->room,rooms,header->rooms);
               for (j=0;j<rooms[number]->noexits;j++)
               {
                  if(rooms[number]->exits[j]->direction == param1) true=rooms[number]->exits[j]->destination;
               }
               pushint(true);
               break;

            case (58): // WEIG
               param1=popint();
               pushint(objects[param1]->weight);
               break;

            case (59): // WITH
               pushint(255);
               break;

            case (60): // STRE
               if (docmd==0) break;
               param1=popint();
               adventure_status->maxweight=param1;
               break;

            case (61): // LF
               if (docmd==0) break;
               printf("\n");
               break;

            case (62): // IF
               param1=popint();
               if (param1 == 1)
               {
                  docmd=1;
                  adventure_status->iftrue=1;
               }
               else
               {
                  docmd=0;
               }
               break;

            case (63): // END
               docmd=1;
               resetint();
               break;
         }
      }
   }
   return docmd;
}

void perform_conditions(condition, count, adventure_status, exitdone)
condition_struct **condition;
int count;
unsigned char exitdone;
run_struct *adventure_status;
{
   int i;
   // This is done to stop multiple conditions firing if moves happen within a condition
   int room=adventure_status->room;
   // docmd is set here to sustain it through multiline conditions
   int docmd=1;
   // reset stack ere we start
   resetint();

   // loop through all conditions and apply them
   for (i=1; i<count; i++)
   {
      if (condition[i]->room == 0 || condition[i]->room == room)
      {
         docmd=do_condition(condition[i]->condition,condition[i]->length, adventure_status, docmd);
      }
      if (adventure_status->done && exitdone) break;
   }
}

int get_input(adventure_status)
run_struct *adventure_status;
{
   char command[255];
   int i,matched=0;
   char *word, *sub_word;

   word=malloc(255);
   sub_word=malloc(255);

   adventure_status->verb=adventure_status->adverb=0;
   adventure_status->noun1=adventure_status->noun2=0;

   input:
   do
   {
      printf("\n%s ", messages[ASK]);
      fgets(command, 250, stdin);
   } while (strlen(command) < 2);

   for (i=0; i<strlen(command); i++)
   {
      command[i]=toupper(command[i]);
   }

   if (!strcmp(command,"!STATUS\n"))
   {
      printf("Current Room: %d\n",adventure_status->room);
      printf("Non-zero counters:\n");
      for (i=0; i<255;i++)
      {
         if (adventure_status->counters[i]>0)
         {
            printf("Counter %d: %d\n",i, adventure_status->counters[i]);
         }
      }
      printf("Set flags:\n");
      for (i=0; i<255;i++)
      {
         if (adventure_status->flags[i]>0)
         {
            printf("flag %d: %d\n",i, adventure_status->flags[i]);
         }
      }
      goto input;
   }
   if (!strcmp(command,"!EXITS\n"))
   {
      printf("Showing exits\n");
      adventure_status->force_exits=1; // for debugging
      goto input;
   }

   word=strtok(command," \n");
   while (word != NULL)
   {
      matched=0;
      if (!adventure_status->verb && !matched)
      {
         adventure_status->verb=find_wordnumber(word, verbs, header->verbs);
         matched=adventure_status->verb;
      }
      // check noun1 in case the word is duplicated in adverbs and nouns
      if (!adventure_status->noun1 && !matched)
      {
         adventure_status->noun1=find_wordnumber(word, nouns, header->nouns);
         matched=adventure_status->noun1;
      }
      if (!adventure_status->adverb && !matched)
      {
         adventure_status->adverb=find_wordnumber(word, adverbs, header->adverbs);
         matched=adventure_status->adverb;
      }
      if (!adventure_status->noun2 && adventure_status->noun1 && !matched)
      {
         adventure_status->noun2=find_wordnumber(word, nouns, header->nouns);
      }
      word=strtok(NULL," \n");
   }
   return (adventure_status->verb>0 || adventure_status->noun1>0);
}

void run_adventure(void)
{
   // Structures for talling the adventure
   run_struct adventure_status;
   int i,number;
   int parsed=0;

   // blank all counters and flags
   for (i=0;i<255;i++)
   {
      adventure_status.counters[i]=0;
      adventure_status.flags[i]=0;
   }
   adventure_status.room=header->startroom;
   adventure_status.maxweight=255;
   // Set light to initially be on
   adventure_status.flags[1]=1;
   adventure_status.moved=1;
   adventure_status.finished=0;
   adventure_status.iftrue=0;
   adventure_status.force_exits=0; // for debugging
   do
   {
      if (adventure_status.moved)
      {
         display_room(adventure_status.room, &adventure_status);
         adventure_status.moved=0;
      }

      // increase turn count
      if (adventure_status.counters[127]<255)
      {
         adventure_status.counters[127]++;
      }
      else
      {
         adventure_status.counters[126]++;
         adventure_status.counters[127]=0;
      }

      adventure_status.done=0;
      perform_conditions(hpc,header->hpc,&adventure_status,0);
      if (adventure_status.finished) break;
      if (!adventure_status.moved)
      {
         do
         {
            parsed=get_input(&adventure_status);
            if (!parsed)
            {
               printf("%s\n",messages[NOTUNDERSTAND]);
            }
         } while (parsed==0);
         // First deal with connections
         // Check for a connection
         number=find_room(adventure_status.room,rooms,header->rooms);
         for (i=0;i<rooms[number]->noexits;i++)
         {
            if (rooms[number]->exits[i]->direction==adventure_status.verb)
            {
               adventure_status.room=rooms[number]->exits[i]->destination;
               adventure_status.moved=1;
               break;
            }
         }
         if (adventure_status.moved)
         {
            continue;
         }

         adventure_status.iftrue=0;
         adventure_status.done=0;
         perform_conditions(locc,header->locc,&adventure_status,1);
         if (adventure_status.moved || adventure_status.done) continue;

         adventure_status.done=0;
         perform_conditions(lpc,header->lpc,&adventure_status,1);
         if (adventure_status.moved || adventure_status.done) continue;
         if (!adventure_status.iftrue)
         {
            if (adventure_status.verb==0)
            {
               printf("%s\n",messages[NOTUNDERSTAND]);
            }
            else
            {
               printf("%s\n",messages[CANTDO]);
            }
         }
      }
   } while (!adventure_status.finished);

   // Print score if required
   if (adventure_status.flags[3] == 0)
   {
      printf("\n%s%d%s%d%s",messages[YOURSCORE],adventure_status.counters[0],
                            messages[YOUTOOK],adventure_status.counters[126]*256+adventure_status.counters[127],
                            messages[TURNS]);
   }
   printf("\n");

}

int main(int argc, char **argv)
{
   char infilename[256];
   FILE *infile;
   int dump=0;
   int startroom=0, opt;
   int i,j,type=0;
   char decoded[2048];

   header=calloc(1,sizeof(header_struct));
   assignmemory(dictionary, 255, 65535);
   assignmemory(verbs,sizeof(word_struct), 255);
   assignmemory(nouns,sizeof(word_struct), 255);
   assignmemory(adverbs,sizeof(word_struct), 255);
   assignmemory(objects,sizeof(object_struct), 255);
   assignmemory(rooms,sizeof(room_struct), 9999);
   assignmemory(hpc,sizeof(condition_struct), 512);
   assignmemory(lpc,sizeof(condition_struct), 512);
   assignmemory(locc,sizeof(condition_struct), 512);
   assignmemory(messages, 255, 512);

   while ((opt = getopt(argc, argv, "pls:t:")) != -1)
   {
      switch (opt)
      {
         case 'l':
            dump=1;
            break;
         case 'p':
            // Play - default behaviour do nowt
            break;
         case 's':
            startroom=atoi(optarg);
            break;
         case 't':
            type=atoi(optarg);
            if (type > 5)
            {
               printf("Unknown type: %optarg\n",type);
            }
            break;
         default:
            fprintf(stderr, "Usage: %s [-p|-l] [-s room] file\n", argv[0]);
            exit(1);
      }
   }
   
   if (optind >= argc)
   {
      fprintf(stderr, "No file name provided\n");
      exit(1);
   }
   strcpy(infilename,argv[optind]);

   infile=fopen(infilename,"rb");
   if (infile == NULL)
   {
      fprintf(stderr,"Could not open file %s.\n",argv[1]);
      exit(1);
   }

   header->type=type;
   if (type == 0)
   {
      header->type=detecttype(infile, header);
   }
   printf("Type: %d\n",header->type);
   readheader(infile, header);
   /*printf("Header value for verbs %x\n", header->verbptr);
   printf("Header value for nouns %x\n", header->nounptr);
   printf("Header value for adverbs %x\n", header->adverbptr);
   printf("Header value for objects %x\n", header->objectptr);
   printf("Header value for rooms %x\n", header->roomptr);
   printf("Header value for hpcs %x\n", header->hpcptr);
   printf("Header value for loccs %x\n", header->loccptr);
   printf("Header value for lpcs %x\n", header->lpcptr);
   printf("Header value for messages %x\n", header->messageptr);
   printf("Header value for images %x\n", header->imageptr);
   printf("Header value for dictionary %x\n", header->dictptr);
   printf("Header value for end %x\n", header->endptr);
   printf("Header value for start room %x\n", header->startptr);
   printf("Header value for offset %d\n", header->offset);*/

   // now read everything - start with the dictionary as everything else uses it
   header->dicts=readdictionary(infile, header);
   printf("Reading verbs: %x\n", header->verbptr);
   header->verbs=readwords(infile,header,verbs,header->verbptr,header->nounptr);
   printf("Reading nouns: %x\n", header->nounptr);
   header->nouns=readwords(infile,header,nouns,header->nounptr,header->adverbptr);
   printf("Reading adverbs: %x\n",header->adverbptr);
   header->adverbs=readwords(infile,header,adverbs,header->adverbptr,header->objectptr);
   printf("Reading objects\n");
   header->objects=readobjects(infile,header,objects,header->objectptr,header->roomptr);
   printf("Reading rooms: %x\n",header->roomptr);
   header->rooms=readrooms(infile,header,rooms,header->roomptr,header->hpcptr);
   printf("Reading messages: %x\n",header->messageptr);
   header->messages=readmessages(infile,header,messages,header->messageptr,header->dictptr);
   printf("Reading high priority conditions\n");
   header->hpc=readconditions(infile,header,hpc,header->hpcptr,header->loccptr);
   printf("Reading low priority conditions: %x\n",header->lpcptr);
   header->lpc=readconditions(infile,header,lpc,header->lpcptr,header->messageptr);
   printf("Reading room conditions: %x\n",header->loccptr);
   header->locc=readroomconditions(infile,header,locc,header->loccptr,header->lpcptr);
   printf("Reading start room\n");
   if (header->type == TYPE_BBC)
   {
      header->startroom=1;
   }
   else
   {   
      header->startroom=readstartroom(infile,header,header->startptr);
   }
   if (startroom > 0) header->startroom=startroom;

   if (dump)
   {
      printf("Number of dictionary items: %d\n",header->dicts);
      printf("Number of verbs: %d\n",header->verbs);
      printf("Number of nouns: %d\n",header->nouns);
      printf("Number of adverbs: %d\n",header->adverbs);
      printf("Number of objects: %d\n",header->objects);
      printf("Number of rooms: %d\n",header->rooms);
      printf("Number of messages: %d\n",header->messages);
      printf("Number of high priority conditions: %d\n",header->hpc);
      printf("Number of room conditions: %d\n",header->locc);
      printf("Number of low priority conditions: %d\n",header->lpc);
      i=find_room(header->startroom,rooms,header->rooms);
      printf("Starting room: %d (%s)\n",header->startroom, rooms[i]->description);

      for (i=0; i<header->verbs; i++) printf("Verb %d: %s\n",verbs[i]->number,verbs[i]->word);
      for (i=0; i<header->nouns; i++) printf("Noun %d: %s\n",nouns[i]->number,nouns[i]->word);
      for (i=0; i<header->adverbs; i++) printf("Adverb %d: %s\n",adverbs[i]->number,adverbs[i]->word);
      for (i=0; i<=header->objects; i++)
      {
         j=find_room(objects[i]->start,rooms,header->rooms);
         printf("Object %d: %s Weight: %d Starts in: %d: %s\n",objects[i]->object,
                                                 objects[i]->description,
                                                 objects[i]->weight,
                                                 objects[i]->start,
                                                 rooms[j]->description);
      }
      for (i=0; i<=header->rooms; i++)
      {
         printf("Room %d: %s %d\n",rooms[i]->room,rooms[i]->description, rooms[i]->picture);
         printf("Exits: %d\n", rooms[i]->noexits);
         for (j=0;j<rooms[i]->noexits;j++)
         {
            printf("%s %d\n",find_word(rooms[i]->exits[j]->direction,verbs,header->verbs,decoded),rooms[i]->exits[j]->destination);
         }
      }
      for (i=1; i<=255; i++)
      {
         if (strlen(messages[i])>0) printf("Message %d: %s\n",i,messages[i]);
      }
      for (i=1; i<header->hpc; i++)
      {
         printf("High priority condition %d: ",i);
         printf("%s ",condition_text(decoded,hpc[i]->condition,hpc[i]->length));
         printf("\n");
      }
      for (i=1; i<header->lpc; i++)
      {
         printf("Low priority condition %d: ",i);
         printf("%s ",condition_text(decoded,lpc[i]->condition,lpc[i]->length));
         printf("\n");
      }
      for (i=1; i<header->locc; i++)
      {
         printf("Room condition %d for room %d: ",i,locc[i]->room);
         printf("%s ",condition_text(decoded,locc[i]->condition,locc[i]->length));
         printf("\n");
      }
   }

   fclose(infile);
   if (!dump) run_adventure();
   return 0;
}
