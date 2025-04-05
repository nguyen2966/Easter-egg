/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

 #include "common.h"
 #include "syscall.h"
 #include "stdio.h"
 #include "libmem.h"
 
 //Cac thu vien ben duoi tu add
 #include "string.h"
 #include "queue.h"
 #include "sched.h"
 
 int __sys_killall(struct pcb_t *caller, struct sc_regs* regs)
 {
     char proc_name[100];
     uint32_t data;
 
     //hardcode for demo only
     uint32_t memrg = regs->a1; // ID of mem region that store proc naame need to kill
     
     /* TODO: Get name of the target proc */
     //proc_name = libread..
     int i = 0;
     data = 0;
     while(data != -1){
         libread(caller, memrg, i, &data);
         proc_name[i]= data;
         printf("Syskill all iteration %d, read data= %d, proc_name[%d]= %d\n",i,data,i,data);
         if(data == -1) proc_name[i]='\0';
         i++;
     }
    
     printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);
     
     /* TODO: Traverse proclist to terminate the proc
      *       stcmp to check the process match proc_name
      */
     struct pcb_t *kill_process [MAX_PRIO]; // this list to add processed required to kill
     int index = 0;
 
     struct queue_t *run_list = caller->running_list;
     struct queue_t *mlq = caller->mlq_ready_queue;
     printf("run_list size: %d\n",run_list->size);
     
     //Process name is read from function read_config in os.c, which has the form input/proc/name
     // Therefore we need to extract the name using strrchr function
 
     for(int i =0 ;i<run_list->size;i++){
         char *proc_get_name = strrchr(run_list->proc[i]->path, '/');  //skip 2 /
         if(proc_get_name && strcmp(proc_get_name+1,proc_name) == 0){   //move the pointer from / to the first char of name
             kill_process[index++] = run_list->proc[i];
             printf("Found process name %s to kill in run list\n",proc_get_name);
             run_list->proc[i]->pc = run_list->proc[i]->code->size; // set program counter = size to force the process to end
             for(int j=i; j < run_list->size - 1;j++){
                 run_list->proc[j] = run_list->proc[j+1];  // Remove process form queue by shifting array
              }
             run_list->size--;
             i--; //to check the newly shifted process, avoid skipping it
         }
     }
 
     for(int i =0 ;i<MAX_PRIO;i++){
         for(int j =0 ;j<mlq[i].size;j++){
             char *mlq_proc_get_name = strrchr(mlq[i].proc[j]->path, '/');
           if(mlq_proc_get_name && strcmp(mlq_proc_get_name+1,proc_name)==0){
           //  pthread_mutex_lock(&queue_lock);
             printf("mlq[%d] size = %d\n",i,mlq[i].size);
             kill_process[index++] = mlq[i].proc[j];
             printf("Found process name %s to kill in mlq\n", mlq_proc_get_name);
             mlq[i].proc[j]->pc = mlq[i].proc[j]->code->size; // set program counter = size to force the process to end
             for(int k=0; k < mlq[i].size - 1;k++){
                 mlq[i].proc[k] = mlq[i].proc[k+1];  // Remove process form queue by shifting array
              } 
              mlq[i].size--; 
              j--; //to check the newly shifted process, avoid skipping it
            // pthread_mutex_unlock(&queue_lock); 
         }
       }
     }
 
     /* TODO Maching and terminating 
      *       all processes with given
      *        name in var proc_name
      */
     
     if(index > 0){
         printf("Remove %d process from mlq and run queue,ready to free\n",index);
         for(int i=0;i<index;i++){
             printf("Free ALlocated region for process %s with ID=%d in kill list\n",proc_name,i);
             for(int j=0;j<kill_process[i]->code->size;j++)
             if(kill_process[i]->code->text[j].opcode==ALLOC){  //Free all allocated region of killed process
                __free(kill_process[i],kill_process[i]->mm->mmap->vm_id,kill_process[i]->code->text[j].arg_0);
         } 
       }
     }
     else {
         printf("Process with name %s does not exist\n",proc_name);
     }
 
     return 0; 
 }
 