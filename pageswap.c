// a function to find the victim process (in proc.c)
// a function to find the victim page (in vm.c)
// a function to set 10% of the accessed pages as unaccesed (in proc.c)
// a function to swap out the page , we have the victim page, not swap it out (here)
// a function to swap in a page
#include "param.h"
#include "types.h"
#include "defs.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "x86.h"

void swap_out(void)
{
    struct proc* v_proc= victim_proc();
    struct victim_page v_page= find_victim_page(v_proc->pgdir);
    if(v_page.available==0)
    {
        unaccessed();
        v_page= find_victim_page(v_proc->pgdir);
    }
    // uint block= swap_block();
    // int dev = swap_block_dev();
    swap_out_page(v_page, 0,0);
}

void swap_out_page(struct victim_page vp, uint blockno, int dev)
{
    uint va=vp.va_start;
    for(int i=0; i<8; i++, va+= BSIZE)
    {
        struct buf *to = bread(dev, blockno+i);
         memmove(to->data, (char *)va, BSIZE);
        bwrite(to);
         brelse(to);
    }
    *vp.pt_entry=((blockno<< 12)|PTE_FLAGS(*vp.pt_entry)|PTE_SO)&(~PTE_P);// setting the top 20 bits as the block number, setting the present bit as unset and the swapped out bit as set
    invlpg((void*)va);// invalidating the tlb entry
    kfree((char *)va);// frees the page in memory
}


