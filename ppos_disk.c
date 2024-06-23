#include "ppos_disk.h"
// #include "ppos_data.h"
#include <stdio.h>

#include <signal.h>

#include "disk.h"
#include "ppos_data.h"
#include "ppos.h"
#include "ppos-core-globals.h"
#include "math.h"

#define FCFS 1
#define SSTF 2
#define CSCAN 3

int algoritmo = FCFS;
// int algoritmo = SSTF;
// int algoritmo = CSCAN;

/*
FCFS:
Task 0 exit: execution time 29995 ms, processor time 0 ms, 17 activations
Blocos percorridos: 14110

SSTF:
Task 0 exit: execution time 26594 ms, processor time 0 ms, 2 activations
Blocos percorridos: 5671

CSCAN:
Task 0 exit: execution time 30547 ms, processor time 0 ms, 3 activations
Blocos percorridos: 10205
*/

#define TIPO_LEITURA 1
#define TIPO_ESCRITA 2

#define STATUS_OCUPADA 1
#define STATUS_FINALIZADA 2
#define STATUS_SUSPENSA 3

#define INFINITO 99999

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

task_t_queue *getServida();

task_t_queue *fcfs();
task_t_queue *sstf();
task_t_queue *cscan();

int menor_caminho();
int menor_superior();

int tam_buffer = -1;
int cabeca_atual = 0;
int cabeca_anterior = 0;
int blocos_percorridos = 0;

void taskmgrbody()
{
    while (1){
        task_t_queue *servida = getServida();

        if (servida != NULL){

            if (servida->status == STATUS_SUSPENSA){

                if (servida->type == TIPO_LEITURA)
                    disk_cmd(
                        DISK_CMD_READ, 
                        servida->block,     
                        servida->buffer);
                else
                    disk_cmd(
                        DISK_CMD_WRITE, 
                        servida->block, 
                        servida->buffer);


                servida->status = STATUS_OCUPADA;
            }
            else if (servida->status == STATUS_FINALIZADA){

                if (servida == t_queue_inicio){
                    t_queue_inicio = t_queue_inicio->next;
                }
                else if (servida == t_queue_fim){
                    t_queue_fim = t_queue_fim->prev;
                }
                else {
                    servida->prev->next = servida->next;
                    servida->next->prev = servida->prev;
                }

                blocos_percorridos += abs(cabeca_anterior-cabeca_atual);

                task_suspend(disk_mgr_task,  NULL);
                task_resume(servida->task);
                task_yield();
            }
        }else{
            task_suspend(disk_mgr_task,  NULL);
            task_yield();
        }

    }
}


// Uma função para tratar os sinais SIGUSR1
void tratador_sigusr1 (){

    task_t_queue* servida = getServida();
    servida->status = STATUS_FINALIZADA;

    task_switch(disk_mgr_task);
}

void tratador_sigusr2 (){

    printf("Blocos percorridos: %d\n", blocos_percorridos);

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

    cabeca_atual = 0;

    // Uma função para tratar os sinais SIGUSR1
    signal(SIGUSR1, tratador_sigusr1);
    signal(SIGUSR2, tratador_sigusr2);
    
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
        task_t_queue* aux = t_queue_fim;
        t_queue_fim = pedido_atual;
        t_queue_fim->prev = aux;
    }
    
    task_switch(disk_mgr_task);

    buffer = pedido_atual->buffer;

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
        task_t_queue* aux = t_queue_fim;
        t_queue_fim = pedido_atual;
        t_queue_fim->prev = aux;
    }
    
    task_switch(disk_mgr_task);
    
    return 0;
}

task_t_queue *getServida(){
    if (t_queue_inicio == NULL)
        return NULL;

    task_t_queue *servida = fcfs();

    if (algoritmo == SSTF)
        servida = sstf();
    else if (algoritmo == CSCAN)
        servida = cscan();

    return servida;
}

task_t_queue *fcfs(){
    if (t_queue_inicio->status == STATUS_FINALIZADA){
        cabeca_anterior = cabeca_atual;
        cabeca_atual = t_queue_inicio->block;
    }

    return t_queue_inicio;
}

task_t_queue *sstf(){

    task_t_queue* servida = t_queue_inicio;

    while (servida != t_queue_fim && servida->block != cabeca_atual)
        servida = servida->next;

    if (servida->block != cabeca_atual){
        cabeca_anterior = cabeca_atual;
        cabeca_atual = menor_caminho();
        servida = sstf();
    }

    return servida;
}

int menor_caminho(){
    // if (t_queue_inicio == NULL)
    //     return cabeca_atual;

    task_t_queue* atual = t_queue_inicio;

    if (atual == t_queue_fim)
        return atual->block;

    int menor = INFINITO;
    int bloco = cabeca_atual;

    while (atual != t_queue_fim){
        int distancia = abs(atual->block - cabeca_atual);

        if (distancia <= menor){
            menor = distancia;
            bloco = atual->block;
        }
        atual = atual->next;
    }

    return bloco;
}

task_t_queue *cscan(){
    // if (t_queue_inicio == NULL)
    //     return NULL;

    task_t_queue* servida = t_queue_inicio;
    // int c_atual = cabeca_atual;

    while (servida != t_queue_fim && servida->block != cabeca_atual)
        servida = servida->next;

    if (servida->block != cabeca_atual){
        cabeca_anterior = cabeca_atual;
        cabeca_atual = menor_superior();
        servida = cscan();
    }

    return servida;
}

int menor_superior(){
    // if (t_queue_inicio == NULL)
    //     return cabeca_atual;

    task_t_queue* atual = t_queue_inicio;

    if (atual == t_queue_fim)
        return atual->block;

    int menor = INFINITO;
    int bloco = cabeca_atual;

    while (atual != t_queue_fim){
        int distancia = atual->block - cabeca_atual;

        if (distancia >= 0 && distancia <= menor){
            menor = distancia;
            bloco = atual->block;
        }
        atual = atual->next;
    }

    // se nao achou
    if (bloco == cabeca_atual){
        menor = INFINITO;
        atual = t_queue_inicio;

        if (atual == t_queue_fim)
            return atual->block;        

        while (atual != t_queue_fim){
            if (atual->block <= menor)
                menor = atual->block;
            
            atual = atual->next;
        }
        bloco = menor;

    }

    return bloco;
}