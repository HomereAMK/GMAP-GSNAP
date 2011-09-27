static char rcsid[] = "$Id: match.c,v 1.73 2006/03/21 02:03:08 twu Exp $";
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifndef HAVE_MEMCPY
# define memcpy(d,s,n) bcopy((s),(d),(n))
#endif
#ifndef HAVE_MEMMOVE
# define memmove(d,s,n) bcopy((s),(d),(n))
#endif

#include "match.h"
#include "matchdef.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>		/* For memcpy */
#include "mem.h"
#include "segmentpos.h"
#include "separator.h"
#include "sequence.h"

#define T Match_T

#ifdef DEBUG
#define debug(x) x
#else
#define debug(x)
#endif

int
Match_querypos (T this) {
  return this->querypos;
}

bool
Match_forwardp (T this) {
  return this->forwardp;
}

bool
Match_fivep (T this) {
  return this->fivep;
}

Genomicpos_T
Match_position (T this) {
  return this->position;
}

Chrnum_T
Match_chrnum (T this) {
  return this->chrnum;
}

Genomicpos_T
Match_chrpos (T this) {
  return this->chrpos;
}

int
Match_incr_npairings (T this) {
  this->npairings += 1;
  return this->npairings;
}

int
Match_npairings (T this) {
  return this->npairings;
}

void
Match_set_weight (T this, double weight) {
  this->weight = weight;
  return;
}

double
Match_weight (T this) {
  return this->weight;
}


int
Match_cmp (const void *a, const void *b) {
  T x = * (T *) a;
  T y = * (T *) b;

  if (x->position < y->position) {
    return -1;
  } else if (x->position > y->position) {
    return 1;
  } else {
    return 0;
  }
}

#ifndef USE_MATCHPOOL
/* Matches now made in matchpool.c */
T
Match_new (int querypos, bool forwardp, bool fivep,
	   Genomicpos_T position, IIT_T chromosome_iit) {
  T new = (T) MALLOC(sizeof(*new));
  int index;

  new->querypos = querypos;
  new->weight = 0.0;		/* Will be entered later */
  new->position = position;
  new->forwardp = forwardp;
  new->fivep = fivep;
  new->npairings = 0;

  if (chromosome_iit == NULL) {
    new->chrnum = 0;
    new->chrpos = position;
  } else {
    index = IIT_get_one(chromosome_iit,position,position);
    new->chrpos = position - Interval_low(IIT_interval(chromosome_iit,index));
    new->chrnum = index;
  }

  return new;
}

void
Match_free (T *old) {
  if (*old) {
    FREE(*old);
  }
  return;
}
#endif


/* Static gbuffer1, gbuffer2 are allowable only for debugging */
#define MAXSTAGE1SIZE 24

void
Match_print_mer (T this, char *queryseq_ptr, Genome_T genome, int stage1size) {
  char gbuffer1[MAXSTAGE1SIZE+1], gbuffer2[MAXSTAGE1SIZE+1], *genomicseg_ptr;
  Sequence_T genomicseg;
  int querypos, i, j;
  Genomicpos_T position;

  querypos = this->querypos;
  position = this->position;

  if (this->forwardp == true) {
    genomicseg = Genome_get_segment(genome,position,stage1size,/*revcomp*/false,gbuffer1,gbuffer2,stage1size);
  } else {
    genomicseg = Genome_get_segment(genome,position-(stage1size-1U),stage1size,/*revcomp*/true,gbuffer1,gbuffer2,stage1size);
  }
  genomicseg_ptr = Sequence_fullpointer(genomicseg);

  for (i = querypos; i < querypos + stage1size; i++) {
    printf("%c",queryseq_ptr[i]);
  }
  printf(" ");
  for (j = 0; j < stage1size; j++) {
    printf("%c",genomicseg_ptr[j]);
  }

  return;
}

