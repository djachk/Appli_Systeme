#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "sched.h"



struct tache {
	taskfunc f;  //pointeur sur fonction
	void* closure;
	struct scheduler* s;
};
typedef struct tache tache;


struct element_pile {
	tache* la_tache;
	struct element_pile* suivant;
	struct element_pile* precedent;
};
typedef struct element_pile element_pile;

struct pile {
	element_pile* top;
	element_pile* racine;
	int taille;
};
typedef struct pile pile;

////////////////////////////////////
//pour debug avec fonction quicksort uniquement
struct quicksort_args {
    int *a;
    int lo, hi;
};
void lire_cloture(void * closure) {
    struct quicksort_args *args = (struct quicksort_args *)closure;
    int lo = args->lo;
    int hi = args->hi;
    printf("cloture lo= %d  hi=%d\n",lo,hi);
}
////////////////////////////////////////////////////
//initialisation d'une pile
void initialisation (pile* p){
	//printf("j'initialise la pile\n");
	element_pile* une_racine=malloc(sizeof *une_racine);
	if (une_racine==NULL) printf("MALLOC A ECHOUE POUR UNR_RACINE");
	element_pile* un_top=malloc(sizeof *un_top);
	if (un_top==NULL) printf("MALLOC A ECHOUE POUR UN TOP");
	une_racine->precedent=NULL;
	une_racine->suivant=un_top;
	une_racine->la_tache=NULL;
	un_top->precedent=une_racine;
	un_top->suivant=NULL;
	un_top->la_tache=NULL;

	//p=malloc(sizeof *p);
  	p->top= un_top;
  	p->racine = une_racine;
  	p->taille = 0;

}

///VARIABLES GLOBALES (d'autres ci_dessous) 
//pthread_mutex_t mutex_pile = PTHREAD_MUTEX_INITIALIZER;
//pile* stack;
void afficher_pile(pile* p);

//ajouter une tache dans la pile
void ajouter_tache(tache* t, pile* p) {
	
	element_pile* nvel_element=malloc(sizeof *nvel_element);
	if (nvel_element==NULL) printf("MALLOC A ECHOUE POUR NOUVEL_ELEMENT");
	element_pile* curr_element=(p->top)->precedent;

	nvel_element->precedent=curr_element;
	nvel_element->suivant=p->top;
	nvel_element->la_tache=t;
	(p->top)->precedent=nvel_element;

	curr_element->suivant=nvel_element;
	(p->taille)+=1;

}

//prendre une tache en haut de la pile
//tache* prendre_tache(pile* p,int id) {
element_pile* prendre_tache(pile* p,int id) {
	if (((p->top)->precedent)==p->racine) {
		//printf("-taille de la pile normalement nulle= %d\n",p->taille);
		//printf("-pas de tache\n");
		return NULL;
	}
	//printf("-taille de la pile non nulle= %d\n",p->taille);
	//printf("-il y a une tache\n");

	element_pile* curr_element=(p->top)->precedent;
	element_pile* nvel_current=curr_element->precedent;
	nvel_current->suivant=p->top;
	(p->top)->precedent=nvel_current;
	//tache * res=curr_element->la_tache;
	element_pile* res=curr_element; //****
	(p->taille)-=1;


	//free(curr_element);
	return res;
}


struct info_thread {
	pthread_t un_thread;
	int compteur;
	int actif;
};
typedef struct info_thread info_thread;




///////////////////////////////////////
//VARIABLES GLOBALES
//info_thread* thread_pool;
//struct scheduler * s;
//pthread_cond_t cond_oisif = PTHREAD_COND_INITIALIZER;
//pthread_mutex_t mutex_oisif = PTHREAD_MUTEX_INITIALIZER;
//taskfunc f0; 
struct scheduler {
	pile* stack;
	pthread_mutex_t mutex_pile;
	pthread_mutex_t mutex_oisif;
	pthread_cond_t cond_oisif;
	info_thread* thread_pool;
};

struct boucle_args {
	int id_thread;
	int nbthreads;
	struct scheduler* s;
};
typedef struct boucle_args boucle_args;
////////////////////////////////////////////

int threads_actifs(int nbthreads, struct scheduler* s) {  //pb synchro???......
	int res=0;
	for (int i=0;i<nbthreads;i++) {
		if (s->thread_pool[i].actif==1) res++;
	}
	return res;
}
//////////////////////////////////////
void afficher_pile(pile* p) {
	printf("------contenu de la pile--------\n");
	element_pile* deb=p->top;
	element_pile* suiv=deb->precedent;
	while (suiv!=p->racine) {
		tache * t=suiv->la_tache;
		lire_cloture(t->closure);
		suiv=suiv->precedent;
	}
	printf("------------------------------\n");
}


 
/////////////////
//boucle principale qu'executent tous les threads
void* boucle_travail(void *arg) {
	boucle_args* args=(boucle_args*) arg;
	int id_thread=args->id_thread;
	int nbthr=args->nbthreads;
	struct scheduler* s=args->s;

	//printf("boucle de travail pour le thread %d nb de threads %d\n",id_thread,nbthr);
	while(1) {  
		pthread_mutex_lock(&(s->mutex_pile));
		//afficher_pile(stack);
		//tache* t=prendre_tache(stack,id_thread);
		element_pile* ep=prendre_tache(s->stack,id_thread); //****
		pthread_mutex_unlock(&(s->mutex_pile));
		if (ep!=NULL) {
			//printf("prise de tache thread %d\n",id_thread);
			pthread_mutex_lock(&(s->mutex_oisif)); //???
			s->thread_pool[id_thread].actif=1;
			pthread_mutex_unlock(&(s->mutex_oisif)); //???
			s->thread_pool[id_thread].compteur+=1;
			
			//taskfunc fonct=t->f;
			//void* cloture=t->closure;
			//struct scheduler* sch=t->s; 
			
			taskfunc fonct=ep->la_tache->f; //***
			void* cloture=ep->la_tache->closure; //***
			struct scheduler* sch=ep->la_tache->s; //*****
			fonct(cloture,sch);  //execution de la fonction
			free(ep);
			//printf("j'ai executé une fonction dans le thread %d\n",id_thread);
		} else {
			pthread_mutex_lock(&(s->mutex_oisif));
			s->thread_pool[id_thread].actif=0;
			if (threads_actifs(nbthr,s)==0) {
				pthread_mutex_unlock(&(s->mutex_oisif));
				break;
			}
			//pthread_mutex_lock(&(s->mutex_oisif));
			if (pthread_cond_wait(&(s->cond_oisif), &(s->mutex_oisif))!=0) printf("ERREUR DANS COND_WAIT");
			s->thread_pool[id_thread].actif=1;
			pthread_mutex_unlock(&(s->mutex_oisif));

		}

	}
	pthread_mutex_lock(&(s->mutex_oisif));
	pthread_cond_broadcast(&(s->cond_oisif));
	pthread_mutex_unlock(&(s->mutex_oisif));
	//printf("JE SORS thread %d------------------------------------------------------------->\n",id_thread);
    pthread_exit(NULL);
}

//////////////////////////////////////////////
int sched_init(int nthreads, int qlen, taskfunc f, void *closure) {

	printf("version une seule pile LIFO, nbre de threads= %d\n",nthreads);
	printf("votre machine a %d coeurs\n", sched_default_threads());

	info_thread*  thread_pool=malloc(nthreads*(sizeof *thread_pool));
	pile* stack=malloc(sizeof *stack);
	initialisation(stack);

	pthread_cond_t cond_oisif = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t mutex_oisif = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t mutex_pile = PTHREAD_MUTEX_INITIALIZER;

	struct scheduler* s=malloc(sizeof *s);
	s->stack=stack;
	s->mutex_pile=mutex_pile;
	s->mutex_oisif=mutex_oisif;
	s->cond_oisif=cond_oisif;
	s->thread_pool=thread_pool;
	

	tache* t=malloc(sizeof *t);
	t->f=f;
	t->closure=closure;
	t->s=s;

	pthread_mutex_lock(&(s->mutex_pile));
	ajouter_tache(t,s->stack);
	pthread_mutex_unlock(&(s->mutex_pile));
	//afficher_pile(s->stack);

	for (int i=0;i<nthreads;i++) {
		s->thread_pool[i].actif=1;
		s->thread_pool[i].compteur=0;
	}


	for (int i=0;i<nthreads;i++) {
		boucle_args* arg=malloc(sizeof *arg);
		arg->id_thread=i;
		arg->nbthreads=nthreads;
		arg->s=s;
		pthread_create( &(s->thread_pool[i].un_thread) , NULL, boucle_travail, arg);
	}
	//printf("TOUS LES THREADS CREES = %d\n",nthreads);




	for (int i=0;i<nthreads;i++) {
		pthread_join(s->thread_pool[i].un_thread , NULL);
		printf("Le thread %d est sorti, taches executees = %d\n",i,s->thread_pool[i].compteur);
		pthread_cond_broadcast(&(s->cond_oisif));
	}
	//printf("TOUT LE MONDE EST SORTI\n");

	return 0;
}

/////////////////////////////////////////////////////////////
int sched_spawn(taskfunc f, void *closure, struct scheduler *s) {

	//printf("j'entre dans spawn\n");
	//afficher_pile(stack);

	tache* tt=malloc(sizeof *tt);
	tt->f=f;
	tt->closure=closure;
	tt->s=s;

	pthread_mutex_lock(&(s->mutex_pile));
	ajouter_tache(tt,s->stack);
	pthread_mutex_unlock(&(s->mutex_pile));

	pthread_mutex_lock(&(s->mutex_oisif));
	pthread_cond_broadcast(&(s->cond_oisif));
	pthread_mutex_unlock(&(s->mutex_oisif));

	return 0;
}


