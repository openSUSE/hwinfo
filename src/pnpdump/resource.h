/* Copyright 1998 Yggdrasil Computing, Inc.
   Written by Adam J. Richter

   This file may be copied according to the terms and conditions of the
   GNU General Public License as published by the Free Software
   Foundation.
*/
#ifndef RESOURCE_H
#define RESOURCE_H

#define DEPENDENCY_PSEUDOTAG	-2

struct resource {
    unsigned int type;		/* IRQ_TAG, DMA_TAG, IOport_TAG, StartDep_TAG,
				   MemRange_TAG, Mem32Range_TAG,
				   FixedMem32Range_TAG */
				/* If this is a parent node of a number
				   of dependent resources, the type of
				   this node is EndDep_TAG.  The
				   StartDep_TAG's are the first resource
				   of each child alternative
                                   (i.e., alterantives[i].resources[0]).*/
    unsigned char tag;		/* The actual tag from which type and possibly
				   len were derived.  Used for debugging. */
    int len;
    unsigned char *data;

    unsigned long value;
    unsigned long start, end, step, size; /* IO or memory  */
    /* For other tags: start = 0, end = num_alternatives-1, step = 1, size = 1 */
    unsigned long mask;		    /* DMA or IRQ, ~0 for all others */
    struct alternative *alternatives; /* StartDep_TAG */
};

struct alternative {
    int priority;
    int len;
    struct resource *resources;
};


extern int
allocate_resource(char tag, unsigned long start, unsigned long len, char *source);
				/* Returns 1 on successful reservation. */

void
deallocate_resource(char tag, unsigned long start, unsigned long len);

int
alloc_resources ( struct resource *res, int count,
		  struct resource *parent_res, int parent_count);

int tag_allocable (unsigned char tag);

void alloc_system_resources(void);

extern char * conflict_source;

#endif /* RESOURCE_H */
