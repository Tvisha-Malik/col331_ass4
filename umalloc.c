#include "types.h"
#include "stat.h"
#include "user.h"
#include "param.h"

// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.

typedef long Align;

union header {
  struct {
    union header *ptr;
    uint size;
  } s;
  Align x;
};

typedef union header Header;

static Header base;
static Header *freep;

void
free(void *ap)
{
  Header *bp, *p;

  bp = (Header*)ap - 1;
  for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;
  if(bp + bp->s.size == p->s.ptr){
    bp->s.size += p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  } else
    bp->s.ptr = p->s.ptr;
  if(p + p->s.size == bp){
    p->s.size += bp->s.size;
    p->s.ptr = bp->s.ptr;
  } else
    p->s.ptr = bp;
  freep = p;
}

static Header*
morecore(uint nu)
{
  char *p;
  Header *hp;

  if(nu < 4096)
    nu = 4096;
  printf(1,"sbrk called \n");
  p = sbrk(nu * sizeof(Header));
  if(p == (char*)-1)
    return 0;
  hp = (Header*)p;
  hp->s.size = nu;
  free((void*)(hp + 1));
  return freep;
}

void*
malloc(uint nbytes)
{
  Header *p, *prevp;
  uint nunits;
  printf(1,"here in malloc 1 \n");
  nunits = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;
  if((prevp = freep) == 0){
    printf(1,"here in malloc 10 \n");
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }

  Header *p1, *prevp1;
  prevp1 = prevp;
  int i = 0;
  for (p1 = prevp1->s.ptr; i < 10; prevp1 = p1, p1 = p1->s.ptr, i++)
  {
    //  printf(1,"current p1 %d\n", p1);
    //   printf(1,"current freep %d\n", freep);
    if (p1 == freep)
    {
      // printf(1,"found freep\n");
      break;
    }
  }
  //  printf(1,"here in malloc 2 \n");
  for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
    if(p->s.size >= nunits){
        // printf(1,"selected p %d\n", p1);
      if(p->s.size == nunits)
        prevp->s.ptr = p->s.ptr;
      else {
        p->s.size -= nunits;
        p += p->s.size;
        p->s.size = nunits;
      }
      freep = prevp;
      // printf(1,"here in malloc 3 \n");
      return (void*)(p + 1);
    }

    if (p == freep)
    {
      // printf(1,"here in malloc 6 \n");
      if ((p = morecore(nunits)) == 0)
      {
        // printf(1,"here in malloc 5 \n");
        return 0;
      }
      //  printf(1,"after morecore \n");
    }
    //  printf(1,"here in malloc 7 \n");
  }
  printf(1, "here in malloc 4 \n");
}
