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
#include "memlayout.h"

#define SWAPSIZE (PGSIZE / BSIZE); // Size of one swap slot, 4096/512 = 8

// Global in-memory array storing metadata of swap slots
struct swap_slot swap_array[NSWAPSLOTS];

void swaparrayinit(int dev)
{
    for (int i = 0; i < NSWAPSLOTS; i++)
    {
        swap_array[i].is_free = 1;
        swap_array[i].start = SWAPSTART + i * SWAPSIZE;
        swap_array[i].dev = dev;
    }
};

struct swap_slot *swapalloc(void)
{
    for (int i = 0; i < NSWAPSLOTS; i++)
    {
        if (swap_array[i].is_free == 1)
        {                              // If the slot is free
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

void swapfree(int dev, int blockno)
{
    for (int i = 0; i < NSWAPSLOTS; i++)
    {
        struct swap_slot *sw = &swap_array[i];
        if (sw->start == blockno && sw->dev == dev)
        {
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
    struct proc *v_proc = victim_proc();
    // cprintf("after victim proc in swapout \n");
    pte_t *v_page = find_victim_page(v_proc->pgdir, v_proc->sz);
    // cprintf("before if in swapout \n");
    if (v_page == 0)
    {
        // unaccessed(); we only have to set 10% of the accessed entries as unaccesed for the victim process
        unacc_proc(v_proc->pgdir);
        v_page = find_victim_page(v_proc->pgdir, v_proc->sz);
    }
    // cprintf("after if in swapout \n");
    if (v_page == 0)
        panic("still cant find victim page \n");
    v_proc->rss -= PGSIZE; // as its page is swapped out
    struct swap_slot *slot = swapalloc();
    // cprintf("after swapalloc in swap out \n");
    swap_out_page(v_page, slot->start, slot->dev);
    lcr3(V2P(v_proc->pgdir));
}

void swap_out_page(pte_t *vp, uint blockno, int dev)
{
    // cprintf("vicitm page is %d \n", *vp);
    uint physicalAddress = PTE_ADDR(*vp);
    char *va = (char *)P2V(physicalAddress);
    struct buf *buffer;
    int ithPartOfPage = 0;
    for (int i = 0; i < 8; i++)
    {
        // cprintf("inside the swap out page loop %d \n", i);
        ithPartOfPage = i * BSIZE;
        buffer = bread(ROOTDEV, blockno + i);
        // struct buf *to = bread(dev, blockno+i);

        // memmove(to->data, (char *)va, BSIZE);
        // bwrite(to);
        //  brelse(to);
        memmove(buffer->data, va + ithPartOfPage, BSIZE); // write 512 bytes to the block
        bwrite(buffer);
        brelse(buffer);
    }
    *vp = ((blockno << 12) | PTE_FLAGS(*vp) | PTE_SO);
    *vp = *vp & (~PTE_P); // setting the top 20 bits as the block number, setting the present bit as unset and the swapped out bit as set
    // cprintf("after update in swap out  %d \n", *vp);
    // *vp.pt_entry = (*vp.pt_entry & 0x000000);
    // *vp.pt_entry = ((blockno << 12) | PTE_SO);
    // *vp.pt_entry = *vp.pt_entry & (~PTE_P);
    // invlpg((void *)va); // invalidating the tlb entry
    kfree(P2V(physicalAddress));

    // cprintf("printing va: %d\n", P2V(physicalAddress));
    // frees the page in memory but the rrs has already been decreased so no need to decrease here
}
void disk_read(uint dev, char *page, int block)
{
    struct buf *buffer;
    int page_block;
    int part_block;
    // 512 size
    for (int i = 0; i < 8; i++)
    {
        part_block = BSIZE * i;
        page_block = block + i;
        buffer = bread(dev, page_block);
        memmove(page + part_block, buffer->data, BSIZE);
        brelse(buffer);
    }
}

//  pte_t *
// walkpgdir2(pde_t *pgdir, const void *va)
// {
//   pde_t *pde;
//   pte_t *pgtab;

//   pde = &pgdir[PDX(va)];
//   if (!(*pde & PTE_P))
//   {
// 		panic("idk");
// 		return 0;
//   }
// 	pgtab = (pte_t *)P2V(PTE_ADDR(*pde));
//   return &pgtab[PTX(va)];
// }

void swap_in_page()
{
    // cprintf("inside swap in \n");

    struct proc *p = myproc();
    uint vpage = rcr2();
    
    /* from vm.c*/
    pte_t *pgdir_adr = walkpgdir(p->pgdir, (void *)vpage, 0);
    // cprintf("pgdir_adr: %d\n", pgdir_adr);
    // cprintf("after walk in swap in  %d \n", *pgdir_adr);
    if (!pgdir_adr)
    {
        panic("Invalid page fault zero");
        return;
    }
    if ((*pgdir_adr & PTE_P))
    {
        panic("Invalid page fault present");
        return;
    }
    uint block_id = (*pgdir_adr >> PTXSHIFT);
    char *phy_page = kalloc();
    if (phy_page == 0)
    {
        panic("Failed to allocate memory for swapped in page");
        return;
    }
    p->rss += PGSIZE;
    disk_read(ROOTDEV, phy_page, (int)block_id);

    // int cal_slot = (block_id-2)/8;
    // struct swap_slot get_slot = swap_array[cal_slot];
    // *pgdir_adr |= get_slot.page_perm;
    // *pgdir_adr |= V2P(phy_page);
    // *pgdir_adr |= PTE_P;
    *pgdir_adr = V2P(phy_page) | PTE_FLAGS(*pgdir_adr) | PTE_P;
    *pgdir_adr = *pgdir_adr & (~PTE_SO);
    // cprintf("after update in swap in  %d \n", *pgdir_adr);

    swapfree(ROOTDEV, block_id);
}
