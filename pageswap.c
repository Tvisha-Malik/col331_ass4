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
    v_proc->rss--;// as its page is swapped out
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
    //rss proc set in swap out
    kfree((char *)va);// frees the page in memory but the rrs has already been decreased so no need to decrease here
}
void disk_read(uint dev, char *page, int block){
    struct buf* buffer;
    int page_block;
    int part_block;
    //512 size
    for(int i=0;i<8;i++){
        part_block=512*i;
        page_block = block+i;
        buffer = bread(dev,page_block);
        memmove(page+part_block,buffer->data,512);
        brelse(buffer);
    }
}
void swap_in_page(){
    uint vpage = rcr2();
    struct proc* p=myproc();
    /* from vm.c*/
    pte_t*  pgdir_adr =  walkpgdir(p->pgdir, (char*) vpage,0);
    if(!pgdir_adr || !(*pgdir_adr & PTE_P)){
        panic("Invalid page falut");
        return;
    }
    uint block_id = (*pgdir_adr>>12);
    char* phy_page = kalloc();
    if(phy_page==0){
        panic("Failed to allocate memory for swapped in page");
        return;
    }
    disk_read(ROOTDEV,phy_page,(int)block_id);
    int cal_slot = (block_id-2)/8;
    struct swap_slot get_slot = swap_array[cal_slot];
    *pgdir_adr |= get_slot.page_perm;
    p->rss +=PGSIZE;
    *pgdir_adr |=(*phy_page & 0xFFFFF000);
    *pgdir_adr |= PTE_P;
    swapfree(ROOTDEV,block_id);
}



