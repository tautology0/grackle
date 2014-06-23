/* Routine to draw all the graphics from The Golden Baton
   At the moment all the important values are hard coded -
   in the fseek() is the pointed to the 0xff 0x00 at the
   start of the image data after the Object Data.

   '32' in the for is the number of rooms hardcoded in!

   Code by David Lodge 19/01/2000
*/
#include <stdio.h>
#include <stdlib.h>
#include <allegro.h>

// Laziness
typedef struct
{
	unsigned int address;
	unsigned int number;
} image_struct;
image_struct images[512];
int colour[4];

void doubledraw(bmp, x, y, c)
BITMAP *bmp;
int x,y,c;
{
   putpixel(bmp,x*2,y,c);
   putpixel(bmp,(x*2)+1,y,c);
}

// Draw a line using the colours fc1 and fc2
void fillline(bmp, x1, y, x2, fc1, fc2)
BITMAP *bmp;
int x1,y,x2,fc1, fc2;
{
   int i=0,colour=fc1;

   for (i=x1; i <= x2; i+=2)
   {
      if ((i % 4) < 2)
      {
         colour=fc1;
      }
      else
      {
         colour=fc2;
      }
      putpixel(bmp,i,y,colour);
      putpixel(bmp,i+1,y,colour);
   }
   //readkey();
}

// Simplistic floodfill to map the simple linear fill that the code seems to use
// p is the pattern:

void gac_floodfill(bmp, x, y, fc1, fc2)
BITMAP *bmp;
int x,y,fc1,fc2;
{
   int colour;
   int x1=x,x2=x;
   int i;
   int top, bottom;

   // First start at defined point

   colour=getpixel(bmp,x,y);
   // Find the boundaries
   // Up (remember that PC origin is wrong - so we go negative)
   top=y;
   bottom=y;
   while (getpixel(bmp,x,top) == colour && top>0) top--;
   while (getpixel(bmp,x,bottom) == colour && bottom<127) bottom++;
   top++; bottom--;

   for (i=top; i<=bottom; i++)
   {
		x1=x; x2=x;
		// Find x limits
	   while (getpixel(bmp,x1,i) == colour && x1 != 0) x1--;
	   while (getpixel(bmp,x2,i) == colour && x2 != 254) x2++;
	   x1++;x2--;
		if ((i%2) == 1)
		{
			fillline(bmp, x1, i, x2, fc1, fc2);
		}
		else
		{
			fillline(bmp, x1, i, x2, fc2, fc1);
		}
   }
}

int plotimage(int start, FILE* infile, FILE* outfile, BITMAP *work, int ignore)
{
	int c,x,y,ox,oy,i;
	int mess=0, image;
	int end;
   int fileptr;
   int fc1,fc2;
	int address=0;

	fprintf(outfile,"Location: %x\n",start);
	fseek(infile,start,SEEK_SET);

	// First get the header
	mess=fgetc(infile)+(fgetc(infile)*256);
	fprintf(outfile,"Rendering picture %d\n",mess);
	mess=fgetc(infile)+(fgetc(infile)*256);
	fprintf(outfile,"Size %d\n",mess);
	end=ftell(infile)+mess-4;
	fprintf(outfile,"End %x\n",end);
	mess=fgetc(infile);
	if (!ignore)
	{
		colour[0]=(mess&0xf0) >> 4;
		colour[1]=mess&0x0f;
	}
	mess=fgetc(infile);
	if (!ignore)
	{
		colour[2]=(mess&0xf0) >> 4;
		colour[3]=mess&0x0f;
	}
	fprintf(outfile,"Colours: %d %d %d %d\n",colour[0],colour[1],colour[2],colour[3]);
	c=3;

	do
	{
		mess=fgetc(infile);
		if (mess & 0x80)
		{ // it's an instruction
			switch (mess &0x0f)
			{
				case 0x1 :
					// a line instruction
					ox=fgetc(infile);
					oy=fgetc(infile);
					x=fgetc(infile);
					y=fgetc(infile);
					if (!ignore) fprintf(outfile,"LINE %d %d %d %d\n",ox,127-oy,x,127-y);
					do_line(work,ox,127-oy,x,127-y,colour[c],doubledraw);
					break;

				case 0x2 :
					// ellipse
					ox=fgetc(infile);
					oy=fgetc(infile);
					x=fgetc(infile);
					y=fgetc(infile);
					if (ox>x) x=ox-x; else x=x-ox;
					if (oy>y) y=oy-y; else y=y-oy;
					//x--;
					//y--;
					if (!ignore) fprintf(outfile,"ELLIPSE %d %d %d %d\n",ox,127-oy,x,y);
					do_ellipse(work,ox,127-oy,x,y,colour[c],doubledraw);
					break;

				case 0x3 :
					// fill
					x=fgetc(infile);
					y=fgetc(infile);
					if (!ignore) fprintf(outfile,"FILL %d %d\n",x,127-y);
					gac_floodfill(work,x*2,127-y,colour[fc1],colour[fc2]);
					break;

				case 0x4 :
				case 0x5 :
				case 0x6 :
				case 0x7 :
					if (!ignore) fprintf(outfile,"INK %d\n",(mess&0x0f)-4);
					c=(mess&0x0f)-4;
					break;

				case 0x8 :
					// a rectangle instruction
					ox=fgetc(infile);
					oy=fgetc(infile);
					x=fgetc(infile);
					y=fgetc(infile);
					if (!ignore) fprintf(outfile,"RECT %d %d %d %d\n",ox,127-oy,x,127-y);
					// as there's no do_rect we have to do this by hand
					do_line(work,ox,127-oy,ox,127-y,colour[c],doubledraw);
					do_line(work,ox,127-y,x,127-y,colour[c],doubledraw);
					do_line(work,x,127-y,x,127-oy,colour[c],doubledraw);
					do_line(work,x,127-oy,ox,127-oy,colour[c],doubledraw);
					break;


				case 0x9 :
					fc1=fgetc(infile);
					fc2=fgetc(infile);
					fc1=(fc1==0x11)?3:(fc1==0x10)?2:fc1;
					fc2=(fc2==0x11)?3:(fc2==0x10)?2:fc2;
					if (!ignore) fprintf(outfile,"SHADE %d %d\n",fc1,fc2);
					break;

				case 0xa :
					image=fgetc(infile)+(fgetc(infile)*256);
					fprintf(outfile,"%x %d\n",mess,image);
					if (image > 0)
					{
						fprintf(outfile,"attempt to import image %d\n",image);
						address=0;
						for (i=0;i<100;i++)
						{
							if (images[i].number==image)
							{
								address=images[i].address;
								fprintf(outfile,"Image: %d Number: %d Address: %x\n",i,image,address);
							}
						}
						fileptr=ftell(infile);
						c=plotimage(address,infile,outfile,work,1);
						if (!ignore) fprintf(outfile,"finished importing image %d\n",image);
						fseek(infile,fileptr,SEEK_SET);
					}
					break;

				case 0xb :
					x=fgetc(infile);
					y=fgetc(infile);
					if (!ignore) fprintf(outfile,"PLOT %d %d\n",x,y);
					doubledraw(work,x,127-y,colour[c]);
					break;

				default:
					fprintf(outfile,"Unknown: %x, %lx %d\n",mess,ftell(infile), c);
					//readkey();
					break;
			}
		}
		else
		{
			if (mess == 0)
			{
				break;
		   }
			fprintf(outfile,"Skipping crap: %d %lx %x\n",mess,ftell(infile),end);
		}
		//if (!ignore) readkey();
	} while (ftell(infile)<end);
	end:
	return c;
}

int main(int argc, char **argv)
{
   FILE *infile,*outfile;
   int mess,outpic;
   int end=0;
   char filename[256];
   BITMAP *work;
   PALETTE pal;
   int nrooms=strtol(argv[2],NULL,0);
   int start=0;

   RGB black = { 0,0,0 };
   RGB blue = { 0,0,63 };
   RGB red = { 63,0,0 };
   RGB magenta = { 63,0,63 };
   RGB green = { 0,63,0 };
   RGB cyan = { 0,63,63 };
   RGB yellow = { 63,63,0 };
   RGB white = { 63,63,63 };
   allegro_init();
   install_keyboard();
   //set_gfx_mode(GFX_VGA, 320, 200, 0, 0);

   /* set up the palette */
   set_color(0,&black);
   set_color(1,&red);
   set_color(2,&green);
   set_color(3,&yellow);
   set_color(4,&blue);
   set_color(5,&magenta);
   set_color(6,&cyan);
   set_color(7,&white);

   get_palette(pal);

   infile=fopen(argv[1],"rb");
   outfile=fopen("debug.txt","w");
   //outfile=stdout;
   fprintf(outfile,"Starting work on %s\n",argv[1]);
   work=create_bitmap(256,128);
   //work=screen;

   fprintf(outfile,"Scanning for images\n");
   fseek(infile,0,SEEK_SET);
   images[1].address=0;
   images[1].number=0;
   outpic=2;
   while (!feof(infile))
   {
      mess=fgetc(infile)+(fgetc(infile)*256);
      start=mess;
      mess=fgetc(infile)+(fgetc(infile)*256);
      end=ftell(infile)+mess;
      if (start == 0) break;
		images[outpic].address=ftell(infile)-4;
		images[outpic].number=start;
		fprintf(outfile,"Image %d (%d): %x\n",outpic,images[outpic].number,images[outpic].address);
		fseek(infile,end,SEEK_SET);
		outpic++;
	}
	fprintf(outfile,"Found %d images\n",outpic);

   fseek(infile,0,SEEK_SET);
   for (outpic=1;outpic<=nrooms; outpic++)
   {
		rectfill(work, 0, 0, 255, 127,0);
		plotimage(images[outpic].address,infile,outfile,work,0);
		sprintf(filename,"image%d.bmp",images[outpic].number);
		fprintf(outfile,"Saving image %d (%d) as %s\n",outpic,images[outpic].number,filename);
		save_bmp(filename, work, pal);
		fprintf(outfile,"File pointer: %lx\n",ftell(infile));

		//readkey();
   }
   fclose(infile);
   allegro_exit();
   return 0;
}



