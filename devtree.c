#include <stdint.h>
#include "devtree.h"
#include "debug.h"

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

static int
strhasprefix(char *s, char *prefix) {
   while(*s && (*s == *prefix))
    {
        s++;
        prefix++;
    }
    return *prefix == 0;
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


Fdtheader *
parsefdtheader(uint32_t *addr, Fdtheader *dest) {
	uint32_t *src = addr;
	uint32_t *end = addr + (sizeof(Fdtheader)/4);
	uint32_t *dst = (uint32_t*)dest;
	while(src != end) {
		uint32_t v = *src++;
		*dst++ = betole32(v);
	}
	return dest;
}

void
indent(int n) {
	for(int i = 0;i < n; i++) {
		sbiprint(1, "\t");
	}
}

Fdtmemreservation mem[16];
int nreservations= 0;

struct fdtparsestate {
	char *stack[16]; // Position of last separator
	int stacksize;
	char path[1024];
} parsestate;

void
printFdt(Fdtheader *fdt, char *base) {
	struct Fdtmemreservation *rebegin = (Fdtmemreservation *)(base + fdt->off_mem_rsvmap);
	struct Fdtmemreservation *reend = (Fdtmemreservation *)(base + fdt->off_dt_struct);

	printstring("Reservations:\n\t");
	for(Fdtmemreservation *p = rebegin; p < reend; p++) {
		printhex64((uintptr_t)p->addr);
		sbiprint(1,":");
		printhex64(p->size);
		sbiprint(2,"\n\t");
	}

	printstring("\nStructure block:\n");

	int depth = 0;
	uint32_t *st_begin = (uint32_t *)(base + fdt->off_dt_struct);
	uint32_t *st_end = (uint32_t *)(base + fdt->off_dt_struct + fdt->size_dt_struct);
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
			printstring(base+fdt->off_dt_strings + s);
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

static int
recurse(uint32_t *st_begin, char *strtab, struct fdtparsestate *parsestate) {
	for(uint32_t *tok = st_begin; ; tok++) {
		switch(betole32(*tok)) {
		case FDT_BEGIN_NODE: {
			char *name = (char*)(tok+1);
			int n = mstrlen(name);
			tok += (n+3)/4;
			parsestate->stack[parsestate->stacksize+1] = mstrcpy(parsestate->stack[parsestate->stacksize], name);
			parsestate->stacksize++;
			parsestate->stack[parsestate->stacksize][0] = '/';
			parsestate->stack[parsestate->stacksize][1] = 0;
			parsestate->stack[parsestate->stacksize]++;
			tok += recurse(++tok, strtab, parsestate);
			break;
			}
		case FDT_END_NODE:
			parsestate->stack[--parsestate->stacksize][0] = 0;
			return tok-st_begin;
			break;
		case FDT_PROP: {
			uint32_t len = betole32(tok[1]);
			uint32_t s = betole32(tok[2]);
			if (parsestate->stacksize >= 2
				&& strhasprefix(parsestate->stack[1], "reserved-memory")
				&& strhasprefix(parsestate->stack[2], "mmode")) {
				if (mstrcmp(strtab + s, "reg") == 0) {
					uintptr_t p;
					p  = ((uint64_t)(betole32(tok[0+3])) << 32) | (uint64_t)betole32(tok[1+3]);
					mem[nreservations].addr = p;
					p  = ((uint64_t)(betole32(tok[2+3])) << 32) | (uint64_t)betole32(tok[3+3]);
					mem[nreservations].size = p;
				} else if (mstrcmp(strtab + s, "no-map") == 0) {
					// Only commit the reservation if it's there
					printstring("Committing reservation ");
					printstring(parsestate->path);
					printstring(" at addr=0x");
					printhex64( (uint64_t)mem[nreservations].addr);
					printstring(" len=0x");
					printhex64( mem[nreservations].size);
					printstring("\n");
					nreservations++;
					if (nreservations > sizeof(mem)/sizeof(mem[0])) {
						printstring("OVERFLOW ON MEMORY RESERVATIONS\n");
					}
				} else if (mstrcmp(strtab + s, "size") == 0) {
					printstring("IGNORING DYNAMIC ALLOC MEMORY RESERVATION\n");
				}
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
descendfdtstructure(Fdtheader *fdt, char *base) {
	int depth = 0;
	uint32_t *st_begin = (uint32_t *)(base + fdt->off_dt_struct);

	nreservations = 0;
	parsestate.stacksize = 0;
	parsestate.stack[0] = parsestate.path;
	parsestate.path[0] = 0;
	int ntok = recurse(st_begin, base+fdt->off_dt_strings, &parsestate);
	if(ntok != fdt->size_dt_struct/4)
		return -1;
	return 0;
}

