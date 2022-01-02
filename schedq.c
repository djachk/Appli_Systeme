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
	pthread_mutex_t* mutex_pile;
};
typedef struct pile pile;


struct info_thread {
	pthread_t un_thread;
	int compteur;
	int vols_reussis;
	int vols_rates;
	int sommeils;
	int actif;
	long thread_self_id;
};
typedef struct info_thread info_thread;

struct scheduler {
	pile** stack;
	int nbre_threads;
	pthread_mutex_t mutex_oisif;
	pthread_cond_t cond_oisif;
	info_thread* thread_pool;
};

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


void afficher_pile(pile* p);

//ajouter une tache dans la pile
void ajouter_tache_haut(tache* t, pile* p) {
	
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

void ajouter_tache_bas(tache* t, pile* p) {
	
	element_pile* nvel_element=malloc(sizeof *nvel_element);
	if (nvel_element==NULL) printf("MALLOC A ECHOUE POUR NOUVEL_ELEMENT");
	element_pile* curr_element=(p->racine)->suivant;

	nvel_element->suivant=curr_element;
	nvel_element->precedent=p->racine;
	nvel_element->la_tache=t;
	(p->racine)->suivant=nvel_element;

	curr_element->precedent=nvel_element;
	(p->taille)+=1;
}

//prendre une tache en haut de la pile
//tache* prendre_tache(pile* p,int id) {
element_pile* prendre_tache_haut(pile* p,int id) {
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

	return res;
}

element_pile* prendre_tache_bas(pile* p,int id) {
	if (((p->racine)->suivant)==p->top) {
		//printf("-taille de la pile normalement nulle= %d\n",p->taille);
		//printf("-pas de tache\n");
		return NULL;
	}
	//printf("-taille de la pile non nulle= %d\n",p->taille);
	//printf("-il y a une tache\n");

	element_pile* curr_element=(p->racine)->suivant;
	element_pile* nvel_current=curr_element->suivant;
	nvel_current->precedent=p->racine;
	(p->racine)->suivant=nvel_current;
	//tache * res=curr_element->la_tache;
	element_pile* res=curr_element; //****
	(p->taille)-=1;

	return res;
}


element_pile* voler_travail(struct scheduler* s, int id) {

	int compteur=0, ind=id, r;
	int taille=s->nbre_threads;
	element_pile* ep;
	//printf("tentative de vol par le thread %d\n", id);
	if (taille>2) {
		r=rand()%(taille-1);
		for (int i=0; i<r; i++) {
			ind++; if (ind>=taille) ind-=taille;
			if (ind==id) {
				ind++; if (ind>=taille) ind-=taille;
			}
		}
	} 

	while(1) {
		
		if (compteur>=taille-1) break;
		ind++;
		if (ind>=taille) ind-=taille;
		if (ind==id) continue;
		//printf("j essaie de voler au thread %d\n",ind);
		pthread_mutex_lock((s->stack[ind]->mutex_pile));
		ep=prendre_tache_haut(s->stack[ind], id);
		pthread_mutex_unlock((s->stack[ind]->mutex_pile));
		if (ep!=NULL) {
			//printf("moi thread %d j ai reussi a voler\n",id);
			s->thread_pool[id].vols_reussis+=1;;
			return ep;
		} else {
			s->thread_pool[id].vols_rates+=1;;
			usleep(1000);
		}
		compteur++;
	}
	return NULL;
}
 


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

int num_thread(long thself, struct scheduler * s) {
	for (int i=0;i<s->nbre_threads;i++) {
		if (s->thread_pool[i].thread_self_id==thself) {
			return i;
		}
	}
	printf("erreur dans recup de num_thread");
	return -1;
}
 
/////////////////
//boucle principale qu'executent tous les threads
void* boucle_travail(void *arg) {
	boucle_args* args=(boucle_args*) arg;
	int id_thread=args->id_thread;
	int nbthr=args->nbthreads;
	struct scheduler* s=args->s;
	element_pile* ep;

	if (s->thread_pool[id_thread].thread_self_id==-1) {
		s->thread_pool[id_thread].thread_self_id=(long) pthread_self();
	}
	//printf("boucle de travail pour le thread %d nb de threads %d\n",id_thread,nbthr);
	while(1) {  
		//if (s->thread_pool[id_thread].compteur%200000==0) printf("thread %d : compteur= %d\n",id_thread, s->thread_pool[id_thread].compteur);
		pthread_mutex_lock((s->stack[id_thread]->mutex_pile));
		//afficher_pile(stack);
		//tache* t=prendre_tache(stack,id_thread);
		ep=prendre_tache_bas(s->stack[id_thread],id_thread); //****
		pthread_mutex_unlock((s->stack[id_thread]->mutex_pile));
		if (ep!=NULL) {
			//printf("prise de tache thread %d\n",id_thread);
			pthread_mutex_lock(&(s->mutex_oisif));
			s->thread_pool[id_thread].actif=1;
			pthread_mutex_unlock(&(s->mutex_oisif));
			s->thread_pool[id_thread].compteur+=1;
			
			taskfunc fonct=ep->la_tache->f; //***
			void* cloture=ep->la_tache->closure; //***
			struct scheduler* sch=ep->la_tache->s; //*****
			fonct(cloture,sch);  //execution de la fonction
			free(ep->la_tache);
			free(ep);
			//printf("j'ai executé une fonction dans le thread %d\n",id_thread);
		} else {

			ep=voler_travail(s,id_thread);
			if (ep!=NULL) {
				pthread_mutex_lock(&(s->mutex_oisif)); //???
				s->thread_pool[id_thread].actif=1;
				pthread_mutex_unlock(&(s->mutex_oisif)); //???
				s->thread_pool[id_thread].compteur+=1;
				//s->thread_pool[id_thread].vols+=1;
			
				taskfunc fonct=ep->la_tache->f; //***
				void* cloture=ep->la_tache->closure; //***
				struct scheduler* sch=ep->la_tache->s; //*****
				fonct(cloture,sch);  //execution de la fonction
				free(ep->la_tache);
				free(ep);

			}else {

				pthread_mutex_lock(&(s->mutex_oisif));
				s->thread_pool[id_thread].actif=0;
				if (threads_actifs(nbthr,s)==0) {
					pthread_mutex_unlock(&(s->mutex_oisif));
					break;
				}
				//printf("thread %d je lock oisif\n", id_thread);
				//pthread_mutex_lock(&(s->mutex_oisif));
				s->thread_pool[id_thread].sommeils +=1;
				if (pthread_cond_wait(&(s->cond_oisif), &(s->mutex_oisif))!=0) printf("ERREUR DANS COND_WAIT");
				//pthread_mutex_unlock(&(s->mutex_oisif));
				//printf("thread %d j ai delocke oisif\n", id_thread);
				s->thread_pool[id_thread].actif=1;
				pthread_mutex_unlock(&(s->mutex_oisif));
			}

		}

	}
	pthread_mutex_lock(&(s->mutex_oisif));
	pthread_cond_broadcast(&(s->cond_oisif));
	pthread_mutex_unlock(&(s->mutex_oisif));
	//printf("JE SORS thread %d------------------------------------------------------------->\n",id_thread);
    pthread_exit(NULL);
    free(arg);
}

//////////////////////////////////////////////
int sched_init(int nthreads, int qlen, taskfunc f, void *closure) {

	int nbcoeurs=sched_default_threads();
	printf("votre machine a %d coeurs\n", nbcoeurs);
	//if (nthreads==0) nthreads=nbcoeurs;
	printf("version N piles deque, nbre de threads= %d\n",nthreads);
	

	pthread_cond_t cond_oisif = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t mutex_oisif = PTHREAD_MUTEX_INITIALIZER;
    info_thread*  thread_pool=malloc(nthreads*(sizeof *thread_pool));

	struct scheduler* s=malloc(sizeof *s);
	pile** stack=malloc(nthreads*(sizeof(pile*)));
	s->stack=stack;
	for (int i=0;i<nthreads;i++) {
		s->stack[i]=malloc(sizeof(pile));
		s->stack[i]->mutex_pile=malloc(sizeof( pthread_mutex_t));
		pthread_mutex_init(s->stack[i]->mutex_pile, NULL);
		initialisation(s->stack[i]);
	}
	s->nbre_threads=nthreads;
	s->mutex_oisif=mutex_oisif;
	s->cond_oisif=cond_oisif;
	s->thread_pool=thread_pool;
	

	tache* t=malloc(sizeof *t);
	t->f=f;
	t->closure=closure;
	t->s=s;

	pthread_mutex_lock((s->stack[0]->mutex_pile));
	ajouter_tache_bas(t,s->stack[0]);
	pthread_mutex_unlock((s->stack[0]->mutex_pile));
	//afficher_pile(s->stack[0]);

	for (int i=0;i<nthreads;i++) {
		s->thread_pool[i].actif=1;
		s->thread_pool[i].compteur=0;
		s->thread_pool[i].vols_reussis=0;
		s->thread_pool[i].vols_rates=0;
		s->thread_pool[i].sommeils=0;
	}

	srand(time(NULL));

	for (int i=0;i<nthreads;i++) {
		boucle_args* arg=malloc(sizeof *arg);
		arg->id_thread=i;
		arg->nbthreads=nthreads;
		arg->s=s;
		s->thread_pool[i].thread_self_id=-1;
		pthread_create( &(s->thread_pool[i].un_thread) , NULL, boucle_travail, arg);
	}
	//printf("TOUS LES THREADS CREES = %d\n",nthreads);




	for (int i=0;i<nthreads;i++) {
		pthread_join(s->thread_pool[i].un_thread , NULL);
		printf("Le thread %d est sorti, taches executees= %d, vols reussis=%d, vols rates=%d, mises en sommeil=%d\n",i,
			s->thread_pool[i].compteur, s->thread_pool[i].vols_reussis,s->thread_pool[i].vols_rates, s->thread_pool[i].sommeils);
		pthread_mutex_lock(&(s->mutex_oisif));
		pthread_cond_broadcast(&(s->cond_oisif));
		pthread_mutex_unlock(&(s->mutex_oisif));
	}
	printf("Tous les threads sortis\n");

	free(thread_pool);
	for (int i=0;i<nthreads;i++) {
		free(s->stack[i]->mutex_pile);
		free(s->stack[i]->top);
		free(s->stack[i]->racine);
		free(s->stack[i]);
	}
	free(s->stack);
	free(s); 

	return 0;
}

/////////////////////////////////////////////////////////////
int sched_spawn(taskfunc f, void *closure, struct scheduler *s) {

	//printf("j'entre dans spawn\n");
	//afficher_pile(stack);

	//printf("Thread ID is: %ld\n", (long) pthread_self());

	long thself=(long) pthread_self();
	int numth=num_thread(thself, s);
	//printf("spawn pour le thread %d\n",numth);

	tache* tt=malloc(sizeof *tt);
	tt->f=f;
	tt->closure=closure;
	tt->s=s;

	pthread_mutex_lock((s->stack[numth]->mutex_pile));
	ajouter_tache_bas(tt,s->stack[numth]);
	pthread_mutex_unlock((s->stack[numth]->mutex_pile));

	pthread_mutex_lock(&(s->mutex_oisif));
	pthread_cond_broadcast(&(s->cond_oisif));
	pthread_mutex_unlock(&(s->mutex_oisif));

	return 0;
}


