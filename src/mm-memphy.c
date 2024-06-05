//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Memory physical module mm/mm-memphy.c
 */

#include "mm.h"
#include <stdlib.h>
#ifdef IODUMP
#include <stdio.h> // update: for file dump
#endif

/*
 *  MEMPHY_mv_csr - move MEMPHY cursor
 *  @mp: memphy struct
 *  @offset: offset
 */
int MEMPHY_mv_csr(struct memphy_struct *mp, int offset)
{
   pthread_mutex_lock(&mp->mutex);
   int numstep = 0;

   mp->cursor = 0;
   while(numstep < offset && numstep < mp->maxsz){
     /* Traverse sequentially */
     mp->cursor = (mp->cursor + 1) % mp->maxsz;
     numstep++;
   }
   pthread_mutex_unlock(&mp->mutex);

   return 0;
}

/*
 *  MEMPHY_seq_read - read MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int MEMPHY_seq_read(struct memphy_struct *mp, int addr, BYTE *value)
{
   if (mp == NULL) {
      pthread_mutex_unlock(&mp->mutex);
     return -1;
   }

   if (!mp->rdmflg) {
      pthread_mutex_unlock(&mp->mutex);
     return -1; /* Not compatible mode for sequential read */
   }

   MEMPHY_mv_csr(mp, addr);
   *value = (BYTE) mp->storage[addr];

   pthread_mutex_unlock(&mp->mutex);

   return 0;
}

/*
 *  MEMPHY_read read MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int MEMPHY_read(struct memphy_struct * mp, int addr, BYTE *value)
{
   if (mp == NULL)
     return -1;

   pthread_mutex_lock(&mp->mutex);

   if (mp->rdmflg)
      *value = mp->storage[addr];
   else /* Sequential access device */
      return MEMPHY_seq_read(mp, addr, value);

   pthread_mutex_unlock(&mp->mutex);

   return 0;
}

/*
 *  MEMPHY_seq_write - write MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int MEMPHY_seq_write(struct memphy_struct * mp, int addr, BYTE value)
{

   if (mp == NULL) {
      pthread_mutex_unlock(&mp->mutex);
     return -1;
   }

   if (!mp->rdmflg) {
      pthread_mutex_unlock(&mp->mutex);
     return -1; /* Not compatible mode for sequential read */
   }

   MEMPHY_mv_csr(mp, addr);
   mp->storage[addr] = value;

   pthread_mutex_unlock(&mp->mutex);

   return 0;
}

/*
 *  MEMPHY_write-write MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int MEMPHY_write(struct memphy_struct * mp, int addr, BYTE data)
{
   if (mp == NULL)
     return -1;

   pthread_mutex_lock(&mp->mutex);

   if (mp->rdmflg) {
      mp->storage[addr] = data;
   }
   else /* Sequential access device */
      return MEMPHY_seq_write(mp, addr, data);

   pthread_mutex_unlock(&mp->mutex);

   return 0;
}

/*
 *  MEMPHY_format-format MEMPHY device
 *  @mp: memphy struct
 */
int MEMPHY_format(struct memphy_struct *mp, int pagesz)
{
   pthread_mutex_lock(&mp->mutex);

    /* This setting come with fixed constant PAGESZ */
    int numfp = mp->maxsz / pagesz;
    struct framephy_struct *newfst, *fst;
    int iter = 0;

    if (numfp <= 0) {
      pthread_mutex_unlock(&mp->mutex);
      return -1;
    }
    
    /* Init head of free framephy list */ 
    fst = malloc(sizeof(struct framephy_struct));
    fst->fpn = iter;
    mp->free_fp_list = fst;

    /* We have list with first element, fill in the rest num-1 element member*/
    for (iter = 1; iter < numfp ; iter++)
    {
       newfst =  malloc(sizeof(struct framephy_struct));
       newfst->fpn = iter;
       newfst->fp_next = NULL;
       fst->fp_next = newfst;
       fst = newfst;
    }
    pthread_mutex_unlock(&mp->mutex);

    return 0;
}

int MEMPHY_get_freefp(struct memphy_struct *mp, int *retfpn)
{
   pthread_mutex_lock(&mp->mutex);

   struct framephy_struct *fp = mp->free_fp_list;

   if (fp == NULL) {
      pthread_mutex_unlock(&mp->mutex);
     return -1;
   }

   *retfpn = fp->fpn;
   mp->free_fp_list = fp->fp_next;

   /* MEMPHY is iteratively used up until its exhausted
    * No garbage collector acting then it not been released
    */
   free(fp);

   pthread_mutex_unlock(&mp->mutex);

   return 0;
}

// --------------------------------------------------------------
int MEMPHY_dump(struct memphy_struct * mp, char* filename, char* note) // update: add 2 params filename, note
{
   /*TODO dump memphy contnt mp->storage 
   *     for tracing the memory content
   */

   #ifdef IODUMP
   FILE* fp = fopen(filename, "a");

   if (fp == NULL)
   {
      perror("Unable to open the file");
      return -1;
   }
 
   fprintf(fp, "\n\n--%s--\n\n", note);

   pthread_mutex_lock(&mp->mutex);
   int mp_too_big = 0;
   if (mp->maxsz >= PAGING_PAGESZ * 16)
      mp_too_big = 1;

   // memory map format - blocks with size of PAGE/FRAME SIZE
   //fprintf(fp, "| ");
   unsigned long numpage = mp->maxsz / PAGING_PAGESZ;
   for (unsigned long pgit = 0; pgit < numpage; ++pgit)
   {
      fprintf(fp, "[Frame %ld]\t", pgit);
      unsigned long baseaddr = pgit*PAGING_PAGESZ;
      unsigned long nextbaseaddr = (pgit+1)*PAGING_PAGESZ;
      fprintf(fp, "[0x%7lX - 0x%7lX]:", baseaddr, nextbaseaddr - 1);

      // if mp is too big --> collapse empty frame
      if (mp_too_big)
      {
         int frame_empty = 1;
         // check if current frame is empty
         for (unsigned long pgoffset = 0; pgoffset < PAGING_PAGESZ; ++pgoffset)
         {
            BYTE* data = mp->storage;
            unsigned long addr = baseaddr + pgoffset;

            if (data[addr] != MEMORY_NULL_DATA)
            {
               frame_empty = 0;
               break;
            }
         }
         if (frame_empty) {
            fprintf(fp, " EMPTY\n");
            continue; // if empty -> do not print anything
         }
      }
      
      // table format (4x16x4)
      fprintf(fp, "\n| ");
      for (unsigned long pgoffset = 0; pgoffset < PAGING_PAGESZ; ++pgoffset)
      {  
         BYTE* data = mp->storage;
         unsigned long addr = baseaddr + pgoffset;

         if (data[addr] == MEMORY_NULL_DATA)
            fprintf(fp, "_");
         else
            fprintf(fp, "%c", data[addr]);

         if ((pgoffset + 1) % 4 == 0)
            fprintf(fp, " ");

         if ((pgoffset + 1) % 16 == 0)
            fprintf(fp, " | ");

         if ((pgoffset + 1) % 64 == 0) 
         {
            fprintf(fp, "\n");
            if ((pgoffset + 1) != PAGING_PAGESZ)
               fprintf(fp, "| ");
         }
      }
   }

   fclose(fp);
   pthread_mutex_unlock(&mp->mutex);
   #endif

   return 0;
}
// --------------------------------------------------------------

int MEMPHY_put_freefp(struct memphy_struct *mp, int fpn)
{
   pthread_mutex_lock(&mp->mutex);
   struct framephy_struct *fp = mp->free_fp_list;
   struct framephy_struct *newnode = malloc(sizeof(struct framephy_struct));

   /* Create new node with value fpn */
   newnode->fpn = fpn;
   newnode->fp_next = fp;
   mp->free_fp_list = newnode;
   pthread_mutex_unlock(&mp->mutex);

   return 0;
}


/*
 *  Init MEMPHY struct
 */
int init_memphy(struct memphy_struct *mp, int max_size, int randomflg)
{
   mp->storage = (BYTE *)malloc(max_size*sizeof(BYTE));
   mp->maxsz = max_size;

   MEMPHY_format(mp,PAGING_PAGESZ);

   mp->rdmflg = (randomflg != 0)?1:0;

   if (!mp->rdmflg )   /* Not Ramdom acess device, then it serial device*/
      mp->cursor = 0;

   pthread_mutex_init(&mp->mutex, NULL);

   return 0;
}

//#endif
