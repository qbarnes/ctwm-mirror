/*
 * Animation routines
 */


#include "ctwm.h"

#include <sys/time.h>
#include <stdio.h>
#include <string.h>

#include "events.h"
#include "icons.h"
#include "image.h"
#include "screen.h"
#include "types.h"
#include "util.h"

#include "animate.h"


#define MAXANIMATIONSPEED 20


int  Animating        = 0;
int  AnimationSpeed   = 0;
Bool AnimationActive  = False;
Bool MaybeAnimate     = True;
struct timeval AnimateTimeout;


static void Animate(void);
static void AnimateButton(TBWindow *tbw);
static void AnimateHighlight(TwmWindow *t);
static void AnimateIcons(ScreenInfo *scr, Icon *icon);


/* XXX Hopefully temporary */
extern FILE *tracefile;

void
TryToAnimate(void)
{
	struct timeval  tp;
	static unsigned long lastsec;
	static long lastusec;
	unsigned long gap;

	if(Animating > 1) {
		return;        /* rate limiting */
	}

	gettimeofday(&tp, NULL);
	gap = ((tp.tv_sec - lastsec) * 1000000) + (tp.tv_usec - lastusec);
	if(tracefile) {
		fprintf(tracefile, "Time = %lu, %ld, %ld, %ld, %lu\n", lastsec,
		        lastusec, (long)tp.tv_sec, (long)tp.tv_usec, gap);
		fflush(tracefile);
	}
	gap *= AnimationSpeed;
	if(gap < 1000000) {
		return;
	}
	if(tracefile) {
		fprintf(tracefile, "Animate\n");
		fflush(tracefile);
	}
	Animate();
	lastsec  = tp.tv_sec;
	lastusec = tp.tv_usec;
}



void
StartAnimation(void)
{

	if(AnimationSpeed > MAXANIMATIONSPEED) {
		AnimationSpeed = MAXANIMATIONSPEED;
	}
	if(AnimationSpeed <= 0) {
		AnimationSpeed = 0;
	}
	if(AnimationActive) {
		return;
	}
	switch(AnimationSpeed) {
		case 0 :
			return;
		case 1 :
			AnimateTimeout.tv_sec  = 1;
			AnimateTimeout.tv_usec = 0;
			break;
		default :
			AnimateTimeout.tv_sec  = 0;
			AnimateTimeout.tv_usec = 1000000 / AnimationSpeed;
	}
	AnimationActive = True;
}


void
StopAnimation(void)
{
	AnimationActive = False;
}


void
SetAnimationSpeed(int speed)
{
	AnimationSpeed = speed;
	if(AnimationSpeed > MAXANIMATIONSPEED) {
		AnimationSpeed = MAXANIMATIONSPEED;
	}
}


void
ModifyAnimationSpeed(int incr)
{
	if((AnimationSpeed + incr) < 0) {
		return;
	}
	if((AnimationSpeed + incr) == 0) {
		if(AnimationActive) {
			StopAnimation();
		}
		AnimationSpeed = 0;
		return;
	}
	AnimationSpeed += incr;
	if(AnimationSpeed > MAXANIMATIONSPEED) {
		AnimationSpeed = MAXANIMATIONSPEED;
	}

	if(AnimationSpeed == 1) {
		AnimateTimeout.tv_sec  = 1;
		AnimateTimeout.tv_usec = 0;
	}
	else {
		AnimateTimeout.tv_sec  = 0;
		AnimateTimeout.tv_usec = 1000000 / AnimationSpeed;
	}
	AnimationActive = True;
}



/*
 * Only called from TryToAnimate
 */
static void
Animate(void)
{
	TwmWindow   *t;
	int         scrnum;
	ScreenInfo  *scr;
	int         i;
	TBWindow    *tbw;
	int         nb;

	if(AnimationSpeed == 0) {
		return;
	}
	if(Animating > 1) {
		return;        /* rate limiting */
	}

	/* Impossible? */
	if(NumScreens < 1) {
		return;
	}

	MaybeAnimate = False;
	scr = NULL;
	for(scrnum = 0; scrnum < NumScreens; scrnum++) {
		if((scr = ScreenList [scrnum]) == NULL) {
			continue;
		}

		for(t = scr->FirstWindow; t != NULL; t = t->next) {
			if(! visible(t)) {
				continue;
			}
			if(t->icon_on && t->icon && t->icon->bm_w && t->icon->image &&
			                t->icon->image->next) {
				AnimateIcons(scr, t->icon);
				MaybeAnimate = True;
			}
			else if(t->mapped && t->titlebuttons) {
				nb = scr->TBInfo.nleft + scr->TBInfo.nright;
				for(i = 0, tbw = t->titlebuttons; i < nb; i++, tbw++) {
					if(tbw->image && tbw->image->next) {
						AnimateButton(tbw);
						MaybeAnimate = True;
					}
				}
			}
		}
		if(scr->Focus) {
			t = scr->Focus;
			if(t->mapped && t->titlehighlight && t->title_height &&
			                t->HiliteImage && t->HiliteImage->next) {
				AnimateHighlight(t);
				MaybeAnimate = True;
			}
		}
	}
	MaybeAnimate |= AnimateRoot();
	if(MaybeAnimate) {
		Animating++;
		SendEndAnimationMessage(scr->currentvs->wsw->w, LastTimestamp());
	}
	XFlush(dpy);
	return;
}


/* Originally in add_window.c */
static void
AnimateButton(TBWindow *tbw)
{
	Image       *image;
	XSetWindowAttributes attr;

	image = tbw->image;
	attr.background_pixmap = image->pixmap;
	XChangeWindowAttributes(dpy, tbw->window, CWBackPixmap, &attr);
	XClearWindow(dpy, tbw->window);
	tbw->image = image->next;
}

/* Originally in add_window.c */
static void
AnimateHighlight(TwmWindow *t)
{
	Image       *image;
	XSetWindowAttributes attr;

	image = t->HiliteImage;
	attr.background_pixmap = image->pixmap;
	if(t->hilite_wl) {
		XChangeWindowAttributes(dpy, t->hilite_wl, CWBackPixmap, &attr);
		XClearWindow(dpy, t->hilite_wl);
	}
	if(t->hilite_wr) {
		XChangeWindowAttributes(dpy, t->hilite_wr, CWBackPixmap, &attr);
		XClearWindow(dpy, t->hilite_wr);
	}
	t->HiliteImage = image->next;
}


/* Originally in icons.c */
static void
AnimateIcons(ScreenInfo *scr, Icon *icon)
{
	Image       *image;
	XRectangle  rect;
	XSetWindowAttributes attr;
	int         x;

	image = icon->image;
	attr.background_pixmap = image->pixmap;
	XChangeWindowAttributes(dpy, icon->bm_w, CWBackPixmap, &attr);

	if(image->mask != None) {
		x = GetIconOffset(icon);
		XShapeCombineMask(dpy, icon->bm_w, ShapeBounding, 0, 0, image->mask, ShapeSet);
		if(icon->has_title) {
			rect.x      = 0;
			rect.y      = icon->height;
			rect.width  = icon->w_width;
			rect.height = scr->IconFont.height + 6;

			XShapeCombineShape(dpy, scr->ShapeWindow, ShapeBounding, x, 0, icon->bm_w,
			                   ShapeBounding, ShapeSet);
			XShapeCombineRectangles(dpy, scr->ShapeWindow, ShapeBounding, 0, 0, &rect, 1,
			                        ShapeUnion, 0);
			XShapeCombineShape(dpy, icon->w, ShapeBounding, 0, 0, scr->ShapeWindow,
			                   ShapeBounding, ShapeSet);
		}
		else
			XShapeCombineShape(dpy, icon->w, ShapeBounding, x, 0, icon->bm_w,
			                   ShapeBounding, ShapeSet);
	}
	XClearWindow(dpy, icon->bm_w);
	icon->image  = image->next;
	return;
}
