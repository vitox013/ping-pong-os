#include "ppos_disk.h"
// #include "ppos_data.h"
#include <stdio.h>

#include <signal.h>

#include "disk.h"
#include "ppos_data.h"
#include "ppos.h"
#include "ppos-core-globals.h"

#define STATUS_SUSPENSO 0
#define STATUS_LENDO 1
#define STATUS_ESCREVENDO 2
#define STATUS_FINALIZADO 3


//Uma tarefa gerenciadora do disco;
task_t *disk_mgr_task;

//Uma fila de pedidos de acesso ao disco;
typedef struct task_t_queue {
    task_t *task;

    int block;
    void *bufferAddress;

    int type;

    struct task_t_queue *prev ;  // aponta para o elemento anterior na fila
    struct task_t_queue *next ;  // aponta para o elemento seguinte na fila

} task_t_queue;
task_t_queue *t_queue_inicio = NULL;
task_t_queue *t_queue_fim = NULL;

void taskmgrbody()
{
    while (1){
        task_t_queue *inicio = t_queue_inicio;

        if (inicio != NULL){
            task_resume(inicio->task);
            t_queue_inicio = inicio->next;
        }
        
        task_suspend(disk_mgr_task,  NULL);
        task_yield();
    }
}

void readBody (void * arg)
{
    task_t_queue *disk = (task_t_queue*)arg;
    int block = disk->block;
    void *buffer = disk->bufferAddress;

    // disk->type = TIPO_LENDO;

    disk_cmd(DISK_CMD_READ, block, buffer);
}

// Uma função para tratar os sinais SIGUSR1
void tratador_sigusr1 (){
    // task_t *task_iterator = readyQueue;

    // while(task_iterator->next != NULL && task_iterator->id != taskMain->id)
    //     task_iterator = task_iterator->next;
    task_t_queue *queue_iterator = t_queue_inicio;

    while (queue_iterator->next != NULL && 
           queue_iterator->task->status == STATUS_SUSPENSO)
        queue_iterator = queue_iterator->next;

    queue_iterator->task->status = STATUS_FINALIZADO;

    task_switch(taskMain);
}

// inicializacao do gerente de disco
// retorna -1 em erro ou 0 em sucesso
// numBlocks: tamanho do disco, em blocos
// blockSize: tamanho de cada bloco do disco, em bytes
int disk_mgr_init (int *numBlocks, int *blockSize){
    if (disk_cmd(DISK_CMD_INIT, -1, NULL) < 0)
        return -1; 

    int disksize = disk_cmd(DISK_CMD_DISKSIZE, -1, NULL);
    if (disksize < 0)
        return -1; 

    int blocksize = disk_cmd(DISK_CMD_BLOCKSIZE, -1, NULL);
    if (blocksize < 0)
        return -1; 

    *numBlocks = disksize;
    *blockSize = blocksize;

    disk_mgr_task = malloc(sizeof(task_t));
    task_create(disk_mgr_task, taskmgrbody, "Tarefa gerenciadora");

    int i = 0;

    for (i = 0; i < disksize; i++){
        task_t *task = malloc(sizeof(task_t));
        task->status = STATUS_SUSPENSO;

        task_t_queue *disk = malloc(sizeof(task_t_queue));

        disk->block = i;
        // disk->bufferAddress = malloc (blocksize);
        disk->task = task;

        if (t_queue_inicio == NULL){
            t_queue_inicio = disk;
            t_queue_fim = disk;
        }
        else{
            t_queue_fim->next = disk;
            t_queue_fim = disk;
        }

        task_create(task, readBody, disk);
        task_suspend(task, NULL);
    }



    // task_yield();

    // Uma função para tratar os sinais SIGUSR1
    signal(SIGUSR1, tratador_sigusr1);
    
    return 0;
}



// leitura de um bloco, do disco para o buffer
int disk_block_read (int block, void *buffer){
    
    task_t_queue *queue_iterator = t_queue_inicio;

    while (queue_iterator->next != NULL && queue_iterator->block != block)
        queue_iterator = queue_iterator->next;

    task_t *task = queue_iterator->task;
    task->status = STATUS_LENDO;
    queue_iterator->bufferAddress = buffer;
    
    task_resume(task);
    task_switch(task);

    if (task->status == STATUS_FINALIZADO)
        buffer = queue_iterator->bufferAddress;

    return 0;
}

// escrita de um bloco, do buffer para o disco
int disk_block_write (int block, void *buffer){
    return 0;
}