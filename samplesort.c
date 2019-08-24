#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mpi.h"

#define SIZE 10

#define ROOT 0        // Constante para identificar o processo 0

// Funcao de comparacao utilizada pelo quick sort
static int intcompare(const void *i, const void *j)
{
  if ((*(int *)i) > (*(int *)j))
    return (1);
  if ((*(int *)i) < (*(int *)j))
    return (-1);
  return (0);
}

int main () {
  // Variaveis usadas no codigo
  int        comm_sz,my_rank;
  int        i,j,k, total_elements, elements_per_bucket, total_elements_to_sort;
  int        element_count, temp;
  int        *Input, *InputData;
  int        *Splitter, *AllSplitter;
  int        *Buckets, *BucketBuffer, *LocalBucket;
  int        *OutputBuffer, *Output;
  long unsigned int duracao;
  struct timeval start, end;
  MPI_Status  status; 
  
  // Inicializa o ambiente MPI e variaveis de controle
  MPI_Init(NULL, NULL);
  MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

  // Processa o input a partir do arquivo de entrada fornecido
  if (my_rank == ROOT) {

    scanf("%d", &total_elements);
    Input = (int *) malloc (total_elements*sizeof(int));
    
    if(Input == NULL) {
      printf("Error : Can not allocate memory \n");
    }
      
    for(i=0; i<total_elements; i++)
      scanf("%d", &Input[i]);
      
    gettimeofday(&start, NULL);
  }

  // Comunica todos os processos sobre o total de elementos a ser ordenado
  MPI_Bcast (&total_elements, 1, MPI_INT, 0, MPI_COMM_WORLD);
  if(( total_elements % comm_sz) != 0) {
      if(my_rank == ROOT)
        printf("Number of Elements are not divisible by comm_sz \n");
      MPI_Finalize();
      exit(0);
  }

  // Processo 0 envia, para cada processo Pi, os elementos que Pi deve ordenar
  // por conta propria. Isto pode ser feito numa unica instrucao atraves do uso
  // de MPI_Scatter().
  elements_per_bucket = total_elements / comm_sz;
  InputData = (int *) malloc (elements_per_bucket * sizeof (int));

  MPI_Scatter(Input, elements_per_bucket, MPI_INT, InputData, 
          elements_per_bucket, MPI_INT, ROOT, MPI_COMM_WORLD);

  // Cada processo ordena, localmente, os elementos sob sua responsabilidade.
  qsort ((char *) InputData, elements_per_bucket, sizeof(int), intcompare);

  // Cada processo escolhe os seus Splitters locais, a serem enviados para P0,
  // atraves de um calculo simples (de forma a separar o seu conjunto local em
  // intervalos igualmente espacados).
  Splitter = (int *) malloc (sizeof (int) * (comm_sz-1));
  for (i = 0; i < (comm_sz-1); i++){
        Splitter[i] = InputData[total_elements/(comm_sz*comm_sz) * (i+1)];
  } 

  // Processo 0 recebe os splitters gerados por todos os processos P0...Pi e
  // prossegue com o calculo dos splitters globais, a serem utilizados por
  // todos os processos para a divisao dos buckets de dados.
  AllSplitter = (int *) malloc (sizeof (int) * comm_sz * (comm_sz-1));
  MPI_Gather (Splitter, comm_sz-1, MPI_INT, AllSplitter, comm_sz-1, 
          MPI_INT, ROOT, MPI_COMM_WORLD);

  if (my_rank == ROOT){
    // Ordenar todos os splitters para prosseguir com a selecao dos oficiais
    qsort ((char *) AllSplitter, comm_sz*(comm_sz-1), sizeof(int), intcompare);

    for (i = 0; i < (comm_sz-1); i++)
      Splitter[i] = AllSplitter[(comm_sz-1)*(i+1)];
  }
  
  // Processo 0 envia os splitters globais para todos os processos do comunicador
  MPI_Bcast (Splitter, comm_sz-1, MPI_INT, 0, MPI_COMM_WORLD);

  // Cada processo cria n buckets locais, onde n eh o numero total de processos,
  // para redistribuir seus elementos locais nas faixas divididas pelos splitters.
  Buckets = (int *) malloc (sizeof (int) * (total_elements + comm_sz));
  
  int current_bucket = 0;
  int current_bucket_element = 1;
  for (i = 0; i < elements_per_bucket; i++) {
    if(current_bucket < (comm_sz-1)) {
       if (InputData[i] < Splitter[current_bucket]) {
          // Elemento pertence ao bucket atual
          Buckets[((elements_per_bucket + 1) * current_bucket) + current_bucket_element++] = InputData[i]; 
       } else {
          // Elemento eh maior que o splitter atual. Eh necessario passar para um
          // outro bucket.
          Buckets[(elements_per_bucket + 1) * current_bucket] = current_bucket_element-1;
          current_bucket_element=1;
          current_bucket++;
          i--;
       }
    }
    else {
       // Elemento pertence ao ultimo bucket pois eh maior que todos os splitters.
       Buckets[((elements_per_bucket + 1) * current_bucket) + current_bucket_element++] = InputData[i];
    }
  }
  Buckets[(elements_per_bucket + 1) * current_bucket] = current_bucket_element - 1;
      
  // Todos os processos se comunicam entre si para poder trocar informacoes sobre
  // os buckets. Desta forma, cada processo envia os buckets que nao sao de sua
  // responsabilidade para todos os outros processos, permitindo que ao final da
  // execucao deste trecho cada processo tenha em maos todos os itens do conjunto
  // original que se encaixam entre os splitters designados para este processo.
  // A funcao MPI_Alltoall() permite fazer esta comunicacao de forma unificada, sem
  // que sejam necessarios milhares de send/receive ou combinacoes gather/scatter.
  BucketBuffer = (int *) malloc (sizeof (int) * (total_elements + comm_sz));
  MPI_Alltoall (Buckets, elements_per_bucket + 1, MPI_INT, BucketBuffer, 
           elements_per_bucket + 1, MPI_INT, MPI_COMM_WORLD);

  // BucketBuffer eh rearranjado de forma a isolar apenas os elementos pertinentes
  // ao processo em questao, em um novo bucket local a ser trabalhado.
  LocalBucket = (int *) malloc (sizeof (int) * 2 * total_elements / comm_sz);

  element_count = 1;

  for (j=0; j<comm_sz; j++) {
  k = 1;
    for (i=0; i<BucketBuffer[(total_elements/comm_sz + 1) * j]; i++) 
      LocalBucket[element_count++] = BucketBuffer[(total_elements/comm_sz + 1) * j + k++];
  }
  LocalBucket[0] = element_count-1;

  // Buckets locais sao ordenados por cada processo atraves do metodo quick sort
  total_elements_to_sort = LocalBucket[0];
  qsort ((char *) &LocalBucket[1], total_elements_to_sort, sizeof(int), intcompare); 

  // Processo 0 concatena todos os buckets pre-ordenados em um unico vetor que,
  // por respeitar a ordem dos processos (e consequentemente dos splitters que
  // foram previamente definidos), ja esta quase totalmente ordenado.
  if(my_rank == ROOT) {
      OutputBuffer = (int *) malloc (sizeof(int) * 2 * total_elements);
      Output = (int *) malloc (sizeof (int) * total_elements);
  }

  MPI_Gather (LocalBucket, 2*elements_per_bucket, MPI_INT, OutputBuffer, 
          2*elements_per_bucket, MPI_INT, ROOT, MPI_COMM_WORLD);

  // Processo 0 rearranja o vetor final de forma a eliminar os elementos nulos
  // (usados como buffer pelos processos-escravo para permitir mais elementos do que
  // o previsto em cada bucket).
  if (my_rank == ROOT) {
      element_count = 0;
      for (current_bucket = 0; current_bucket < comm_sz; current_bucket++) {
        current_bucket_element = 1;
        for(i=0; i<OutputBuffer[(2 * total_elements/comm_sz) * current_bucket]; i++) {
          Output[element_count++] = OutputBuffer[(2*total_elements/comm_sz) * current_bucket + current_bucket_element++];
        }
      }

      gettimeofday(&end, NULL);
      duracao = ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
      
      // Impressao do vetor final ordenado
      for (i = 0; i < total_elements; i++) {
        printf( "%d ", Output[i]);
      }

      // Impressao do tempo total de ordenacao
      printf("\n\nDuracao: %lu us\n\n", duracao);

      // Desalocar vetores
      free(Input);
      free(OutputBuffer);
      free(Output);
   }

    free(InputData);
    free(Splitter);
    free(AllSplitter);
    free(Buckets);
    free(BucketBuffer);
    free(LocalBucket);

   // Finalizar MPI
   MPI_Finalize();
   return 0;
}