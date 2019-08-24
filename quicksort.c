//gcc quicksort_paralelo_pthreads.c -o quicksort_paralelo_pthreads -std=c99 -pedantic -Wall -lpthread -lm

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <sys/time.h>
#include <pthread.h>

#define THREAD_COUNT 16 //definir como potencia de 2

typedef struct
{
  int *vector;
  int left_bound, right_bound;
} thread_arg;

// Declaracao das funcoes
void *quicksort_pthreads(void *vargp);
int partition(int *a, int l, int r);
int middle(int a, int b, int c);

//Variaveis globais
int thread_created = 0;
pthread_t *thread_handles_global;

//A funcao quicksort normal. a diferenca eh que, quando o numero de chucks criados eh igual ao numero de Threads que serao usadas, ela cria uma Thread usando pthread_create
void quicksort_paralelo(int *vector, int l, int r, int chunk_count)
{
   int j;
   pthread_t *thread_handles;
   thread_arg *thread_param;

   if(( l < r ) && (chunk_count<THREAD_COUNT))
   {
   	// divide and conquer
       j = partition( vector, l, r);
       quicksort_paralelo( vector, l, j-1, chunk_count*2);
       quicksort_paralelo( vector, j+1, r, chunk_count*2);
   }
   else if(chunk_count==THREAD_COUNT){
   
     j = partition( vector, l, r);
    
     thread_handles = malloc(2*sizeof(pthread_t)); //criando duas threads, uma para cada chunk 
     thread_param = malloc(2*sizeof(thread_arg));
     
     //atribuindo as variaveis aos parametros 
     thread_param[0].vector = vector; 
     thread_param[1].vector = vector;
     
     thread_param[0].left_bound = l;
     thread_param[0].right_bound = j-1;
     
     thread_param[1].left_bound = j+1;
     thread_param[1].right_bound = r;     
     
     //criando uma thread para cada chunk
     pthread_create(&(thread_handles[0]), NULL, quicksort_pthreads, (void *)&(thread_param[0]));
     pthread_create(&(thread_handles[1]), NULL, quicksort_pthreads, (void *)&(thread_param[1]));
     
     //copiando as threads_handles para thread_handles_global para esperar ela terminar na funcao main    
     thread_handles_global[thread_created++] = thread_handles[0];
     thread_handles_global[thread_created++] = thread_handles[1];
   }
}

//outra funcao normal do quicksort, somente seguindo o padrao para ser chamada por pthreads. criaremos THREAD_COUNT threads. por questoes de desempenho, nao coloque esse valor maior que o numero de cores no processaror.
void *quicksort_pthreads(void *vargp)
{
    int j, l, r;
    int *vector;
    thread_arg *arguments;
    arguments = (thread_arg*) vargp;
    
    l = arguments->left_bound;
    r = arguments->right_bound;
    vector = arguments->vector;
    
   if( l < r ) 
   {
   	// divide and conquer
       j = partition( vector, l, r);
       quicksort_paralelo( vector, l, j-1,-1);
       quicksort_paralelo( vector, j+1, r,-1);
   }
   
   return 0;
}

int partition(int *a, int l, int r) {
   int pivot, temp;
   int i, j;

   pivot = a[l];
   i = l; j = r+1;
		
   while(1)
   {
   	 do ++i; while( a[i] <= pivot && i <= r );
   	 do --j; while( a[j] > pivot );
   	 if( i >= j ) break;
   	 temp = a[i]; 
   	 a[i] = a[j]; 
   	 a[j] = temp;
   }
   temp = a[l]; 
   a[l] = a[j]; 
   a[j] = temp;
   return j;
}

void quicksort_serial(int *a, int l, int r)
{
   int j;

   if( l < r ) 
   {
    // divide and conquer
       j = partition( a, l, r);
       quicksort_serial( a, l, j-1);
       quicksort_serial( a, j+1, r);
   }
}

int main(void) {
	int i, nelements;
	int chunk_count = 2;
	int *vector;
  int *vector_serial;
	long unsigned int duracao_p, duracao_s;
	struct timeval start_p, end_p, start_s, end_s;
	
	//a primeira linha indica o numero de elementos
	scanf("%d", &nelements);
	
	//alloc memory before reading the elements
  vector = malloc(nelements*sizeof(int));
	thread_handles_global = malloc(THREAD_COUNT*sizeof(pthread_t));

  //lendo todos os elementos a serem ordenados
	for(i=0; i<nelements; i++)
	  scanf("%d", &vector[i]);

  vector_serial = malloc(nelements*sizeof(int));
  memcpy (vector_serial, vector, nelements * sizeof *vector);


	gettimeofday(&start_p, NULL);

  quicksort_paralelo(vector, 0, nelements-1, chunk_count);
  
  for(i=0; i<thread_created; i++)
  {
    pthread_join(thread_handles_global[i], NULL);
  }

	gettimeofday(&end_p, NULL);
	duracao_p = ((end_p.tv_sec * 1000000 + end_p.tv_usec) - (start_p.tv_sec * 1000000 + start_p.tv_usec));


  gettimeofday(&start_s, NULL);
  quicksort_serial(vector_serial, 0, nelements-1);
  gettimeofday(&end_s, NULL);
  duracao_s = ((end_s.tv_sec * 1000000 + end_s.tv_usec) - (start_s.tv_sec * 1000000 + start_s.tv_usec));

	
  //imprimindo o resultado de tempo de ordenação
	for(i=0; i<nelements; i++)
	  printf("%d ", vector[i]);
  printf ("\n\nSpeedup: %lf\n\n", (double) duracao_s/duracao_p);
	  
  pthread_exit((void *)NULL);

	return 0;
}