#include <stdint.h>
#include "devtree.h"
#include "debug.h"


struct Fdtheader
{
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
    uint32_t tokens[];
};

struct Fdtmemreservation
{
    uintptr_t addr;
    uint64_t size;
};


enum {
	FDT_BEGIN_NODE = 1,
	FDT_END_NODE =2,
	FDT_PROP = 3,
	FDT_NOP = 4,
	FDT_END = 9,
};

void
storefdt(char *dest, char *base) {
	Fdtheader *fdt = (Fdtheader *)base;
	uint32_t size = betole32(fdt->totalsize);
	if (size > 0x2000) {
		printstring("ftd too large for storage copy\n"); // Should allocate dynamically, but we don't have mappings yet.  Double the size of this space (in hello.s) and recompile.
		return;
	}
	for(int i=0; i < size; i++) {
		dest[i] = base[i];
	}
}

static uint32_t
mstrlen(char *s) {
	int n = 0;
	while(*s++ != 0)
		n++;
	return n;
}

static int
mstrcmp(char *l, char *r) {
   while(*l && (*l == *r))
    {
        l++;
        r++;
    }
    return *l - *r;
}

/* returns pointer to the null at the end of the destination */
static char *
mstrcpy(char *d, char *s) {
	while(*s) {
		*d++ = *s++;
	}
	*d = 0;
	return d;
}


void
parsefdtheader(char *addr, Fdtheader *dest) {
	uint32_t *src = (uint32_t *)addr;
	uint32_t *end = src + (sizeof(Fdtheader)/4);
	uint32_t *dst = (uint32_t*)dest;
	while(src != end) {
		uint32_t v = *src++;
		*dst++ = betole32(v);
	}
}

void
indent(int n) {
	for(int i = 0;i < n; i++) {
		sbiprint(1, "\t");
	}
}

void
printfdt(char *base) {
	Fdtheader fdt;
	parsefdtheader(base, &fdt);
	struct Fdtmemreservation *rebegin = (Fdtmemreservation *)(base + fdt.off_mem_rsvmap);
	struct Fdtmemreservation *reend = (Fdtmemreservation *)(base + fdt.off_dt_struct);

	printstring("Reservations:\n\t");
	for(Fdtmemreservation *p = rebegin; p < reend; p++) {
		printhex64((uintptr_t)p->addr);
		sbiprint(1,":");
		printhex64(p->size);
		sbiprint(2,"\n\t");
	}

	printstring("\nStructure block:\n");

	int depth = 0;
	uint32_t *st_begin = (uint32_t *)(base + fdt.off_dt_struct);
	uint32_t *st_end = (uint32_t *)(base + fdt.off_dt_struct + fdt.size_dt_struct);
	for(uint32_t *tok = st_begin; tok < st_end; tok++) {
		switch(betole32(*tok)) {
		case FDT_BEGIN_NODE: 
			int n = mstrlen((char*)(tok+1));
			indent(depth);
			printstring((char*)(tok+1));
			printstring("{\n"); 
			tok += (n+3)/4;
			depth++;
			break;
		case FDT_END_NODE:
			depth--;
			indent(depth);
			printstring("}\n");
			break;
		case FDT_PROP:
			uint32_t len = betole32(tok[1]);
			uint32_t s = betole32(tok[2]);
			indent(depth);
			printstring(base+fdt.off_dt_strings + s);
			printstring("(");
			printhex32(len);
			printstring(") : ");
			for(int i=0; i <  (len+3)/4 ; i++) {
				printhex32(betole32(tok[i+3])); printstring(" ");
			}
			tok += 2;
			tok += (len+3)/4;
			printstring("\n");
			break;
		case FDT_NOP:
			break;
		case FDT_END:
			break;
		}
	}
}

void
fail(char *s){
	printstring(s);
	for(;;)
		;
}

Fdtprop *
fdtfindprop(Fdtparserstate *ps, int level, char *name){
	int i;
	for(i=0; i < ps->nprops[level]; i++){
		if(mstrcmp(name, ps->props[level][i].name) == 0)
			return &ps->props[level][i];
	}
	return 0;		
}

static int
recurse(uint32_t *st_begin, char *strtab, Fdtparserstate *ps) {
	for(uint32_t *tok = st_begin; ; tok++) {
		switch(betole32(*tok)) {
		case FDT_BEGIN_NODE: {
			char *name = (char*)(tok+1);
			int n = mstrlen(name);
			tok += (n+3)/4;
			ps->stack[ps->stacksize+1] = 
				mstrcpy(ps->stack[ps->stacksize], name);
			ps->stacksize++;
			if(ps->stacksize >= FDTMAXDEPTH){
				fail("FDTMAXDEPTH exceeded\n");
			}
			ps->stack[ps->stacksize][0] = '/';
			ps->stack[ps->stacksize][1] = 0;
			ps->stack[ps->stacksize]++;
			ps->nprops[ps->stacksize]=0;
			tok += recurse(++tok, strtab, ps);
			break;
			}
		case FDT_END_NODE:
			ps->onprop(ps);
			ps->stack[--ps->stacksize][0] = 0;
			ps->nprops[ps->stacksize] = 0;
			return tok-st_begin;
			break;
		case FDT_PROP: {
			uint32_t len = betole32(tok[1]);
			uint32_t s = betole32(tok[2]);
			int pidx = ps->stacksize - 1;
			ps->props[pidx][ps->nprops[pidx]].name = strtab + s;
			ps->props[pidx][ps->nprops[pidx]].len = len;
			ps->props[pidx][ps->nprops[pidx]].value = (char *)(tok+3);
			ps->nprops[pidx]++;
			if(ps->nprops[pidx] >= FDTMAXPROPS){
				fail("FDTMAXPROPS exceeded\n");
			}
			tok += 2;
			tok += (len+3)/4;
			break;
			}
		case FDT_NOP:
			break;
		case FDT_END:
			return tok-st_begin;
		}
	}
}

/* Return -1 on error */
int
parsefdt(char *base, fdtpropcb onprop){
	Fdtheader fdt;
	parsefdtheader(base, &fdt);
	int depth = 0;
	uint32_t *st_begin = (uint32_t *)(base + fdt.off_dt_struct);

	Fdtparserstate ps;

	ps.stacksize = 0;
	ps.stack[0] = ps.path;
	ps.path[0] = 0;
	ps.onprop = onprop;

	int ntok = recurse(st_begin, base+fdt.off_dt_strings, &ps);
	if(ntok != fdt.size_dt_struct/4)
		return -1;
	return 0;
}

