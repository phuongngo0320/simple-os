//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, int vmaid, struct vm_rg_struct* rg_elmt) // update: add param vmaid, add * (rg_elmt) 
{
  // -------------------------------------------------------------------------------------
  //struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list; // update: replace this line
  struct vm_area_struct* cur_vma = get_vma_by_num(mm, vmaid);
  struct vm_rg_struct* rg_node = cur_vma->vm_freerg_list;
  // -------------------------------------------------------------------------------------

  if (rg_elmt->rg_start >= rg_elmt->rg_end) // update: rg_elmt as pointer
    return -1;

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node; // update: rg_elmt as pointer

  /* Enlist the new region */
  
  // -------------------------------------------------------------------------------------
  // mm->mmap->vm_freerg_list = &rg_elmt; // update: replace this line
  cur_vma->vm_freerg_list = rg_elmt;
  // -------------------------------------------------------------------------------------
  return 0;
}

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  struct vm_area_struct *pvma= mm->mmap;

  if(mm->mmap == NULL)
    return NULL;

  int vmait = 0;
  
  while (vmait < vmaid)
  {
    if(pvma == NULL)
	  return NULL;

    pvma = pvma->vm_next;
  }

  return pvma;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if(rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
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
  /*Allocate at the toproof */
  struct vm_rg_struct rgnode;

  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {

    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;

    *alloc_addr = rgnode.rg_start;

    #ifdef MEM_DEBUG
    printf("Got free region [%ld-%ld]\n", rgnode.rg_start, rgnode.rg_end);
    printf("List of regions in symbol table is now:\n");
    for (int i = 0; i < PAGING_MAX_SYMTBL_SZ; ++i)
      printf("rg[%ld->%ld]\n", caller->mm->symrgtbl[i].rg_start, caller->mm->symrgtbl[i].rg_end);
    #endif

    // -------------------------------------------------------------------------
    // set initial value of memory
    for (unsigned long addr = rgnode.rg_start; addr < rgnode.rg_end; ++addr) {
      pg_setval(caller->mm, addr, MEMORY_INIT_DATA, caller);
    }
    // -------------------------------------------------------------------------

    return 0;
  }

  #ifdef MEM_DEBUG
  printf("No free region!\n");
  #endif

  /* TODO get_free_vmrg_area FAILED handle the region management (Fig.6)*/

  /*Attempt to increate limit to get space */
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  //int inc_sz = PAGING_PAGE_ALIGNSZ(size); // update: remove this line
  //int inc_limit_ret
  int old_sbrk;

  old_sbrk = cur_vma->sbrk;

  // ---------------------------------------------------------------
  /* TODO INCREASE THE LIMIT
   * inc_vma_limit(caller, vmaid, inc_sz)
   */
  
  if (cur_vma->sbrk + size > cur_vma->vm_end) // sbrk > end
  {
    int inc_sz = cur_vma->sbrk + size - cur_vma->vm_end;
    int inc_limit_ret = inc_vma_limit(caller, vmaid, inc_sz);
    if (inc_limit_ret < 0)
      return -1;
  }
  /*Successful increase limit */
  cur_vma->sbrk += size;
  // ---------------------------------------------------------------

  caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
  caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size;

  #ifdef MEM_DEBUG
  printf("List of regions in symbol table is now:\n");
  for (int i = 0; i < PAGING_MAX_SYMTBL_SZ; ++i)
    printf("rg[%ld->%ld]\n", caller->mm->symrgtbl[i].rg_start, caller->mm->symrgtbl[i].rg_end);

  printf("SBRK is now: %ld\n", cur_vma->sbrk);
  #endif

  *alloc_addr = old_sbrk;
  // -------------------------------------------------------------------------
  // set initial value of memory
  for (unsigned long addr = old_sbrk; addr < old_sbrk + size; ++addr) {
    pg_setval(caller->mm, addr, MEMORY_INIT_DATA, caller);
  }
  // -------------------------------------------------------------------------
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
  struct vm_rg_struct* freergnode = malloc(sizeof(struct vm_rg_struct*));

  if(rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return -1;

  // -------------------------------------------------------------------------------------
  /* TODO: Manage the collect freed region to freerg_list */

  // get rgnode from symbol table
  struct vm_rg_struct* rgnode = get_symrg_byid(caller->mm, rgid);

  // deep copy rgnode to freergnode
  freergnode->rg_start = rgnode->rg_start;
  freergnode->rg_end = rgnode->rg_end;
  freergnode->rg_next = NULL;

  // set all byte memory NULL
  for (unsigned long addr = rgnode->rg_start; addr < rgnode->rg_end; ++addr)
    pg_setval(caller->mm, addr, MEMORY_NULL_DATA, caller);

  // remove the freed region from symbol table
  rgnode->rg_start = 0;
  rgnode->rg_end = 0;
  rgnode->rg_next = NULL;
  // -------------------------------------------------------------------------------------

  /*enlist the obsoleted memory region */
  enlist_vm_freerg_list(caller->mm, vmaid, freergnode); // update: pass param vmaid

  #ifdef MEM_DEBUG
  printf("Free Region List after FREE:\n");
  print_list_rg(caller->mm->mmap->vm_freerg_list);
  #endif
  return 0;
}

/*pgalloc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int pgalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  int addr;

  /* By default using vmaid = 0 */
  return __alloc(proc, 0, reg_index, size, &addr);
}

/*pgfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int pgfree_data(struct pcb_t *proc, uint32_t reg_index)
{
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
  uint32_t pte = mm->pgd[pgn];
 
  if (!PAGING_PAGE_PRESENT(pte)) {
    #ifdef MEM_DEBUG
    printf("FATAL ERROR: pg_getpage - page not present!\n");
    #endif
    return -1; // page is null
  }

  if (PAGING_PAGE_SWAPPED(pte))
  { /* Page is not online, make it actively living */
    int vicpgn;
    int swpfpn; 
    int vicfpn;
    uint32_t vicpte;

    // ---------------------------------------------------
    //int tgtfpn = PAGING_SWP(pte);//the target frame storing our variable
    int tgtfpn = PAGING_PTE_SWPOFF(pte) / PAGING_PAGESZ; // update: macro to get swap frame number
    int swptyp = PAGING_PTE_SWPTYP(pte); // get swaptype to identify the MEMSWAP that contains the target
    // ---------------------------------------------------
    /* TODO: Play with your paging theory here */
    /* Find victim page */

    #ifdef MEM_DEBUG
    printf("SWAP detected in getpage!\n");
    printf("Target frame %d is in SWAP %d\n", tgtfpn, swptyp);
    #endif
    
    int ram_get_freefp_ret = MEMPHY_get_freefp(caller->mram, &vicfpn);

    if (ram_get_freefp_ret == 0) // got free frame in RAM --> bring target page to free frame
    {
      // copy target page to victim frame (free frame)
      __swap_cp_page(&caller->mswp[swptyp], tgtfpn, caller->mram, vicfpn);
      
      // update page table
      pte_set_fpn(&pte, vicfpn);
      mm->pgd[pgn] = pte;

      enlist_pgn_node(&caller->mm->fifo_pgn, pgn);

      #ifdef MEM_DEBUG
      printf("Bring target page to free frame %d\n", vicfpn);
      printf("Refs:\n");
      print_list_pgn(caller->mm->fifo_pgn);
      #endif
    }
    else // RAM is full --> choose a victim page and swap
    {
      find_victim_page(caller->mm, &vicpgn);
      vicpte = caller->mm->pgd[vicpgn]; // get victim page's PTE
      vicfpn = PAGING_PTE_FPN(vicpte); // get victim page's frame

      /* Get free frame in MEMSWP */
      int mpget_freefp_ret = 0;
      int mswp_full_count = 0;
      while (1)
      { 
        mpget_freefp_ret = MEMPHY_get_freefp(caller->active_mswp, &swpfpn);
        if (mpget_freefp_ret < 0)
        {
          // current active MEMSWAP is out of free frame
          mswp_full_count++;

          if (mswp_full_count == PAGING_MAX_MMSWP) {
            #ifdef MEM_DEBUG
            printf("OOM: pg_getpage out of memory\n");
            #endif
            return -3000; // out of memory (all MEMSWAPs are out of free frame)
          }

          // else: switch active one to next MEMSWAP
          int next_id = caller->active_mswp->memphy_id + 1;
          if (next_id == PAGING_MAX_MMSWP)
            next_id = 0;

          caller->active_mswp = &caller->mswp[next_id];
        }
        else
          break;
      }

      /* Do swap frame from MEMRAM to MEMSWP and vice versa*/
      /* Copy victim frame to swap */
      //__swap_cp_page();
      __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
      /* Copy target frame from swap to mem */
      //__swap_cp_page();
      __swap_cp_page(&caller->mswp[swptyp], tgtfpn, caller->mram, vicfpn);
      MEMPHY_put_freefp(&caller->mswp[swptyp], tgtfpn);

      /* Update page table */
      //pte_set_swap() &mm->pgd;
      pte_set_swap(&vicpte, caller->active_mswp->memphy_id, swpfpn*PAGING_PAGESZ);
      mm->pgd[vicpgn] = vicpte;

      /* Update its online status of the target page */
      //pte_set_fpn() & mm->pgd[pgn];
      pte_set_fpn(&pte, vicfpn); // update: change tgtfpn to vicfpn
      mm->pgd[pgn] = pte;

      // add page to FIFO queue
      enlist_pgn_node(&caller->mm->fifo_pgn, pgn);

      #ifdef MEM_DEBUG
      printf("Got swap frame: %d\n", swpfpn);
      printf("Swapped: \n\tPage %d: Victim frame %d to swap frame %d \n\tPage %d: Target frame %d to victim frame %d\n", vicpgn, vicfpn, swpfpn, pgn, tgtfpn, vicfpn);
      printf("Refs:\n");
      print_list_pgn(caller->mm->fifo_pgn);
      #endif
    }
  }

  // ---------------------------------------------------
  //*fpn = PAGING_FPN(pte);
  *fpn = PAGING_PTE_FPN(pte); // update: macro to get FPN
  // ---------------------------------------------------

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
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if(pg_getpage(mm, pgn, &fpn, caller) != 0) 
    return -1; /* invalid page access */

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  MEMPHY_read(caller->mram,phyaddr, data);

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
  if(pg_getpage(mm, pgn, &fpn, caller) != 0) 
    return -1; /* invalid page access */

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  MEMPHY_write(caller->mram,phyaddr, value);

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

  if(currg == NULL || cur_vma == NULL) /* Invalid memory identify */ {
    #ifdef MEM_DEBUG
    printf("Invalid memory: cannot identify region/area ID");
    #endif
	  return -1;
  }

  // ---------------------------------------------------------------------
  if (currg->rg_start + offset >= currg->rg_end) {
    #ifdef MEM_DEBUG
    printf("Read position is out of region range\n");
    #endif
    return -1; // update: offset out of region range
  }
  // ---------------------------------------------------------------------

  pg_getval(caller->mm, currg->rg_start + offset, data, caller);

  return 0;
}


/*pgwrite - PAGING-based read a region memory */
int pgread(
		struct pcb_t * proc, // Process executing the instruction
		uint32_t source, // Index of source register
		uint32_t offset, // Source address = [source] + [offset]
		uint32_t destination) 
{
  BYTE data;
  int val = __read(proc, 0, source, offset, &data);

  destination = (uint32_t) data;
// #ifdef IODUMP
  printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  //   MEMPHY_dump(proc->mram, "ram.txt", "READ");
  // #endif

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
  
  if(currg == NULL || cur_vma == NULL) /* Invalid memory identify */
	  return -1;

  // ---------------------------------------------------------------------
  if (currg->rg_start + offset >= currg->rg_end) {
    #ifdef MEM_DEBUG
    printf("Write position is out of region range!\n");
    #endif
    return -1; // update: offset out of region range
  }
  // ---------------------------------------------------------------------

  pg_setval(caller->mm, currg->rg_start + offset, value, caller);
  
  return 0;
}

/*pgwrite - PAGING-based write a region memory */
int pgwrite(
		struct pcb_t * proc, // Process executing the instruction
		BYTE data, // Data to be wrttien into memory
		uint32_t destination, // Index of destination register
		uint32_t offset)
{
// #ifdef IODUMP
  printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
//   MEMPHY_dump(proc->mram, "ram.txt", "WRITE");
// #endif

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


  for(pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte= caller->mm->pgd[pagenum];

    if (!PAGING_PAGE_PRESENT(pte))
    {
      // ---------------------------------------------------
      //fpn = PAGING_FPN(pte);
      fpn = PAGING_PTE_FPN(pte); // update: macro to get FPN
      // ---------------------------------------------------
      MEMPHY_put_freefp(caller->mram, fpn);
    } else {
      // ---------------------------------------------------
      //fpn = PAGING_SWP(pte);
      fpn = PAGING_PTE_SWPOFF(pte) / PAGING_PAGESZ; // update: macro to get swap frame number
      // ---------------------------------------------------
      MEMPHY_put_freefp(caller->active_mswp, fpn);    
    }
  }

  return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct* get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, int size, int alignedsz)
{
  struct vm_rg_struct * newrg;
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  newrg = malloc(sizeof(struct vm_rg_struct));

  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end = newrg->rg_start + size;

  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart, int vmaend)
{
  //struct vm_area_struct *vma = caller->mm->mmap;

  // ---------------------------------------------------------------
  /* TODO validate the planned memory area is not overlapped */
  struct vm_area_struct* vma;
  struct vm_area_struct* cur_vma = get_vma_by_num(caller->mm, vmaid);

  for (vma = caller->mm->mmap; vma != NULL; vma = vma->vm_next)
  {
    if (vma == cur_vma)
      continue; // same vm area (do not check)
    if (vma->vm_start == vma->vm_end)
      continue; // empty vm area (do not check)

    if (OVERLAP(vma->vm_start, vma->vm_end, cur_vma->vm_start, (unsigned long) vmaend)) {
      printf("Invalid memory detected!: ");
      printf("[%ld - %ld] and [%ld - %d]\n\n", vma->vm_start, vma->vm_end, cur_vma->vm_start, vmaend);
      return -1;
    }
  }
  // ---------------------------------------------------------------
  return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size 
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz)
{
  #ifdef MEM_DEBUG
  printf("Increase VMA limit...\n");
  #endif

  struct vm_rg_struct * newrg = malloc(sizeof(struct vm_rg_struct));
  int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
  int incnumpage =  inc_amt / PAGING_PAGESZ;
  struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  int old_end = cur_vma->vm_end;

  /*Validate overlap of obtained region */
  if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0) {
    #ifdef MEM_DEBUG
    printf("Requested memory overlapped!\n");
    #endif
    return -1; /*Overlap and failed allocation */
  }

  #ifdef MEM_DEBUG  
  printf("Requested memory is valid, continue...\n");
  #endif

  /* The obtained vm area (only) 
   * now will be alloc real ram region */
  cur_vma->vm_end += inc_amt; // update: inc_sz (unaligned) --> inc_amt (aligned)
  if (vm_map_ram(caller, area->rg_start, area->rg_end, 
                    old_end, incnumpage , newrg) < 0)
    return -1; /* Map the memory to MEMRAM */

  return 0;
}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, int *retpgn) 
{
  struct pgn_t *pg = mm->fifo_pgn;

  /* TODO: Implement the theorical mechanism to find the victim page */

  // FIFO page replacement
  
  // get last element of FIFO queue
  if (pg == NULL) {// no page in FIFO queue
    return -1; 
  }
  if (pg->pg_next == NULL) // only 1 page in FIFO queue
  {
    *retpgn = pg->pgn;
    mm->fifo_pgn = NULL;
    free(pg); // remove element
  }
  else
  {
    while (pg->pg_next->pg_next != NULL)
      pg = pg->pg_next;

    *retpgn = pg->pg_next->pgn;
    free(pg->pg_next); // remove last element
    pg->pg_next = NULL;
  }

  //free(pg);
  #ifdef MEM_DEBUG
  printf("For victim page, I choose you: page %d\n", *retpgn);
  #endif

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

  /* Traverse on list of free vm region to find a fit space */
  while (rgit != NULL)
  {
    if (rgit->rg_start + size <= rgit->rg_end)
    { /* Current region has enough space */
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;

      /* Update left space in chosen region */
      if (rgit->rg_start + size < rgit->rg_end)
      {
        rgit->rg_start = rgit->rg_start + size;
      }
      else
      { /*Use up all space, remove current node */
        /*Clone next rg node */
        struct vm_rg_struct *nextrg = rgit->rg_next;

        /*Cloning */
        if (nextrg != NULL)
        {
          rgit->rg_start = nextrg->rg_start;
          rgit->rg_end = nextrg->rg_end;

          rgit->rg_next = nextrg->rg_next;

          free(nextrg);
        }
        else
        { /*End of free list */
          rgit->rg_start = rgit->rg_end;	//dummy, size 0 region
          rgit->rg_next = NULL;
        }
      }
      return 0; // update: return after newrg found
    }
    else
    {
      rgit = rgit->rg_next;	// Traverse next rg
    }
  }

 if(newrg->rg_start == -1) // new region not found
   return -1;

 return 0;
}

//#endif
