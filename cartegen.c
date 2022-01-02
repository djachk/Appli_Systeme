#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#include "sched.h"
#include <math.h>
#include <SDL/SDL.h>

struct cartegen_args {
    int** c;
    int idep;
    int jdep;
    int dim;
    int alt;
};

struct cartegen_args* new_args(int** c, int idep, int jdep, int dim, int alt){
    
    struct cartegen_args* args = malloc(sizeof(struct cartegen_args));
    if(args == NULL)
        return NULL;

    args->c = c;
    args->idep=idep;
    args->jdep=jdep;
    args->dim = dim;
    args->alt=alt;
    return args;
}

void uncote(int** c, int idep, int jdep, int dim, int direction, int alt) {   // direction 0 horizontal, 1 vertical, longueur=nbre de points puissance de 2
	//if (longueur==1) return;
	int delta=dim;
	int lalt=alt;
	if (direction==0) {
		while(delta>1) {
			int ind=idep+delta/2;
			while (ind < idep+dim) {
				int unealt=rand()%lalt;
				c[ind][jdep]=(c[ind-delta/2][jdep] + c[ind+delta/2][jdep])/2 + unealt;
				ind+=delta;
			}
			delta/=2;
			lalt/=2; if (lalt==0) lalt=4;
		}
	} else {
		while(delta>1) {
			int ind=jdep+delta/2;
			while (ind < jdep+dim) {
				int unealt=rand()%lalt;
				c[idep][ind]= (c[idep][ind-delta/2] + c[idep][ind+delta/2])/2 + unealt;
				ind+=delta;
			}
			delta/=2;
			lalt/=2; if (lalt==0) lalt=4;
		}
	}
}



void initbords(int** c, int dim, int alt) {
	uncote(c,0,0,dim,0,alt);
	uncote(c,0,dim,dim,0,alt);
	uncote(c,0,0,dim,1,alt);
	uncote(c,dim,0,dim,1,alt);
}


void cartegen(void *closure, struct scheduler *s) {
	
	int rc;

	struct cartegen_args *args = (struct cartegen_args *)closure;
	int** c=args->c;
    int idep=args->idep;
    int jdep=args->jdep;
    int dim=args->dim;
    int alt=args->alt;
    if (alt==0) alt=4;

    //if (dim==1) return;
    if (dim <= 512) {
    	for(int j=1;j<dim;j++) {
    		uncote(c,idep,jdep+j,dim,0,alt);
    	}
    return;
    }

    uncote(c,idep,jdep+dim/2,dim,0,alt);
    uncote(c,idep+dim/2,jdep,dim,1,alt);

    //printf("je fais 4 spawn, idep=%d jdep=%d dim=%d alt=%d\n", idep,jdep,dim,alt);
    rc = sched_spawn(cartegen, new_args(c,idep,jdep,dim/2,alt/2 ), s);            
    assert(rc >= 0);
    rc = sched_spawn(cartegen, new_args(c,idep+dim/2,jdep,dim/2,alt/2 ), s);            
    assert(rc >= 0);
    rc = sched_spawn(cartegen, new_args(c,idep,jdep+dim/2,dim/2,alt/2 ), s);            
    assert(rc >= 0);
    rc = sched_spawn(cartegen, new_args(c,idep+dim/2,jdep+dim/2,dim/2,alt/2 ), s);            
    assert(rc >= 0);

}


int main(int argc, char **argv) {

	int k=3;
	int rc, nthreads=4;
	int dim;
	int alt=100;
	struct timespec begin, end;
	double delay;
	//printf("%d\n",dim);
	while(1) {
        int opt = getopt(argc, argv, "k:a:t:");
        if(opt < 0)
            break;
        switch(opt) {
        case 'k':
            k = atoi(optarg);
            if(k <= 0)
                goto usage;
            break;
         case 'a':
            alt = atoi(optarg);
            if(alt <= 0)
                goto usage;
            break;           
        case 't':
            nthreads = atoi(optarg);
            if(nthreads <= 0)
                goto usage;
            break;
        default:
            goto usage;
        }
    }

	dim=(int) pow(2,k);
	//printf("dim=%d\n",dim);
	//printf("nthreads%d\n",nthreads);
	//printf("alt=%d\n",alt);


	int** c=malloc((dim+1)*sizeof(int*));
	for (int i=0;i<dim+1;i++) {
		c[i]=malloc((dim+1)*sizeof(int));
	}
	srand(time(NULL));
	c[0][0]=0;
	c[0][dim]=0;
	c[dim][0]=0;
	c[dim][dim]=0;

    clock_gettime(CLOCK_MONOTONIC, &begin);

	initbords(c,dim,alt);

    rc = sched_init(nthreads, 1, cartegen, new_args(c, 0, 0, dim, alt));
    assert(rc >= 0);

    clock_gettime(CLOCK_MONOTONIC, &end);
    delay = end.tv_sec + end.tv_nsec / 1000000000.0 - (begin.tv_sec + begin.tv_nsec / 1000000000.0);
    printf("Done in %lf seconds.\n", delay);

	//affichage
	if (k <= 4) {
		for(int i=0;i<dim+1;i++) {
			for(int j=0;j<dim+1;j++) {
				printf(" %d",c[i][j]);
				if (j==dim) printf("\n");
			}
		}
	} else if (k<=6) {
		for(int j=0;j<dim+1;j++) {
				printf(" %d",c[0][j]);
				if (j==dim) printf("\n");
		}	
		for(int j=0;j<dim+1;j++) {
				printf(" %d",c[dim][j]);
				if (j==dim) printf("\n");
		}				
	}



	//Demarrer SDL 
	int taillex=700,tailley=700;
	SDL_Init( SDL_INIT_EVERYTHING ); 
 	SDL_Surface* ecran = SDL_SetVideoMode(taillex, tailley, 32, SDL_SWSURFACE | SDL_DOUBLEBUF); 

	void putpixel(int x, int y, Uint32 couleur) 
	{    
		*( (Uint32*)(ecran->pixels)+x+y*ecran->w ) = couleur; 
	} 
	
	Uint32 blanc=SDL_MapRGB(ecran->format, 255,255,255);
	//putpixel(500, 100, blanc);
	int* vu=malloc((taillex+1)*sizeof(int));
	for (int i=0; i<taillex+1;i++) {
		vu[i]=0;
	}
	double pas=(double)dim/(double)taillex;
	
	for (int j=1; j<tailley-1;j++) {
		if(j%5==0) {
			for (int i=1; i<taillex-1;i++) {
				int alt=c[(int)(i*pas)][(int)(j*pas)];
				if (alt+j/2>vu[i]) {
					if (tailley-alt-j/2 > 0) {
						putpixel(i,tailley-alt-j/2,blanc);
						vu[i]=alt + j/2;
					}
				}
			}
		}
	}
	SDL_Flip(ecran); 
	pause();
	//Quitter SDL 
	SDL_Quit();   


	return 0;

 usage:
    printf("cartegn [-k k] [-a altitude] [-t threads] \n");
    return 1;
}