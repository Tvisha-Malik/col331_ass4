// a function to find the victim process (in proc.c)
// a function to find the victim page (in vm.c)
// a function to set 10% of the accessed pages as unaccesed (in proc.c)
// a function to swap out the page , we have the victim page, not swap it out
// a function to swap in a page
#include "mmu.h"
#include "types.h"
#include "buf.h"
#include "defs.h"
#include "fs.h"

void swap_out(struct victim_page vp, uint blockno, int dev)
{
    uint va=vp.va_start;
    for(int i=0; i<8; i++, va+= BSIZE)
    {
        struct buf *to = bread(dev, blockno+i);
         memmove(to->data, va, BSIZE);
        bwrite(to);
         brelse(to);
    }
    *vp.pt_entry=((blockno<< 12)|PTE_FLAGS(*vp.pt_entry)|PTE_SO)&(~PTE_P);// setting the top 20 bits as the block number, setting the present bit as unset and the swapped out bit as set
    kfree(va);// frees the page in memory

}


