#include "hd.h"

int InitInt10(hd_data_t *, int);
int CallInt10(int *ax, int *bx, int *cx, unsigned char *buf, int len, int cpuemu);
int CallInt13(int *ax, int *bx, int *cx, int *dx, unsigned char *buf, int len, int cpuemu);
void FreeInt10(void);
