#include <stdint.h>
struct fdt_header {
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
};

struct fdt_reserve_entry {
    uint64_t address;
    uint64_t size;
};

enum {
	FDT_BEGIN_NODE = 1,
	FDT_END_NODE =2,
	FDT_PROP = 3,
	FDT_NOP = 4,
	FDT_END = 9,
};


struct fdt_header fdt_header = {1,0,0,0,0,0,0,0,0,0};

extern void asm_print(int l, char *s);

uint32_t
mstrlen(char *s) {
	int n = 0;
	while(*s++ != 0)
		n++;
	return n;
}
void
printString(char *s) {
	asm_print(mstrlen(s),s);
}

struct sbiret { long error, value; };

struct sbiret sbi_ecall(int ext, int fid, unsigned long arg0,
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

uint32_t betole(uint32_t v) {
	return ((v & 0xff) << 24) | ((v & 0xff00) << 8) | ((v & 0xff0000)>>8) | ((v & 0xff000000)>>24);
}

struct fdt_header *
parseFdtHeader(uint32_t *addr) {
	printString("Hello\n");
	uint32_t *src = addr;
	uint32_t *end = addr + (sizeof(struct fdt_header)/4);
	uint32_t *dst = (uint32_t*)&fdt_header;
	while(src != end) {
		uint32_t v = *src++;
		*dst++ = betole(v);
	}
	return &fdt_header;
}

char digits[16] = "0123456789abcdef";

void
printHex32(uint32_t val) {
	char buf[8];
	for(int i=7; i>=0;i--) {
		buf[i] = digits[val & 0xf];
		val = val >> 4;
	}
	
	asm_print(8, buf);
}
void
printHex64(uint64_t val) {
	char buf[16];
	for(int i=15; i>=0;i--) {
		buf[i] = digits[val & 0xf];
		val = val >> 4;
	}
	
	asm_print(16, buf);
}

void
indent(int n) {
	for(int i = 0;i < n; i++) {
		asm_print(1, "\t");
	}
}

void
printFdt(struct fdt_header *fdt, char *base) {
	struct fdt_reserve_entry *rebegin = (struct fdt_reserve_entry *)(base + fdt->off_mem_rsvmap);
	struct fdt_reserve_entry *reend = (struct fdt_reserve_entry *)(base + fdt->off_dt_struct);

	printString("Reservations:\n\t");
	for(struct fdt_reserve_entry *p = rebegin; p < reend; p++) {
		printHex64(p->address);
		asm_print(1,":");
		printHex64(p->size);
		asm_print(2,"\n\t");
	}

	printString("\nStructure block:\n");

	int depth = 0;
	uint32_t *st_begin = (uint32_t *)(base + fdt->off_dt_struct);
	uint32_t *st_end = (uint32_t *)(base + fdt->off_dt_struct + fdt->size_dt_struct);
	for(uint32_t *tok = st_begin; tok < st_end; tok++) {
		switch(betole(*tok)) {
		case FDT_BEGIN_NODE: 
			tok++;
			int n = mstrlen((char*)tok);
			indent(depth);
			printString((char*)tok);
			printString("{\n"); 
			tok += (n+3)/4;
			depth++;
			break;
		case FDT_END_NODE:
			depth--;
			indent(depth);
			printString("}\n");
			break;
		case FDT_PROP:
			uint32_t len = betole(tok[1]);
			uint32_t s = betole(tok[2]);
			indent(depth);
			printString(base+fdt->off_dt_strings + s);
			printString("(");
			printHex32(len);
			printString(") : ");
			for(int i=0; i <  (len+3)/4 ; i++) {
				printHex32(betole(tok[i+3])); printString(" ");
			}
			tok += 2;
			tok += (len+3)/4;
			printString("\n");
			break;
		case FDT_NOP:
			break;
		case FDT_END:
			break;
		}
	}
	
}

void hexdump(uint32_t *a, int len) {
	printString("00000000: ");
	for(int i=0;i < len/4; i++) {
		printHex32(a[i]);
		if (i % 8 == 7) {
			printString("\n");
			printHex32(4*(i + 1));
			printString(": ");
		} else {
			printString(" ");
		}
	}
	printString("\n");
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
printString("VPN(0x0000000, 2)=");
printHex32(VPN(0x0000000, 2));
printString("\nVPN(0x8000000, 2)=");
printHex32(VPN(0x8000000, 2));
	pt[VPN(0x00000000, 2)] = 0x1 | (((uintptr_t)(pt)+4096) >> 2); 
	pt[VPN(0x80000000, 2)] = 0x1 | (((uintptr_t)(pt)+4096) >> 2); 

printString("\nVPN(0x00000000, 1)=");
printHex32(VPN(0x00000000, 1));
printString("\nVPN(0x80000000, 1)=");
printHex32(VPN(0x80000000, 1));
printString("\nVPN(0x80200000, 1)=");
printHex32(VPN(0x80200000, 1));
	pt[512+VPN(0x00000000, 1)] = 0xF | (0x80000000 >> 2); // phyiscal page
	pt[512+VPN(0x80000000, 1)] = 0xF | (0x80000000 >> 2); // phyiscal page
	pt[512+VPN(0x80200000, 1)] = 0xF | (0x80200000 >> 2); // phyiscal page
	
	// We should now have a page table that points to exactly one 1M page at 0x8020000, at VA 0.
		
}

extern uint64_t pt[];

void 
pre_main(uint32_t *addr) {

	printHex32(0xDEADF00D);
	printString("\n");
	printString("Parsing header\n");
	parseFdtHeader(addr);
	hexdump(addr, fdt_header.totalsize);
	printFdt(&fdt_header, (char*)addr);
	printString("\n");

/*	mkpagetab0(pt);
	extern void mmuenable();
	mmuenable();
	printString("MMU Initialized\n");
*/}

void
main() {
	printString("MMU Initialized\n");
}