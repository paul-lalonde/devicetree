typedef struct Fdtheader Fdtheader;
typedef struct Fdtmemreservation Fdtmemreservation;
typedef struct Fdtparserstate Fdtparserstate;

typedef void (*fdtpropcb)(Fdtparserstate*, char *prop, int datalen, char *data);

struct Fdtparserstate
{
	char *stack[16]; // Position of last separator
	int stacksize;
	char path[1024];
	fdtpropcb onprop;
};

uint32_t betole32(uint32_t v);
void printfdt(char *base);
void storefdt(char *dest, char *src);
int parsefdt(char *fdt, fdtpropcb onprop);
