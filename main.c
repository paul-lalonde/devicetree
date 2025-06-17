#include <stdint.h>
#include "devtree.h"

extern char fdt_header[0x2000];

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

struct sbiret { long error, value; };

struct sbiret sbiecall(int ext, int fid, unsigned long arg0,
                        unsigned long arg1, unsigned long arg2,
                        unsigned long arg3, unsigned long arg4,
                        unsigned long arg5)
{
        struct sbiret ret;

        register uintptr_t a0 asm ("a0") = (uintptr_t)(arg0);
        register uintptr_t a1 asm ("a1") = (uintptr_t)(arg1);
        register uintptr_t a2 asm ("a2") = (uintptr_t)(arg2);
        register uintptr_t a3 asm ("a3") = (uintptr_t)(arg3);
        register uintptr_t a4 asm ("a4") = (uintptr_t)(arg4);
        register uintptr_t a5 asm ("a5") = (uintptr_t)(arg5);
        register uintptr_t a6 asm ("a6") = (uintptr_t)(fid);
        register uintptr_t a7 asm ("a7") = (uintptr_t)(ext);
        asm volatile ("ecall"
                      : "+r" (a0), "+r" (a1)
                      : "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
                      : "memory");
        ret.error = a0;
        ret.value = a1;

        return ret;
}

uint32_t beget4(char *s) {
	return ((uint32_t)(s[0]) << 24) | ((uint32_t)(s[1]) << 16) | ((uint32_t)(s[2]) << 8) | (uint32_t)s[0];
}

uint32_t betole32(uint32_t v) {
	return ((v & 0xff) << 24) | ((v & 0xff00) << 8) | ((v & 0xff0000)>>8) | ((v & 0xff000000)>>24);
}


char digits[16] = "0123456789abcdef";

void
sbiprint(int len, char *s){
	sbiecall(0x4442434E,0,len,(uintptr_t)s,0,0,0,0);
}

void
printstring(char *s) {
	sbiprint(mstrlen(s),s);
}

void
printhex32(uint32_t val) {
	char buf[8];
	for(int i=7; i>=0;i--) {
		buf[i] = digits[val & 0xf];
		val = val >> 4;
	}
	
	sbiprint(8, buf);
}

void
printhex64(uint64_t val) {
	char buf[16];
	for(int i=15; i>=0;i--) {
		buf[i] = digits[val & 0xf];
		val = val >> 4;
	}
	
	sbiprint(16, buf);
}

void hexdump(uint32_t *a, int len) {
	printstring("00000000: ");
	for(int i=0;i < len/4; i++) {
		printhex32(a[i]);
		if (i % 8 == 7) {
			printstring("\n");
			printhex32(4*(i + 1));
			printstring(": ");
		} else {
			printstring(" ");
		}
	}
	printstring("\n");
}

#define VPN(a, l) ((a >> (12 + 9 * l)) & 0x1FF)

void
mkpagetab0(uint64_t *pt) {
	// We clear two page tables, one for vpn2 and one for vpn1
	for(int i=0; i < 2 * 4096/sizeof(uint64_t); i++)
		pt[i] = 0; // Guaranteed invalid pte because bit 0 (valid) is clear.
	// Map start of memory in level 2
	// Map va 0 to our level 1 page table

	// We also map va 80200000 there so we have the correct next instruction.
	// We can remove these mappings after fixing our instruction pointer.
printstring("VPN(0x0000000, 2)=");
printhex32(VPN(0x0000000, 2));
printstring("\nVPN(0x8000000, 2)=");
printhex32(VPN(0x8000000, 2));
	pt[VPN(0x00000000, 2)] = 0x1 | (((uintptr_t)(pt)+4096) >> 2); 
	pt[VPN(0x80000000, 2)] = 0x1 | (((uintptr_t)(pt)+4096) >> 2); 

printstring("\nVPN(0x00000000, 1)=");
printhex32(VPN(0x00000000, 1));
printstring("\nVPN(0x80000000, 1)=");
printhex32(VPN(0x80000000, 1));
printstring("\nVPN(0x80200000, 1)=");
printhex32(VPN(0x80200000, 1));
	pt[512+VPN(0x00000000, 1)] = 0xF | (0x80000000 >> 2); // phyiscal page
	pt[512+VPN(0x80000000, 1)] = 0xF | (0x80000000 >> 2); // phyiscal page
	pt[512+VPN(0x80200000, 1)] = 0xF | (0x80200000 >> 2); // phyiscal page
	
	// We should now have a page table that points to exactly one 1M page at 0x8020000, at VA 0.
		
}


extern uint64_t pt[];

void 
pre_main(char *addr) {

	printhex32(0xDEADF00D);
	printstring("\n");
	//hexdump(addr, fdt_header.totalsize);
	storefdt(fdt_header, addr);
	printfdt(fdt_header);
	/* descendfdtstructure(&fdt_header, (char*)addr); */
	/* We now have our table of memory reservations */
	/* We'll later inject that into our memory reservation table */
/*	mkpagetab0(pt);
	extern void mmuenable();
	mmuenable();
	printstring("MMU Initialized\n");
*/}

static int
strhasprefix(char *s, char *prefix) {
   while(*s && (*s == *prefix))
    {
        s++;
        prefix++;
    }
    return *prefix == 0;
}

struct Fmemreservation
{
    uintptr_t addr;
    uint64_t size;
} mem[16];

int nreservations= 0;

void
onfdtprop(Fdtparserstate *parserstate, char *prop, int datalen, char *data){
	if (parserstate->stacksize >= 2
		&& strhasprefix(parserstate->stack[1], "reserved-memory")
		&& strhasprefix(parserstate->stack[2], "mmode")) {
		if (mstrcmp(prop, "reg") == 0) {
			uintptr_t p;
			p  = ((uint64_t)(beget4(data)) << 32) | (uint64_t)beget4(data+4);
			mem[nreservations].addr = p;
			p  = ((uint64_t)(beget4(data+8)) << 32) | (uint64_t)beget4(data+12);
			mem[nreservations].size = p;
		} else if (mstrcmp(prop, "no-map") == 0) {
			// Only commit the reservation if it's there
			printstring("Committing reservation ");
			printstring(parserstate->path);
			printstring(" at addr=0x");
			printhex64( (uint64_t)mem[nreservations].addr);
			printstring(" len=0x");
			printhex64( mem[nreservations].size);
			printstring("\n");
			nreservations++;
			if (nreservations > sizeof(mem)/sizeof(mem[0])) {
				printstring("OVERFLOW ON MEMORY RESERVATIONS\n");
			}
		} else if (mstrcmp(prop, "size") == 0) {
			printstring("IGNORING DYNAMIC ALLOC MEMORY RESERVATION\n");
		}
	}
}

void
main() {
	printstring("MMU Initialized - in mmu-mediated main().\n");

	nreservations = 0;
	parsefdt((char*)&fdt_header, onfdtprop);
}