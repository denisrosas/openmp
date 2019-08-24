#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <sys/time.h>
#include <omp.h>

#define TRUE 1
#define FALSE 0
#define THREAD_COUNT 4

int main(void) {
	int i, j, nelements, list_is_sorted = FALSE;
	int *vector, *vector_serial, aux;
	long unsigned int duracao_p, duracao_s;
	struct timeval start_p, end_p, start_s, end_s;
	
	scanf("%d", &nelements);
	
	vector = malloc(nelements*sizeof(int));
	
	for(i=0; i<nelements; i++)
	  scanf("%d", &vector[i]);

	vector_serial = malloc(nelements*sizeof(int));
  	memcpy (vector_serial, vector, nelements * sizeof *vector);




  	// EXECUCAO DO METODO PARALELO
  	// ========================================================================
	gettimeofday(&start_p, NULL);

	  while(list_is_sorted==FALSE)
	  {
	    #pragma omp parallel private(aux) num_threads(THREAD_COUNT)
	    {
				list_is_sorted = TRUE;
				#pragma omp for
				for(i = 0; i < nelements - 1; i = i + 2)
				{
					if(vector[i] > vector[i+1] )
					{
						aux = vector[i];
						vector[i] = vector[i+1];
						vector[i+1] = aux;
						#pragma omp atomic write
							list_is_sorted = FALSE;
			  	}
	      }
			  #pragma omp for /*reduction(+:list_is_sorted)*/
				for(i = 1; i < nelements - 1; i = i + 2)
				{
					if( vector[i] > vector[i+1] )
					{
						aux = vector[i];
						vector[i] = vector[i+1];
						vector[i+1] = aux;
						#pragma omp atomic write
							list_is_sorted = FALSE;
			    }
		    }
	    }
	  }
	gettimeofday(&end_p, NULL);
	duracao_p = ((end_p.tv_sec * 1000000 + end_p.tv_usec) - (start_p.tv_sec * 1000000 + start_p.tv_usec));




  	// EXECUCAO DO METODO SEQUENCIAL
  	// ========================================================================

  	gettimeofday(&start_s, NULL);

	  //Bubble sort algorithm
	  for(i=nelements-1; i >= 1; i--) {
	    list_is_sorted = TRUE; 
	    for(j=0; j < i ; j++) {
	 
	      if(vector_serial[j]>vector_serial[j+1]) {
	        aux = vector_serial[j];
	        vector_serial[j] = vector_serial[j+1];
	        vector_serial[j+1] = aux;
	        list_is_sorted = FALSE; //if there are any swaps, the list is not sorted yet
	      }
	    }
	    if(list_is_sorted == TRUE)// if there wasnt any swaps we can assume the list is sorted and we can end bubble sort
	      break;
	  }

	gettimeofday(&end_s, NULL);
	duracao_s = ((end_s.tv_sec * 1000000 + end_s.tv_usec) - (start_s.tv_sec * 1000000 + start_s.tv_usec));




    //imprimindo o resultado de tempo de ordenação
	for(i=0; i<nelements; i++)
	  printf("%d ", vector[i]);
    printf ("\n\nSpeedup: %lf\n\n", (double) duracao_s/duracao_p);

	return 0;
}
