#if defined(__i386__)

#include <stdio.h>
#include <sys/io.h>

#include "hd.h"
#include "hd_int.h"
#include "isa.h"

#define AVM_CONFIG_OFF	0x1800	/* offset for config register */
#define AVM_TEST_MASK	0x28    /* allways zero */
#define AVM_HSCX_A_VSTR	0x40e   /* HSCX A version reg */
#define AVM_HSCX_B_VSTR	0xc0e   /* HSCX B version reg */

int avm_a1_detect(isa_isdn_t **ii) {
	int adr,i;
	unsigned char val,v1,v2;
	int found=0;
	unsigned short AVM_ADR[4]={0x200,0x240,0x300,0x340};
	isa_isdn_t *card;

	for (i=0;i<4;i++) {
		adr=AVM_ADR[i] + AVM_CONFIG_OFF;
		val = inb(adr);
		if (val & AVM_TEST_MASK)
			continue;
		/* May be we found an AVM A1 or AVM Fritz!Classic */
		/* Checking HSCX VERSIONS */
		v1 = 0xf & inb(AVM_ADR[i] + AVM_HSCX_A_VSTR);
		if ((v1 != 5) && (v1 != 4))
			continue;
		v2 = 0xf & inb(AVM_ADR[i] + AVM_HSCX_B_VSTR);
		if (v1 != v2)
			continue;
		/* 99% we found an AVM A1 or AVM Fritz!Classic */
		/* printf("# AVM A1 or Fritz!Classic found\n");
		printf("TYPE=5 SUBTYPE=0 IO=0x%3x\n", AVM_ADR[i]); */
		card = new_isa_isdn(ii);
		card->type = 5; card->has_io = 1; card->io = AVM_ADR[i];
		found++;
	}
	return(found);
}

#define ELSA_CONFIG	5
#define ELSA_PC		1
#define ELSA_PCC8	2
#define ELSA_PCC16	3
#define ELSA_PCF	4
#define ELSA_PCFPRO	5

#define ELSA_IRQ_IDX		0x38	/* Bit 3,4,5 des Config-Reg */
#define ELSA_IRQ_IDX_PCC8	0x30	/* Bit 4,5 des Config-Reg */
#define ELSA_IRQ_IDX_PC		0x0c	/* Bit 2,3 des Config-Reg */


int
probe_elsa_adr(unsigned int adr)
{
	int i, in1, in2, p16_1 = 0, p16_2 = 0, p8_1 = 0, p8_2 = 0, pc_1 = 0,
	 pc_2 = 0, pfp_1 = 0, pfp_2 = 0;

	for (i = 0; i < 16; i++) {
		in1 = inb(adr + ELSA_CONFIG);	/* 'toggelt' bei */
		in2 = inb(adr + ELSA_CONFIG);	/* jedem Zugriff */
		p16_1 += 0x04 & in1;
		p16_2 += 0x04 & in2;
		p8_1 += 0x02 & in1;
		p8_2 += 0x02 & in2;
		pc_1 += 0x01 & in1;
		pc_2 += 0x01 & in2;
		pfp_1 += 0x40 & in1;
		pfp_2 += 0x40 & in2;
	}
	if (65 == ++p16_1 * ++p16_2) {
		return (ELSA_PCC16);
	} else if (1025 == ++pfp_1 * ++pfp_2) {
		return (ELSA_PCFPRO);
	} else if (33 == ++p8_1 * ++p8_2) {
		return (ELSA_PCC8);
	} else if (17 == ++pc_1 * ++pc_2) {
		return (ELSA_PC);
	}
	return (0);
}

int
probe_elsa(isa_isdn_t **ii)
{
	int i, subtyp, val, irq, found=0;
	isa_isdn_t *card;

	unsigned int CARD_portlist[] =
	{0x160, 0x170, 0x260, 0x360, 0};

	for (i = 0; CARD_portlist[i]; i++) {
		if ((subtyp = probe_elsa_adr(CARD_portlist[i]))) {
			found++;
			val = inb(CARD_portlist[i] + ELSA_CONFIG);
			if (subtyp == ELSA_PC) {
				int CARD_IrqTab[8] =
				{7, 3, 5, 9, 0, 0, 0, 0};
				irq = CARD_IrqTab[(val & ELSA_IRQ_IDX_PC) >> 2];
			} else if (subtyp == ELSA_PCC8) {
				int CARD_IrqTab[8] =
				{7, 3, 5, 9, 0, 0, 0, 0};
				irq = CARD_IrqTab[(val & ELSA_IRQ_IDX_PCC8) >> 4];
			} else {
				int CARD_IrqTab[8] =
				{15, 10, 15, 3, 11, 5, 11, 9};
				irq = CARD_IrqTab[(val & ELSA_IRQ_IDX) >> 3];
			}
			switch(subtyp) {
				case ELSA_PC:
					/* printf("# Elsa ML PC found\n");
					printf("TYPE=6 SUBTYPE=%d IO=0x%03x IRQ=%d\n",
						subtyp, CARD_portlist[i], irq); */
				case ELSA_PCC8:
					/* printf("# Elsa ML PCC-8 found\n");
					printf("TYPE=6 SUBTYPE=%d IO=0x%03x IRQ=%d\n",
						subtyp, CARD_portlist[i], irq); */
				case ELSA_PCC16:
					/* printf("# Elsa ML PCC-16 found\n");
					printf("TYPE=6 SUBTYPE=%d IO=0x%03x IRQ=%d\n",
						subtyp, CARD_portlist[i], irq); */
				case ELSA_PCF:
					/* printf("# Elsa ML PCF found\n");
					printf("TYPE=6 SUBTYPE=%d IO=0x%03x IRQ=%d\n",
						subtyp, CARD_portlist[i], irq); */
				case ELSA_PCFPRO:
					/* printf("# Elsa ML PCFPro found\n");
					printf("TYPE=6 SUBTYPE=%d IO=0x%03x IRQ=%d\n",
						subtyp, CARD_portlist[i], irq); */

				        card = new_isa_isdn(ii);
				        card->type = 6; card->subtype = subtyp;
				        card->has_io = 1; card->io = CARD_portlist[i];
				        card->has_irq = 1; card->irq = irq;
					break;
			}
		}
	}
	return (found);
}


#define TELES_CONFIG_OFF	0xc00
#define TELES_ID1		0x51
#define TELES_ID2		0x93
#define TELES_16_0		0x1e
#define TELES_16_0_AB		0x1f
#define TELES_16_3_0		0x1c
#define TELES_16_3_1		0x39
#define TELES_16_3_3		0x38
#define TELES_16_3_AB		0x46


int telesdetect(isa_isdn_t **ii) {
	int adr,val,i;
	int found=0;
	unsigned short TELES_ADR[3]={0x380,0x280,0x180};
	isa_isdn_t *card;

	for (i=0;i<3;i++) {
		adr=TELES_ADR[i] + TELES_CONFIG_OFF;

		val = inb(adr);
		if (val != TELES_ID1)
			continue;
		adr++;
		val = inb(adr);
		if (val != TELES_ID2)
			continue;
		adr++;
		val = inb(adr);
		switch(val) {
			case TELES_16_0:
				/* printf("# Teles 16.0 found\n");
				printf("TYPE=1 SUBTYPE=0 IO=0x%3x\n",
					TELES_ADR[i] + TELES_CONFIG_OFF); */
			case TELES_16_0_AB:
				/* printf("# Teles 16.0 AB found\n");
				printf("TYPE=1 SUBTYPE=1 IO=0x%3x\n",
					TELES_ADR[i] + TELES_CONFIG_OFF); */
			        card = new_isa_isdn(ii);
			        card->type = 1;
			        if(val == TELES_16_0_AB) card->subtype = 1;
			        card->has_io = 1; card->io = TELES_ADR[i] + TELES_CONFIG_OFF;
				found++;
				break;
			case TELES_16_3_0:
				/* printf("# Teles 16.3 v1.0 found\n");
				printf("TYPE=3 SUBTYPE=0 IO=0x%3x\n",
					TELES_ADR[i]); */
			case TELES_16_3_1:
				/* printf("# Teles 16.3 v1.1 found\n");
				printf("TYPE=3 SUBTYPE=0 IO=0x%3x\n",
					TELES_ADR[i]); */
			case TELES_16_3_3:
				/* printf("# Teles 16.3 v1.3 found\n");
				printf("TYPE=3 SUBTYPE=0 IO=0x%3x\n",
					TELES_ADR[i]); */
			case TELES_16_3_AB:
				/* printf("# Teles 16.3 AB Video found\n");
				printf("TYPE=3 SUBTYPE=1 IO=0x%3x\n",
					TELES_ADR[i]); */
			        card = new_isa_isdn(ii);
			        card->type = 3;
			        if(val == TELES_16_3_AB) card->subtype = 1;
			        card->has_io = 1; card->io = TELES_ADR[i];
				found++;
				break;
			default:
				if (0 /* guess */)
					printf("# may be a Teles 16.0/16.3 detected at IO=0x%3x byte 3 is 0x%02x\n",
						TELES_ADR[i] + TELES_CONFIG_OFF, val);
				break;
		}
	}
	return(found);
}

isa_isdn_t *isdn_detect()
{
  isa_isdn_t *ii = NULL;

  if(iopl(3) < 0) return ii;

  avm_a1_detect(&ii);
  probe_elsa(&ii);
  telesdetect(&ii);

  iopl(0);

  return ii;
}

#endif	/* i386 */
