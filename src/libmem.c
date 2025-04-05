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
 //  printf("[DEBUG] __alloc: Allocating memory region, vmaid=%d, rgid=%d, size=%d\n", vmaid, rgid, size);
   /*Allocate at the toproof */
   struct vm_rg_struct rgnode;
 
   //try allocating by free_vmrg_area
   if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
   {
    // printf("[DEBUG] __alloc: Found free region from start=%d to end=%d\n", rgnode.rg_start, rgnode.rg_end);
     caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
     caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
 
     *alloc_addr = rgnode.rg_start;   //return address of the allocated region
 
     pthread_mutex_unlock(&mmvm_lock);
     return 0;
   }
 
   struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);  //get vma from passed vmaid
   if (!cur_vma){
 //    printf("[ERROR] __alloc: Failed to get vma for vmaid=%d\n", vmaid);
     pthread_mutex_unlock(&mmvm_lock);
     return -1;
   }
 
   int inc_sz = PAGING_PAGE_ALIGNSZ(size); //increased size allocated
  // printf("[DEBUG] __alloc: No free region found, increasing limit. Aligned size=%d\n", inc_sz);
 
   int old_sbrk = cur_vma->sbrk;   // keep the old_sbrk
 
   if(inc_vma_limit(caller, vmaid, inc_sz) == 0){
     // if not break the limit, do the thing
     if (inc_sz > size){
       struct vm_rg_struct* freeNode = malloc(sizeof(struct vm_rg_struct));
       freeNode->rg_start = size + old_sbrk + 1;
       freeNode->rg_end = inc_sz + old_sbrk;
      // printf("[DEBUG] __alloc: Adding remaining space to free list: %d to %d\n", freeNode->rg_start, freeNode->rg_end);
       enlist_vm_freerg_list(caller->mm, freeNode);
     }
 
     //return where the allocated region is
     caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
     caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size;
 
     *alloc_addr = old_sbrk;
     cur_vma->sbrk += inc_sz;
   //  printf("[DEBUG] __alloc: Allocated at sbrk=%d, new sbrk=%d\n", old_sbrk, cur_vma->sbrk);
   } else {
    // printf("[ERROR] __alloc: Failed to increase VMA limit\n");
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
   printf("[DEBUG] __free: Freeing memory region, vmaid=%d, rgid=%d\n", vmaid, rgid);
 //  
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
     printf("[ERROR] __free: Invalid region bounds: start=%d, end=%d\n", free_rg->rg_start, free_rg->rg_end);
     pthread_mutex_unlock(&mmvm_lock);
     return -1;
   }
 
   struct vm_area_struct* cur_vma = get_vma_by_num(caller->mm, vmaid);
   if (!cur_vma){
     printf("[ERROR] __free: Failed to get vma for vmaid=%d\n", vmaid);
     pthread_mutex_unlock(&mmvm_lock);
     return -1;
   }
 
   // create a region to replace the region needs to remove
   struct vm_rg_struct* new_free_rg = malloc(sizeof(struct vm_rg_struct));
   if (!new_free_rg){
     printf("[ERROR] __free: Failed to allocate memory for new free region\n");
     pthread_mutex_unlock(&mmvm_lock);
     return -1;
   }
   
   // replace the removed rg (free_rg) by new_free_rg and add it to freerg_list
   new_free_rg->rg_start = free_rg->rg_start;
   new_free_rg->rg_end = free_rg->rg_end;
   printf("[DEBUG] __free: Adding region to free list: %d to %d\n", new_free_rg->rg_start, new_free_rg->rg_end);
   enlist_vm_freerg_list(caller->mm, new_free_rg);
 
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
  // printf("[DEBUG] liballoc: Called with size=%u, reg_index=%u\n", size, reg_index);
 
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
   printf("[DEBUG] libfree: Called with reg_index=%u\n", reg_index);
 
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
  // printf("[DEBUG] pg_getpage: Starting for pgn=%d\n", pgn);
   
   if (pgn < 0 || pgn >= PAGING_MAX_PGN) {
   //  printf("[ERROR] pg_getpage: Invalid page number: %d\n", pgn);
     return -1;
   }
   
   if (!mm) {
  //   printf("[ERROR] pg_getpage: mm is NULL\n");
     return -1;
   }
   
   uint32_t pte = mm->pgd[pgn];   //PTE BIT of the page needs to find
 //  printf("[DEBUG] pg_getpage: Got PTE=0x%x for pgn=%d\n", pte, pgn);
 
   if (!PAGING_PAGE_PRESENT(pte))
   { /* Page is not online, make it actively living */
   //  printf("[DEBUG] pg_getpage: Page is not present, need to swap\n");
     int swpfpn;
     int vicpgn;
     int vicfpn;
     uint32_t vicpte;
 
     int tgtfpn = PAGING_PTE_SWP(pte);  // GET THE FRAME (CURRENTLY ON DISK) NEEDS TO LOAD TO RAM
   //  printf("[DEBUG] pg_getpage: Target frame on disk: tgtfpn=%d\n", tgtfpn);
 
     /* TODO: Play with your paging theory here */
     /* Find victim page */
     //printf("[DEBUG] pg_getpage: Finding victim page...\n");
     
     if (!caller || !caller->mm) {
      // printf("[ERROR] pg_getpage: caller or caller->mm is NULL\n");
       return -1;
     }
     
     int victim_result = find_victim_page(caller->mm, &vicpgn);
     if (victim_result != 0) {
    //   printf("[ERROR] pg_getpage: Failed to find victim page, result=%d\n", victim_result);
       return -1;
     }
     
     //printf("[DEBUG] pg_getpage: Found victim page: vicpgn=%d\n", vicpgn);
     
     if (vicpgn < 0 || vicpgn >= PAGING_MAX_PGN) {
       //printf("[ERROR] pg_getpage: Invalid victim page number: %d\n", vicpgn);
       return -1;
     }
     
     vicpte = mm->pgd[vicpgn];
    // printf("[DEBUG] pg_getpage: Victim PTE=0x%x\n", vicpte);
     
     if (!PAGING_PAGE_PRESENT(vicpte)) {
      // printf("[ERROR] pg_getpage: Victim page is not present\n");
       return -1;
     }
     
     vicfpn = PAGING_FPN(vicpte);  // GET THE FPN OF THE VICTIM PAGE ON RAM
   //  printf("[DEBUG] pg_getpage: Victim frame number: vicfpn=%d\n", vicfpn);
 
     /* Get free frame in MEMSWP */
    // printf("[DEBUG] pg_getpage: Getting free frame in swap space\n");
     if (!caller->active_mswp) {
     //  printf("[ERROR] pg_getpage: caller->active_mswp is NULL\n");
       return -1;
     }
     
     int swap_result = MEMPHY_get_freefp(caller->active_mswp, &swpfpn);
     if (swap_result != 0) {
  //     printf("[ERROR] pg_getpage: Failed to get free frame in swap space, result=%d\n", swap_result);
       return -1;
     }
    // printf("[DEBUG] pg_getpage: Got free frame in swap: swpfpn=%d\n", swpfpn);
 
     /* TODO: Implement swap frame from MEMRAM to MEMSWP and vice versa*/
   //  printf("[DEBUG] pg_getpage: Swapping vicfpn=%d to swpfpn=%d\n", vicfpn, swpfpn);
     
     if (!caller->mram) {
     //  printf("[ERROR] pg_getpage: caller->mram is NULL\n");
       return -1;
     }
     
     int swap_cp_result1 = __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
     if (swap_cp_result1 != 0) {
   //    printf("[ERROR] pg_getpage: Failed to swap from RAM to swap space, result=%d\n", swap_cp_result1);
       return -1;
     }
     
     //printf("[DEBUG] pg_getpage: Swapping tgtfpn=%d to vicfpn=%d\n", tgtfpn, vicfpn);
     int swap_cp_result2 = __swap_cp_page(caller->active_mswp, tgtfpn, caller->mram, vicfpn);
     if (swap_cp_result2 != 0) {
    //   printf("[ERROR] pg_getpage: Failed to swap from swap space to RAM, result=%d\n", swap_cp_result2);
       return -1;
     }
 
    // printf("[DEBUG] pg_getpage: Freeing target frame in swap space: tgtfpn=%d\n", tgtfpn);
     MEMPHY_put_freefp(caller->active_mswp, tgtfpn);
     
     // set pages' pte appropriately
   //  printf("[DEBUG] pg_getpage: Setting victim page as swapped: vicpgn=%d, swpfpn=%d\n", vicpgn, swpfpn);
     pte_set_swap(&mm->pgd[vicpgn], 0, swpfpn);
     
    // printf("[DEBUG] pg_getpage: Setting current page as present: pgn=%d, vicfpn=%d\n", pgn, vicfpn);
     pte_set_fpn(&pte, vicfpn);
 
    // printf("[DEBUG] pg_getpage: Adding page to FIFO list: pgn=%d\n", pgn);
     int enlist_result = enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
     if (enlist_result != 0) {
      // printf("[ERROR] pg_getpage: Failed to add page to FIFO list, result=%d\n", enlist_result);
     }
   } else {
   //  printf("[DEBUG] pg_getpage: Page is already present\n");
   }
 
   *fpn = PAGING_FPN(pte);
 //  printf("[DEBUG] pg_getpage: Returning fpn=%d\n", *fpn);
 
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
 
  // printf("[DEBUG] pg_getval: Reading from addr=%d, pgn=%d, offset=%d\n", addr, pgn, off);
 
   uint32_t pte = mm->pgd[pgn];
  // printf("[DEBUG] pg_getval: PTE=0x%x\n", pte);
 
   fpn = PAGING_FPN(pte);
   //printf("[DEBUG] pg_getval: Initial fpn=%d\n", fpn);
 
   /* Get the page to MEMRAM, swap from MEMSWAP if needed */
   if (pg_getpage(mm, pgn, &fpn, caller) != 0) {
    // printf("[ERROR] pg_getval: Failed to get page\n");
     return -1; /* invalid page access */
   }
    
   int phys_addr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
   //printf("[DEBUG] pg_getval: Reading from physical address=%d\n", phys_addr);
   
   int read_result = MEMPHY_read(caller->mram, phys_addr, data);
   if (read_result != 0) {
    // printf("[ERROR] pg_getval: Failed to read from memory, result=%d\n", read_result);
     return -1;
   }
   
   //printf("[DEBUG] pg_getval: Read value=%d\n", *data);
 
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
 
   printf("[DEBUG] pg_setval: Writing to addr=%d, pgn=%d, offset=%d, value=%d\n", addr, pgn, off, value);
 
   /* Get the page to MEMRAM, swap from MEMSWAP if needed */
   if (pg_getpage(mm, pgn, &fpn, caller) != 0) {
     printf("[ERROR] pg_setval: Failed to get page\n");
     return -1; /* invalid page access */
   }
 
   int phy_addr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
   printf("[DEBUG] pg_setval: Writing to physical address=%d\n", phy_addr);
   
   int write_result = MEMPHY_write(caller->mram, phy_addr, value);
   if (write_result != 0) {
     printf("[ERROR] pg_setval: Failed to write to memory, result=%d\n", write_result);
     return -1;
   }
   
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
   printf("[DEBUG] __read: vmaid=%d, rgid=%d, offset=%d\n", vmaid, rgid, offset);
   
   struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
   struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
 
   if (currg == NULL || cur_vma == NULL) { /* Invalid memory identify */
     printf("[ERROR] __read: Invalid region or VMA\n");
     return -1;
   }
 
   printf("[DEBUG] __read: Reading from virtual address=%d\n", currg->rg_start + offset);
   int result = pg_getval(caller->mm, currg->rg_start + offset, data, caller);
   if (result != 0) {
     printf("[ERROR] __read: Failed to get value\n");
   }
 
   return result;
 }
 
 /*libread - PAGING-based read a region memory */
 int libread(
     struct pcb_t *proc, // Process executing the instruction
     uint32_t source,    // Index of source register
     uint32_t offset,    // Source address = [source] + [offset]
     uint32_t *destination)
 {
  // printf("[DEBUG] libread: source=%u, offset=%u\n", source, offset);
   
   BYTE data;
   int val = __read(proc, 0, source, offset, &data);
 
   if (val == 0 && destination != NULL) {
     *destination = data;
     printf("[DEBUG] ibread: Read successful, value=%u\n", data);
   } else {
     *destination = -1 ;// no data to read, set to -1 so the sycall stop to read
     printf("[ERROR] libread: Read failed or destination is NULL\n");
     return -1;
   }
 
 #ifdef IODUMP
  // printf("read region=%d offset=%d value=%d\n", source, offset, data);
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
//   printf("[DEBUG] __write: vmaid=%d, rgid=%d, offset=%d, value=%d\n", vmaid, rgid, offset, value);
   
   struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
   struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
 
   if (currg == NULL || cur_vma == NULL) { /* Invalid memory identify */
    // printf("[ERROR] __write: Invalid region or VMA\n");
     return -1;
   }
 
  // printf("[DEBUG] __write: Writing to virtual address=%d\n", currg->rg_start + offset);
   int result = pg_setval(caller->mm, currg->rg_start + offset, value, caller);
   if (result != 0) {
    // printf("[ERROR] __write: Failed to set value\n");
   }
 
   return result;
 }
 
 /*libwrite - PAGING-based write a region memory */
 int libwrite(
     struct pcb_t *proc,   // Process executing the instruction
     BYTE data,            // Data to be wrttien into memory
     uint32_t destination, // Index of destination register
     uint32_t offset)
 {
  // printf("[DEBUG] libwrite: data=%u, destination=%u, offset=%u\n", data, destination, offset);
 
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
  // printf("[DEBUG] free_pcb_memph: Starting\n");
   
   int pagenum, fpn;
   uint32_t pte;
 
   for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
   {
     pte = caller->mm->pgd[pagenum];
  //   printf("[DEBUG] free_pcb_memph: Processing page %d, PTE=0x%x\n", pagenum, pte);
 
     if (!PAGING_PAGE_PRESENT(pte))
     {
       fpn = PAGING_PTE_FPN(pte);
    //   printf("[DEBUG] free_pcb_memph: Page %d is present, freeing fpn=%d\n", pagenum, fpn);
       MEMPHY_put_freefp(caller->mram, fpn);
     }
     else
     {
       fpn = PAGING_PTE_SWP(pte);
    //   printf("[DEBUG] free_pcb_memph: Page %d is swapped, freeing swap fpn=%d\n", pagenum, fpn);
       MEMPHY_put_freefp(caller->active_mswp, fpn);
     }
   }
 
   printf("[DEBUG] free_pcb_memph: Completed\n");
   return 0;
 }
 
 /*find_victim_page - find victim page
  *@caller: caller
  *@pgn: return page number
  *
  */
 int find_victim_page(struct mm_struct *mm, int *retpgn)
 {
  // printf("[DEBUG] find_victim_page: Starting\n");
   
   pthread_mutex_lock(&mmvm_lock);
 
   if (mm == NULL) {
   //  printf("[ERROR] find_victim_page: mm is NULL\n");
     pthread_mutex_unlock(&mmvm_lock);
     return -1;
   }
   
   if (mm->fifo_pgn == NULL){
     //printf("[ERROR] find_victim_page: FIFO page list is empty\n");
     pthread_mutex_unlock(&mmvm_lock);
     return -1;
   }
 
   struct pgn_t *pg = mm->fifo_pgn;
   if (!pg){
   //  printf("[ERROR] find_victim_page: First page in FIFO list is NULL\n");
     pthread_mutex_unlock(&mmvm_lock);
     return -1;
   }
   
   //printf("[DEBUG] find_victim_page: First page in FIFO list is %d\n", pg->pgn);
   
   /* TODO: Implement the theorical mechanism to find the victim page */
   struct pgn_t *prev_pg = mm->fifo_pgn;
   
   if (pg->pg_next == NULL) {
     // Only one page in the list
    // printf("[DEBUG] find_victim_page: Only one page in FIFO list\n");
     *retpgn = pg->pgn;
     mm->fifo_pgn = NULL;
     free(pg);
   } else {
     // Multiple pages, iterate to find the last one
   //  printf("[DEBUG] find_victim_page: Multiple pages in FIFO list, finding last...\n");
     while(pg->pg_next){
       prev_pg = pg;
       pg = pg->pg_next;
    //   printf("[DEBUG] find_victim_page: Checking page %d\n", pg->pgn);
     }
     *retpgn = pg->pgn;
    // printf("[DEBUG] find_victim_page: Found victim page %d\n", *retpgn);
     prev_pg->pg_next = NULL;
     free(pg);
   }
 
   //printf("[DEBUG] find_victim_page: Returning victim page %d\n", *retpgn);
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
  // printf("[DEBUG] get_free_vmrg_area: vmaid=%d, size=%d\n", vmaid, size);
   
   struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
   if (!cur_vma) {
     //printf("[ERROR] get_free_vmrg_area: Failed to get VMA\n");
     return -1;
   }
 
   struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;
 
   if (rgit == NULL) {
    // printf("[DEBUG] get_free_vmrg_area: Free region list is empty\n");
     return -1;
   }
 
   /* Probe unintialized newrg */
   newrg->rg_start = newrg->rg_end = -1;
 
   //traverse the list of free list
   //printf("[DEBUG] get_free_vmrg_area: Searching for suitable free region\n");
   while(rgit != NULL){
     int rg_size = rgit->rg_end - rgit->rg_start;
   //  printf("[DEBUG] get_free_vmrg_area: Checking region: start=%d, end=%d, size=%d\n", 
          //  rgit->rg_start, rgit->rg_end, rg_size);
 
     if (rg_size >= size){
       newrg->rg_start = rgit->rg_start;
       newrg->rg_end = rgit->rg_end;
     //  printf("[DEBUG] get_free_vmrg_area: Found suitable region: start=%d, end=%d\n", 
            //  newrg->rg_start,newrg->rg_end);
       }
       rgit = rgit->rg_next;
     if (rgit == NULL) return -1;  //no regions can be chosen
 
   }
 
   return 0;
 }
 //#endif