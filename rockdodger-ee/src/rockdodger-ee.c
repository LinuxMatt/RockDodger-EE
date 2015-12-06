/* Rock Dodger EE. Avoid the rocks as long as you can!
 * Copyright (C) 2008-2015 Matt Thirtytwo <matt59491@gmail.com>
 * Copyright (C) 2001 Paul Holt <pcholt@gmail.com>
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

#ifndef VERSION
#define VERSION "0.9"
#endif
#define COMPILEDATE __DATE__
#ifndef EDITION
#define EDITION " - Extra Edition"
#endif

#ifdef WIN32
#define random rand
#endif

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "SFont.h"

#include "data.h" // All data are inserted into the binary executable

#define BASE_XSIZE 640
#define XSIZE 640
#define YSIZE 480
#define NROCKS 6    // Number of rock image files, not number of rocks visible
#define MAX_GREEBLE_IMAGES 100    // Max number of greeblie image files
#define MAX_GREEBLES 100    // Max number of greeblies
#define MAX_ROCKS 120 // MAX Rocks visible at once
#define MAXROCKHEIGHT 100
#define MAX_BLACK_POINTS 500
#define MAX_ENGINE_DOTS 50000
#define MAX_BANG_DOTS 500000
#define MAX_SPACE_DOTS 12000
#define W 100
#define M 255
#define RANDOM_LIST_SIZE 32767

enum states {
	TITLE_PAGE,
	GAME_PLAY,
	DEAD_PAUSE,
	GAME_OVER,
	HIGH_SCORE_ENTRY,
	HIGH_SCORE_DISPLAY,
	DEMO
};

/* The input states are names for mappings between input device states and game
 * inputs.
 */
enum inputstates {
	UP,
	DOWN,
	LEFT,
	RIGHT,
	LASER,
	SHIELD,
	FAST,
	NOFAST,
	SCREENSHOT,
	_INPUTSTATESEND
};
#define NUM_INPUTS (_INPUTSTATESEND+1)
#define JOYSTICK_DEADZONE 1024

#define COND_ERROR(a) if ((a)) {initerror=strdup(SDL_GetError());return 1;}
#define CONDERROR(a) if ((a)) {fprintf(stderr,"Error: %s\n",SDL_GetError());exit(1);}
#define NULLERROR(a) CONDERROR((a)==NULL)
#define NULL_ERROR(a) COND_ERROR((a)==NULL)

/***********
 * STRUCTS
 **********/
struct rock_struct {
	// Array of black pixel coordinates around the rim of our ship.
	// This is scanned every frame to see if it's still black, and as
	// soon as it isn't we BLOW UP
	float x,y,xvel,yvel;
	int active;
	SDL_Surface *image;
	int type_number;
	float heat;
	// rock_struct.greeb is an index into greeb[], the destination (or landing
	// site) of the rock's greeb. This of course limits the number of greebs
	// flying to the same rock.
	int greeb;
};

struct black_point_struct {
	int x,y;
};

struct bangdots {
	// Bang dots have the same colour as shield dots.
	// Bang dots get darker as they age.
	// Some are coloured the same as the ex-ship.
	float x,y,dx,dy;
	Uint16 c; // when zero, use heatcolor[bangdotlife]
	float life;	// When reduced to 0, set active=0
	int active;
	float decay;// Amount by which to reduce life each time dot is drawn
};

struct enginedots {
	// Engine dots stream out the back of the ship, getting darker as they go.
	int active;
	float x,y,dx,dy;
	// The life of an engine dot
	// is a number starting at between 0 and 50 and counting backward.
	float life;	// When reduced to 0, set active=0
};

struct spacedot {
	// Space dots are harmless background items
	// All are active. When one falls off the edge, another is created at the start.
	float x,y,dx;
	Uint16 color;
};

struct greeble {
	int active;
	float x,y; // When landed, these represent an offset from the host rock
	int target_rock_number; // no longer used?. Should use rock_struct.greeb
	int landed;
	int boredom; // Goes up while landed
};

// High score table
struct highscore {
	int score;
	char name[1024];
	int level;
	int allocated;
};
static struct highscore high[8];

static struct highscore defaulthigh[8] = {
	{13000,"Pad",2,0},
	{12500,"Pad",2,0},
	{6500,"Pad",1,0},
	{5000,"Pad",1,0},
	{3000,"Pad",1,0},
	{2500,"Pad",1,0},
	{2000,"Pad",1,0},
	{1500,"Pad",1,0}
};

/*************
 *   VARS
 ************/

static SDL_Joystick* joysticks[1];

// SDL_Surface global variables
static SDL_Surface *surf_screen        = NULL;	// Screen
static SDL_Surface *surf_t_rock        = NULL;	// Title element "rock"
static SDL_Surface *surf_t_dodgers     = NULL;	// Title element "dodgers"
static SDL_Surface *surf_t_game        = NULL;	// Title element "game"
static SDL_Surface *surf_t_over        = NULL;	// Title element "over"
static SDL_Surface *surf_t_paused      = NULL;	// Title element "paused"
static SDL_Surface *surf_ship          = NULL;	// Spaceship element
static SDL_Surface *surf_small_ship    = NULL;	// Indicator of number of ships remaining
static SDL_Surface *surf_font_big      = NULL;	// The big font

static SDL_Surface *surf_gauge_shield[2];       // The shield gauge 0=on,1=off
static SDL_Surface *surf_gauge_laser[2];	    // The laser gauge 0=on,1=off
static SDL_Surface *surf_rock[NROCKS];          // THE ROCKS
static SDL_Surface *surf_deadrock[NROCKS];	    // THE DEAD ROCKS (blank white rocks)
static SDL_Surface *surf_greeblies[MAX_GREEBLE_IMAGES];	// THE GREEBLIES

// Structure global variables
static struct enginedots edot[MAX_ENGINE_DOTS];
static struct enginedots *dotptr = edot;
static struct rock_struct rock[MAX_ROCKS];
static struct rock_struct *rockptr = rock;
static struct black_point_struct black_point[MAX_BLACK_POINTS];
static struct black_point_struct *blackptr = black_point;
static struct bangdots bdot[MAX_BANG_DOTS];
static struct spacedot sdot[MAX_SPACE_DOTS];
static struct greeble greeb[MAX_GREEBLES];

// Other global variables
static char topline[1024];
static char *initerror = "";
static char name[1024];
static float random_list[RANDOM_LIST_SIZE];

static float xship = 0;
static float yship = 0;	// X position, 0..XSIZE
static float xvel = 0;
static float yvel = 0;	// Change in X position per tick.
static float rockrate = 0;
static float rockspeed = 0;
static float shieldlevel = 0;
static float laserlevel = 0;
static float shieldpulse = 0;
static float countdown = 0;
static float level = 0;
static float fadetimer = 0;
static float faderate = 0;
static float state_timeout = 600.0;
static float movementrate = 0;

static Uint16 heatcolor[W*3];

static int xsize = XSIZE;
static int ysize = YSIZE;
static int font_height = 0;
static int nships = 0;
static int score = 0;
static int initticks = 0;
static int ticks_since_last = 0;
static int last_ticks = 0;
static int initialshield = 0;
static int gameover = 0;
static int fast = 0;
static int maneuver = 0;
static int laser = 0;
static int shieldsup=0;
static int oss_sound_flag=0;
static int scorerank = 0;
static int joystick_flag = 1;
static int pausedown=0;
static int paused=0;
// bangdot start (bd1) and end (bd2) position:
static int bd1 = 0;
static int bd2 = 0;

static enum states state   = TITLE_PAGE;

static char *sequence[] = {
	"Press SPACE to start.",
	"Use arrow keys to move the ship.",
	"Press S for shield and D for laser.",
	"You can bounce off the sides of the screen.",
	"Avoid the rocks as long as you can !",
	"",
	"https://github.com/LinuxMatt/RockDodger-EE",
	"",
	"This game is based on the original Rock Dodger",
	"written by Paul Holt and released under GPL",
	"at http://spacerocks.sourceforge.net",
	"",
	"This program is free software;",
	"you can redistribute it and/or modify it",
	"under the terms of the GNU General Public License",
	"as published by the Free Software Foundation;",
	"either version 2 of the License, or",
	"(at your option) any later version.",
	/*
	   "If you have a <500Mhz machine, tell me how this game performs!",
	   "",
	   "The little green guys are called Greeblies...",
	   "The graphics for the Greeblies are not finished yet.  Contributions welcome.",
	   "More features planned, including powerups, heat-seekers...",
	   "...and shockwave generators. Stay tuned!",
	   */
	""
};

extern char *optarg;
extern int optind, opterr, optopt;

/*
 *  PROTOTYPES
 */

/*
 * The init_sound() routine is called first.
 * The play_sound() routine is called with the index number of the sound we wish to play.
 * The play_tune() routine is called with the index number of the tune we wish to play.
 */
static int init_sound();
static void play_sound(int i);
static void play_tune(int i);
static int makebangdots(int xbang, int ybang, int xvel, int yvel, SDL_Surface *s, int power, int amtdots);

/*
 *   FUNCTIONS
 */

/*
 * Random numbers involve a call to the operating system, and may take some
 * time to complete. I can't call the random number generator multiple times
 * per frame.  In one old version, it would take half a second before the ship
 * explosion was generated when the rnd() function was calling the operating
 * system function.
 * The solution is to generate random numbers during initialisation, and store
 * in a circular array.  This will (of course) repeat, but it is not noticeable
 * (unless RANDOM_LIST_SIZE is very very small).
 */
// #define getrandom( min, max ) ((rand() % (int)(((max) + 1) - (min))) + (min))

static void initrnd() {
	int i;
	for (i=0; i<RANDOM_LIST_SIZE; i++) {
		random_list[i] = (float)random()/(float)RAND_MAX;
	}
}

static float rnd()
{
	static int random_list_pointer = 0;

	++random_list_pointer;
	if (random_list_pointer>=RANDOM_LIST_SIZE) {
		random_list_pointer=0;
	}
	return random_list[random_list_pointer];
}

static void handlesignal(int signal)
{
	exit(128+signal);
}

static void init_greeblies() {
	int i;
	for (i=0; i<MAX_GREEBLES; i++) greeb[i].active=0;
	for (i=0; i<MAX_ROCKS; i++) rock[i].greeb=0;
}

static void activate_greeblie(struct greeble *g)
{
	int j = 0;

	// Choose an active rock to become attracted to. If there are no such
	// available rocks in the first 10% of spots we search, then give up
	// without activating any greeblies at all.
	for (j=0; j<(MAX_ROCKS/10); j++) {
		int i=rnd()*MAX_ROCKS;
		if (rock[i].active && rock[i].greeb==0 && 4*rock[i].x>3*xsize) {
			// Create a new greeb, alive, flying, from the right side of the screen at a
			// random position.
			g->active = 1;
			g->landed = 0;
			g->x = xsize;
			g->y = rnd()*ysize;
			// It's flying to this rock, and the rock knows it.
			g->target_rock_number = i;
			rock[i].greeb = g-greeb;
			return;
		}
	}
}

static void activate_one_greeblie()
{
	int i = 0;
	for (i=0; i<MAX_GREEBLES; i++) {
		if (!greeb[i].active) {
			activate_greeblie(&greeb[i]);
			return;
		}
	}
}

static void move_all_greeblies()
{
	int i = 0;
	for (i=0; i<MAX_GREEBLES; i++) {
		struct greeble *g = NULL;
		g = &greeb[i];
		if (g->active) {
			//rock[g->target_rock_number].greeb = i;
			if (g->landed) {
				g->boredom++;
				// Landed greebles may take it into their head to jump off their
				// rock and travel to a new one. Boredom can cause this, but a
				// ship getting close to their rock will do it too.
				//printf("%f %f %f\n",yship,rock[g->target_rock_number].y,g->y);
				if ( ( (rnd()<(0.001*(g->boredom-100))) || g->target_rock_number ) &&
						(abs(yship-(rock[g->target_rock_number].y+g->y))<10) &&
						(xship<rock[g->target_rock_number].x)) {

					int j = 0;
					for (j=0; j<(MAX_ROCKS/10) && g->landed; j++) { // Find a target
						int i = rnd()*MAX_ROCKS;
						int dx = 0;
						struct rock_struct *r = &rock[i];
						if (r->active && r->greeb==0 && (dx=(rock[g->target_rock_number].x-r->x))>0 && dx<xsize/3) {
							// Change the greeble coordinates to represent free
							// space coordinates
							g->x += rock[g->target_rock_number].x;
							g->y += rock[g->target_rock_number].y;
							rock[g->target_rock_number].greeb = 0;
							g->target_rock_number = i;
							r->greeb = g-greeb;
							g->active = 1;
							g->landed = 0;
						}
					}
					// It's no problem if we don't find a target, just don't
					// bother leaving the current one.
				}
				// If the greeble leaves the left side of the screen, he's gone.
				if (rock[g->target_rock_number].x + g->x<0) {
					g->active=0;
					rock[g->target_rock_number].greeb = 0;
				}
			} else {
				float dx = g->x - rock[g->target_rock_number].x+10;
				float dy = g->y - rock[g->target_rock_number].y+10;
				float dist = sqrt(dx*dx+dy*dy);

				// Greebles are attracted to rocks. If the greeble is within a
				// certain distance of the target rock, set it to 'landed' mode.
				if ( dist < 10) {
					g->landed=1;
					g->boredom=0;
					// Change the greeble coordinates to represent the
					// offset from the host rock.
					g->x = dx-10;
					g->y = dy-10;
				} else {
					g->x -= 20*movementrate*dx/dist;
					g->y -= 10*movementrate*dy/dist;
				}
			}
		}
	}
}

static void display_greeb(struct greeble *g)
{
	static SDL_Rect src,dest;
	if (g->active) {
		if (g->landed) {
			dest.x = (int)(g->x+rock[g->target_rock_number].x);
			dest.y = (int)(g->y+rock[g->target_rock_number].y);
		} else {
			dest.x = (int)(g->x);
			dest.y = (int)(g->y);
		}
		src.x = 0;
		src.y = 0;
		src.w = surf_greeblies[0]->w;
		src.h = surf_greeblies[0]->h;
		dest.w = src.w;
		dest.h = src.h;
		SDL_BlitSurface(surf_greeblies[0],&src,surf_screen,&dest);
	}
}

static FILE *hs_fopen(char *mode)
{
	FILE *f = NULL;

#ifndef WIN32
	mode_t mask;
	mask = umask(0111);
	if ( (f=fopen("/usr/share/rockdodger/.highscore",mode)) ) {
		umask(mask);
		return f;
	} else {
		char s[1024];
		umask(0177);
		snprintf(s,1024,"%s/.rockdodger_high",getenv("HOME"));
		if ( (f=fopen(s,mode)) ) {
			umask(mask);
			return f;
		} else {
			umask(mask);
			return 0;
		}
	}
#else // WINDOWS
	if ( (f=fopen("highscore.txt",mode)) ) {
		return f;
	} else {
		return 0;
	}
#endif
}

static void read_high_score_table()
{
	FILE *f = NULL;
	int i = 0;
	if ( (f=hs_fopen("rb")) ) {
		// If the file exists, read from it
		for (i=0; i<8; i++) {
			char s[1024];
			int highscore = 0;
			if (fscanf (f, "%d %1023[^\n]", &highscore, s)!=2)
				break;
			// if (high[i].allocated) free(high[i].name);
			strncpy(high[i].name, s, 127);
			high[i].score = highscore;
			high[i].allocated = 1;
		}
		fclose(f);
	}
}

static void write_high_score_table()
{
	FILE *f = NULL;
	int i = 0;
	if ( (f=hs_fopen("wb")) ) {
		// If the file exists, write to it
		for (i=0; i<8; i++) {
			fprintf (f, "%d %s\n", high[i].score, high[i].name);
		}
		fclose(f);
	}
}

static void init_engine_dots()
{
	int i = 0;
	for (i=0; i<MAX_ENGINE_DOTS; i++) {
		edot[i].active=0;
	}
}

static void init_space_dots()
{
	int i = 0;
	int intensity = 0;
	for (i=0; i<MAX_SPACE_DOTS; i++) {
		float r = 0;
		sdot[i].x = rnd()*(xsize-5);
		sdot[i].y = rnd()*(ysize-5);
		r = rnd()*rnd();
		sdot[i].dx = -r*4;
		// -1/((1-r)+.3);
		intensity = (int)(r*180+70);
		sdot[i].color = SDL_MapRGB(surf_screen->format,intensity,intensity,intensity);
	}
}

static void inc_score(int dscore)
{
	score += dscore;
}

static void kill_greeb(int hitgreeb)
{
	struct greeble *g = NULL;
	int r = 0;

	g = &greeb[hitgreeb];
	g->active = 0;
	if ( (r=g->target_rock_number) )
		rock[r].greeb = 0;
	if (g->landed) {
		play_sound(1+(int)(rnd()*3));
		makebangdots(
				(int)(g->x + rock[g->target_rock_number].x),
				(int)(g->y + rock[g->target_rock_number].y),
				0, 0, surf_greeblies[0], 15, 1
				);
	} else {
		makebangdots( (int)(g->x), (int)(g->y), 0, 0, surf_greeblies[0], 15, 1);
		play_sound(1+(int)(rnd()*3));
	}
	inc_score(2500);
}

static void drawlaser()
{
	int i = 0;
	int xc = 0;
	int hitrock  = 0;
	int hitgreeb = 0;
	Uint16 c = 0;
	Uint16 *rawpixel = NULL;

	if (laserlevel<0) return;
	laserlevel-=movementrate;
	if (laserlevel<0) {
		// If the laser runs out completely, there will be a delay before it can be brought back up
		laserlevel=-W/2;
		return;
	}
	hitrock = -1;
	hitgreeb = -1;
	xc = xsize;
	// let xc = x coordinate of the collision between the laser and a space rock
	// 1. Calculate xc and determine the asteroid that was hit
	for (i=0; i<MAX_ROCKS; i++) {
		if (rock[i].active) {
			if (yship+12>rock[i].y && yship+12<rock[i].y+rock[i].image->h && xship+32<rock[i].x+(rock[i].image->w/2) && rock[i].x+(rock[i].image->w/2) < xc) {
				xc = rock[i].x+(rock[i].image->w/2);
				hitrock = i;
			}
		}
	}
	for (i=0; i<MAX_GREEBLES; i++) {
		int greebheight = 16;
		int greebwidth  = 16;
		struct greeble *g = NULL;
		g = &greeb[i];

		if (g->active) {
			int greebx = (int)g->x;
			int greeby = (int)g->y;
			if (g->landed) {
				greebx += rock[g->target_rock_number].x;
				greeby += rock[g->target_rock_number].y;
			}
			if (yship+12>greeby && yship+12<greeby+greebheight &&
					xship+32<greebx+(greebwidth/2) && greebx+(greebwidth/2) <
					xc) {
				xc = greebx+(greebwidth/2);
				hitgreeb = i;
			}
		}
	}
	if (hitrock>=0) {
		rock[hitrock].heat += movementrate*3;
	}
	if (hitgreeb>=0) kill_greeb(hitgreeb);

	// Plot a number of random dots between xship and xsize
	SDL_LockSurface(surf_screen);
	rawpixel = (Uint16 *) surf_screen->pixels;
	c = SDL_MapRGB(surf_ship->format,rnd()*128,128+rnd()*120,rnd()*128);

	for (i=0; i<(xc-xship)*5; i+=10) {
		int x = 0;
		int y = 0;
		x = rnd()*(xc-(xship+32))+xship+32;
		y = yship+12+(rnd()-0.5)*1.5;
		rawpixel[surf_screen->pitch/2*y+x]=c;
	}
	SDL_UnlockSurface(surf_screen);
}

static int makebangdots(int xbang, int ybang, int xvel, int yvel, SDL_Surface *s, int power, int amtdots)
{
	// TODO - stop generating dots after a certain amount of time has passed, to cope with slower CPUs.

	int x = 0;
	int y = 0;
	int endcount = 0;
	Uint16 *rawpixel = NULL;
	Uint16 c = 0;
	double theta = 0;
	double r = 0;
	int begin_generate = 0;

	begin_generate = SDL_GetTicks();
	SDL_LockSurface(s);
	rawpixel = (Uint16 *) s->pixels;

	endcount = 0;
	while (endcount<amtdots) {
		for (x=0; x<s->w; x++) {
			for (y=0; y<s->h; y++) {
				c = rawpixel[s->pitch/2*y+x];
				if (c && c != SDL_MapRGB(s->format,0,255,0)) {
					theta = rnd()*M_PI*2;
					r = 1-(rnd()*rnd());

					bdot[bd2].dx = (power/50.0)*45.0*cos(theta)*r+xvel;
					bdot[bd2].dy = (power/50.0)*45.0*sin(theta)*r+yvel;
					bdot[bd2].x = x+xbang;
					bdot[bd2].y = y+ybang;

					// Replace the last few bang dots with the pixels from the exploding object
					bdot[bd2].c = (endcount>0)?c:0;
					bdot[bd2].life = 100;
					bdot[bd2].decay = rnd()*3+1;
					bdot[bd2].active = 1;

					bd2++;
					if (bd2>=MAX_BANG_DOTS)
						bd2=0;

					// If the circular buffer is filled, who cares? They've had their chance.
					//if (bd2==bd1-1) goto exitloop;
				}
			}
		}
		if (SDL_GetTicks() - begin_generate > 3) endcount++;
	}
	// exitloop:
	SDL_UnlockSurface(s);
	return 1;

}

static void draw_bang_dots(SDL_Surface *s)
{
	int i = 0;
	int first_i = 0;
	int last_i = 0;
	Uint16 *rawpixel = 0;

	rawpixel = (Uint16 *) s->pixels;
	first_i = -1;

	for (i=bd1; (bd1<=bd2)?(i<bd2):(i>=bd1 && i<bd2); last_i = ++i) {

		i %= MAX_BANG_DOTS;

		if (bdot[i].x<=0 || bdot[i].x>=xsize || bdot[i].y<=0 || bdot[i].y>=ysize) {
			// If the dot has drifted outside the perimeter, kill it
			bdot[i].active = 0;
		}
		if (bdot[i].active) {
			//printf("%d %d\n",bd1,bd2);
			if (first_i < 0)
				first_i = i;
			//last_i = i+1;
			rawpixel[(int)(s->pitch/2*(int)(bdot[i].y))+(int)(bdot[i].x)]
				= bdot[i].c ? bdot[i].c : heatcolor[(int)(bdot[i].life*3)];
			bdot[i].life-=bdot[i].decay * movementrate;
			bdot[i].x += bdot[i].dx*movementrate;
			bdot[i].y += bdot[i].dy*movementrate;

			if (bdot[i].life<0)
				bdot[i].active = 0;
		}
	}
	if (first_i>=0) {
		bd1 = first_i;
		bd2 = last_i;
		//printf("new %d %d\n",bd1,bd2);
		//fprintf (stderr,"%d - %d\n", bd1,bd2);
	} else {
		bd1 = 0;
		bd2 = 0;
		//fprintf (stderr,"reset\n");
	}
}

static void draw_space_dots(SDL_Surface *s)
{
	int i = 0;
	Uint16 *rawpixel = NULL;
	rawpixel = (Uint16 *) s->pixels;

	for (i=0; i<MAX_SPACE_DOTS; i++) {
		if (sdot[i].y<0) sdot[i].y=0;
		rawpixel[(int)(s->pitch/2*(int)sdot[i].y)+(int)(sdot[i].x)] = sdot[i].color;
		sdot[i].x += sdot[i].dx*movementrate;
		if (sdot[i].x<0)
			sdot[i].x=xsize;
	}
}

static void draw_engine_dots(SDL_Surface *s)
{
	int i = 0;
	Uint16 *rawpixel = 0;
	rawpixel = (Uint16 *) s->pixels;

	for (i=0; i<MAX_ENGINE_DOTS; i++) {
		if (edot[i].active) {
			edot[i].x += edot[i].dx*movementrate;
			edot[i].y += edot[i].dy*movementrate;
			if ((edot[i].life-=movementrate*3)<0 || edot[i].y<0 || edot[i].y >= ysize)
				edot[i].active=0;
			else
				if (edot[i].x<0 || edot[i].x>xsize) {
					edot[i].active=0;
				} else {
					int heatindex = 0;
					int p1 = 0;
					int p2 = 0;
					int p3 = 0;
					int pixelindex = 0;

					heatindex = edot[i].life * 6;
					//rawpixel[(int)(s->pitch/2*(int)(edot[i].y))+(int)(edot[i].x)] = lifecolor[(int)(edot[i].life)];
					p1 = (int) (s->pitch/2);
					p2 = (int) (edot[i].y);
					p3 = (int) (edot[i].x);
					pixelindex = (p1 * p2) + p3;
					if(pixelindex > (xsize * ysize)) {
						fprintf(stderr, "ERROR: out of range pixelindex = %d > %d (p1=%d * p2=%d + p3=%d)\n",
								pixelindex, xsize * ysize, p1, p2, p3);
					} else {
						rawpixel[pixelindex] = heatindex>3*W ? heatcolor[3*W-1] : heatcolor[heatindex];
					}
				}
		}
	}
}

static void create_engine_dots(int newdots)
{
	int i = 0;
	double theta = 0;
	double r = 0;
	double dx = 0;
	double dy = 0;

	if (state!=GAME_PLAY)
		return;

	for (i=0; i<newdots*movementrate; i++) {
		if (dotptr->active==0) {

			theta = rnd()*M_PI*2;
			r = rnd();
			dx = cos(theta)*r;
			dy = sin(theta)*r;

			dotptr->active=1;
			dotptr->x=xship+surf_ship->w/2-14;
			dotptr->y=yship+surf_ship->h/2+(rnd()-0.5)*5-1;
			dotptr->dx=10*(dx-1.5)+xvel;
			dotptr->dy=1*dy+yvel;
			dotptr->life=45+rnd(1)*5;

			dotptr++;
			if (dotptr-edot>=MAX_ENGINE_DOTS) dotptr = edot;
		}
	}
}

static void create_engine_dots2(int newdots, int m)
{
	int i = 0;
	double theta = 0;
	double r = 0;
	double dx = 0;
	double dy = 0;

	// Don't create fresh engine dots when
	// the game is not being played and a demo is not being shown
	if (state!=GAME_PLAY && state!=DEMO) return;

	for (i=0; i<newdots; i++) {
		if (dotptr->active==0) {

			theta = rnd()*M_PI*2;
			r = rnd();
			dx = cos(theta)*r;
			dy = sin(theta)*r;

			dotptr->active=1;
			dotptr->x=xship+surf_ship->w/2+(rnd()-0.5)*1;
			dotptr->y=yship+surf_ship->h/2+(rnd()-0.5)*1;

			switch(m) {
				case 0:
					dotptr->x-=14;
					dotptr->dx=5*(dx-1.5)+xvel;
					dotptr->dy=1*dy+yvel;
					break;
				case 1:
					dotptr->dy=5*(dy-1.5)+yvel;
					dotptr->dx=1*dx+xvel;
					break;
				case 2:
					dotptr->x+=14;
					dotptr->dx=-5*(dx-1.5)+xvel;
					dotptr->dy=1*dy+yvel;
					break;
				case 3:
					dotptr->dy=5*(dy+1.5)+yvel;
					dotptr->dx=1*dx+xvel;
					break;
			}
			dotptr->life=45+rnd(1)*20;
			dotptr++;
			if (dotptr-edot>=MAX_ENGINE_DOTS)
				dotptr = edot;
		}
	}
}

static int drawdots(SDL_Surface *s)
{
	int m = 0;

	SDL_LockSurface(s);

	// Draw the background stars (aka space dots - they can't possibly be stars
	// because then we'd be moving past them at something like 10,000 times the
	// speed of light)
	draw_space_dots(s);

	// Draw all the engine dots
	draw_engine_dots(s);

	// Create more engine dots comin out da back
	if (!gameover)
		create_engine_dots(400);

	// Create engine dots out the side we're moving from
	for (m=0; m<4; m++)
		if (maneuver & 1<<m) // 'maneuver' is a bit field
			create_engine_dots2(5,m);

	// Draw all outstanding bang dots
	//if (bangdotlife-- > 0)
	draw_bang_dots(s);

	SDL_UnlockSurface(s);
	return 1;
}

static SDL_Surface *img_load_h(unsigned char *h, int size)
{
	SDL_RWops *rw = NULL;
	SDL_Surface *s = NULL;
#ifdef DEBUG
	fprintf(stderr, "SDL_RWFromMem %d bytes...\n", size);
#endif
	rw = SDL_RWFromMem(h, size);
#ifdef DEBUG
	fprintf(stderr, "IMG_Load_RW...\n");
#endif
	s  = IMG_Load_RW(rw,0);
#ifdef DEBUG
	fprintf(stderr, "SDL_FreeRW...\n");
#endif
	SDL_FreeRW(rw);
	if ( s == NULL ) {
		fprintf(stderr, "IMG_Load_RW failed: %s\n", SDL_GetError());
		return NULL;
	}
	return s;
}

static int init(int fullscreen)
{
	int i = 0;
	int j = 0;
	SDL_Surface *temp = NULL;
	Uint16 *raw_pixels = NULL;
	Uint32 flag = 0;

	signal(SIGINT,  handlesignal);
	signal(SIGTERM, handlesignal);
#ifndef WIN32
	signal(SIGQUIT, handlesignal);
#endif
	read_high_score_table();

	if (oss_sound_flag) {
		// Initialise SDL with audio and video
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)!=0) {
			oss_sound_flag=0;
			printf ("Can't open sound, starting without it\n");
			atexit(SDL_Quit);
		} else {
			atexit(SDL_Quit);
			atexit(SDL_CloseAudio);
			oss_sound_flag = init_sound();
		}
	} else {
		// Initialise with video only
		COND_ERROR(SDL_Init(SDL_INIT_VIDEO)!=0);
		atexit(SDL_Quit);
	}
	// Attempt to initialize a joystick
	if (joystick_flag) {
		if (SDL_InitSubSystem (SDL_INIT_JOYSTICK) != 0) {
			joystick_flag = 0;
			printf("Can't initialize joystick subsystem, starting without it.\n");
		} else {
			int njoys = 0;
			njoys = SDL_NumJoysticks ();
			printf ("%d joystick(s) detected\n", njoys);
			if (njoys == 0) {
				printf ("That's not enough joysticks to start with joystick support\n");
				joystick_flag = 0;
			} else {
				joysticks[0] = SDL_JoystickOpen (0);
				if (joysticks[0] == NULL) {
					printf ("Couldn't open joystick %d\n", 0);
					joystick_flag = 0;
				}
			}
		}
	}
	if (oss_sound_flag)
		play_tune(0);

	// Attempt to get the required video size
	flag = SDL_DOUBLEBUF | SDL_HWSURFACE;
	if (fullscreen) flag |= SDL_FULLSCREEN;
	surf_screen = SDL_SetVideoMode(xsize,ysize,16,flag);
	NULL_ERROR(surf_screen);

	// Set the title bar text
	SDL_WM_SetCaption("Rock Dodgers", "rockdodgers");

	// Set the heat color from the range 0 (cold) to 300 (blue-white)
	for (i=0; i<W*3; i++) {
		heatcolor[i] = SDL_MapRGB( surf_screen->format,
				(i<W) ? (i*M/W) : (M),
				(i<W) ? 0 : (i<2*W) ? ((i-W)*M/W) : M ,
				(i<2*W) ? 0 : ((i-W)*M/W)
				// Got that?
				);
	}

#define MAKE_RLE_SURFACE(src, dest, ext, red, blue, green) \
	NULL_ERROR(temp = img_load_h(src##_##ext, src##_##ext##_len)); \
	SDL_SetColorKey(temp,SDL_SRCCOLORKEY | SDL_RLEACCEL, (Uint16) SDL_MapRGB(temp->format,red,blue,green)); \
	NULL_ERROR(surf_##dest = SDL_DisplayFormat(temp)); SDL_FreeSurface(temp)

	// Load the Title elements
	MAKE_RLE_SURFACE(rock, t_rock, png, 255, 0, 0);
	MAKE_RLE_SURFACE(dodgers, t_dodgers, png, 255, 0, 0);
	MAKE_RLE_SURFACE(game, t_game, png, 255, 0, 0);
	MAKE_RLE_SURFACE(laser0, gauge_laser[0], png, 0, 0, 0);
	MAKE_RLE_SURFACE(laser1, gauge_laser[1], png, 0, 0, 0);
	MAKE_RLE_SURFACE(shield0, gauge_shield[0], png, 0, 0, 0);
	MAKE_RLE_SURFACE(shield1, gauge_shield[1], png, 0, 0, 0);
	MAKE_RLE_SURFACE(over, t_over, png, 255, 0, 0);
	MAKE_RLE_SURFACE(paused, t_paused, png, 255, 0, 0);

	NULL_ERROR(surf_font_big = img_load_h(__20P_Betadance_png, __20P_Betadance_png_len));
	InitFont(surf_font_big);
	font_height = surf_font_big->h;

	// Load the spaceship surface from the spaceship file
	MAKE_RLE_SURFACE(ship, ship, bmp, 0, 255, 0);

	// Load the little spaceship surface from the spaceship file
	MAKE_RLE_SURFACE(ship_small, small_ship, bmp, 0, 255, 0);

	// Create the array of black points;
	SDL_LockSurface(surf_ship);
	raw_pixels = (Uint16 *) surf_ship->pixels;
	for (i=0; i<surf_ship->w; i++) {
		for (j=0; j<surf_ship->h; j++) {
			if (raw_pixels[j*(surf_ship->pitch)/2+i] == 0) {
				blackptr->x = i;
				blackptr->y = j;
				blackptr++;
			}
		}
	}
	SDL_UnlockSurface(surf_ship);
	init_engine_dots();
	init_space_dots();
	init_greeblies();
	// Load all our lovely rocks
	MAKE_RLE_SURFACE(rock0, rock[0], bmp, 0, 255, 0);
	MAKE_RLE_SURFACE(rock1, rock[1], bmp, 0, 255, 0);
	MAKE_RLE_SURFACE(rock2, rock[2], bmp, 0, 255, 0);
	MAKE_RLE_SURFACE(rock3, rock[3], bmp, 0, 255, 0);
	MAKE_RLE_SURFACE(rock4, rock[4], bmp, 0, 255, 0);
	MAKE_RLE_SURFACE(rock5, rock[5], bmp, 0, 255, 0);
	MAKE_RLE_SURFACE(deadrock0, deadrock[0], bmp, 0, 255, 0);
	MAKE_RLE_SURFACE(deadrock1, deadrock[1], bmp, 0, 255, 0);
	MAKE_RLE_SURFACE(deadrock2, deadrock[2], bmp, 0, 255, 0);
	MAKE_RLE_SURFACE(deadrock3, deadrock[3], bmp, 0, 255, 0);
	MAKE_RLE_SURFACE(deadrock4, deadrock[4], bmp, 0, 255, 0);
	MAKE_RLE_SURFACE(deadrock5, deadrock[5], bmp, 0, 255, 0);
	// Load all the available greeblies
	MAKE_RLE_SURFACE(greeblie0, greeblies[0], bmp, 0, 0, 255);
	// Remove the mouse cursor
#ifdef SDL_DISABLE
	SDL_ShowCursor(SDL_DISABLE);
#endif
	return 0;
}

static void showgauge(int x, SDL_Surface *surf[2], float fraction)
{
	static int endx = 0;
	static int w    = 0;
	static SDL_Rect src, dest;
	src.y = 0;
	if (fraction>0) {
		if (fraction>1) fraction=1.0;
		src.x=0;
		w = src.w = surf[0]->w * fraction;
		src.h = surf[0]->h;
		endx = src.w;
		dest.w = src.w;
		dest.h = src.h;
		dest.x = x;
		dest.y = ysize-src.h-10;
		SDL_BlitSurface(surf[0],&src,surf_screen,&dest);
	} else {
		endx = 0;
		w = 0;
	}
	src.x = endx;
	src.w = surf[1]->w - w;
	src.h = surf[1]->h;
	dest.w = src.w;
	dest.h = src.h;
	dest.x = x+endx;
	dest.y = ysize-src.h-10;
	SDL_BlitSurface(surf[1],&src,surf_screen,&dest);
}

static int draw() {
	int i = 0;
	SDL_Rect src,dest;
	struct black_point_struct *p = NULL;
	Uint16 *raw_pixels = NULL;
	int bang = 0;
	int offset = 0;
	int x = 0;
	char *text = NULL;
	float fadegame = 0;
	float fadeover = 0;
	int nsequence  = 0;
#ifdef DEBUG
	char *statedisplay = NULL, buf[1024];
#endif
	static int last_rock = 1;

	nsequence = sizeof(sequence)/sizeof(char *);
	bang   = 0;
	src.x  = 0;
	src.y  = 0;
	dest.x = 0;
	dest.y = 0;

	// Draw a fully black background
	{
		Uint32 c = 0;
		float t = 0;
		float l = level-(int)level;
		if (l>0.98) {
			t = 4*(1-(1-l)/(1-0.98));
			play_sound(4);
			switch((int)t) {
				case 0:
					// ramp up red
					c = SDL_MapRGB(surf_screen->format,(int)(255*t),0,0);
					break;
				case 1:
					// ramp down red, ramp up green
					c = SDL_MapRGB(surf_screen->format,(int)(255*(1-t)),(int)(255*(t-1)),0);
					break;
				case 2:
					// ramp down green, ramp up blue
					c = SDL_MapRGB(surf_screen->format,0,(int)(255*(3-t)),(int)(255*(t-3)));
					break;
				case 3:
					// ramp down blue
					c = SDL_MapRGB(surf_screen->format,0,0,(int)(255*(4-t)));
					break;
			}
		} else {
			c = 0;
		}
		SDL_FillRect(surf_screen, NULL, c);
	}

#ifdef DEBUG
	// Show the current state
	switch (state) {
		case TITLE_PAGE:
			statedisplay = "title_page";
			break;
		case GAME_PLAY:
			statedisplay = "gameplay";
			break;
		case DEAD_PAUSE:
			statedisplay = "dead_pause";
			break;
		case GAME_OVER:
			statedisplay = "game_over";
			break;
		case HIGH_SCORE_ENTRY:
			statedisplay = "high_score_entry";
			break;
		case HIGH_SCORE_DISPLAY:
			statedisplay = "high_score_display";
			break;
		case DEMO:
			statedisplay = "demo";
			break;
	}
	snprintf(buf,1024, "mode=%s", statedisplay);
	PutString(surf_screen,0,ysize-50,buf);
#endif

	// Draw the background dots
	drawdots(surf_screen);

	// If it's firing, draw the laser
	if (laser) {
		drawlaser();
	} else {
		if (laserlevel<3*W) {
			laserlevel += movementrate/4;
		}
	}
	// Draw ship
	if (!gameover && (state==GAME_PLAY || state==DEMO) )  {
		src.w = surf_ship->w;
		src.h = surf_ship->h;
		dest.w = src.w;
		dest.h = src.h;
		dest.x = (int)xship;
		dest.y = (int)yship;
		SDL_BlitSurface(surf_ship,&src,surf_screen,&dest);
	}
	// Before the rocks are drawn, the greeblies are shown playing amongst them.
	if (rnd()<.02*level*movementrate || state==TITLE_PAGE)
		activate_one_greeblie();
	move_all_greeblies();

	// Draw all the rocks, in all states.
	for (i=0; i<MAX_ROCKS; i++) {
		struct rock_struct *r;
		r=&rock[i];

		if (r->active) {
			if (!r->greeb) last_rock = i;
			src.w = r->image->w;
			src.h = r->image->h;
			dest.w = src.w;
			dest.h = src.h;
			dest.x = (int) r->x;
			dest.y = (int) r->y;

			// Draw the rock
			SDL_BlitSurface(r->image,&src,surf_screen,&dest);

			// Draw the heated part of the rock, in an alpha which reflects the
			// amount of heat in the rock.
			if (r->heat>0) {
				SDL_Surface *deadrock;
				deadrock = surf_deadrock[r->type_number];
				SDL_SetAlpha(deadrock,SDL_SRCALPHA,r->heat*255/r->image->h);
				dest.x = (int) r->x;   // kludge
				SDL_BlitSurface(deadrock,&src,surf_screen,&dest);
				if (rnd()<0.3) r->heat-=movementrate;
			}
			// If the rock is heated past a certain point, the water content of
			// the rock flashes to steam, releasing enough energy to destroy
			// the rock in spectacular fashion.
			if (rock[i].heat>rock[i].image->h) {
				int g;

				rock[i].active=0;
				play_sound(1+(int)(rnd()*3));
				makebangdots(rock[i].x,rock[i].y,rock[i].xvel,rock[i].yvel,rock[i].image,10,3);

				// If a greeblie was going to this rock
				if ( (g=rock[i].greeb) ) {
					// ...and had landed, then the greeblie explodes too.
					if (greeb[g].landed)
						kill_greeb(g);
					else {
						// If the greeblie is not yet landed, then it must now
						// find another rock to jump to. Choose the last active
						// rock found.
						rock[i].greeb=0;
						rock[last_rock].greeb=g;
						greeb[g].target_rock_number = last_rock;
					}
				}
			}
		}
		// Draw any greeblies attached to (or going to) the rock, whether the rock is active or not.
		if (r->greeb && greeb[r->greeb].target_rock_number==i)
			display_greeb(&greeb[r->greeb]);

	}
	// If it's game over, show the game over graphic in the dead centre
	switch (state) {

		case GAME_OVER:
			if (fadetimer<3.0/faderate)
				fadegame=fadetimer/(3.0/faderate);
			else
				fadegame=1.0;

			if (fadetimer<3.0/faderate)
				fadeover=0.0;
			else
				if (fadetimer<6.0/faderate)
					fadeover = ((3.0/faderate)-fadetimer)/(6.0/faderate);
				else
					fadeover = 1.0;

			src.w = surf_t_game->w;
			src.h = surf_t_game->h;
			dest.w = src.w;
			dest.h = src.h;
			dest.x = (xsize-src.w)/2;
			dest.y = (ysize-src.h)/2-40;
			SDL_SetAlpha(surf_t_game, SDL_SRCALPHA, (int)(fadegame*(200+55*cos(fadetimer+=movementrate/1.0))));
			SDL_BlitSurface(surf_t_game,&src,surf_screen,&dest);

			src.w = surf_t_over->w;
			src.h = surf_t_over->h;
			dest.w = src.w;
			dest.h = src.h;
			dest.x = (xsize-src.w)/2;
			dest.y = (ysize-src.h)/2+40;
			SDL_SetAlpha(surf_t_over, SDL_SRCALPHA, (int)(fadeover*(200+55*sin(fadetimer))));
			SDL_BlitSurface(surf_t_over,&src,surf_screen,&dest);
			break;

		case TITLE_PAGE:
			src.w = surf_t_rock->w;
			src.h = surf_t_rock->h;
			dest.w = src.w;
			dest.h = src.h;
			dest.x = (xsize-src.w)/2 + cos(fadetimer/6.5)*10;
			dest.y = (ysize/2-src.h)/2 + sin(fadetimer/5)*10;
			SDL_SetAlpha(surf_t_rock, SDL_SRCALPHA, (int)(200+55*sin(fadetimer+=movementrate/2.0)));
			SDL_BlitSurface(surf_t_rock,&src,surf_screen,&dest);
			src.w = surf_t_dodgers->w;
			src.h = surf_t_dodgers->h;
			dest.w = src.w;
			dest.h = src.h;
			dest.x = (xsize-src.w)/2+sin(fadetimer/6.5)*10;
			dest.y = (ysize/2-src.h)/2 + surf_t_rock->h + 20 + sin((fadetimer+1)/5)*10;
			SDL_SetAlpha(surf_t_dodgers, SDL_SRCALPHA, (int)(200+55*sin(fadetimer-1.0)));
			SDL_BlitSurface(surf_t_dodgers,&src,surf_screen,&dest);

			text = "Version " VERSION EDITION "  (" COMPILEDATE ")";
			x = (xsize-SFont_wide(text))/2 + sin(fadetimer/4.5)*10;
			PutString(surf_screen,x,ysize-50+sin(fadetimer/2)*5,text);

			text = sequence[(int)(fadetimer/40)%nsequence];
			x = (xsize-SFont_wide(text))/2 + cos(fadetimer/4.5)*10;
			PutString(surf_screen,x,ysize-100+cos(fadetimer/3)*5,text);

			break;

		case HIGH_SCORE_ENTRY:

			if (score >= high[7].score) {
				play_tune(2);
				if (SFont_Input (surf_screen, 330, 50+(scorerank+2)*font_height, 300, name)) {
					// Insert name into high score table

					// Lose the lowest name forever (loser!)
					//if (high[7].allocated)
					//	free(high[7].name);		    // THIS WAS CRASHING SO I REMOVED IT

					// Insert new high score
					high[scorerank].score = score;
					strncpy(high[scorerank].name, name, 127);    // MEMORY NEVER FREED!
					high[scorerank].allocated = 1;

					// Set the global name string to "", ready for the next winner
					name[0]=0;

					// Change state to briefly show high scores page
					state = HIGH_SCORE_DISPLAY;
					state_timeout=200;

					// Write the high score table to the file
					write_high_score_table();

					// Play the title page tune
					play_tune(0);
				}
			} else {
				state = HIGH_SCORE_DISPLAY;
				state_timeout=400;
			}

		case HIGH_SCORE_DISPLAY:
			// Display de list o high scores mon.
			PutString(surf_screen,180,50,"High scores");
			for (i=0; i<8; i++) {
				char s[1024];
				sprintf(s, "#%1d",i+1);
				PutString(surf_screen, 150, 50+(i+2)*font_height,s);
				sprintf(s, "%04d", high[i].score);
				PutString(surf_screen, 200, 50+(i+2)*font_height,s);
				sprintf(s, "%3s", high[i].name);
				PutString(surf_screen, 330, 50+(i+2)*font_height,s);
			}
			fadetimer+=movementrate/2.0;
			text = "Version " VERSION EDITION "  (" COMPILEDATE ")";
			x = (xsize-SFont_wide(text))/2 + sin(fadetimer/4.5)*10;
			PutString(surf_screen,x,ysize-50+sin(fadetimer/2)*5,text);

			text = sequence[(int)(fadetimer/40)%nsequence];
			x = (xsize-SFont_wide(text))/2 + cos(fadetimer/4.5)*10;
			PutString(surf_screen,x,ysize-100+cos(fadetimer/3)*5,text);
			break;
		default:
			break;
	}

	if (!gameover && state==GAME_PLAY) {
		int max_offset = surf_screen->pitch/2 * ysize;
		// Show the freaky shields
		SDL_LockSurface(surf_screen);
		raw_pixels = (Uint16 *) surf_screen->pixels;
		if ( ((initialshield > 0) || shieldsup) && (shieldlevel > 0)) {
			int x = 0;
			int y = 0;
			Uint16 c = 0;

			if (initialshield>0) {
				initialshield-=movementrate;
				c = SDL_MapRGB(surf_screen->format,0,255,255);
			} else {
				c = heatcolor[(int)shieldlevel];
				shieldlevel-=movementrate;
			}
			shieldpulse += 0.1;
			for (p = black_point; p < blackptr; p++) {
				x = p->x + (int)xship + (rnd()+rnd()-1)*sin(shieldpulse)*4 + 1;
				y = p->y + (int)yship + (rnd()+rnd()-1)*sin(shieldpulse)*4 + 1;
				if (x >= 0 && y >= 0 && x < xsize && y < ysize) {
					offset = surf_screen->pitch/2 * y + x;
					raw_pixels[offset] = c;
				}
			}
		} else {
			// When the shields are off, check that the black points
			// on the ship are still black, and not covered up by rocks
			for (p=black_point; p<blackptr; p++) {
				offset = surf_screen->pitch/2 * (p->y + (int)yship) + p->x + (int)xship;
				if(offset >= 0 && offset < max_offset) {
					if (raw_pixels[offset]) {
						// Set the bang flag
						bang = 1;
					}
				} else {
					fprintf(stderr, "Warning: getting out of bounds in screen->pixels ! %d >= %d\n", offset, max_offset);
				}
			}
		}
		SDL_UnlockSurface(surf_screen);
	}

	// Draw the score when playing the game or when the game is freshly over
	if (1 || state==GAME_PLAY || state==DEAD_PAUSE || state==GAME_OVER ) {
		int scorepos = 0;
		scorepos = xsize-250;
		snprintf(topline, 50, "Score: %d", score);
		PutString(surf_screen,scorepos,0,topline);
		snprintf(topline, 50, "Level: %d", (int)level);
		PutString(surf_screen,scorepos,20,topline);
	}

	// Draw all the little ships
	if (state==GAME_PLAY || state==DEAD_PAUSE || state==GAME_OVER) {

		for (i=0; i<nships-1; i++) {
			src.w = surf_small_ship->w;
			src.h = surf_small_ship->h;
			dest.w = src.w;
			dest.h = src.h;
			dest.x = (i+1)*(src.w+10);
			dest.y = 20;
			SDL_BlitSurface(surf_small_ship,&src,surf_screen,&dest);
		}
		// Show the shield gauge
		showgauge(10, surf_gauge_shield, shieldlevel/(3*W));
		// Show the laser gauge
		showgauge(200, surf_gauge_laser, laserlevel/(3*W));

		// todo - alter the shield surface to reflect the amount of shields
		// todo - alter the laser surface to reflect the amount of laser power
		// todo - reduce the laser power after every shot
	}
	// Update the score
	ticks_since_last = SDL_GetTicks()-last_ticks;
	last_ticks = SDL_GetTicks();
	if (ticks_since_last>200 || ticks_since_last<0) {
		movementrate = 0;
	} else {
		movementrate = ticks_since_last/50.0;
		if (state==GAME_PLAY)
			inc_score(ticks_since_last/10*level);
	}
	// Update the surface
	SDL_Flip(surf_screen);

	return bang;
}

static void pausegame()
{
	SDL_Rect src,dest;

	paused = 1;
	// SDL_SetAlpha(surf_t_paused, SDL_SRCALPHA, 0);
	src.w = surf_t_paused->w;
	src.h = surf_t_paused->h;
	dest.w = src.w;
	dest.h = src.h;
	dest.x = (XSIZE-src.w)/2;
	dest.y = (YSIZE-src.h)/2;
	SDL_BlitSurface(surf_t_paused,&src,surf_screen,&dest);
	// Update the surface
	SDL_Flip(surf_screen);
}

static void unpausegame()
{
	paused=0;
}

static void clearBuffer() {
	SDL_Event event;

	// Delete the event buffer
	while (SDL_PollEvent(&event))
		;
	// start input
	SDL_EnableUNICODE(1);
}

static int gameloop() {
	int i=0;
	Uint8 *keystate;
	int inputstate[NUM_INPUTS];
	SDL_Joystick *joystick;
	int stick_x, stick_y, button;

	for(;;) {
		if (!paused) {
			// Count down the game loop timer, and change state when it gets to zero or less;

			if ((state_timeout -= movementrate*3) < 0) {
				switch(state) {
					case DEAD_PAUSE:
						// Create a new ship and start all over again
						state = GAME_PLAY;
						play_tune(1);
						initialshield = 150;
						xship = 10;
						yship = ysize/2;
						xvel=2;
						yvel=0;
						shieldlevel = 3*W;
						laserlevel = 3*W;
						break;
					case GAME_OVER:
						state = HIGH_SCORE_ENTRY;
						clearBuffer();
						name[0]=0;
						state_timeout=5.0e6;

						if (score>=high[7].score) {
							// Read the high score table from the storage file
							read_high_score_table();

							// Find ranking of this score, store as scorerank
							for (i=0; i<8; i++) {
								if (high[i].score <= score) {
									scorerank = i;
									break;
								}
							}

							// Move all lower scores down a notch
							for (i=7; i>=scorerank; i--)
								high[i] = high[i-1];

							// Insert blank high score
							high[scorerank].score = score;
							strcpy(high[scorerank].name, "");
							high[scorerank].allocated = 0;
						}
						break;
					case HIGH_SCORE_DISPLAY:
						state = TITLE_PAGE;
						state_timeout=500.0;
						break;
					case HIGH_SCORE_ENTRY:
						// state = TITLE_PAGE;
						// play_tune(1);
						// state_timeout=100.0;
						break;
#ifdef DEMO_ENABLED
					case TITLE_PAGE:
						state = DEMO;
						state_timeout=100.0;
						break;
					case DEMO:
						state = HIGH_SCORE_DISPLAY;
						state_timeout=100.0;
						break;
#else
					case TITLE_PAGE:
						state = HIGH_SCORE_DISPLAY;
						state_timeout=200.0;
						break;
#endif
					default:
						break;
				}
			}
			if (level-(int)level < 0.8) {
				if (state!=GAME_PLAY) countdown -= 0.3 * movementrate;
				else countdown -= rockrate * movementrate;

				while (countdown<0) {
					countdown++;
					// Create a rock
					rockptr++;
					if (rockptr-rock>=MAX_ROCKS)
						rockptr=rock;
					if (!rockptr->active && !rockptr->greeb) {
						rockptr->x = (float)xsize;
						rockptr->y = rnd()*ysize;
						rockptr->xvel = -(rockspeed)*(1+rnd());
						rockptr->yvel = (rockspeed/3.0) * (rnd()-0.5);
						rockptr->type_number = random() % NROCKS;
						rockptr->heat = 0;
						rockptr->image = surf_rock[rockptr->type_number];// [random()%NROCKS];
						rockptr->active = 1;
					}
				}
			}
			// Increase difficulty
			if (state==GAME_PLAY) level += movementrate/250;

			//rockspeed=5.0*xsize/BASE_XSIZE + sqrt((int)level)/2.0;
			//rockrate = 0.2 + sqrt((int)level)/4.0;

			rockspeed=5.0*xsize/BASE_XSIZE + sqrt((int)level)/5.0;
			rockrate = 0.2 + sqrt((int)level)/20.0;

			/*
			 * rockspeed=5.0*xsize/640 + sqrt((int)level)/5.0;
			 * rockrate = 0.2 + sqrt((int)level)/20.0;
			 */ //greebrate = 0.2 + sqrt((int)level)/20.0;

			// Move all the rocks
			for (i=0; i<MAX_ROCKS; i++) if (rock[i].active) {
				rock[i].x += rock[i].xvel*movementrate;
				rock[i].y += rock[i].yvel*movementrate;
				if (rock[i].x<-32.0)
					rock[i].active=0;
			}

			// FRICTION? In space? Oh well.
			xvel *= pow((double)0.9,(double)movementrate);
			yvel *= pow((double)0.9,(double)movementrate);
			// if (abs(xvel)<0.00001) xvel=0;
			// if (abs(yvel)<0.00001) yvel=0;

			// INERTIA
			xship += xvel*movementrate;
			yship += yvel*movementrate;

			// BOUNCE X  (okay, throwing all pretense of realism out the
			// window)
			if (xship<0 || xship>xsize-surf_ship->w) {
				// BOUNCE from left and right wall
				xship -= xvel*movementrate;
				xvel *= -1.19;
				if(xship > xsize - surf_ship->w) xship = xsize - surf_ship->w - 1;
				if(xship < 0) xship = 0;
			}
			// BOUNCE Y
			if (yship<0 || yship>ysize-surf_ship->h) {
				// BOUNCE from top and bottom wall
				yship -= yvel;
				yvel *= -1.19;
				if(yship > ysize - surf_ship->h) yship = ysize - surf_ship->h - 1;
				if(yship < 0) yship = 0;
			}
			if (draw() && state==GAME_PLAY)  {
				if (oss_sound_flag) {
					// Play the explosion sound
					play_sound(0);
				}
				makebangdots(xship,yship,xvel,yvel,surf_ship,30,3);
				if (--nships<=0) {
					gameover=1;
					state=GAME_OVER;
					state_timeout = 200.0;
					fadetimer=0.0;
					faderate=movementrate;
				} else {
					state=DEAD_PAUSE;
					state_timeout = 100.0;
				}
			}
			SDL_PumpEvents();
			keystate = SDL_GetKeyState(NULL);

			if (state!=HIGH_SCORE_ENTRY && (keystate[SDLK_q] || keystate[SDLK_ESCAPE])) {
				return 0;
			}
#ifdef CHEAT
			if (keystate[SDLK_5]) {score += 10000; level++;}
#endif
			if (keystate[SDLK_SPACE] && (state==HIGH_SCORE_DISPLAY || state==TITLE_PAGE || state==DEMO)) {

				for (i=0; i<MAX_ROCKS; i++)
					rock[i].active=0;
				init_greeblies();

				rockspeed=5.0*xsize/BASE_XSIZE;
				nships = 4;
				score = 0;
				level=1;
				state = GAME_PLAY;
				play_tune(1);
				xvel=-1;
				gameover=0;
				yvel=0;
				xship=0;
				yship=ysize/2;
				shieldlevel = 3*W;
				laserlevel = 3*W;
				initialshield = 150;
				play_sound(4);
			}
			maneuver=0;
			laser=0;
		} else {
			SDL_PumpEvents();
			keystate = SDL_GetKeyState(NULL);
			// if (keystate[SDLK_PAUSE]) printf ("p\n");
		}
		if (state==GAME_PLAY) {
			if (!gameover) {
				if (!paused) {
					for(i=0; i<NUM_INPUTS; i++) {
						inputstate[i] = 0;
					}
					if (joystick_flag == 1) {
						joystick = joysticks[0];
						stick_x = SDL_JoystickGetAxis(joystick, 0);
						stick_y = SDL_JoystickGetAxis(joystick, 1);

						if (stick_x > JOYSTICK_DEADZONE)        { inputstate[RIGHT] |= 1; }
						if (stick_x < -JOYSTICK_DEADZONE)       { inputstate[LEFT]  |= 1; }
						if (stick_y > JOYSTICK_DEADZONE)        { inputstate[DOWN]      |= 1; }
						if (stick_y < -JOYSTICK_DEADZONE)       { inputstate[UP]    |= 1; }

						button = SDL_JoystickGetButton(joystick, 0);
						if (button == 1)        { inputstate[LASER] |= 1; }

						button = SDL_JoystickGetButton(joystick, 1);
						if (button == 1)        { inputstate[SHIELD] |= 1; }
					}
					if (keystate) {
						if (keystate[SDLK_UP])      { inputstate[UP]   |= 1; }
						if (keystate[SDLK_DOWN])    { inputstate[DOWN] |= 1; }
						if (keystate[SDLK_LEFT])    { inputstate[LEFT] |= 1; }
						if (keystate[SDLK_RIGHT])   { inputstate[RIGHT]|= 1; }
						if (keystate[SDLK_d])       { inputstate[LASER]|= 1; }
#ifdef CHEAT
						if (keystate[SDLK_1])       { inputstate[FAST] |= 1; }
						if (keystate[SDLK_2])       { inputstate[NOFAST]|= 1;}
#endif
						if (keystate[SDLK_3])       { inputstate[SCREENSHOT]|=1; }
						if (keystate[SDLK_s])       { inputstate[SHIELD] |= 1; }
					}
					if (inputstate[UP])             { yvel -= 1.5*movementrate; maneuver|=1<<3;}
					if (inputstate[DOWN])           { yvel += 1.5*movementrate; maneuver|=1<<1;}
					if (inputstate[LEFT])           { xvel -= 1.5*movementrate; maneuver|=1<<2;}
					if (inputstate[RIGHT])          { xvel += 1.5*movementrate; maneuver|=1;}
					if (inputstate[LASER])          { laser=1; }
					if (inputstate[FAST])           { fast=1; }
					if (inputstate[NOFAST])         { fast=0; }
					if (inputstate[SCREENSHOT])     { SDL_SaveBMP(surf_screen, "snapshot.bmp"); }
					shieldsup = inputstate[SHIELD];
				}

				if (keystate && (keystate[SDLK_PAUSE] || keystate[SDLK_p]))   {
					if (!pausedown) {
						if (paused) {
							unpausegame();
						}
						else {
							pausegame();
						}
						pausedown=1;
					}
				} else {
					pausedown=0;
				}
			} else {
				shieldsup = 0;
				paused = 0;
				pausedown = 0;
			}
		}
		SDL_Delay (50);
		// DEBUG mode to slow down the action, and see if this game is playable on a 486
		if (fast) SDL_Delay (100);
	}
}

// sound.c
#define TUNE_TITLE_PAGE		0
#define TUNE_GAMEPLAY		1
#define TUNE_HIGH_SCORE_ENTRY	2
#define NUM_TUNES		3
#define SOUND_BANG		0
#define NUM_SOUNDS		6
#define MUSIC_VOL (SDL_MIX_MAXVOLUME * 8/10)
#define SNDFX_VOL (MUSIC_VOL / 2)

static Mix_Music *music[NUM_TUNES];
static int music_volume[NUM_TUNES] = {MUSIC_VOL, MUSIC_VOL, MUSIC_VOL};
static Mix_Chunk *wav[NUM_SOUNDS];
static int audio_rate      =  0;
static Uint16 audio_format =  0;
static int audio_channels  =  0;
static int playing         = -1;

static int init_sound()
{

	// Return 1 if the sound is ready to roll, and 0 if not.
	int audio_buffers = 1024;
	// int volume = SDL_MIX_MAXVOLUME;
	// int volume = (SDL_MIX_MAXVOLUME / 2);
#ifdef DEBUG
	printf ("InitialiseX sound\n");
#endif
	// Initialise output with SDL_mixer
	// if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, AUDIO_S16, MIX_DEFAULT_CHANNELS, 512) < 0) {
	if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, audio_buffers) < 0) {
		fprintf(stderr, "Couldn't open SDL_mixer audio: %s\n", SDL_GetError());
		return 0;
	}
	// What kind of sound did we get?  Ah who cares. As long as it can play
	// some basic bangs and simple music.
	Mix_QuerySpec(&audio_rate, &audio_format, &audio_channels);
	printf("Opened audio at %d Hz %d bit %s\n", audio_rate,
			(audio_format&0xFF), (audio_channels > 1) ? "stereo" : "mono");

	{
		int i = 0;

#define LOAD_MUSIC(mod, ptr) \
		rw = SDL_RWFromMem(mod, mod##_len); \
		if(! (ptr = Mix_LoadMUS_RW(rw))) fprintf (stderr, "Failed to load music\n"); \
		SDL_FreeRW(rw)

#define LOAD_WAV(wav, ptr) \
		rw = SDL_RWFromMem(wav, wav##_len); \
		if(! (ptr = Mix_LoadWAV_RW(rw,0))) fprintf (stderr, "Failed to load wave\n"); \
		SDL_FreeRW(rw)

		SDL_RWops *rw = NULL;
		// Preload all the tunes into memory
		LOAD_MUSIC(magic_mod, music[0]);
		LOAD_MUSIC(getzznew_mod, music[1]);
		LOAD_MUSIC(__4est_fulla3s_mod, music[2]);
		// Preload all the wav files into memory
		LOAD_WAV(booom_wav,   wav[0]);
		LOAD_WAV(cboom_wav,   wav[1]);
		LOAD_WAV(boom_wav,    wav[2]);
		LOAD_WAV(bzboom_wav,  wav[3]);
		LOAD_WAV(speedup_wav, wav[4]);
		wav[5] = NULL;

		Mix_AllocateChannels(16);
		for(i = 1; i < 10; i++)
			Mix_Volume(i,SNDFX_VOL);

		Mix_VolumeMusic(MUSIC_VOL);
	}
	return 1;
}

static void play_sound(int i)  {
	if(!oss_sound_flag) return;
#ifdef DEBUG
	printf ("play sound %d on first free channel\n",i);
#endif
	Mix_Volume(i+1, SDL_MIX_MAXVOLUME/4);
	// Mix_PlayChannel(-1, wav[i], 0);
	Mix_PlayChannel(i+1, wav[i], 0);
}

static void play_tune(int i)
{
	if(!oss_sound_flag) return;
	if (playing==i) return;
	if (playing) {
		Mix_FadeOutMusic(15);
#ifdef DEBUG
		printf("Stop playing %d\n",playing);
#endif
	}
#ifdef DEBUG
	printf ("Play music %d\n",i);
	printf ("volume %d\n",music_volume[i]);
#endif
	Mix_FadeInMusic(music[i],-1,20);
	Mix_VolumeMusic(music_volume[i]);
	playing = i;
}

static void cleanup_mem() {
	int i = 0;

	if(surf_screen) SDL_FreeSurface(surf_screen);
	if(surf_t_rock) SDL_FreeSurface(surf_t_rock);
	if(surf_t_dodgers) SDL_FreeSurface(surf_t_dodgers);
	if(surf_t_game) SDL_FreeSurface(surf_t_game);
	if(surf_t_over) SDL_FreeSurface(surf_t_over);
	if(surf_t_paused) SDL_FreeSurface(surf_t_paused);
	if(surf_ship) SDL_FreeSurface(surf_ship);
	if(surf_small_ship) SDL_FreeSurface(surf_small_ship);
	if(surf_font_big) SDL_FreeSurface(surf_font_big);
	if(surf_gauge_shield[0]) SDL_FreeSurface(surf_gauge_shield[0]);
	if(surf_gauge_shield[1]) SDL_FreeSurface(surf_gauge_shield[1]);
	if(surf_gauge_laser[0])  SDL_FreeSurface(surf_gauge_laser[0]);
	if(surf_gauge_laser[1])  SDL_FreeSurface(surf_gauge_laser[1]);
	if(surf_greeblies[0])    SDL_FreeSurface(surf_greeblies[0]);

	for(i = 0; i < NROCKS; i++) {
		if(surf_rock[i])     SDL_FreeSurface(surf_rock[i]);
		if(surf_deadrock[i]) SDL_FreeSurface(surf_deadrock[i]);
	}
}

int main(int argc, char **argv)
{
	int i = 0;
	int x = 0;
	int fullscreen = 0;

	oss_sound_flag = 1;

	while ((x=getopt(argc,argv,"wsfkhx:y:"))>=0) {
		switch(x) {
			/* NOT TESTED YET
			   case 'x':
			   xsize = atoi(optarg);
			   printf ("xsize %d\n",xsize);
			   break;
			   case 'y':
			   ysize = atoi(optarg);
			   printf ("ysize %d\n",ysize);
			   break;
			   */
			case 's':
				oss_sound_flag = 0;
				break;
			case 'k':
				joystick_flag = 0;
				break;
			case 'w':
				fullscreen = 0;
				break;
			case 'f':
				fullscreen = 1;
				break;
			case 'h':
				printf ("Rock Dodger " VERSION EDITION "\n"
						"  -h This help message\n"
						"  -f Full screen\n"
						"  -w Window mode\n"
						"  -k Keyboard only - disable joystick\n"
						"  -s Silent mode (no OSS sound)\n");
				exit(0);
				break;
			default:
				break;
		}
	}
	memset(high, 0, sizeof(high));
	memcpy(high, defaulthigh, sizeof(high));

	surf_gauge_shield[0] = NULL;
	surf_gauge_shield[1] = NULL;
	surf_gauge_laser[0] = NULL;
	surf_gauge_laser[1] = NULL;
	surf_greeblies[0] = NULL;
	for(i = 0; i < NROCKS; i++) {
		surf_rock[i] = NULL;
		surf_deadrock[i] = NULL;
	}

	initrnd();
	if (init(fullscreen)) {
		printf ("Cannot start: '%s'\n",initerror);
		return 1;
	}
	while(1) {
		for (i=0; i<MAX_ROCKS; i++)
			rock[i].active=0;
		rockspeed=5.0*xsize/BASE_XSIZE;
		initticks = SDL_GetTicks();
		if (gameloop()==0) break;
		SDL_Delay(1000);
	}
	cleanup_mem();
	SDL_Quit();
	return 0;
}

