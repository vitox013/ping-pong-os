// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DINF UFPR
// Versão 1.2 -- Julho de 2017

// interface do gerente de disco rígido (block device driver)

#ifndef __DISK_MGR__
#define __DISK_MGR__

// estruturas de dados e rotinas de inicializacao e acesso
// a um dispositivo de entrada/saida orientado a blocos,
// tipicamente um disco rigido.
// cada pedido indica a tarefa solicitante,
// o tipo de pedido (leitura ou escrita), o bloco desejado e o endereço do buffer
// de dados;
// estrutura que representa um disco no sistema operacional
// baseado do arquivo disk.c
typedef struct disk_t {
  int type ;			// estado do disco
  int numblocks ;		// numero de blocos do disco
  int blocksize ;		// tamanho dos blocos em bytes
  int block ;       // bloco desejado
  char *buffer ;		// buffer da proxima operacao (read/write)

  //adicionado o ponteiro para proximo e anterior como está em queue.h
  struct disk_t *prev ;  // aponta para o elemento anterior na fila
  struct disk_t *next ;  // aponta para o elemento seguinte na fila
} disk_t ;

// inicializacao do gerente de disco
// retorna -1 em erro ou 0 em sucesso
// numBlocks: tamanho do disco, em blocos
// blockSize: tamanho de cada bloco do disco, em bytes
int disk_mgr_init (int *numBlocks, int *blockSize) ;

// leitura de um bloco, do disco para o buffer
int disk_block_read (int block, void *buffer) ;

// escrita de um bloco, do buffer para o disco
int disk_block_write (int block, void *buffer) ;

#endif
