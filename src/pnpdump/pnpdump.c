#define ABORT_ONRESERR
#define REALTIME
#define NEEDSETSCHEDULER

#include <stdio.h>

#  if defined __GLIBC__ && __GLIBC__ >= 2 && !defined(__powerpc__)
#    include <sys/io.h>
#  else
#    ifdef __alpha__
#       include <sys/io.h>
#    else
#       include <asm/io.h>
#    endif
#  endif												   /* __GLIBC__ */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>										   /* For strncpy */

#define TAG_DEBUG 0
#define DUMPADDR 0

#include "pnp.h"
#include "resource.h"
#include "hd.h"
#include "hd_int.h"
#include "isapnp.h"
#include "iopl.h"

#define NUM_CARDS 128
#define IDENT_LEN 9
#define TMP_LEN 16
#define LARGE_LEN 65536

#ifdef REALTIME
/*
 * Code to set scheduler to Realtime, Round-Robin, so usleeps right etc
 */
#if defined __GLIBC__ && __GLIBC__ >= 2
#  include <sched.h>
#  include <time.h>
#else
#  include <linux/unistd.h>
#  include <linux/sched.h>
#  include <sys/time.h>

#  ifdef NEEDSETSCHEDULER
_syscall3(int, sched_setscheduler, pid_t, pid, int, policy, struct sched_param *, sched_p)
#  endif
#  ifdef NEEDNANOSLEEP
_syscall2(int, nanosleep, struct timespec *, rqtp, struct timespec *, rmtp)
#  endif
#endif /* __GLIBC__ */

static int realtimeok = 0;
static struct sched_param s_parm = { 0 };
static int s_policy = 0;
static hd_data_t *hd_data;

static void set_sched_rr(void);
static void set_sched_normal(void);

#if 1
#undef printf
#undef fprintf
#undef perror
#undef putchar
#define printf(a...)
#define fprintf(a...)
#define perror(a)
#define putchar(a)
#endif

void set_sched_rr()
{
  struct sched_param sched_p = { 50 };

  if(
    sched_getparam(0, &s_parm) < 0 ||
    (s_policy = sched_getscheduler(0)) < 0 ||
    sched_setscheduler(0, SCHED_RR, &sched_p) < 0
  ) {
    ADD2LOG("pnpdump: couldn't set real-time scheduling, may be a bit slow (%d.%d)\n", SCHED_RR, sched_p.sched_priority);
  }
  else {
    ADD2LOG("pnpdump: real-time scheduling (%d.%d)\n", SCHED_RR, sched_p.sched_priority);
    realtimeok = 1;
  }
}


void set_sched_normal()
{
  if(!realtimeok) return;

  if(sched_setscheduler(0, s_policy, &s_parm) < 0) {
    ADD2LOG("pnpdump: couldn't change to normal scheduling (%d.%d)\n", s_policy, s_parm.sched_priority);
  }
  else {
    ADD2LOG("pnpdump: normal scheduling (%d.%d)\n", s_policy, s_parm.sched_priority);
    realtimeok = 0;
  }
}

/*
 * Use nanosleep for realtime delays
 */
void
delaynus(long del)
{
	struct timespec t;
	t.tv_sec = 0;
	t.tv_nsec = del * 1000;
	/*
	 * Need to handle case where binary for later kernel run on an earlier
	 * one, which doesn't support nanosleep (emergency backup spare !)
	 */
	if (realtimeok)
	{
		if (nanosleep(&t, (struct timespec *) 0) < 0)
		{
			perror("nanosleep failed");
			realtimeok = 0;
		}
	}
	else
		usleep(del);
}

#else
#define delaynus(x) usleep(x)
#undef NEEDSETSCHEDULER
#undef NEEDNANOSLEEP
#endif /* REALTIME */

#define ASSERT(condition)								   /* as nothing */

typedef void (*resource_handler)
     (FILE * output_file, struct resource * res, int selected);

static void read_board_resources(struct resource *first_element,
								 int dependent_clause,
								 struct resource **result_array_ptr,
								 int *count);

static void read_board(int csn, struct resource **res_list, int *list_len);

static void unread_resource_data(char prev);

/* Global state variables used for printing resource information. */
unsigned long serialno = 0;
char devid[8];											   /* Copy as devidstr
														    * returns pointer to
														    * static buffer subject
														    * to change */
int nestdepth;
int curid;
int curdma, depdma;
int curio, depio;
int curint, depint;
int curmem, depmem;
int starteddeps;
char *indep = "";

static isapnp_t *ip;
static isapnp_card_t *ic;

unsigned char serial_identifier[NUM_CARDS + 1][IDENT_LEN];
unsigned char csumdiff[NUM_CARDS + 1];

unsigned char tmp[TMP_LEN];
unsigned char large[LARGE_LEN];

int showmasks = 0;
int do_autoconfig = 0;
int do_fullreset = 0;

int csum = 0;
int backspaced = 0;
char backspaced_char;
int read_port = MIN_READ_ADDR;
int boards_found = 0;

int do_isolate(void);
void initiate(void);
void dumpdata(int);
void read_idents(void);

int do_dumpregs = 0;
int do_ignorecsum = 0;
void dumpregs(int);

char *devidstr(unsigned char, unsigned char, unsigned char, unsigned char);

/*
 * Single argument is initial readport
 * Two arguments is no-of-boards and the readport to use
 */
int pnpdump(hd_data_t *hd_data_loc, int read_boards)
{
	int i;
	struct resource *resources;
	int resource_count;
	int alloc_result;
	isapnp_t *p = hd_data_loc->isapnp;

	hd_data = hd_data_loc;

	do_dumpregs = 1;

	ip = p;
	ic = NULL;

#if 0
	case 'c': do_autoconfig = 1;
	case 'm': showmasks = 1;
	case 'r': do_fullreset = 1;
	case 'd': do_dumpregs = 1;
	case 'i': do_ignorecsum = 1;
#endif

	/* If a number of boards and read_port have been given, don't ISOLATE */
	boards_found = read_boards;

	/* Read decimal or hex number */
	read_port = ip->read_port ? ip->read_port | 3 : 0x203;
	if(read_port < MIN_READ_ADDR || read_port > MAX_READ_ADDR) read_port = 0x203;

#ifdef __alpha__
	/* ALPHA only has ioperm, apparently, so cover all with one permission */
	if (ioperm(MIN_READ_ADDR, WRITEDATA_ADDR - MIN_READ_ADDR + 1, 1))
#else
	/*
	 * Have to get unrestricted access to io ports, as WRITE_DATA port >
	 * 0x3ff
	 */
	if (acquire_pnp_io_privileges() != 0)
//	if (iopl(3) < 0)
#endif
	{
		perror("Unable to get io permission for WRITE_DATA");
		return 1;
	}
#ifdef REALTIME
	set_sched_rr();
#endif /* REALTIME */

	/* Have to do this anyway to check readport for clashes */
	PROGRESS(1, 0, "resources");
	alloc_system_resources();

	/* Now get board info */
	initiate();
	ip->read_port = read_port;
	resources = NULL;
	resource_count = 0;
	for (i = 1; i <= boards_found; i++)
	{
		PROGRESS(3, i, "add board");
		ic = add_isapnp_card(ip, i);
		read_board(i, &resources, &resource_count);
		memcpy(ic->serial, serial_identifier[i], 8);
		ic->ldev_regs = new_mem(sizeof *ic->ldev_regs * ic->log_devs);
		if(do_dumpregs)
			dumpregs(i);
	}
#ifdef REALTIME
	set_sched_normal();
#endif /* REALTIME */
	if (do_autoconfig)
	{
		alloc_result = alloc_resources(resources, resource_count, NULL, 0);
	}
	else
	{
		alloc_result = 0;
	}

#if 0
	/* Release resources */
#ifdef __alpha__
	ioperm(MIN_READ_ADDR, WRITEDATA_ADDR - MIN_READ_ADDR + 1, 0);
#else
	(void) iopl(0);
#endif
#endif
        relinquish_pnp_io_privileges();

	return 0;
}


/*
 * Wait for resource data byte
 *
 * Return true if error occurred (timeout)
 */
int
statuswait(void)
{
#ifdef REALTIME
#define TIMEOUTLOOPS 100
//        static int cnt = 0;
	int to;												   /* For timeout */
	/*
	 * Try for up to 1ms
	 */
//        PROGRESS(4, ++cnt, "statuswait");
	for (to = 0; to < TIMEOUTLOOPS; to++)
	{
		if (STATUS & 1)
			break;
		delaynus(10L);
	}
	if (to >= TIMEOUTLOOPS)
	{
//                PROGRESS(5, cnt, "timeout");
		fprintf(stderr, "Timeout attempting to read resource data - is READPORT correct ?\n");
		return 1;
	}
#else /* !REALTIME */
#if defined _OS2_ || defined __DJGPP__
#define TIMEOUTLOOPS 2
	int to;												   /* For timeout */
	/*
	 * Try for up to 2ms
	 */
	for (to = 0; to < TIMEOUTLOOPS; to++)
	{
		usleep(1000);
		if (STATUS & 1)
			break;
	}
	if (to >= TIMEOUTLOOPS)
	{
		fprintf(stderr, "Timeout attempting to read resource data - is READPORT correct ?\n");
		return (1);
	}
#else /* !(OS2 or DJGPP) */
	/*
	 * Infinite loop potentially, but if we usleep, we may lose 10ms
	 */
	while (!(STATUS & 1));

#endif /* !(OS2 or DJGPP) */
#endif /* !REALTIME */
	return 0;
}

int port;

void
sendkey(void)
{
	static char initdata[INIT_LENGTH] = INITDATA;
	int i;
	ADDRESS(0);
	ADDRESS(0);
	for (i = 0; i < INIT_LENGTH; i++)
	{
		ADDRESS(initdata[i]);
	}
}

void
initiate(void)
{
        int x = 20;		// ###### evil HACK: check just the 1st 20 addresses or so
	int i;
	char buf[32];

	if (!boards_found)
	{
		/* All cards now isolated, read the first one */
		for (port = read_port; port <= MAX_READ_ADDR && x--; port += READ_ADDR_STEP)
		{
		  sprintf(buf, "port 0x%03x", port);
		  PROGRESS(2, 20 - x, buf);
		  fprintf(stderr, ">>port 0x%03x\n", port);	// ######
			/* Check it doesn't clash with anything */
			if(!allocate_resource(IOport_TAG, port, 1, "readport"))
			{
				continue;
			}
			deallocate_resource(IOport_TAG, port, 1);
			/* Make sure all cards are in Wait for key */
			CONFIGCONTROL;
			WRITE_DATA(CONFIG_WAIT_FOR_KEY);
			/* Make them listen */
			delaynus(2000L);
			sendkey();
			/* Reset the cards */
			CONFIGCONTROL;
			WRITE_DATA(CONFIG_RESET_CSN);
			if(do_fullreset)
			{
				CONFIGCONTROL;
				WRITE_DATA(CONFIG_RESET);
				delaynus(2000L);
			}
			printf("# Trying port address %04x\n", port);
			if (do_isolate())
				break;
		}
		if (port > MAX_READ_ADDR)
		{
			printf("# No boards found\n");
			return;		// ######## was: exit(0)
		}
		printf("# Board %d has serial identifier", boards_found);
		for (i = IDENT_LEN; i--;)
			printf(" %02x", serial_identifier[boards_found][i]);
		printf("\n");
		while (do_isolate())
		{
			printf("# Board %d has serial identifier", boards_found);
			for (i = IDENT_LEN; i--;)
				printf(" %02x", serial_identifier[boards_found][i]);
			printf("\n");
		}
		printf("\n# (DEBUG)\n(READPORT 0x%04x)\n", port);
		if(do_ignorecsum)
			printf("(IGNORECRC)\n");
		if(do_fullreset)
			printf("(ISOLATE CLEAR)\n");
		else
			printf("(ISOLATE PRESERVE)\n");
		printf("(IDENTIFY *)\n");
	}
	else
	{
		CONFIGCONTROL;
		WRITE_DATA(CONFIG_WAIT_FOR_KEY);
		sendkey();
		read_idents();
		printf("\n# (DEBUG)\n(READPORT 0x%04x)\n(CSN %d)\n(IDENTIFY *)\n", read_port, boards_found);
	}
	printf("(VERBOSITY 2)\n(CONFLICT (IO FATAL)(IRQ FATAL)(DMA FATAL)(MEM FATAL)) # or WARNING\n\n");
}

void
read_idents(void)
{
	int csn = 0;
	int i;
	for (csn = 1; csn <= boards_found; csn++)
	{
		Wake(csn);
		for (i = 0; i < IDENT_LEN; i++)
		{
			if (statuswait())
				return;
			serial_identifier[csn][i] = RESOURCEDATA;
		}
		printf("# Board %d has serial identifier", csn);
		for (i = IDENT_LEN; i--;)
			printf(" %02x", serial_identifier[csn][i]);
		printf("\n");
		if (serial_identifier[csn][0] == 0xff)
			boards_found = csn - 1;
	}
}

int
do_isolate(void)
{
	unsigned char c1, c2;
	int i;
	int index;
	int newbit;
	int goodaddress = 0;
	if (boards_found >= NUM_CARDS)
	{
		fprintf(stderr, "Too many boards found, recompile with NUM_CARDS bigger\n");
		return 0;
	}
	csum = 0x6a;
	/* Assume we will find one */
	boards_found++;
	Wake(0);
	SetRdPort(port);
	delaynus(1000L);
	SERIALISOLATION;
	delaynus(2000L);
	for (index = 0; index < IDENT_LEN - 1; index++)
	{
		for (i = 0; i < 8; i++)
		{
			newbit = 0x00;
			delaynus(250L);
			c1 = READ_DATA;
			delaynus(250L);
			c2 = READ_DATA;
			if (c1 == 0x55)
			{
				if (c2 == 0xAA)
				{
					goodaddress = 1;
					newbit = 0x80;
				}
				else
				{
					goodaddress = 0; 
				}
			}
			/* printf("\n %02x %02x - bit %02x, goodaddress %d",c1,c2, newbit, goodaddress); */
			serial_identifier[boards_found][index] >>= 1;
			serial_identifier[boards_found][index] |= newbit;
			/* Update checksum */
			if (((csum >> 1) ^ csum) & 1)
				newbit ^= 0x80;
			csum >>= 1;
			csum |= newbit;
		}
		/* printf(" *** %02x \n", serial_identifier[boards_found][index]); */
	}
	/* printf("computed csum is %02x", csum); */
	for (i = 0; i < 8; i++)
	{
		newbit = 0x00;
		delaynus(250L);
		c1 = READ_DATA;
		delaynus(250L);
		c2 = READ_DATA;
		if (c1 == 0x55)
		{
			if (c2 == 0xAA)
			{
				goodaddress = 1;
				newbit = 0x80;
			}
		}
		/* printf("\n %02x %02x - bit %02x, goodaddress %d",c1,c2, newbit, goodaddress); */
		serial_identifier[boards_found][index] >>= 1;
		serial_identifier[boards_found][index] |= newbit;
	}
	/* printf(" *** csum *** %02x\n", serial_identifier[boards_found][index]); */
	if (goodaddress && (do_ignorecsum || (csum == serial_identifier[boards_found][index])))
	{
		CARDSELECTNUMBER;
		WRITE_DATA(boards_found);
		if((csumdiff[boards_found] = (csum - serial_identifier[boards_found][index])))
			printf("# WARNING: serial identifier mismatch for board %d: expected 0x%02X, got 0x%02X\n", boards_found, serial_identifier[boards_found][index], csum);
		return (1);
	}
	/* We didn't find one */
	boards_found--;
	return (0);
}

char *
devidstr(unsigned char d1, unsigned char d2, unsigned char d3, unsigned char d4)
{
	static char resstr[] = "PNP0000";
	sprintf(resstr, "%c%c%c%x%x%x%x", 'A' + (d1 >> 2) - 1, 'A' + (((d1 & 3) << 3) | (d2 >> 5)) - 1,
			'A' + (d2 & 0x1f) - 1, d3 >> 4, d3 & 0x0f, d4 >> 4, d4 & 0x0f);
	return resstr;
}

void
lengtherror(int len, char *msg)
{
	int i;
	printf("# Bad tag length for %s in 0x%02x", msg, tmp[0]);
	if (tmp[0] & 0x80)
	{
		printf(" 0x%02x 0x%02x", len & 255, len >> 8);
		for (i = 0; i < len; i++)
			printf(" 0x%02x", large[i]);
	}
	else
	{
		for (i = 1; i <= len; i++)
			printf(" 0x%02x", tmp[i]);
	}
	printf("\n");
}

#ifdef ABORT_ONRESERR
#define BREAKORCONTINUE   goto oncorruptresourcedata
#else
#define BREAKORCONTINUE   break
#endif

static void 
unread_resource_data(char prev)
{
	csum = (csum - prev) & 0xFF;
	backspaced = 1;
	backspaced_char = prev;
}

/* returns 0 on success, nonzero on failure. */

static inline int
read_resource_data(unsigned char *result)
{
	if (backspaced)
	{
		*result = backspaced_char;
		backspaced = 0;
		return 0;
	}
	if (statuswait())
		return 1;
	*result = RESOURCEDATA;
	csum = (csum + *result) & 0xFF;
	return 0;
}

static int
read_one_resource(struct resource *res)
{
	unsigned char *p;
	int i;
	int cnt = 1;

	if (read_resource_data(&res->tag) != 0)
		return 1;

        if(res->tag == 0xff) {
          ic->broken = 1;
          return 1;
        }

	if (res->tag & 0x80)
	{
		/* Large item */
		res->type = res->tag;
		for (i = 1; i <= 2; i++)
		{
			cnt++;
			if (read_resource_data(&tmp[i]) != 0)
				return 1;
		}
		res->len = (tmp[2] << 8) | tmp[1];
	}
	else
	{
		/* Small item */
		res->type = (res->tag >> 3) & 0x0f;
		res->len = res->tag & 7;
	}

        if(res->len + cnt > 10000) {
          /* be sensible about the resource length to avoid infinite loops */
          ic->broken = 1;
          return 1;
        }
	p = add_isapnp_card_res(ic, res->len, res->type);
	if ((res->data = malloc(res->len * sizeof(unsigned char))) == NULL)
	{
		fprintf(stderr, "read_one_resource: memory allocation failure.\n");
		return 0;	// ##### was: exit(1)
	}
	for (i = 0; i < res->len; i++)
	{
		cnt++;
		if (read_resource_data(&res->data[i]) != 0)
			return 1;
		p[i] = res->data[i];
	}

	return 0;
}

static void
fill_in_resource(struct resource *res)
{
	res->mask = ~0;
	res->start = 0;				/* Set some defaults */
	res->size = 1;
	res->step = 1;
	switch (res->type)
	{
	case IRQ_TAG:
		res->end = 16;
		res->mask = res->data[0] | (res->data[1] << 8);
		break;
	case DMA_TAG:
		res->end = 8;
		res->mask = res->data[0];
		break;
	case IOport_TAG:
		res->start = (res->data[2] << 8) | res->data[1];
		res->end = ((res->data[4] << 8) | res->data[3]) + 1;
		res->step = res->data[5];
		res->size = res->data[6];
		break;
	case FixedIO_TAG:
		res->start = (res->data[1] << 8) | res->data[0];
		res->size = res->data[2];
		res->end = res->start + 1;
		break;
	case MemRange_TAG:
		res->start = (((long) res->data[2] << 8) | res->data[1]) * 256;
		res->end = (((long) res->data[4] << 8) | res->data[3]) * 256 + 1;
		res->step = (res->data[6] << 8) | res->data[5];
		res->size = (((long) res->data[8] << 8) | res->data[7]) * 256;
		break;
	case Mem32Range_TAG:
		res->start = res->end = 0;
		/* STUB */
		break;
	case FixedMem32Range_TAG:
		res->start = res->end = 0;
		/* STUB */
		break;
	default:
		break;
	}
	/* Set initial value. */
	for (res->value = res->start; res->value < res->end; res->value += res->step)
	{
		if ((1 << (res->value & 0x1F)) & res->mask)
			break;
	}
}

static void
read_alternative(struct alternative *alt, struct resource *start_element)
{
	ASSERT(start_element->type == StartDep_TAG);
	switch (start_element->len)
	{
	case 0:
		alt->priority = 1;
		break;					/* 1 == "acceptable" */
	case 1:
		alt->priority = start_element->data[0];
		break;
	default:
		fprintf(stderr, "read_alternative: Illegal StartDep_TAG length %d.\n", start_element->len);
		break;
	}
	alt->resources = NULL;
	alt->len = 0;
	read_board_resources(start_element, 1, &alt->resources, &alt->len);
	*start_element = alt->resources[alt->len - 1];
	(alt->len)--;
}

static void
read_board_resources(struct resource *first_element,
					 int dependent_clause,
					 struct resource **result_array_ptr,
					 int *count)
{
	struct resource *result_array;
	int elts;

	result_array = *result_array_ptr;
	elts = *count;
	if (!result_array)
	{
		result_array = malloc(sizeof(struct resource));
		*count = elts = 0;
	}
	if (!result_array)
	{
		fprintf(stderr,
				"read_board_resources: malloc of result_array failed.\n");
		return;	// ###### was: exit(1)
	}
	if (first_element != NULL)
	{
		elts++;
		if ((result_array =
			 realloc(result_array, elts * sizeof(struct resource))) == NULL)
		{
			fprintf(stderr, "read_board_resources: realloc of result_array failed.\n");
			return;		// ##### was: exit(1)
		}
		result_array[elts - 1] = *first_element;
	}
	do
	{
		elts++;
		result_array = realloc(result_array, elts * sizeof(struct resource));
		if (!result_array)
		{
			fprintf(stderr,
				 "read_board_resources: realloc of result_array failed.\n");
			return;		// ###### was: exit(1)
		}
		memset(tmp, 0, sizeof(tmp));
		if(read_one_resource(&result_array[elts - 1])) return;
		if (result_array[elts - 1].type == StartDep_TAG && !dependent_clause)
		{
			int num_alts = 1;
			struct alternative *alts;
			alts = malloc(sizeof(struct alternative));
			do
			{
				if (num_alts != 1)
				{
					alts = realloc(alts, num_alts * sizeof(struct alternative));
				}
				if (!alts)
				{
					fprintf(stderr, "read_board_resources: memory allocation for alternatives failed.\n");
					return;		// ##### was: exit(1)
				}
				read_alternative(&alts[num_alts - 1], &result_array[elts - 1]);
				num_alts++;
			}
			while (result_array[elts - 1].type == StartDep_TAG);
			result_array[elts - 1].start = 0;
			result_array[elts - 1].end = num_alts - 1;
			result_array[elts - 1].mask = ~0;
			result_array[elts - 1].step = 1;
			result_array[elts - 1].alternatives = alts;
		}
		else
		{
			fill_in_resource(&result_array[elts - 1]);
		}
	}
	while (result_array[elts - 1].type != End_TAG &&
		   (!dependent_clause ||
			(result_array[elts - 1].type != EndDep_TAG &&
			 result_array[elts - 1].type != StartDep_TAG)));
	*count = elts;
	*result_array_ptr = result_array;
}

static void
read_board(int csn, struct resource **res_list, int *list_len)
{
	int i;
	struct resource *res_array;
	int array_len;

	res_array = *res_list;
	array_len = *list_len;
	if (res_array == NULL || array_len == 0)
	{
		res_array = malloc(sizeof(struct resource));
		array_len = 1;
	}
	else
	{
		array_len++;
		res_array = realloc(res_array,
							array_len * sizeof(struct resource));
	}
	if (!res_array)
	{
		fprintf(stderr,
				"read_board: failed to allocate memory for res_array.\n");
		return;		// ##### was: exit(1)
	}

	res_array[array_len - 1].type = NewBoard_PSEUDOTAG;
	res_array[array_len - 1].start = csn;
	res_array[array_len - 1].end = csn + 1;
	if ((res_array[array_len - 1].data = malloc(IDENT_LEN)) == NULL)
	{
		fprintf(stderr,
		   "read_board: failed to allocate memory for board identifier.\n");
		return;		// ###### was: exit(1)
	}

	Wake(csn);
	/*
	 * Check for broken cards that don't reset their resource pointer
	 * properly
	 */
	/* Get the first byte */
	if (read_resource_data(&res_array[array_len - 1].data[0]) != 0)
		return;
	/* Just check the first byte, if these match, or the serial identifier had a checksum error, we assume it's ok */
	if((!csumdiff[csn])&&(res_array[array_len - 1].data[0] != serial_identifier[csn][0]))
	{
		/*
		 * Assume the card is broken and this is the start of the resource
		 * data.
		 */
		unread_resource_data(res_array[array_len - 1].data[0]);
		res_array[array_len - 1].len = 1;
		goto broken;			/* Wouldn't need this if people read the spec */
	}
	res_array[array_len - 1].len = IDENT_LEN;
	/*
	 * Read resource data past serial identifier - As it should be spec
	 * section 4.5 para 2
	 */
	for (i = 1; i < IDENT_LEN; i++)
	{
		if (read_resource_data(&res_array[array_len - 1].data[i]) != 0)
			return;
	}
	if(csumdiff[csn])
	{
		/* Original serial identifier had checksum error, so make the serial
		 * identifier what we read from the resource data, as that's what
		 * isapnp will see from an (identify *), assuming checksum ok
		 */
		int data;
		int newbit;
		int j;
		csum = 0x6a;
		for (i = 0; i < IDENT_LEN - 1; i++)
		{
			data = res_array[array_len - 1].data[i];
			for(j = 0; j < 8; j++)
			{
				newbit = (data & 1) << 7;
				data >>= 1;
				if(((csum >> 1) ^ csum) & 1)
					newbit ^= 0x80;
				csum >>= 1;
				csum |= newbit;
			}
		}
		if(csum == res_array[array_len - 1].data[IDENT_LEN - 1])
		{
			printf("# WARNING: Making Board %d serial identifier the resource data version\n", csn);
			for (i = 0; i < IDENT_LEN; i++)
			{
				serial_identifier[csn][i] = res_array[array_len - 1].data[i];
			}
		}
		else
			printf("# WARNING: Board %d resource data identifier has checksum error too\n", csn);
	}
	/* Now for the actual resource data */
	csum = 0;
  broken:
	*res_list = res_array;
	*list_len = array_len;
	read_board_resources(NULL, 0, res_list, list_len);
}


void dumpregs(int csn)
{
  int logical_device, addr, last_addr;

  Wake(csn);

  for(addr = 0x06; addr < 0x30; addr++) {
    ADDRESS(addr);
    ic->card_regs[addr] = READ_DATA;
  }

  for(logical_device = 0; logical_device < ic->log_devs; logical_device++) {
    LOGICALDEVICENUMBER;
    WRITE_DATA(logical_device);
    if(READ_DATA != logical_device) break;

    /*
     * #####
     * More or less a hack; what should we really do with broken cards???
     */
    last_addr = ic->broken ? 0x80 : 0x100;

    for(addr = 0x30; addr < last_addr; addr++) {
//      PROGRESS(logical_device, addr, "dumpregs");
      ADDRESS(addr);
      ic->ldev_regs[logical_device][addr - 0x30] = READ_DATA;
    }
  }
}

