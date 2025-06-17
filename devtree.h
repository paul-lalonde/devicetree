typedef struct Fdtheader Fdtheader;
typedef struct Fdtprop Fdtprop;
typedef struct Fdtmemreservation Fdtmemreservation;
typedef struct Fdtparserstate Fdtparserstate;

typedef void (*fdtpropcb)(Fdtparserstate*);

struct Fdtprop {
	char *name;
	char *value;
	int len;
};

#define FDTMAXPROPS 16
#define FDTMAXPATH 1024
#define FDTMAXDEPTH 16
struct Fdtparserstate
{
	char *stack[FDTMAXDEPTH]; // Position of last separator
	int stacksize;
	char path[FDTMAXPATH];
	Fdtprop props[FDTMAXDEPTH][FDTMAXPROPS]; 
	int nprops[FDTMAXDEPTH];
	fdtpropcb onprop;
};

uint32_t betole32(uint32_t v);
void printfdt(char *base);
void storefdt(char *dest, char *src);
int parsefdt(char *fdt, fdtpropcb onprop);
Fdtprop *fdtfindprop(Fdtparserstate *ps, int level, char *name);