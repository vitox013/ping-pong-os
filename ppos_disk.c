#include "ppos_disk.h"
// #include "ppos_data.h"
#include <stdio.h>

#include <signal.h>

#include "disk.h"
#include "ppos_data.h"
#include "ppos.h"
#include "ppos-core-globals.h"

#define TIPO_LEITURA 1
#define TIPO_ESCRITA 2

#define STATUS_OCUPADA 1
#define STATUS_FINALIZADA 2
#define STATUS_SUSPENSA 3

//Uma tarefa gerenciadora do disco;
task_t *disk_mgr_task;

//Uma fila de pedidos de acesso ao disco;
typedef struct task_t_queue {
    task_t *task;

    int block;
    void *buffer;

    int type;

    struct task_t_queue *prev ;  // aponta para o elemento anterior na fila
    struct task_t_queue *next ;  // aponta para o elemento seguinte na fila

    int status;

} task_t_queue;
task_t_queue *t_queue_inicio = NULL;
task_t_queue *t_queue_fim = NULL;

int tam_buffer = -1;

void taskmgrbody()
{
    while (1){
        task_t_queue *primeira = t_queue_inicio;

        if (primeira != NULL){

            if (primeira->status == STATUS_SUSPENSA){

                if (primeira->type == TIPO_LEITURA)
                    disk_cmd(
                        DISK_CMD_READ, 
                        primeira->block, 
                        primeira->buffer);
                else
                    disk_cmd(
                        DISK_CMD_WRITE, 
                        primeira->block, 
                        primeira->buffer);


                primeira->status = STATUS_OCUPADA;
            }
            else if (primeira->status == STATUS_FINALIZADA){
                t_queue_inicio = t_queue_inicio->next;

                task_resume(primeira->task);
            }
        }
        
        task_suspend(disk_mgr_task,  NULL);
        task_yield();
    }
}

// void readBody (void * arg)
// {
//     task_t_queue *disk = (task_t_queue*)arg;
//     int block = disk->block;
//     void *buffer = disk->buffer;

//     // disk->type = TIPO_LENDO;

//     disk_cmd(DISK_CMD_READ, block, buffer);
// }

// Uma função para tratar os sinais SIGUSR1
void tratador_sigusr1 (){
    // task_t *task_iterator = readyQueue;
    
    t_queue_inicio->status = STATUS_FINALIZADA;
    task_switch(disk_mgr_task);
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

    tam_buffer = blocksize;

    disk_mgr_task = malloc(sizeof(task_t));
    task_create(disk_mgr_task, taskmgrbody, "Tarefa gerenciadora");
    task_suspend(disk_mgr_task, NULL);

    // Uma função para tratar os sinais SIGUSR1
    signal(SIGUSR1, tratador_sigusr1);
    
    return 0;
}



// leitura de um bloco, do disco para o buffer
int disk_block_read (int block, void *buffer){
    
    task_t_queue *pedido_atual;
    pedido_atual = (task_t_queue*)malloc(10*sizeof(task_t_queue*));

    pedido_atual->block = block;
    pedido_atual->buffer = buffer;
    pedido_atual->type = TIPO_LEITURA;
    pedido_atual->task = taskExec;
    
    task_suspend(taskExec, NULL);
    pedido_atual->status = STATUS_SUSPENSA;

    if (t_queue_inicio == NULL){
        t_queue_inicio = pedido_atual;
        t_queue_fim = pedido_atual;
    }
    else {
        t_queue_fim->next = pedido_atual;
        t_queue_fim = pedido_atual;
    }
    
    task_switch(disk_mgr_task);

    buffer = pedido_atual->buffer;

    // unsigned char *char_buffer  = buffer;
    // if (char_buffer[tam_buffer-1] == '\n')
    //     char_buffer[tam_buffer-1] = '\0';

    return 0;
}

// escrita de um bloco, do buffer para o disco
int disk_block_write (int block, void *buffer){

    task_t_queue *pedido_atual;
    pedido_atual = (task_t_queue*)malloc(10*sizeof(task_t_queue*));

    pedido_atual->block = block;
    pedido_atual->buffer = buffer;
    pedido_atual->type = TIPO_ESCRITA;
    pedido_atual->task = taskExec;
    
    task_suspend(taskExec, NULL);
    pedido_atual->status = STATUS_SUSPENSA;

    if (t_queue_inicio == NULL){
        t_queue_inicio = pedido_atual;
        t_queue_fim = pedido_atual;
    }
    else {
        t_queue_fim->next = pedido_atual;
        t_queue_fim = pedido_atual;
    }
    
    task_switch(disk_mgr_task);
    
    return 0;
}