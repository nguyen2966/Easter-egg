#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
        /* TODO: put a new process to queue [q] */
        if(q->size == MAX_QUEUE_SIZE || q==NULL) return;
    //    printf("Enqueue PID: %d to prio %d queue, size before: %d\n", proc->pid, proc->prio, q->size);
        //if(empty(q))q = malloc(sizeof(struct queue_t));
        q->proc[q->size++] = proc;
}

struct pcb_t * dequeue(struct queue_t * q) {
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
        if(empty(q)) return NULL ;

        struct pcb_t* highest_prio = q->proc[0];
        int index = 0;

        for( int i = 0; i<q->size; i++){
             if(q->proc[i]->prio > highest_prio->prio){
                 highest_prio = q->proc[i];
                 index = i;
             }
        }

        for(int j=index; j < q->size - 1;j++){
               q->proc[j] = q->proc[j+1]; 
        }
        q->size--;
        return highest_prio;

    
}

