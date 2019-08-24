#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>       // for time(NULL)
#include <mpi.h>

// Defines para simplificar a compreensao o envio e recebimento de mensagens
// atraves de MPI
#define INFO  1        // Tag para informar tamanho e altura da arvore
#define DADOS 2        // Tag para enviar o vetor a ser ordenado pelo no
#define RESP  3        // Tag para enviar o resultado da ordenacao no no

// Versao paralela do algoritmo de merge.
void merge_paralelo (int *vector, int size, int my_height);

// Como cada no folha precisa ordenar, localmente, os elementos pelos quais
// eh responsavel, podemos minimizar o tempo de ordenacao ao escolher um
// algoritmo mais apropriado. Desta forma, a versao serial do merge sort eh
// utilizada para ordenacao local do vetor de cada no folha.
void merge_serial (int *A, int a, int *B, int b, int *C);
void merge_sort_serial(int *A, int n);

int main (int argc, char* argv[]) {
   int my_rank, comm_sz;         // variaveis do MPI
   int size;                     // tamanho do vetor a ser ordenado
   int *vetor_paralelo,          // vetor usado na ordenacao paralela
       *vetor_serial;            // vetor usado na ordenacao paralela
   struct timeval start_s,       // tempo de inicio da ordenacao serial
                  end_s,         // tempo de fim da ordenacao serial
                  start_p,       // tempo de inicio da ordenacao paralelo
                  end_p;         // tempo de fim da ordenacao paralelo
   long unsigned int duracao_s,  // duracao total da ordenacao serial
                     duracao_p;  // duracao total da ordenacao serial
   int i;                        // contador generico

   // Inicializar MPI
   MPI_Init(&argc, &argv);

   // Obter numero do processo atual e o total de processos em execucao
   MPI_Comm_rank (MPI_COMM_WORLD, &my_rank);
   MPI_Comm_size (MPI_COMM_WORLD, &comm_sz);

   // Processo 0 (Mestre)
   if (my_rank == 0) {
      int root_height = 0, nodeCount = 1;

      while ( nodeCount < comm_sz ) {
         nodeCount *= 2; root_height++;
      }
      
      // Ler numeros a serem ordenados a partir da entrada padrao (stdin)
      // fazendo uso do arquivo obtido do gerador de entradas aleatorias
      scanf("%d", &size);
      vetor_paralelo = (int *) malloc(size*sizeof(int));
      for (i = 0; i < size; i++) {
         scanf("%d", &vetor_paralelo[i]);
      }

      // Copiar vetor de entrada para ordena-lo com um metodo serial e medir
      // o tempo gasto.
      vetor_serial = (int*) malloc (size*sizeof(int));
      memcpy (vetor_serial, vetor_paralelo, size * sizeof *vetor_serial);

      // Obter o tempo de ordenacao paralelo para calcular o speedup.
      gettimeofday(&start_p, NULL);
      merge_paralelo (vetor_paralelo, size, root_height);
      gettimeofday(&end_p, NULL);
      duracao_p = ((end_p.tv_sec * 1000000 + end_p.tv_usec) - (start_p.tv_sec * 1000000 + start_p.tv_usec));

   } else {
   // Demais processos (Escravos)
      int info[2];
      int height;
      int parent;

      // Receber informacoes do trabalho a ser feito
      MPI_Recv(info, 2, MPI_INT, MPI_ANY_SOURCE, INFO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      size   = info[0];
      height = info[1];
      vetor_paralelo = (int*) calloc (size, sizeof *vetor_paralelo);

      MPI_Recv(vetor_paralelo, size, MPI_INT, MPI_ANY_SOURCE, DADOS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

      // Executar ou redividir o trabalho
      merge_paralelo (vetor_paralelo, size, height );
   }

   // Processo Mestre precisa terminar a computacao de tempos para calcular o
   // speedup obtido.
   if (my_rank == 0) {
      // Medir tempo gasto na execucao da ordenacao serial
      gettimeofday(&start_s, NULL);
      merge_sort_serial(vetor_serial, size);
      gettimeofday(&end_s, NULL);
      duracao_s = ((end_s.tv_sec * 1000000 + end_s.tv_usec) - (start_s.tv_sec * 1000000 + start_s.tv_usec));

      // Imprimir o resultado da ordenacao
      for (i = 0; i < size; i++)
         printf("%d ", vetor_paralelo[i]);
      printf ("\n\nSpeedup: %lf\n\n", (double) duracao_s/duracao_p);
   }

   MPI_Finalize();
}

/**
 * Merge paralelo em MPI.
 * Cada no envia metade dos seus elementos ao seu filho direito
 * e processa a ordenacao (recursivamente) de seu filho esquerdo.
 * O no folha ordena toda a metade referente ao filho esquerdo.
 */
void merge_paralelo (int *vector, int size, int my_height) {
   int parent;
   int my_rank, comm_sz;
   int prox, right_child;

   // Obter numero do processo atual e o total de processos em execucao
   MPI_Comm_rank (MPI_COMM_WORLD, &my_rank);
   MPI_Comm_size (MPI_COMM_WORLD, &comm_sz);

   // Calcular no pai e no filho (a direita)
   parent = my_rank & ~(1 << my_height);
   prox = my_height - 1;
   if (prox >= 0)
      right_child = my_rank | ( 1 << prox );

   // No intermediario na arvore. Desta forma, ele contem filhos e faz-se
   // necessario dividir seu trabalho com seus filhos para minimizar o
   // tempo total de ordenacao paralela
   if (my_height > 0) {
      // Caso o processo que teoricamente seria o filho direito tenha um
      // numero superior ao total de processos, logicamente o no atual
      // nao possui filho a direita.
      // Neste caso, deve-se descer um nivel na arvore para trabalhar seu
      // filho esquerdo.
      if (right_child >= comm_sz)
         merge_paralelo ( vector, size, prox );
      else {
         int size_esq  = size / 2;
         int size_dir = size - size_esq;
         int *vetor_esq = (int*) malloc (size_esq * sizeof(int));
         int *vetor_dir = (int*) malloc (size_dir * sizeof(int));
         int info[2];
         int i, j, k;

         memcpy (vetor_esq, vector, size_esq*sizeof *vetor_esq);
         memcpy (vetor_dir, vector+size_esq, size_dir*sizeof *vetor_dir);

         info[0] = size_dir;
         info[1] = prox;
         MPI_Send( info, 2, MPI_INT, right_child, INFO, MPI_COMM_WORLD);
         MPI_Send( vetor_dir, size_dir, MPI_INT, right_child, DADOS, MPI_COMM_WORLD);

         merge_paralelo ( vetor_esq, size_esq, prox );
         MPI_Recv(vetor_dir, size_dir, MPI_INT, right_child, RESP, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

         // Uma vez tendo os dois vetores ordenados, chegou a hora de fazer
         // o merge em um unico vetor. Este vetor sera enviado ao no-pai de
         // forma que ele possa fazer um novo merge com os demais elementos
         // e, recursivamente, chegar ao vetor completo ordenado
         i = 0; j = 0; k = 0;
         while ( i < size_esq && j < size_dir )
            if ( vetor_esq[i] > vetor_dir[j])
               vector[k++] = vetor_dir[j++];
            else
               vector[k++] = vetor_esq[i++];
         while ( i < size_esq )
            vector[k++] = vetor_esq[i++];
         while ( j < size_dir )
            vector[k++] = vetor_dir[j++];
      }

   // No folha. Ele nao divide seu trabalho com nenhum outro processo e,
   // desta forma, sua unica funcao eh ordenar, por conta propria, todo
   // o vetor que lhe foi passado como parametro
   } else {
      merge_sort_serial(vector, size);
   }

   // Apos terminar de ordenar o vetor recebido (tenha o processo sido feito
   // por conta propria ou dividido com outros nos-filho), deve-se enviar o
   // resultado da computacao ao no-pai para que ele possa fazer o merge dos
   // resultados.
   // Caso o no-pai calculado seja o proprio no atual, o processo de ordenacao
   // chegou ao fim uma vez que todas as respostas ja foram enviadas atraves
   // da arvore de calculo.
   if (parent != my_rank)
      MPI_Send( vector, size, MPI_INT, parent, RESP, MPI_COMM_WORLD);
}


// ____________________________________________________________________________
//
// Versao serial do algoritmo de merge sort.
// Fonte: http://www.cs.cityu.edu.hk/~lwang/ccs4335/mergesort.c

void merge_serial (int *A, int a, int *B, int b, int *C) 
{
  int i,j,k;
  i = 0; 
  j = 0;
  k = 0;
  while (i < a && j < b) {
    if (A[i] <= B[j]) {
     /* copy A[i] to C[k] and move the pointer i and k forward */
     C[k] = A[i];
     i++;
     k++;
    }
    else {
      /* copy B[j] to C[k] and move the pointer j and k forward */
      C[k] = B[j];
      j++;
      k++;
    }
  }
  /* move the remaining elements in A into C */
  while (i < a) {
    C[k]= A[i];
    i++;
    k++;
  }
  /* move the remaining elements in B into C */
  while (j < b)  {
    C[k]= B[j];
    j++;
    k++;
  }
}  

void merge_sort_serial(int *A, int n) 
{
  int i;
  int *A1, *A2;
  int n1, n2;

  if (n < 2)
    return;   /* the array is sorted when n=1.*/
  
  /* divide A into two array A1 and A2 */
  n1 = n / 2;   /* the number of elements in A1 */
  n2 = n - n1;  /* the number of elements in A2 */
  A1 = (int*)malloc(sizeof(int) * n1);
  A2 = (int*)malloc(sizeof(int) * n2);
  
  /* move the first n/2 elements to A1 */
  for (i =0; i < n1; i++) {
    A1[i] = A[i];
  }
  /* move the rest to A2 */
  for (i = 0; i < n2; i++) {
    A2[i] = A[i+n1];
  }
  /* recursive call */
  merge_sort_serial(A1, n1);
  merge_sort_serial(A2, n2);

  /* conquer */
  merge_serial(A1, n1, A2, n2, A);
  free(A1);
  free(A2);
}