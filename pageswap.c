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

#define SWAPSIZE (PGSIZE / BSIZE); // Size of one swap slot, 4096/512 = 8

// Global in-memory array storing metadata of swap slots
struct swap_slot swap_array[NSWAPSLOTS];


void swaparrayinit(int dev){
    for (int i = 0; i < NSWAPSLOTS; i++){
        swap_array[i].is_free = 1;
        swap_array[i].start = SWAPSTART + i * SWAPSIZE;
        swap_array[i].dev = dev;
    }
};

struct swap_slot* swapalloc(void){
    for (int i = 0; i < NSWAPSLOTS; i++){
        if (swap_array[i].is_free == 1){ // If the slot is free
            swap_array[i].is_free = 0; // Mark it as used
            return &swap_array[i];
        }
    }
    panic("swapalloc: out of swap slots");
};

// void swapfree(struct swap_slot* slot){
//     if (slot->is_free == 1)
//         panic("swapfree: slot already free");
//     slot->is_free = 1;
// };

void swapfree(int dev, int blockno){
    for (int i = 0; i < NSWAPSLOTS; i++){
        struct swap_slot* sw = &swap_array[i];
        if (sw->start == blockno && sw->dev == dev){
            if (sw->is_free == 1)
                panic("swapfree: slot already free");
            sw->is_free = 1;
            return;
        }
    }
    panic("swapfree: blockno not found");
};

void clean_slots(int pid)
{

     for (int i = 0; i < NSWAPSLOTS; i++){
        struct swap_slot* sw = &swap_array[i];
        if (sw->pid=pid && sw->is_free == 0){
            sw->is_free = 1;
            return;
        }
    }
}

void swap_out(void)
{
    struct proc* v_proc= victim_proc();
    struct victim_page v_page= find_victim_page(v_proc->pgdir);
    if(v_page.available==0)
    {
        //unaccessed(); we only have to set 10% of the accessed entries as unaccesed for the victim process
         unacc_proc(v_proc->pgdir);
        v_page= find_victim_page(v_proc->pgdir);
    }
    struct swap_slot* slot = swapalloc();
    swap_out_page(v_page, slot->start, slot->dev);
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


