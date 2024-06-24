#include "ppos_disk.h"
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

/*************************************************************************

SAÍDAS pingpong-disco1.txt

Task 0 exit: execution time 22818 ms, processor time 18 ms, 513 activations
Blocos percorridos FCFS: 1173

Task 0 exit: execution time 22788 ms, processor time 11 ms, 513 activations
Blocos percorridos SSTF: 765

Task 0 exit: execution time 23024 ms, processor time 10 ms, 513 activations
Blocos percorridos CSCAN: 765

**************************************************************************

SAÍDAS pingpong-disco2.txt

Task 0 exit: execution time 29995 ms, processor time 0 ms, 17 activations
Blocos percorridos FCFS: 12395

Task 0 exit: execution time 26590 ms, processor time 0 ms, 2 activations
Blocos percorridos SSTF: 5670

Task 0 exit: execution time 30540 ms, processor time 0 ms, 3 activations
Blocos percorridos CSCAN: 10204

*************************************************************************/

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

    /*
    cada pedido indica a tarefa solicitante,
    o tipo de pedido (leitura ou escrita), o bloco desejado e o endereço do buffer
    de dados;
    */
    int block;
    void *buffer;

    int type;

    struct task_t_queue *prev ;  // aponta para o elemento anterior na fila
    struct task_t_queue *next ;  // aponta para o elemento seguinte na fila

    // indica se está ocupado, finalizado ou suspenso.
    int status;

} task_t_queue;

// Armazena início e fim da fila
task_t_queue *t_queue_inicio = NULL;
task_t_queue *t_queue_fim = NULL;

task_t_queue *getServida();

task_t_queue *fcfs();
task_t_queue *sstf();
task_t_queue *cscan();

int menor_caminho();
int menor_superior();

int cabeca_atual = 0;
int cabeca_anterior = 0;
int blocos_percorridos = 0;

void taskmgrbody()
{
    while (1){
        // Coleta a servida conforme a especificacao do algoritmo
        task_t_queue *servida = getServida();

        if (servida != NULL){
            // Se a servida está suspensa até então
            if (servida->status == STATUS_SUSPENSA){

                // Roda comando de leitura
                if (servida->type == TIPO_LEITURA)
                    disk_cmd(
                        DISK_CMD_READ, 
                        servida->block,     
                        servida->buffer);
                // Roda comando de escrita
                else
                    disk_cmd(
                        DISK_CMD_WRITE, 
                        servida->block, 
                        servida->buffer);

                // Define o status como ocupada para ficar iterando no laço while até finalizar
                servida->status = STATUS_OCUPADA;
            }
            // Se finalizou a operação
            else if (servida->status == STATUS_FINALIZADA){
                
                // É retirada da fila
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

                // Calcula blocos percorridos pelo valor absoluto da diferença do anterior com o atual
                blocos_percorridos += abs(cabeca_anterior-cabeca_atual);

                // Suspende a tarefa manager para prosseguir com a próxima
                task_suspend(disk_mgr_task,  NULL);
                task_resume(servida->task);
                task_yield();
            }
        }else{
            task_suspend(disk_mgr_task,  NULL);
            task_yield();
        }
        if (countTasks == 1){
            task_exit(0);
        }

    }
}


// Uma função para tratar os sinais SIGUSR1
void tratador_sigusr1 (){

    // Depois que finaliza a operação
    task_t_queue* servida = getServida();
    servida->status = STATUS_FINALIZADA;

    task_switch(disk_mgr_task);
}

// Gambiarra tratada quando da um exit para reotrnar à manager e prosseguir.
void tratador_sigusr2 (){
    if (countTasks == 1){
        if (algoritmo == FCFS)
            printf("Blocos percorridos FCFS: %d\n", blocos_percorridos);
        else if (algoritmo == SSTF)
            printf("Blocos percorridos SSTF: %d\n", blocos_percorridos);
        else if (algoritmo == CSCAN)
            printf("Blocos percorridos CSCAN: %d\n", blocos_percorridos);
    }

    task_switch(disk_mgr_task);
}

// inicializacao do gerente de disco
// retorna -1 em erro ou 0 em sucesso
// numBlocks: tamanho do disco, em blocos
// blockSize: tamanho de cada bloco do disco, em bytes
int disk_mgr_init (int *numBlocks, int *blockSize){

    // Comando de iniciar disco
    if (disk_cmd(DISK_CMD_INIT, -1, NULL) < 0)
        return -1; 

    // Comando de retornar o tamanho do disco
    int disksize = disk_cmd(DISK_CMD_DISKSIZE, -1, NULL);
    if (disksize < 0)
        return -1; 

    // Comando de retornar o tamanho de cada bloco do disco
    int blocksize = disk_cmd(DISK_CMD_BLOCKSIZE, -1, NULL);
    if (blocksize < 0)
        return -1; 

    *numBlocks = disksize;
    *blockSize = blocksize;

    // Inicializa a tarefa gerenciadora e logo suspende
    disk_mgr_task = malloc(sizeof(task_t));
    task_create(disk_mgr_task, taskmgrbody, "Tarefa gerenciadora");
    task_suspend(disk_mgr_task, NULL);

    cabeca_atual = 0;

    // Uma função para tratar os sinais SIGUSR1
    signal(SIGUSR1, tratador_sigusr1);
    // Tratador gambiarra para quando ocorre task_exit(0)
    signal(SIGUSR2, tratador_sigusr2);
    
    return 0;
}

// leitura de um bloco, do disco para o buffer
int disk_block_read (int block, void *buffer){
    
    // Cria elemento para adicionar na fila
    task_t_queue *pedido_atual;
    pedido_atual = (task_t_queue*)malloc(10*sizeof(task_t_queue*));

    // Atribui informações ao elemento
    pedido_atual->block = block;
    pedido_atual->buffer = buffer;
    pedido_atual->type = TIPO_LEITURA;
    pedido_atual->task = taskExec;
    
    /*
    Cada tarefa que solicita uma operação de leitura/escrita no disco deve ser suspensa
    até que a operação solicitada seja completada.
    */
    task_suspend(taskExec, NULL);
    pedido_atual->status = STATUS_SUSPENSA;

    // Adiciona à fila de tarefas
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

    // Após suspensa, muda para a tarefa gerenciadora    
    task_switch(disk_mgr_task);

    // Só volta pra cá depois que a operação tiver sido finalizada e removida da fila
    buffer = pedido_atual->buffer;

    return 0;
}

// escrita de um bloco, do buffer para o disco
int disk_block_write (int block, void *buffer){

    // Cria elemento para adicionar na fila
    task_t_queue *pedido_atual;
    pedido_atual = (task_t_queue*)malloc(10*sizeof(task_t_queue*));

    // Atribui informações ao elemento
    pedido_atual->block = block;
    pedido_atual->buffer = buffer;
    pedido_atual->type = TIPO_ESCRITA;
    pedido_atual->task = taskExec;
    
    /*
    Cada tarefa que solicita uma operação de leitura/escrita no disco deve ser suspensa
    até que a operação solicitada seja completada.
    */
    task_suspend(taskExec, NULL);
    pedido_atual->status = STATUS_SUSPENSA;

    // Adiciona à fila de tarefas
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
    
    // Após suspensa, muda para a tarefa gerenciadora    
    task_switch(disk_mgr_task);
    
    // Só volta pra cá depois que a operação tiver sido finalizada e removida da fila
    return 0;
}

task_t_queue *getServida(){
    if (t_queue_inicio == NULL)
        return NULL;

    task_t_queue *servida = NULL;

    // escolhe a operação conforme o algoritmo especificado no início.
    if (algoritmo == FCFS)
        servida = fcfs();
    else if (algoritmo == SSTF)
        servida = sstf();
    else // if algoritmo == SCAN)
        servida = cscan();

    return servida;
}

// First come first served
task_t_queue *fcfs(){
    // Apenas armazena a anterior depois de finalizada
    if (t_queue_inicio->status == STATUS_FINALIZADA){
        cabeca_anterior = cabeca_atual;
        cabeca_atual = t_queue_inicio->block;
    }
    // Retorna o início da fila
    return t_queue_inicio;
}

// Shortest Seek-Time First
task_t_queue *sstf(){

    task_t_queue* servida = t_queue_inicio;

    // Percorre a fila até encontrar a cabeça atual do disco
    while (servida != t_queue_fim && servida->block != cabeca_atual)
        servida = servida->next;

    // Se não encontrou a cabeça atual para retornar, 
    // quer dizer que foi removida da fila e precisa encontrar a próxima
    if (servida->block != cabeca_atual){
        cabeca_anterior = cabeca_atual;
        cabeca_atual = menor_caminho();
        servida = sstf();
    }

    return servida;
}

int menor_caminho(){

    task_t_queue* atual = t_queue_inicio;
    // Se for o único da fila, não encontra o proximo
    if (atual == t_queue_fim)
        return atual->block;

    // Valores iniciais para comparar
    int menor = INFINITO;
    int bloco = cabeca_atual;

    while (atual != t_queue_fim){
        // Pega o mais próximo independente se tá antes ou depois
        int distancia = abs(atual->block - cabeca_atual);

        // Computa como menor se for menor
        if (distancia <= menor){
            menor = distancia;
            bloco = atual->block;
        }
        atual = atual->next;
    }

    return bloco;
}

task_t_queue *cscan(){

    task_t_queue* servida = t_queue_inicio;
    // Se for o único da fila, não encontra o proximo
    while (servida != t_queue_fim && servida->block != cabeca_atual)
        servida = servida->next;

    // Se não encontrou a cabeça atual para retornar, 
    // quer dizer que foi removida da fila e precisa encontrar a próxima
    if (servida->block != cabeca_atual){
        cabeca_anterior = cabeca_atual;
        cabeca_atual = menor_superior();
        servida = cscan();
    }

    return servida;
}

int menor_superior(){
    task_t_queue* atual = t_queue_inicio;

    // Se for o único da fila, não encontra o proximo
    if (atual == t_queue_fim)
        return atual->block;

    // Valores iniciais para comparar
    int menor = INFINITO;
    int bloco = cabeca_atual;

    while (atual != t_queue_fim){
        // Pega o mais próximo em ordem crescente
        int distancia = atual->block - cabeca_atual;

        // Computa como menor se for menor e não negativo.
        if (distancia >= 0 && distancia <= menor){
            menor = distancia;
            bloco = atual->block;
        }
        atual = atual->next;
    }

    // Se não achou o maior em ordem crescente
    if (bloco == cabeca_atual){
        // Valores iniciais para comparar
        menor = INFINITO;
        atual = t_queue_inicio;

        // Se for o único da fila, não encontra o proximo
        if (atual == t_queue_fim)
            return atual->block;        

        // Encontra o primeiro menor da fila de leitura
        while (atual != t_queue_fim){
            if (atual->block <= menor)
                menor = atual->block;
            
            atual = atual->next;
        }
        bloco = menor;

    }

    return bloco;
}