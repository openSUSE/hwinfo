/* Copyright 1998 Yggdrasil Computing, Inc.
   Written by Adam J. Richter

   This file may be copied according to the terms and conditions of the
   GNU General Public License as published by the Free Software
   Foundation.

   isapnp.gone handling added to alloc_system_resources() by P.Fox

   alloc_in_range_list() reordered to handle source name by P.Fox
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

extern _IO_ssize_t getdelim(char **, size_t *, int, FILE *);

#ifndef __DJGPP__
#include <linux/pci.h>
#endif

#include "resource.h"
#include "pnp.h"
#include "hd.h"
#include "hd_int.h"

#if 1
#undef fprintf
#define fprintf(a...)
#endif


#ifndef GONEFILE
#define GONEFILE "/etc/isapnp.gone" /* File with unuseable resources */
#endif

#define ASSERT(x)	/* as nothing for now */

char * conflict_source = 0; /* Resource which clashed last */

struct range_list {
    unsigned long start, end;	/* end = start+len */
    struct range_list *next;
	char *source; /* Name of file where resource allocated, the pointer must be the same for the same source and resource type */
};

static int
alloc_in_range_list (unsigned long start, unsigned long end,
					 struct range_list **list, char *source) {
	struct range_list *new;
    if (start == end)
		return 1;	/* adding empty range. */
    ASSERT(start < end);
	/* Run up the list until the new start is at the list entry end, or before the list entry start */
    while (*list && start > (*list)->end)
	{
        list = &(*list)->next;
    }
    ASSERT(*list == NULL || start <= (*list)->end);
	/* Test and deal with overlaps first */
	if(*list)
	{
		if((start >= (*list)->start && start < (*list)->end)
		   ||(start == (*list)->end && (*list)->next && (*list)->next->start < end)
		   ||(start < (*list)->start && end > (*list)->start))
		{
			if((start == (*list)->end && (*list)->next && (*list)->next->start < end))
				conflict_source = (*list)->next->source;
			else
				conflict_source = (*list)->source;
			return 0;
		}
		/* Ok, the new one fits in */
		ASSERT(end >= (*list)->start);
		ASSERT(start <= (*list)->end);
		/* Test for contiguous regions */
		/* First new end matches source start ==> gap before new start */
		if (end == (*list)->start && source == (*list)->source) {
			/* We are contiguous with next element, and have the same source */
			(*list)->start = start;
			return 1;
		}
		/* If new start matches end, need to test the new end against the next region start */
		if (start == (*list)->end)
		{
			if ((*list)->next && (*list)->next->start == end)
			{
				/* we exactly bridge to subsequent element. */
				if(source == (*list)->source)
				{
					if(source == (*list)->next->source)
					{
						/* If both sources are the same, can merge */
						struct range_list *obselete = (*list)->next;
						(*list)->end = obselete->end;
						(*list)->next = obselete->next;
						free(obselete);
						return 1;
					}
					else
					{
						/* next element is contiguous with us and the subsequent element has a different source. */
						(*list)->end = end;
						return 1;
					}
				}
				if(source == (*list)->next->source)
				{
					/* next element is contiguous with us and the current element has a different source. */
					(*list)->next->start = start;
					return 1;
				}
				/* Both have different sources, so drop out to put in new element */
			}
			else if(source == (*list)->source)
			{
				/* next element is contiguous with us and we do not reach subsequent element. */
				(*list)->end = end;
				return 1;
			}
		}
	}
	/* There is a gap between us and next element, or no next element, or the sources are different. */
	new = malloc(sizeof(struct range_list));
	if (!new) {
		fprintf (stderr, "alloc_in_range_list: malloc failure.\n");
		return 1;	// ####### was: exit(1)
	}
	new->start = start;
	new->end = end;
	new->next = *list;
	new->source = source;
	*list = new;
	return 1;
}

static void
dealloc_in_range_list (unsigned long start, unsigned long end,
					   struct range_list **list) {
    if (start == end) return;	/* deleting empty range. */
    ASSERT(start < end);
    while (*list && start >= (*list)->end) {
        list = &(*list)->next;
    }
    while (*list && end > (*list)->start) {
		/* we overlap with this element */
        ASSERT(start < (*list)->end);
		if((*list)->end > end) {
			(*list)->start = end;
			return;
		} else {
			struct range_list *tmp = *list;
			*list = (*list)->next;
			free(tmp);
		}
    }
}

struct range_list *tag_range_lists[] = {NULL, NULL, NULL, NULL};

static int
tag_to_index (unsigned char tag) {
    switch (tag) {
	case IRQ_TAG: return 0;
	case DMA_TAG: return 1;
	case IOport_TAG:
	case FixedIO_TAG:
	    return 2;
	case MemRange_TAG:
	case Mem32Range_TAG:
	case FixedMem32Range_TAG:
	    return 3;
	default:		
	    return -1;		/* Unrecognized tag */
    }
}

int
tag_allocable (unsigned char tag) {
    return tag_to_index(tag) != -1;
}

int
allocate_resource(char tag, unsigned long start, unsigned long len, char *source) {
    return alloc_in_range_list(start, start + len,
							   &tag_range_lists[tag_to_index(tag)], source);
}

void
deallocate_resource(char tag, unsigned long start, unsigned long len) {
    dealloc_in_range_list(start, start + len,
						  &tag_range_lists[tag_to_index(tag)]);
}

#define INBUFSIZ 256 /* Be reasonable */

#ifndef __DJGPP__
void allocate_pci_resources( void )
{
   char *line = 0;
   size_t lineMax = 0;

   FILE *fp = fopen( PROC_PCI_DEVICES, "rt" );
   if( !fp )
   {
      fprintf(stderr, PROC_PCI_DEVICES " not found, so PCI resource conflict not checked\n");
      return;
   }
   while( getdelim( &line, &lineMax, '\n', fp ) > 0 )
   {
      int i;
      unsigned long val;
      char *seg, *end;

      if( !strtok( line, " \t\n" ) )
         continue;

      strtok( 0, " \t\n" );
      seg = strtok( 0, " \t\n" );
      if( !seg )
         continue;

      val = strtoul( seg, &end, 16 );
      if( *end == 0 )
         allocate_resource( IRQ_TAG, val, 1, "pci" );

      for( i = 0; i < 7; i++ )
      {
         seg = strtok( 0, " \t\n" );
         if( !seg )
            break;
         val = strtoul( seg, &end, 16 );
         if( *end )
            continue;

         if( (val & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_MEMORY )
         {
            val = val & PCI_BASE_ADDRESS_MEM_MASK;
            if( val )
               allocate_resource( MemRange_TAG, val, 1, "pci" );
         }else{
            val = val & PCI_BASE_ADDRESS_IO_MASK;
            if( val )
               allocate_resource( IOport_TAG, val, 1, "pci" );
         }
      }
   }

   if( line )
      free( line );
   fclose( fp );
   return;
}
#endif

void
alloc_system_resources(void) {
    FILE *input;
	static char inbuf[INBUFSIZ];
    int interrupt, dma, io_start, io_end, mem_start;

    /* Avoid allocating DMA channels used by other devices in /proc. */
    if ((input = fopen(PROC_INTERRUPTS, "r")) != NULL) {
        fscanf(input, "%*[^\n]\n"); /* skip first line */
        while (fscanf (input, "%d%*[^\n]\n", &interrupt) == 1) {
			(void) allocate_resource(IRQ_TAG, interrupt, 1, PROC_INTERRUPTS);
		}
        fclose(input);
    }
    if ((input = fopen(PROC_DMA, "r")) != NULL) {
        while (fscanf (input, "%d%*[^\n]\n", &dma) == 1) {
			(void) allocate_resource(DMA_TAG, dma, 1, PROC_DMA);
		}
        fclose(input);
    }
    if ((input = fopen(PROC_IOPORTS, "r")) != NULL) {
        while (fscanf (input, "%x-%x%*[^\n]\n", &io_start, &io_end) == 2) {
			/* /proc/ioports addresses are inclusive, hence + 1 below */
			(void) allocate_resource(IOport_TAG, io_start, io_end - io_start + 1, PROC_IOPORTS);
		}
        fclose(input);
    }

#ifndef __DJGPP__
    allocate_pci_resources();
#endif

	/* Now read in manual stuff from isapnp.gone file */
    if ((input = fopen(GONEFILE, "r")) != NULL)
	{
		int line = 0;
		/*
		 * File format - 
		 *
		 * Leading whitespace removed, then..
		 * Blank lines ignored
		 * # in column 1 is comment
		 * 
		 * One entry per line, after entry is rubbish and ignored
		 *
		 * IO base [, size] - default size 8
		 * IRQ n
		 * DMA n
		 * MEM base, size
		 *
		 * Numbers may be decimal or hex (preceded by 0x)
		 *
		 * Anything else is rubbish and complained about noisily
		 */
        while (fgets(inbuf, INBUFSIZ, input))
		{
			unsigned long n;
			int size;
			char *inptr = inbuf;
			line++;
			while(isspace(*inptr))
				inptr++;
			if((*inptr == '#')||(*inptr == 0))
				continue;
			if(toupper(*inptr) == 'I')
			{
				/* IO or IRQ */
				inptr++;
				if((toupper(*inptr) == 'O')&&(isspace(inptr[1])))
				{
					/* IO */
					int size = 8;
					inptr += 2;
					n = strtoul(inptr, &inptr, 0);
					io_start = (int)n;
					while(isspace(*inptr))
						inptr++;
					/* Check for optional size */
					if(*inptr == ',')
					{
						inptr++;
						n = strtoul(inptr, &inptr, 0);
						if(!n)
							goto garbage;
						size = n;
					}
					(void) allocate_resource(IOport_TAG, io_start, size, GONEFILE);
					continue;
				}
				if((toupper(*inptr) == 'R')&&(toupper(inptr[1]) == 'Q')&&(isspace(inptr[2])))
				{
					/* IRQ */
					inptr += 3;
					n = strtoul(inptr, &inptr, 0);
					interrupt = (int)n;
					(void) allocate_resource(IRQ_TAG, interrupt, 1, GONEFILE);
					continue;
				}
				goto garbage;
			}
			else if(toupper(*inptr) == 'D')
			{
				/* DMA */
				inptr++;
				if((toupper(*inptr) == 'M')&&(toupper(inptr[1]) == 'A')&&(isspace(inptr[2])))
				{
					inptr += 3;
					n = strtoul(inptr, &inptr, 0);
					dma = (int)n;
					(void) allocate_resource(DMA_TAG, dma, 1, GONEFILE);
					continue;
				}
				goto garbage;
			}
			else if(toupper(*inptr) == 'M')
			{
				/* MEM */
				inptr++;
				if((toupper(*inptr) == 'E')&&(toupper(inptr[1]) == 'M')&&(isspace(inptr[2])))
				{
					/* MEM */
					inptr += 3;
					n = strtoul(inptr, &inptr, 0);
					mem_start = (int)n;
					while(isspace(*inptr))
						inptr++;
					/* Check for optional size */
					if(*inptr != ',')
						goto garbage;
					inptr++;
					n = strtoul(inptr, &inptr, 0);
					if(!n)
						goto garbage;
					size = n;
					(void) allocate_resource(MemRange_TAG, mem_start, size, GONEFILE);
					continue;
				}
				goto garbage;
			}
			else
			{
			garbage:
				fprintf(stderr, "Garbage in %s file on line %d: %s", GONEFILE, line, inbuf);
			}
		}
        fclose(input);
    }
}
