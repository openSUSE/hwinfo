/* Copyright 1998 Yggdrasil Computing, Inc.
   Written by Adam J. Richter

   This file may be copied according to the terms and conditions of the
   GNU General Public License as published by the Free Software
   Foundation.
*/

#include <stdio.h>
#include <malloc.h>

#include "resource.h"
#include "pnp.h"

#define ASSERT(condition)	/* as nothing */

static int
inc_value (struct resource *res) {
    if (res->type != EndDep_TAG) {
        do {
	    res->value += res->step;
	    if (res->value >= res->end) return 0;
	} while (!((1 << (res->value & 0x1f)) & res->mask));
	return 1;
    }
    else {
        int min_priority;
	int best_priority;
	int best_index;
	int i;
        if (res->value == -1) {
	    min_priority = -1;
        }
	else {
	    /* Cycle through all alternatives, but try lowest
	       priority numbers first. */
	    min_priority = res->alternatives[res->value].priority;
	    for (;;) {
	        res->value++;
		if (res->value >= res->end) break;
		if (res->alternatives[res->value].priority == min_priority) return 1;
	    }
	}
	best_index = -1;
	best_priority = 4000;   /* XXX.  This assignement is unnecessary.
				   It only servers to get rid of a compiler
				   warning. */
	/* We want to choose the numerically lowest priority, that is
	   "top" priority. */
	for (i = 0; i < res->end; i++) {
	    if (res->alternatives[i].priority > min_priority &&
		(res->alternatives[i].priority < best_priority ||
		 best_index == -1)) {
	        best_index = i;
		best_priority = res->alternatives[i].priority;
	    }
	}
	res->value = best_index;
	return (best_index != -1);
    }
}

int
alloc_resources ( struct resource *res, int count,
		  struct resource *parent_res, int parent_count) {
    while (count > 0 && res->type != EndDep_TAG && !tag_allocable(res->type)) {
        res++;
        count--;
    }
    if (count == 0) {
        return parent_res == NULL ? 1 :
	    alloc_resources(parent_res, parent_count, NULL, 0);
    }			       
    res->value = res->start - res->step;
    while (inc_value(res)) {
        if (res->type == EndDep_TAG) {
	    ASSERT(parent_res == NULL);
	    ASSERT(parent_count == 0);
	    if (alloc_resources(res->alternatives[res->value].resources,
				res->alternatives[res->value].len,
				res+1, count-1)) {
	        return 1;
	    }
	} else {
	    if (allocate_resource(res->type, res->value, res->size, "pnpdump")) {
	        if (alloc_resources(res+1,count-1, parent_res, parent_count))
		    return 1;
		deallocate_resource(res->type, res->value, res->size);
	    }
	}
    }
    /* Resource allocation failed, reset to some sensible default value. */
    for(res->value = res->start; res->value < res->end; res->value += res->step) {
        if ((1 << (res->value & 0x1f)) & res->mask) break;
    }
    return 0;
}
