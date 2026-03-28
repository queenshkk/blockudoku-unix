#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include "GrilleSDL.h"
#include "Ressources.h"
#include <string.h>
#include <stdbool.h>

// Dimensions de la grille de jeu
#define NB_LIGNES   12
#define NB_COLONNES 19

// Nombre de cases maximum par piece
#define NB_CASES    4

// Macros utilisees dans le tableau tab
#define VIDE        0
#define BRIQUE      1
#define DIAMANT     2

int tab[NB_LIGNES][NB_COLONNES]
={ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};

typedef struct
{
  int ligne;
  int colonne;
} CASE;

typedef struct
{
  CASE cases[NB_CASES];
  int  nbCases;
  int  couleur;
} PIECE;

PIECE pieces[12] = { 0,0,0,1,1,0,1,1,4,0,       // carre 4
                     0,0,1,0,2,0,2,1,4,0,       // L 4
                     0,1,1,1,2,0,2,1,4,0,       // J 4
                     0,0,0,1,1,1,1,2,4,0,       // Z 4
                     0,1,0,2,1,0,1,1,4,0,       // S 4
                     0,0,0,1,0,2,1,1,4,0,       // T 4
                     0,0,0,1,0,2,0,3,4,0,       // I 4
                     0,0,0,1,0,2,0,0,3,0,       // I 3
                     0,1,1,0,1,1,0,0,3,0,       // J 3
                     0,0,1,0,1,1,0,0,3,0,       // L 3
                     0,0,0,1,0,0,0,0,2,0,       // I 2
                     0,0,0,0,0,0,0,0,1,0 };     // carre 1



char* message=NULL;
int tailleMessage=0;
int indiceCourant=0;


PIECE pieceEnCours;
CASE casesInserees[NB_CASES];
int nbCasesInserees=0;

int score=0;
bool MAJScore=false;

int lignesCompletes[NB_CASES];
int nbLignesCompletes=0;
int colonnesCompletes[NB_CASES];
int nbColonnesCompletes=0;
int carresComplets[NB_CASES];
int nbCarresComplets=0;
int nbAnalyses=0;

int combos=0;
bool MAJCombos=false;

bool traitementEnCours=false;

pthread_mutex_t mutexMessage=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexCasesInserees=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexScore=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexAnalyse=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexTraitement=PTHREAD_MUTEX_INITIALIZER;


pthread_cond_t condCasesInserees=PTHREAD_COND_INITIALIZER;
pthread_cond_t condScore=PTHREAD_COND_INITIALIZER;
pthread_cond_t condAnalyse=PTHREAD_COND_INITIALIZER;
pthread_cond_t condTraitement=PTHREAD_COND_INITIALIZER;


pthread_t tabThreadCase[9][9];
pthread_key_t cleCase;


void DessinePiece(PIECE piece);
int  CompareCases(CASE case1,CASE case2);
void TriCases(CASE *vecteur,int indiceDebut,int indiceFin);

void *FctThreadDeFileMessage(void *p);
void *FctThreadPiece(void *p);
void *FctThreadEvent(void *p);
void *FctThreadScore(void *p);
void *FctThreadCases(void *p);
void LiberationVarSpecifique(void *p);
void *FctThreadNettoyeur(void *p);

void SetMessage(const char *texte, bool signalOn);
void HandlerSIGALARM(int s);
void HandlerSIGUSR1(int s);
void RotationPiece(PIECE * pPiece);

///////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc,char* argv[])
{
  EVENT_GRILLE_SDL event;
  
  pthread_t threadDefileMessage, threadPiece, threadEvent, threadScore, threadNettoyeur;

  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGALRM);
  sigaddset(&mask, SIGUSR1);
  pthread_sigmask(SIG_SETMASK,&mask,NULL); 

  printf("J'arme le SIGALRM...\n");
  struct sigaction A;
  A.sa_handler=HandlerSIGALARM;
  A.sa_flags=0;
  sigemptyset(&A.sa_mask);
  sigaction(SIGALRM, &A, NULL);

  printf("J'arme le SIGUSR1...\n");
  A.sa_handler=HandlerSIGUSR1;
  A.sa_flags=0;
  sigemptyset(&A.sa_mask);
  sigaction(SIGUSR1, &A, NULL);


  srand((unsigned)time(NULL));

  // Ouverture de la fenetre graphique
  printf("(MAIN %p) Ouverture de la fenetre graphique\n",pthread_self()); fflush(stdout);
  if (OuvertureFenetreGraphique() < 0)
  {
    printf("Erreur de OuvrirGrilleSDL\n");
    fflush(stdout);
    exit(1);
  }

  DessineVoyant(8, 10, VERT);

  printf("MAIN %p) Lancement de threadDefileMessage\n", pthread_self()); fflush(stdout);
  if(pthread_create(&threadDefileMessage,NULL,FctThreadDeFileMessage,NULL)!=0){
    printf("Erreur création threadDefileMessage\n");
    exit(1);
  }

  SetMessage("Bienvenue dans Blockudoku ", true);

  printf("MAIN %p) Lancement de threadPiece\n", pthread_self()); fflush(stdout);
  if(pthread_create(&threadPiece,NULL,FctThreadPiece,NULL)!=0){
    printf("Erreur création threadPiece\n");
    exit(1);
  }

  printf("MAIN %p) Lancement de threadEvent\n", pthread_self()); fflush(stdout);
  if(pthread_create(&threadEvent,NULL,FctThreadEvent,NULL)!=0){
    printf("Erreur création threadEvent\n");
    exit(1);
  }

  printf("MAIN %p) Lancement de threadScore\n", pthread_self()); fflush(stdout);
  if(pthread_create(&threadScore,NULL,FctThreadScore,NULL)!=0){
    printf("Erreur création threadScore\n");
    exit(1);
  }

  pthread_key_create(&cleCase, LiberationVarSpecifique);

  for(int L=0; L<9; L++){
    for(int C=0; C<9; C++){
      CASE *newCase=(CASE*)malloc(sizeof(CASE));
      newCase->ligne=L;
      newCase->colonne=C;

      printf("MAIN %p) Lancement des threadCases\n", pthread_self()); fflush(stdout);
      if(pthread_create(&tabThreadCase[L][C],NULL,FctThreadCases,newCase)!=0){
        printf("Erreur création threadCases\n");
        exit(1);
      }
    }
  }

  printf("MAIN %p) Lancement de threadNettoyeur\n", pthread_self()); fflush(stdout);
  if(pthread_create(&threadNettoyeur,NULL,FctThreadNettoyeur,NULL)!=0){
    printf("Erreur création threadNettoyeur\n");
    exit(1);
  }

 
  // Exemples d'utilisation du module Ressources --> a supprimer
  /*DessineChiffre(1,15,7);
  char buffer[40];
  sprintf(buffer,"coucou");
  for (int i=0 ; i<strlen(buffer) ; i++) DessineLettre(10,2+i,buffer[i]);
  DessineBrique(7,3,false);
  DessineBrique(7,5,true);*/

/*
  printf("(MAIN %p) Attente du clic sur la croix\n",pthread_self());  
  bool ok = false;

  while(!ok)
  {
    event = ReadEvent();
    if (event.type == CROIX) ok = true;
    /*if (event.type == CLIC_GAUCHE)
    {
      DessineDiamant(event.ligne,event.colonne,ROUGE);
      tab[event.ligne][event.colonne] = DIAMANT;
    }
  }*/


  // Fermeture de la fenetre
  /*printf("(MAIN %p) Fermeture de la fenetre graphique...",pthread_self()); fflush(stdout);
  FermetureFenetreGraphique();
  printf("OK\n");*/

  //exit(0);

  pthread_exit(NULL);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/////// Fonctions fournies ////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
void DessinePiece(PIECE piece)
{
  int Lmin,Lmax,Cmin,Cmax;
  int largeur,hauteur,Lref,Cref;

  Lmin = piece.cases[0].ligne;
  Lmax = piece.cases[0].ligne;
  Cmin = piece.cases[0].colonne;
  Cmax = piece.cases[0].colonne;

  for (int i=1 ; i<=(piece.nbCases-1) ; i++)
  {
    if (piece.cases[i].ligne > Lmax) Lmax = piece.cases[i].ligne;
    if (piece.cases[i].ligne < Lmin) Lmin = piece.cases[i].ligne;
    if (piece.cases[i].colonne > Cmax) Cmax = piece.cases[i].colonne;
    if (piece.cases[i].colonne < Cmin) Cmin = piece.cases[i].colonne;
  }

  largeur = Cmax - Cmin + 1;
  hauteur = Lmax - Lmin + 1;

  switch(largeur)
  {
    case 1 : Cref = 15; break;
    case 2 : Cref = 15; break;
    case 3 : Cref = 14; break;
    case 4 : Cref = 14; break;  
  }

  switch(hauteur)
  {
    case 1 : Lref = 4; break;
    case 2 : Lref = 4; break;
    case 3 : Lref = 3; break;
    case 4 : Lref = 3; break;
  }

  for (int L=3 ; L<=6 ; L++) for (int C=14 ; C<=17 ; C++) EffaceCarre(L,C);
  for (int i=0 ; i<piece.nbCases ; i++) DessineDiamant(Lref + piece.cases[i].ligne,Cref + piece.cases[i].colonne,piece.couleur);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int CompareCases(CASE case1,CASE case2)
{
  if (case1.ligne < case2.ligne) return -1;
  if (case1.ligne > case2.ligne) return +1;
  if (case1.colonne < case2.colonne) return -1;
  if (case1.colonne > case2.colonne) return +1;
  return 0;
}

void TriCases(CASE *vecteur,int indiceDebut,int indiceFin)
{ // trie les cases de vecteur entre les indices indiceDebut et indiceFin compris
  // selon le critere impose par la fonction CompareCases()
  // Exemple : pour trier un vecteur v de 4 cases, il faut appeler TriCases(v,0,3); 
  int  i,iMin;
  CASE tmp;

  if (indiceDebut >= indiceFin) return;

  // Recherche du minimum
  iMin = indiceDebut;
  for (i=indiceDebut ; i<=indiceFin ; i++)
    if (CompareCases(vecteur[i],vecteur[iMin]) < 0) iMin = i;

  // On place le minimum a l'indiceDebut par permutation
  tmp = vecteur[indiceDebut];
  vecteur[indiceDebut] = vecteur[iMin];
  vecteur[iMin] = tmp;

  // Tri du reste du vecteur par recursivite
  TriCases(vecteur,indiceDebut+1,indiceFin); 
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void *FctThreadDeFileMessage (void *p){
  int i=0, indice;

  sigset_t mask;
  sigfillset(&mask);
  sigdelset(&mask, SIGALRM);
  pthread_sigmask(SIG_SETMASK,&mask,NULL); 
  printf("threadDefileMessage (%p) : En attente d'un SIGALARM...\n", pthread_self());

  while(1){
    pthread_mutex_lock(&mutexMessage);
      for(i=0; i<17; i++){
        indice=(indiceCourant+i)%tailleMessage;
        DessineLettre(10, i+1, message[indice]);
      }

      indiceCourant++;

      if(indiceCourant>=tailleMessage){
        indiceCourant=0;
      }
    
    
    pthread_mutex_unlock(&mutexMessage);

    struct timespec temps;
    temps.tv_sec=0;
    temps.tv_nsec=400000000;
    nanosleep(&temps, NULL);
  }

  pthread_exit(NULL);
}


void *FctThreadPiece(void *p){
  int numPiece, nbRotations, i, couleur;
  int Lmin, Cmin;
  int ok;
  CASE tmp[NB_CASES];

  while(1){
    numPiece=rand()%12;

    pieceEnCours=pieces[numPiece];
    couleur=rand()%4;

    switch(couleur){
      case 0:
        pieceEnCours.couleur=JAUNE;
        break;
      case 1:
        pieceEnCours.couleur=ROUGE;
        break;
      case 2:
        pieceEnCours.couleur=VERT;
        break;
      case 3:
        pieceEnCours.couleur=VIOLET;
        break;
    }
   
    nbRotations=rand()%4;

    for(i=0; i<nbRotations; i++){
      RotationPiece(&pieceEnCours);
    }

    DessinePiece(pieceEnCours);

    printf("ThreadPiece(%p) : « Tant que le joueur n'a pas inséré suffisament de cases, j’attends… »\n", pthread_self());

    ok=0;
    while(ok==0){
      pthread_mutex_lock(&mutexCasesInserees);

      while(nbCasesInserees<pieceEnCours.nbCases){
        pthread_cond_wait(&condCasesInserees, &mutexCasesInserees);
        printf("ThreadPiece(%p) : notification reçue (nbCasesInserees=%d)\n", pthread_self(), nbCasesInserees);
      }

      for(i=0; i<nbCasesInserees; i++){
        tmp[i]=casesInserees[i];
      }

      TriCases(tmp, 0, nbCasesInserees-1);

      Lmin=tmp[0].ligne;
      Cmin=tmp[0].colonne;

      for(i=1; i<nbCasesInserees; i++){
        if(tmp[i].ligne<Lmin){
          Lmin=tmp[i].ligne;
        }

        if(tmp[i].colonne<Cmin){
          Cmin=tmp[i].colonne;
        }
      }

      for(i=0; i<nbCasesInserees; i++){
        tmp[i].ligne-=Lmin;
        tmp[i].colonne-=Cmin;
      }
      ok=1;

      for(i=0; i<pieceEnCours.nbCases; i++){
        if(tmp[i].ligne!=pieceEnCours.cases[i].ligne
          || tmp[i].colonne != pieceEnCours.cases[i].colonne){
          ok=0;
          break;
        }
      }

      if(ok==1){
        pthread_mutex_lock(&mutexTraitement);
        traitementEnCours=true;
        pthread_mutex_unlock(&mutexTraitement);
        DessineVoyant(8, 10, BLEU);

        for(i=0; i<nbCasesInserees; i++){
          tab[casesInserees[i].ligne][casesInserees[i].colonne]=BRIQUE;
          DessineBrique(casesInserees[i].ligne, casesInserees[i].colonne, false);
        }

        pthread_mutex_lock(&mutexScore);
        score=score+pieceEnCours.nbCases;
        MAJScore=true;
        pthread_cond_signal(&condScore);
        pthread_mutex_unlock(&mutexScore);

        pthread_mutex_lock(&mutexAnalyse);
        nbLignesCompletes=0;
        nbColonnesCompletes=0;
        nbCarresComplets=0;
        nbAnalyses=0;
        pthread_mutex_unlock(&mutexAnalyse);

       
        for(i=0; i<pieceEnCours.nbCases; i++){
          pthread_kill(tabThreadCase[casesInserees[i].ligne][casesInserees[i].colonne], SIGUSR1);
        }

        pthread_mutex_lock(&mutexTraitement);
        printf("Tant que (traitementEnCours, j'attends...\n");
        while(traitementEnCours){
          pthread_cond_wait(&condTraitement, &mutexTraitement);
        }
        pthread_mutex_unlock(&mutexTraitement);

      }
      else{
        for(i=0; i<nbCasesInserees; i++){
          tab[casesInserees[i].ligne][casesInserees[i].colonne]=VIDE;
          EffaceCarre(casesInserees[i].ligne, casesInserees[i].colonne);
        }

       
      }
        nbCasesInserees=0;
        pthread_mutex_unlock(&mutexCasesInserees);
    }

  
  }
  pthread_exit(NULL);

}

void *FctThreadEvent(void *p){
  EVENT_GRILLE_SDL event;
  int i, ligne, colonne;
  int oldCouleur;

  while(1){
    event=ReadEvent();

    switch(event.type){
      case CLIC_GAUCHE:
        pthread_mutex_lock(&mutexTraitement);
        if(traitementEnCours){
          DessineVoyant(8, 10, ROUGE);
          pthread_mutex_unlock(&mutexTraitement);

          struct timespec temps;
          temps.tv_sec=0;
          temps.tv_nsec=400000000;
          nanosleep(&temps, NULL);

          pthread_mutex_lock(&mutexTraitement);
          if(!traitementEnCours){
            DessineVoyant(8,10,VERT);
          }
          else{
            DessineVoyant(8,10,BLEU);
          }
          pthread_mutex_unlock(&mutexTraitement);
          break;
        }
        pthread_mutex_unlock(&mutexTraitement);

        pthread_mutex_lock(&mutexCasesInserees);
       

        if(event.ligne>=0 && event.ligne<9 && event.colonne>=0 && event.colonne <9
          && tab[event.ligne][event.colonne]==VIDE
          && nbCasesInserees<NB_CASES){

          tab[event.ligne][event.colonne]=DIAMANT;
          DessineDiamant(event.ligne, event.colonne, pieceEnCours.couleur);

          casesInserees[nbCasesInserees].ligne=event.ligne;
          casesInserees[nbCasesInserees].colonne=event.colonne;
          nbCasesInserees++;
          
          pthread_cond_signal(&condCasesInserees);
          pthread_mutex_unlock(&mutexCasesInserees);

        }
        else{
            pthread_mutex_unlock(&mutexCasesInserees);
            DessineVoyant(8, 10, ROUGE);
            struct timespec temps;
            temps.tv_sec=0;
            temps.tv_nsec=400000000;
            nanosleep(&temps, NULL);

            pthread_mutex_lock(&mutexTraitement);
            if(!traitementEnCours){
              DessineVoyant(8,10,VERT);
            }
            else{
              DessineVoyant(8,10,BLEU);
            }
            pthread_mutex_unlock(&mutexTraitement); 
          }
      
        break;
      case CLIC_DROIT:
        pthread_mutex_lock(&mutexCasesInserees);

        for(i=0; i<nbCasesInserees; i++){
          EffaceCarre(casesInserees[i].ligne, casesInserees[i].colonne);
          ligne=casesInserees[i].ligne;
          colonne=casesInserees[i].colonne;
          tab[ligne][colonne]=VIDE;
        }
        
        nbCasesInserees=0;
        pthread_mutex_unlock(&mutexCasesInserees);
        break;
      case CROIX:
        FermetureFenetreGraphique();
        exit(0);
        break;
      default:
        break;
    }

  }
  pthread_exit(NULL);
}

void *FctThreadScore(void *p){
  int score1, score2, score3, score4;
  int comb1, comb2, comb3, comb4;

  while(1){
    pthread_mutex_lock(&mutexScore);

    printf("ThreadScore (%p) : Tant que !MAJScore, j'attends...\n", pthread_self());
    while(!MAJScore && !MAJCombos){
      pthread_cond_wait(&condScore, &mutexScore);
      printf("ThreadScore (%p) : notification reçue (score=%d, combos=%d)\n", pthread_self(), score, combos);
    }

    score1=(score/1000)%10;
    score2=(score/100)%10;
    score3=(score/10)%10;
    score4=score%10;

    comb1=(combos/1000)%10;
    comb2=(combos/100)%10;
    comb3=(combos/10)%10;
    comb4=combos%10;

    DessineChiffre(1,14, score1);
    DessineChiffre(1,15, score2);
    DessineChiffre(1,16, score3);
    DessineChiffre(1,17, score4);

    DessineChiffre(8,14, comb1);
    DessineChiffre(8,15, comb2);
    DessineChiffre(8,16, comb3);
    DessineChiffre(8,17, comb4);

    MAJScore=false;
    MAJCombos=false;

    pthread_mutex_unlock(&mutexScore);

  }
  pthread_exit(NULL);
}

void *FctThreadCases(void *p){
  sigset_t mask;
  sigfillset(&mask);
  sigdelset(&mask, SIGUSR1);
  pthread_sigmask(SIG_SETMASK, &mask, NULL);

  CASE *c=(CASE*)p;
  pthread_setspecific(cleCase, c);

  printf("ThreadCase (%p) : En attente d'un SIGUSR1...\n", pthread_self());
  while(1){
    pause();
  }

  pthread_exit(NULL);
}

void LiberationVarSpecifique(void *p){
  printf("Libération de ma variable spécifique de type CASE\n");
  free(p);
}

void *FctThreadNettoyeur(void *p){
  int i, L, C, nbCombo;
  int numCarre, Ldeb, Cdeb, nbCasesAttendu;

  while(1){
    printf("ThreadNettoyeur (%p) : nbAnalyses<pieceEnCours.nbCases, j'attends...\n", pthread_self());
    
    pthread_mutex_lock(&mutexAnalyse);
    //nbCasesAttendu=pieceEnCours.nbCases;
    while(nbAnalyses<pieceEnCours.nbCases){
      pthread_cond_wait(&condAnalyse, &mutexAnalyse);
      printf("ThreadNettoyeur (%p) : notification reçue (nbAnalyses=%d)\n", pthread_self(), nbAnalyses);
    }

    if(nbLignesCompletes==0 && nbColonnesCompletes==0 && nbCarresComplets==0){
      nbAnalyses=0;
      pthread_mutex_unlock(&mutexAnalyse);

      pthread_mutex_lock(&mutexTraitement);
      traitementEnCours=false;
      pthread_cond_signal(&condTraitement);
      pthread_mutex_unlock(&mutexTraitement);
      DessineVoyant(8,10,VERT);

    }
    else{
      pthread_mutex_unlock(&mutexAnalyse);
    
      struct timespec temps;
      temps.tv_sec=2;
      temps.tv_nsec=0;
      nanosleep(&temps, NULL);

      nbCombo=0;
      pthread_mutex_lock(&mutexAnalyse);

      for(i=0; i<nbLignesCompletes; i++){
        L=lignesCompletes[i];
        for(C=0; C<9; C++){
          tab[L][C]=VIDE;
          EffaceCarre(L, C);
        }
        nbCombo++;
        pthread_mutex_lock(&mutexScore);
        combos++;
        pthread_mutex_unlock(&mutexScore);
      }

      for(i=0; i<nbColonnesCompletes; i++){
        C=colonnesCompletes[i];
        for(L=0; L<9; L++){
          tab[L][C]=VIDE;
          EffaceCarre(L, C);
        }
        nbCombo++;
        pthread_mutex_lock(&mutexScore);
        combos++;
        pthread_mutex_unlock(&mutexScore);
      }

      for(i=0; i<nbCarresComplets; i++){
        numCarre=carresComplets[i];
        Ldeb=(numCarre/3) * 3;
        Cdeb=(numCarre%3) * 3;

        for(L=Ldeb; L<Ldeb+3; L++){
          for(C=Cdeb; C<Cdeb + 3; C++){
            tab[L][C]=VIDE;
            EffaceCarre(L, C);
          }
        }
        nbCombo++;
        pthread_mutex_lock(&mutexScore);
        combos++;
        pthread_mutex_unlock(&mutexScore);
      }

      nbAnalyses=0;
      nbLignesCompletes=0;
      nbColonnesCompletes=0;
      nbCarresComplets=0;
      pthread_mutex_unlock(&mutexAnalyse);

      pthread_mutex_lock(&mutexScore);

      MAJCombos=true;

      if(nbCombo==1){
        score+=10;
        SetMessage(" Simple Combo", true);
      }
      else if(nbCombo==2){
        score+=25;
        SetMessage(" Double Combo", true);
      }
      else if(nbCombo==3){
        score+=40;
        SetMessage(" Triple Combo", true);
      }
      else if(nbCombo>=4){
        score+=55;
        SetMessage(" Quadruple Combo", true);
      }
      MAJScore=true;

      pthread_cond_signal(&condScore);
      pthread_mutex_unlock(&mutexScore);

      pthread_mutex_lock(&mutexTraitement);
      traitementEnCours=false;
      pthread_cond_signal(&condTraitement);
      pthread_mutex_unlock(&mutexTraitement);
      DessineVoyant(8,10,VERT);

    }
  }

  pthread_exit(NULL);
}


void SetMessage(const char *texte, bool signalOn){
  alarm(0);

  pthread_mutex_lock(&mutexMessage);
  
  if(message!=NULL){
    free(message);
  }

  message=(char*)malloc(strlen(texte)+1);
  if(message!=NULL){
    strcpy(message, texte);
    tailleMessage=strlen(message);
    indiceCourant=0;
  }

  pthread_mutex_unlock(&mutexMessage);

  if(signalOn==true){
    alarm(10);
  }
}


void RotationPiece(PIECE *pPiece){
  PIECE newPiece;

  int i;

  int Lmin, Cmin;

  for(i=0; i<pPiece->nbCases; i++){
    newPiece.cases[i].ligne=-pPiece->cases[i].colonne;
    newPiece.cases[i].colonne=pPiece->cases[i].ligne;

  }

  newPiece.nbCases=pPiece->nbCases;
  newPiece.couleur=pPiece->couleur;

  Lmin=newPiece.cases[0].ligne;
  Cmin=newPiece.cases[0].colonne;


  for(i=1; i<newPiece.nbCases; i++){
    if(newPiece.cases[i].ligne<Lmin){
      Lmin=newPiece.cases[i].ligne;
    }

    if(newPiece.cases[i].colonne<Cmin){
      Cmin=newPiece.cases[i].colonne;
    }
  }

  for(i=0; i<newPiece.nbCases; i++)
  {
    newPiece.cases[i].ligne-=Lmin;
    newPiece.cases[i].colonne-=Cmin;
  }

  TriCases(newPiece.cases, 0, newPiece.nbCases-1);

  *pPiece=newPiece;
}


void HandlerSIGALARM(int s){
  printf("threadDefileMessage (%p) : J'ai reçu SIGALRM...\n", pthread_self());
  SetMessage(" Jeu en cours", false);
}

void HandlerSIGUSR1(int s){
  int L, C;
  int i, j;
  int complet, present, numCarre, Ldeb, Cdeb;

  CASE *c=(CASE*)pthread_getspecific(cleCase);
  L=c->ligne;
  C=c->colonne;

  pthread_mutex_lock(&mutexAnalyse);
  complet=1;
  for(j=0; j<9; j++){
    if(tab[L][j]!=BRIQUE){
      complet=0;
      break;
    }
  }

  if(complet==1){
    present=0;
    for(i=0; i<nbLignesCompletes; i++){
      if(lignesCompletes[i]==L){
        present=1;
        break;
      }
    }

    if(present==0){
      lignesCompletes[nbLignesCompletes]=L;
      nbLignesCompletes++;

      for(j=0; j<9; j++){
        DessineBrique(L, j, true);
      }
    }
  }

  complet=1;
  for(i=0; i<9; i++){
    if(tab[i][C]!=BRIQUE){
      complet=0;
      break;
    }
  }

  if(complet==1){
    present=0;
    for(i=0; i<nbColonnesCompletes; i++){
      if(colonnesCompletes[i]==C){
        present=1;
        break;
      }
    }

    if(present==0){
      colonnesCompletes[nbColonnesCompletes]=C;
      nbColonnesCompletes++;

      for(i=0; i<9; i++){
        DessineBrique(i, C, true);
      }
    }
  }

  numCarre=(L/3)*3 + (C/3);
  Ldeb=(L/3)*3;
  Cdeb=(C/3)*3;

  complet=1;
  for(i=Ldeb; i<Ldeb+3; i++){
    for(j=Cdeb; j<Cdeb+3; j++){
      if(tab[i][j]!=BRIQUE){
        complet=0;
        break;
      }
    }
    if(complet==0) break;
  }

  if(complet==1){
    present=0;
    for(i=0; i<nbCarresComplets; i++){
      if(carresComplets[i]==numCarre){
        present=1;
        break;
      }
    }

    if(present==0){
      carresComplets[nbCarresComplets]=numCarre;
      nbCarresComplets++;

      for(i=Ldeb; i<Ldeb+3; i++){
        for(j=Cdeb; j<Cdeb+3; j++){
          DessineBrique(i, j, true);
        }
      }

    }
  }

  nbAnalyses++;
  pthread_cond_signal(&condAnalyse);
  pthread_mutex_unlock(&mutexAnalyse);

 
}