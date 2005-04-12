/*
 * These are fixed and write only. Reads are done from a port with a
 * relocatable address...
 */
#define ISAPNP_ADDR_PORT	0x279
#define ISAPNP_DATA_PORT	0xa79


/*
 * ISA PnP resource types
 */
#define RES_PNP_VERSION		0x01
#define RES_LOG_DEV_ID		0x02
#define RES_COMPAT_DEV_ID	0x03
#define RES_IRQ			0x04
#define RES_DMA			0x05
#define RES_START_DEP		0x06
#define RES_END_DEP		0x07
#define RES_IO			0x08
#define RES_FIXED_IO		0x09
#define RES_VENDOR_SMALL	0x0e
#define RES_END			0x0f

#define RES_MEM_RANGE		0x81
#define RES_ANSI_NAME		0x82
#define RES_UNICODE_NAME	0x83
#define RES_VENDOR_LARGE	0x84
#define RES_MEM32_RANGE		0x85
#define RES_FIXED_MEM32_RANGE	0x86


/*
 * ISA PnP configuration regs
 */
#define CFG_MEM24		0x40
#define CFG_MEM32_0		0x76
#define CFG_MEM32_1		0x80
#define CFG_MEM32_2		0x90
#define CFG_MEM32_3		0xa0
#define CFG_IO_HI_BASE		0x60
#define CFG_IO_LO_BASE		0x61
#define CFG_IRQ			0x70
#define CFG_IRQ_TYPE		0x71
#define CFG_DMA			0x74


/* gather ISA-PnP info */
void hd_scan_isapnp(hd_data_t *hd_data);


/*
 * Interface functions to the pnpdump lib.
 */
int pnpdump(hd_data_t *hd_data, int read_boards);
unsigned char *add_isapnp_card_res(isapnp_card_t *, int, int);
isapnp_card_t *add_isapnp_card(isapnp_t *, int);

