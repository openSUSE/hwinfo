/* (C) kkeil@suse.de */

#ifndef IHW_INCLUDE
#define IHW_INCLUDE

#define IHW_VERSION	0x0101
#define	CLASS_PCI	1
#define CLASS_ISAPNP	2
#define CLASS_ISALEGAL	3
#define CLASS_PCMCIA	4
#define CLASS_PC104	5
#define CLASS_PARALLEL	6
#define CLASS_SERIAL	7
#define CLASS_USB	8
#define CLASS_ONBOARD	9

/* parameter types */
#define P_NONE		0x0
#define P_IRQ		0x1
#define P_MEM		0x2
#define P_IO		0x3
#define P_IO0		0x4
#define P_IO1		0x5
#define P_IO2		0x6
#define P_BASE0		0x8
#define P_BASE1		0x9
#define P_BASE2		0xa
#define P_BASE3		0xb
#define P_BASE4		0xc
#define P_BASE5		0xd

#define P_TYPE_MASK	0xff

#define P_DEFINE	0x0100
#define P_SOFTSET	0x0200
#define P_HARDSET	0x0400
#define P_READABLE	0x0800
#define P_ISAPNP	0x1000
#define P_PCI		0x2000

#define P_PROPERTY_MASK	0xffff00

#ifndef PCI_ANY_ID
#define PCI_ANY_ID	0xffff
#endif

/* card info */

typedef struct	{
	int	handle;		/* internal identifier, set to -1 to get the */
				/* first entry */
	const char *name;	/* Ascii cardname */
	int	type;		/* Type to identify the driver */
				/* should be set the I4L_TYPE rc variable */
	int	subtype;	/* Subtype of the driver type */
				/* should be set the I4L_SUBTYPE rc variable */
	int	Class;		/* CLASS of the card */
	int	vendor;		/* Vendor ID for ISAPNP and PCI cards */
	int     device;		/* Device ID for ISAPNP and PCI cards */
	int	subvendor;	/* Subvendor ID for PCI cards */
				/* A value of 0xffff is ANY_ID */
	int     subdevice;	/* Subdevice ID for PCI cards */
				/* A value of 0xffff is ANY_ID */
} ihw_card_info;

/* parameter info */
typedef struct  {
	int     	handle;		/* internal identifier, set to -1 to */
					/* get the first entry */
	const char	*name;		/* Name of the parameter */
	unsigned int	type;		/* type of parameter (P_... */
	unsigned int	flags;		/* additional information about the */
					/* parameter */
	unsigned long	def_value;	/* default value */
       	const unsigned long *list;	/* possible values of the parameter */
       					/* The first element gives the count */
       					/* of values */
} ihw_para_info;

/* get card informations in alphabetically order, for the first entry */
/* set ici->handle to a negativ value */
/* with every next call, you get the next record */
/* returns NULL if here are no more entries */

extern	ihw_card_info	*ihw_get_device(ihw_card_info   *ici);

/* get card informations  for the card with TYPE (set in ici->type) and */
/* SUBTYPE (set in ici->subtype) returns NULL if no card match */

extern  ihw_card_info	*ihw_get_device_from_type(ihw_card_info   *ici);

/* get card informations  for the card with VENDOR,DEVICE,SUBVENDOR, */
/* SUBDEVICE for  ISAPNP and PCI cards. SUBVENDOR and SUBDEVICE should be */
/* set to 0xffff for ISAPNP cards returns NULL if no card found */

extern  ihw_card_info	*ihw_get_device_from_id(ihw_card_info   *ici);

/* Get a parameter information for a card identified with "device_handle" */
/* For the first parameter set ipi->handle to a negativ value */
/* returns NULL, if here are no more parameter */
extern  ihw_para_info	*ihw_get_parameter(int device_handle, ihw_para_info *ipi);

#endif
