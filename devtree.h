typedef struct Fdtheader Fdtheader;
typedef struct Fdtmemreservation Fdtmemreservation;

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

uint32_t betole32(uint32_t v);
Fdtheader *parsefdtheader(uint32_t *addr, Fdtheader *dest);
void printFdt(Fdtheader *fdt, char *base);
int descendfdtstructure(Fdtheader *fdt, char *base);
