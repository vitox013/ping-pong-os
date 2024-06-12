#include "ppos_disk.h"
// #include "ppos_data.h"
#include <stdio.h>

#include "disk.h"
#include "ppos_data.h"


//Uma tarefa gerenciadora do disco;
task_t disk_mgr_task;

//Uma fila de pedidos de acesso ao disco;
disk_t disk_queue;

// inicializacao do gerente de disco
// retorna -1 em erro ou 0 em sucesso
// numBlocks: tamanho do disco, em blocos
// blockSize: tamanho de cada bloco do disco, em bytes
int disk_mgr_init (int *numBlocks, int *blockSize){
    //projeto B {
    if (disk_cmd(DISK_CMD_INIT, -1, NULL) < 0)
        return -1; 
    //} projeto B

    int disksize = disk_cmd(DISK_CMD_DISKSIZE, -1, NULL);
    if (disksize < 0)
        return -1; 

    int blocksize = disk_cmd(DISK_CMD_DISKSIZE, -1, NULL);
    if (disk_cmd(DISK_CMD_BLOCKSIZE, -1, NULL) < 0)
        return -1; 

    *numBlocks = disksize;
    *blockSize = blocksize;
    
    return 0;
}

// leitura de um bloco, do disco para o buffer
int disk_block_read (int block, void *buffer){



    if (disk_cmd(DISK_CMD_READ, block, buffer) < 0)
        return -1;

    return 0;
}

// escrita de um bloco, do buffer para o disco
int disk_block_write (int block, void *buffer){
    return 0;
}