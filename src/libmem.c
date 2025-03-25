/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c
 */

 #include "string.h"
 #include "mm.h"
 #include "syscall.h"
 #include "libmem.h"
 #include <stdlib.h>
 #include <stdio.h>
 #include <pthread.h>
 
 static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;
 
 /*enlist_vm_freerg_list - add new rg to freerg_list
  *@mm: memory region
  *@rg_elmt: new region
  *
  */
 int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
 {
   struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;
 
   if (rg_elmt->rg_start >= rg_elmt->rg_end)
     return -1;
 
   rg_elmt->rg_next = rg_node;
 
   /* Enlist the new region */
   mm->mmap->vm_freerg_list = rg_elmt;
 
   return 0;
 }
 
 /*get_symrg_byid - get mem region by region ID
  *@mm: memory region
  *@rgid: region ID act as symbol index of variable
  *
  */
 struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
 {
   if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
     return NULL;
 
   return &mm->symrgtbl[rgid];
 }
 
 /*__alloc - allocate a region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@rgid: memory region ID (used to identify variable in symbole table)
  *@size: allocated size
  *@alloc_addr: address of allocated memory region
  *
  */
 int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
 {
   pthread_mutex_lock(&mmvm_lock);
   /*Allocate at the toproof */
   struct vm_rg_struct rgnode;
 
   //try allocating by free_vmrg_area
   if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
   {
 
     caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
     caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
 
     *alloc_addr = rgnode.rg_start;   //return address of the allocated region
 
     pthread_mutex_unlock(&mmvm_lock);
     return 0;
   }
 
   struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);  //get vma from passed vmaid
   if (!cur_vma){
     pthread_mutex_lock(&mmvm_lock);
     return -1;
   }
 
   int inc_sz = PAGING_PAGE_ALIGNSZ(size); //increased size allocated
 
   int old_sbrk = cur_vma->sbrk;   // keep the old_sbrk
 
   if(!inc_vma_limit(caller, vmaid, inc_sz)){
     // if not break the limit, do the thing
     if (inc_sz > size){
       struct vm_rg_struct* freeNode = malloc(sizeof(struct vm_rg_struct));
       freeNode->rg_start = size + old_sbrk + 1;
       freeNode->rg_end = inc_sz + old_sbrk;
       enlist_vm_freerg_list(caller->mm, freeNode);
     }
 
     //return where the allocated region is
     caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
     caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size;
 
     *alloc_addr = old_sbrk;
     cur_vma->sbrk += inc_sz;
   }
 
   pthread_mutex_unlock(&mmvm_lock);
 
   return 0;
 }
 
 /*__free - remove a region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@rgid: memory region ID (used to identify variable in symbole table)
  *@size: allocated size
  *
  */
 int __free(struct pcb_t *caller, int vmaid, int rgid)
 {
   
   // Dummy initialization for avoding compiler dummay warning
   // in incompleted TODO code rgnode will overwrite through implementing
   // the manipulation of rgid later
 
   if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ)
     return -1;
 
   /* TODO: Manage the collect freed region to freerg_list */
   pthread_mutex_lock(&mmvm_lock);
 
   // get the region needs to free
   struct vm_rg_struct* free_rg = &caller->mm->symrgtbl[rgid];
   if (free_rg->rg_start >= free_rg->rg_end){
     pthread_mutex_unlock(&mmvm_lock);
     return -1;
   }
 
   struct vm_area_struct* cur_vma = get_vma_by_num(caller->mm, vmaid);
   if (!cur_vma){
     pthread_mutex_unlock(&mmvm_lock);
     return -1;
   }
 
   // create a region to replace the region needs to remove
   struct vm_rg_struct* new_free_rg = malloc(sizeof(struct vm_rg_struct));
   if (!new_free_rg){
     pthread_mutex_unlock(&mmvm_lock);
     return -1;
   }
   
   // replace the removed rg (free_rg) by new_free_rg and add it to freerg_list
   new_free_rg->rg_start = free_rg->rg_start;
   new_free_rg->rg_end = free_rg->rg_end;
   enlist_vm_freerg_list(caller->mm, &new_free_rg);
 
   // delete info of free_rg in vmem
   free_rg->rg_start = 0;
   free_rg->rg_end = 0;
   
   pthread_mutex_unlock(&mmvm_lock);
 
   return 0;
 }
 
 /*liballoc - PAGING-based allocate a region memory
  *@proc:  Process executing the instruction
  *@size: allocated size
  *@reg_index: memory region ID (used to identify variable in symbole table)
  */
 int liballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
 {
   /* TODO Implement allocation on vm area 0 */
   int addr;
 
   /* By default using vmaid = 0 */
   return __alloc(proc, 0, reg_index, size, &addr);
 }
 
 /*libfree - PAGING-based free a region memory
  *@proc: Process executing the instruction
  *@size: allocated size
  *@reg_index: memory region ID (used to identify variable in symbole table)
  */
 
 int libfree(struct pcb_t *proc, uint32_t reg_index)
 {
   /* TODO Implement free region */
 
   /* By default using vmaid = 0 */
   return __free(proc, 0, reg_index);
 }
 
 /*pg_getpage - get the page in ram
  *@mm: memory region
  *@pagenum: PGN
  *@framenum: return FPN
  *@caller: caller
  *
  */
 int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
 {
   uint32_t pte = mm->pgd[pgn];   //PTE BIT of the page needs to find
 
   if (!PAGING_PAGE_PRESENT(pte))
   { /* Page is not online, make it actively living */
     int vicpgn, swpfpn;
     int vicfpn;
     uint32_t vicpte;
 
     int tgtfpn = PAGING_PTE_SWP(pte);  // GET THE FRAME (CURRENTLY ON DISK) NEEDS TO LOAD TO RAM
 
     /* TODO: Play with your paging theory here */
     /* Find victim page */
     find_victim_page(caller->mm, &vicpgn);
     vicpte = mm->pgd[vicpgn];
     vicfpn = PAGING_FPN(vicpte);  // GET THE FPN OF THE VICTIM PAGE ON RAM
 
     /* Get free frame in MEMSWP */
     
     MEMPHY_get_freefp(caller->active_mswp, &swpfpn);   //get an empty frame on disk to move the victim page to
 
     /* TODO: Implement swap frame from MEMRAM to MEMSWP and vice versa*/
     __swap_cp_page(caller->mram, vicfpn, caller->mswp, swpfpn);
     __swap_cp_page(caller->mswp, tgtfpn, caller->mram, vicfpn);
 
     MEMPHY_put_freefp(caller->active_mswp, tgtfpn);
     
     // set pages' pte appropriately
     pte_set_swap(mm->pgd[vicpgn], 0, swpfpn);
     pte_set_fpn(&pte, vicfpn);
 
     enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
   }
 
   *fpn = PAGING_FPN(pte);
 
   return 0;
 }
 
 /*pg_getval - read value at given offset
  *@mm: memory region
  *@addr: virtual address to acess
  *@value: value
  *
  */
 int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
 {
   int pgn = PAGING_PGN(addr);
   int off = PAGING_OFFST(addr);
   int fpn; // update when call pg_getpage
 
   uint32_t pte = mm->pgd[pgn];
 
   int fpn = PAGING_FPN(pte);
 
   /* Get the page to MEMRAM, swap from MEMSWAP if needed */
   if (pg_getpage(mm, pgn, &fpn, caller) != 0)
     return -1; /* invalid page access */
    
   int phys_addr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
   MEMPHY_read(caller->mram, phys_addr, data);
   
 
   return 0;
 }
 
 /*pg_setval - write value to given offset
  *@mm: memory region
  *@addr: virtual address to acess
  *@value: value
  *
  */
 int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
 {
   int pgn = PAGING_PGN(addr);
   int off = PAGING_OFFST(addr);
   int fpn;
 
   /* Get the page to MEMRAM, swap from MEMSWAP if needed */
   if (pg_getpage(mm, pgn, &fpn, caller) != 0)
     return -1; /* invalid page access */
 
   int phy_addr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
   MEMPHY_write(caller->mram, phy_addr, value);  
   return 0;
 }
 
 /*__read - read value in region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@offset: offset to acess in memory region
  *@rgid: memory region ID (used to identify variable in symbole table)
  *@size: allocated size
  *
  */
 int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
 {
   struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
   struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
 
   if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
     return -1;
 
   pg_getval(caller->mm, currg->rg_start + offset, data, caller);
 
   return 0;
 }
 
 /*libread - PAGING-based read a region memory */
 int libread(
     struct pcb_t *proc, // Process executing the instruction
     uint32_t source,    // Index of source register
     uint32_t offset,    // Source address = [source] + [offset]
     uint32_t *destination)
 {
   BYTE data;
   int val = __read(proc, 0, source, offset, &data);
 
   /* TODO update result of reading action*/
   // destination
 #ifdef IODUMP
   printf("read region=%d offset=%d value=%d\n", source, offset, data);
 #ifdef PAGETBL_DUMP
   print_pgtbl(proc, 0, -1); // print max TBL
 #endif
   MEMPHY_dump(proc->mram);
 #endif
 
   return val;
 }
 
 /*__write - write a region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@offset: offset to acess in memory region
  *@rgid: memory region ID (used to identify variable in symbole table)
  *@size: allocated size
  *
  */
 int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
 {
   struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
   struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
 
   if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
     return -1;
 
   pg_setval(caller->mm, currg->rg_start + offset, value, caller);
 
   return 0;
 }
 
 /*libwrite - PAGING-based write a region memory */
 int libwrite(
     struct pcb_t *proc,   // Process executing the instruction
     BYTE data,            // Data to be wrttien into memory
     uint32_t destination, // Index of destination register
     uint32_t offset)
 {
 #ifdef IODUMP
   printf("write region=%d offset=%d value=%d\n", destination, offset, data);
 #ifdef PAGETBL_DUMP
   print_pgtbl(proc, 0, -1); // print max TBL
 #endif
   MEMPHY_dump(proc->mram);
 #endif
 
   return __write(proc, 0, destination, offset, data);
 }
 
 /*free_pcb_memphy - collect all memphy of pcb
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@incpgnum: number of page
  */
 int free_pcb_memph(struct pcb_t *caller)
 {
   int pagenum, fpn;
   uint32_t pte;
 
   for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
   {
     pte = caller->mm->pgd[pagenum];
 
     if (!PAGING_PAGE_PRESENT(pte))
     {
       fpn = PAGING_PTE_FPN(pte);
       MEMPHY_put_freefp(caller->mram, fpn);
     }
     else
     {
       fpn = PAGING_PTE_SWP(pte);
       MEMPHY_put_freefp(caller->active_mswp, fpn);
     }
   }
 
   return 0;
 }
 
 /*find_victim_page - find victim page
  *@caller: caller
  *@pgn: return page number
  *
  */
 int find_victim_page(struct mm_struct *mm, int *retpgn)
 {
   pthread_mutex_lock(&mmvm_lock);
 
   
   if (mm->fifo_pgn == NULL){
     pthread_mutex_unlock(&mmvm_lock);
     return -1;
   }
 
   struct pgn_t *pg = mm->fifo_pgn;
   if (!pg){
     pthread_mutex_unlock(&mmvm_lock);
     return -1;
   }
   
   /* TODO: Implement the theorical mechanism to find the victim page */
   struct pgn_t *prev_pg = mm->fifo_pgn;
   
   while(pg->pg_next){
     prev_pg = pg;
     pg = pg->pg_next;
   }
   *retpgn = pg->pgn;
   prev_pg->pg_next = NULL;
 
   free(pg);
 
   pthread_mutex_unlock(&mmvm_lock);
   return 0;
 }
 
 /*get_free_vmrg_area - get a free vm region
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@size: allocated size
  *
  */
 int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
 {
   struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
 
   struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;
 
   if (rgit == NULL)
     return -1;
 
   /* Probe unintialized newrg */
   newrg->rg_start = newrg->rg_end = -1;
 
   //traverse the list of free list
   while(rgit != NULL){
     int rg_size = rgit->end - rgit->rg_start;
 
     if (rg_size >= size){
       newrg->rg_start = rgit->rg_start;
       newrg->rg_end = rgit->rg_end;
       break;
     }
     
     rgit = rgit->rg_next;
     if (rgit == NULL) return -1;  //no regions can be chosen
 
   }
 
   return 0;
 }
 
 // #endif
 