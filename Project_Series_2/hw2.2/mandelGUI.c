//Implementation of multithraded fractals computation 
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <pthread.h>
#include "mandelCore.h"
#include <sched.h>
#include <unistd.h>
#define WinW 300
#define WinH 300
#define ZoomStepFactor 0.5
#define ZoomIterationFactor 2

#include "mybsem.h"

static Display *dsp = NULL;
static unsigned long curC;
static Window win;
static GC gc;

mybsem sem2 ,sem3, sem4;

typedef struct parameters{
	mandel_Pars *pars;
	int maxIters;
	int *res;
	mybsem sem1;
	int done;
}worker;

/* basic win management rountines */

static void openDisplay() {
	if (dsp == NULL) {
		dsp = XOpenDisplay(NULL);
	} 
}

static void closeDisplay() {
	if (dsp != NULL) {
		XCloseDisplay(dsp);
		dsp=NULL;
	}
}

void openWin(const char *title, int width, int height) {
	unsigned long blackC,whiteC;
	XSizeHints sh;
	XEvent evt;
	long evtmsk;
	
	whiteC = WhitePixel(dsp, DefaultScreen(dsp));
	blackC = BlackPixel(dsp, DefaultScreen(dsp));
	curC = blackC;
	
	win = XCreateSimpleWindow(dsp, DefaultRootWindow(dsp), 0, 0, WinW, WinH, 0, blackC, whiteC);
	
	sh.flags=PSize|PMinSize|PMaxSize;
	sh.width=sh.min_width=sh.max_width=WinW;
	sh.height=sh.min_height=sh.max_height=WinH;
	XSetStandardProperties(dsp, win, title, title, None, NULL, 0, &sh);
	
	XSelectInput(dsp, win, StructureNotifyMask|KeyPressMask);
	XMapWindow(dsp, win);
	do {
		XWindowEvent(dsp, win, StructureNotifyMask, &evt);
	} while (evt.type != MapNotify);
	
	gc = XCreateGC(dsp, win, 0, NULL);
	
}

void closeWin() {
	XFreeGC(dsp, gc);
	XUnmapWindow(dsp, win);
	XDestroyWindow(dsp, win);
}

void flushDrawOps() {
	XFlush(dsp);
}

void clearWin() {
	XSetForeground(dsp, gc, WhitePixel(dsp, DefaultScreen(dsp)));
	XFillRectangle(dsp, win, gc, 0, 0, WinW, WinH);
	flushDrawOps();
	XSetForeground(dsp, gc, curC);
}

void drawPoint(int x, int y) {
	XDrawPoint(dsp, win, gc, x, WinH-y);
	flushDrawOps();
}

void getMouseCoords(int *x, int *y) {
	XEvent evt;
	
	XSelectInput(dsp, win, ButtonPressMask);
	do {
		XNextEvent(dsp, &evt);
	} while (evt.type != ButtonPress);
	*x=evt.xbutton.x; *y=evt.xbutton.y;
}

/* color stuff */

void setColor(char *name) {
	XColor clr1,clr2;
	
	if (!XAllocNamedColor(dsp, DefaultColormap(dsp, DefaultScreen(dsp)), name, &clr1, &clr2)) {
		printf("failed\n"); return;
	}
	XSetForeground(dsp, gc, clr1.pixel);
	curC = clr1.pixel;
}

char *pickColor(int v, int maxIterations) {
	static char cname[128];
	
	if (v == maxIterations) {
		return("black");
	}
	else {
		sprintf(cname,"rgb:%x/%x/%x",v%64,v%128,v%256);
		return(cname);
	}
}


void *mandel_foo(void * arg){
	worker *my_parameters = (worker *)arg;  
	
	while(1){
		
		mybsem_down(&my_parameters->sem1);
		
		mandel_Calc(my_parameters->pars, my_parameters->maxIters, my_parameters->res);
		printf("thread %lu: completed the calculation\n", pthread_self());
		
		mybsem_down(&sem2);
		my_parameters->done = 1;
		mybsem_up(&sem4);
		mybsem_down(&sem3);
		mybsem_up(&sem2);
	}
	return NULL;
}


int main(int argc, char *argv[]) {
	mandel_Pars pars,*slices;
	int i,j,k,x,y,nofslices,maxIterations,level,*res;
	int xoff,yoff;
	long double reEnd,imEnd,reCenter,imCenter;
	int pthread_res;
	pthread_t *worker_threads;
	worker *main_results;
	int all_finished = 0;
	
	printf("\n");
	printf("This program starts by drawing the default Mandelbrot region\n");
	printf("When done, you can click with the mouse on an area of interest\n");
	printf("and the program will automatically zoom around this point\n");
	printf("\n");
	printf("Press enter to continue\n");
	getchar();
	
	pars.reSteps = WinW; /* never changes */
	pars.imSteps = WinH; /* never changes */
	
	/* default mandelbrot region */
	
	pars.reBeg = (long double) -2.0;
	reEnd = (long double) 1.0;
	pars.imBeg = (long double) -1.5;
	imEnd = (long double) 1.5;
	pars.reInc = (reEnd - pars.reBeg) / pars.reSteps;
	pars.imInc = (imEnd - pars.imBeg) / pars.imSteps;
	
	printf("enter max iterations (50): ");
	scanf("%d",&maxIterations);
	printf("enter no of slices: ");
	scanf("%d",&nofslices);
	
	/* adjust slices to divide win height */
	
	while (WinH % nofslices != 0) { nofslices++;}
	
	/* allocate slice parameter and result arrays */
	
	slices = (mandel_Pars *) malloc(sizeof(mandel_Pars)*nofslices);
	res = (int *) malloc(sizeof(int)*pars.reSteps*pars.imSteps);
	worker_threads = (pthread_t *) malloc(sizeof(pthread_t)*nofslices); //Allocate memory for the array of thread ids
	main_results = (worker *) malloc(sizeof(worker)*(nofslices));         //Allocate memory for each worker's parameters
	
	/* open window for drawing results */
	
	openDisplay();
	openWin(argv[0], WinW, WinH);
	
	level = 1;
	
	// create N threads
	for (i=0; i<nofslices; i++) {
		mybsem_init(&main_results[i].sem1, 0);
		
		pthread_res = pthread_create(&worker_threads[i], NULL, mandel_foo, (void *)&main_results[i]);
		if(pthread_res){
			printf("Error with thread %d\n", i);
			exit(1);
		}
		
	}
	
	printf("All threads created\n");
	
	mybsem_init(&sem2, 1);
	mybsem_init(&sem3, 0);
	mybsem_init(&sem4, 0);

	while (1) {
		
		clearWin();
		
		mandel_Slice(&pars,nofslices,slices);
		
		for (i=0; i<nofslices; i++) {
			// Assign values to the workers struct to calculate the fractal concurrently
			main_results[i].pars = &slices[i];
			main_results[i].maxIters = maxIterations;
			main_results[i].res = &res[i*slices[i].imSteps*slices[i].reSteps];
			main_results[i].done = 0;
			mybsem_up(&main_results[i].sem1);
		}
		
		y=0;
		for (k=0; k<nofslices; k++) {
			
			mybsem_down(&sem4);
			mybsem_up(&sem3);
			
			y=0;
			for (i=0; i<nofslices; i++) {
				if(main_results[i].done == 1){
					printf("starting slice nr. %d\n",i+1);
					for (j=0; j<main_results[i].pars->imSteps; j++) {
						for (x=0; x<main_results[i].pars->reSteps; x++) {
							setColor(pickColor(res[y*main_results[i].pars->reSteps+x],main_results[i].maxIters));
							drawPoint(x,y);
						}
						y++;
					}
					main_results[i].done = 2;
				}
				else{
					y += main_results[i].pars->imSteps;
				}
			}
		}
		
		printf("done\n");
		
		/* get next focus/zoom point */
		
		getMouseCoords(&x,&y);
		xoff = x;
		yoff = WinH-y;
		
		/* adjust region and zoom factor  */
		
		reCenter = pars.reBeg + xoff*pars.reInc;
		imCenter = pars.imBeg + yoff*pars.imInc;
		pars.reInc = pars.reInc*ZoomStepFactor;
		pars.imInc = pars.imInc*ZoomStepFactor;
		pars.reBeg = reCenter - (WinW/2)*pars.reInc;
		pars.imBeg = imCenter - (WinH/2)*pars.imInc;
		
		maxIterations = maxIterations*ZoomIterationFactor;
		level++;
	}
	
	/* never reach this point; for cosmetic reasons */
	
	free(slices);
	free(res);
	
	closeWin();
	closeDisplay();
	
}
