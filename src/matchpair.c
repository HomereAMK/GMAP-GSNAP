static char rcsid[] = "$Id: matchpair.c,v 1.45 2006/03/05 03:27:28 twu Exp $";
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "matchpair.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mem.h"
#include "list.h"
#include "listdef.h"
#include "matchdef.h"
#include "pair.h"
#include "pairdef.h"
#include "comp.h"

#define MAXCANDIDATES 10
#define MIN_STAGE1_FSUPPORT 0.20
#define MAX_STAGE1_STRETCH 2000.0


#ifdef DEBUG
#define debug(x) x
#else
#define debug(x)
#endif

/* Debugging of path finding */
#ifdef DEBUG1
#define debug1(x) x
#else
#define debug1(x)
#endif

/* Summary of clustering */
#ifdef DEBUG2
#define debug2(x) x
#else
#define debug2(x)
#endif

/* Filtering of poor matchpairs */
#ifdef DEBUG3
#define debug3(x) x
#else
#define debug3(x)
#endif



#define T Matchpair_T
struct T {
  Match_T bound5;
  Match_T bound3;
  unsigned int low;		/* low position */
  unsigned int high;		/* high position */
  int clustersize;		/* for cluster algorithm */
  int stage1size;
  int support;
  Matchpairend_T matchpairend;
  bool usep;
  bool watsonp;
};

Match_T
Matchpair_bound5 (T this) {
  return this->bound5;
}

Match_T
Matchpair_bound3 (T this) {
  return this->bound3;
}

bool
Matchpair_usep (T this) {
  return this->usep;
}

void
Matchpair_set_usep (T this) {
  this->usep = true;
  return;
}

void
Matchpair_clear_usep (T this) {
  this->usep = false;
  return;
}

bool
Matchpair_watsonp (T this) {
  return this->watsonp;
}

int
Matchpair_clustersize (T this) {
  return this->clustersize;
}

Matchpairend_T
Matchpair_matchpairend (T this) {
  return this->matchpairend;
}

int
Matchpair_support (T this) {
  return this->support;
}


static double
compute_fsupport (Match_T bound5, Match_T bound3, int stage1size, int trimlength) {
  return (double) (Match_querypos(bound3) - Match_querypos(bound5) + stage1size)/(double) trimlength;
}


static double
compute_stretch (Match_T bound5, Match_T bound3) {
  Genomicpos_T position5, position3;
  int querypos5, querypos3;

  querypos5 = Match_querypos(bound5);
  querypos3 = Match_querypos(bound3);
  
  if (querypos5 == querypos3) {
    return 1.0;
  } else {
    position5 = Match_position(bound5);
    position3 = Match_position(bound3);
    if (position3 > position5) {
#ifdef PMAP
      return (double) (position3 - position5)/(double) (querypos3 - querypos5)/3.0;
#else
      return (double) (position3 - position5)/(double) (querypos3 - querypos5);
#endif
    } else {
#ifdef PMAP
      return (double) (position5 - position3)/(double) (querypos3 - querypos5)/3.0;
#else
      return (double) (position5 - position3)/(double) (querypos3 - querypos5);
#endif
    }
  }
}

T
Matchpair_new (Match_T bound5, Match_T bound3, int stage1size, int clustersize, int trimlength,
	       Matchpairend_T matchpairend) {
  T new;
  unsigned int temp;

  debug3(printf("support = %d/trimlength = %d\n",
		Match_querypos(bound3) - Match_querypos(bound5) + stage1size,trimlength));
  debug3(printf("stretch = %f\n",compute_stretch(bound5,bound3)));

  if (clustersize == 1) {
    debug3(printf("clustersize is 1, so must be salvage\n"));
  } else if (compute_fsupport(bound5,bound3,stage1size,trimlength) < MIN_STAGE1_FSUPPORT) {
    debug3(printf("Insufficient coverage of the query sequence\n"));
    return NULL;
  } else if (compute_stretch(bound5,bound3) > MAX_STAGE1_STRETCH) {
    debug3(printf("Genomic region is too large relative to matching cDNA region\n"));
    return NULL;
  }

  new = (T) MALLOC(sizeof(*new));
  new->bound5 = bound5;
  new->bound3 = bound3;
  new->clustersize = clustersize;
  new->stage1size = stage1size;
  new->support = Match_querypos(bound3) - Match_querypos(bound5) + stage1size;
  new->matchpairend = matchpairend;
  new->usep = true;
  new->watsonp = Match_forwardp(bound5);

  new->low = Match_position(bound5);
  new->high = Match_position(bound3);
  if (new->high < new->low) {
    temp = new->high;
    new->high = new->low;
    new->low = temp;
  }
  
  return new;
}

void
Matchpair_free (T *old) {
  if (*old) {
    FREE(*old);
  }
  return;
}


/* Not intended for qsort.  Returns 0 when not comparable. */
static int
Matchpair_dominate (const void *a, const void *b) {
  T x = * (T *) a;
  T y = * (T *) b;
  
  if (x->watsonp != y->watsonp) {
    return 0;			/* Different strands */

  } else if (y->low > x->high || x->low > y->high) {
    return 0;			/* No overlap */
  } else if (x->low < y->low) {
    if (x->high < y->high) {
      return 0;			/* Crossing */
    } else {
      return -1;
    }
  } else if (y->low < x->low) {
    if (y->high < x->high) {
      return 0;			/* Crossing */
    } else {
      return 1;
    }
  } else {
    if (x->high < y->high) {
      return 1;
    } else if (y->high < x->high) {
      return -1;
    } else {
      return 0;			/* Equal */
    }
  }
}

static bool
Matchpair_equal (const void *a, const void *b) {
  T x = * (T *) a;
  T y = * (T *) b;
  
  if (x->watsonp != y->watsonp) {
    return false;		/* Different strands */

  } else if (y->low != x->low) {
    return false;
  } else if (y->high != x->high) {
    return false;
  } else {
    return true;
  }
}

int
Matchpair_size_cmp (const void *a, const void *b) {
  T x = * (T *) a;
  T y = * (T *) b;

  if (x->clustersize > y->clustersize) {
    return -1;
  } else if (x->clustersize < y->clustersize) {
    return +1;
  } else {
    return 0;
  }
}

int
Matchpair_support_cmp (const void *a, const void *b) {
  T x = * (T *) a;
  T y = * (T *) b;

  if (x->support > y->support) {
    return -1;
  } else if (x->support < y->support) {
    return +1;
  } else {
    return 0;
  }
}


List_T
Matchpair_filter_duplicates (List_T matchpairlist) {
  List_T unique = NULL, p, q;
  T x, y, matchpair;
  int n, i, j;
  bool *eliminate;

  n = List_length(matchpairlist);
  if (n == 0) {
    return NULL;
  }

  debug(
	for (p = matchpairlist, i = 0; p != NULL; p = p->rest, i++) {
	  matchpair = (T) p->first;
	  printf("  Initial %d: %u-%u (watsonp = %d)\n",
		 i,matchpair->low,matchpair->high,matchpair->watsonp);
	}
	);

  eliminate = (bool *) CALLOC(n,sizeof(bool));

  /* Check for duplicates */
  for (p = matchpairlist, i = 0; p != NULL; p = p->rest, i++) {
    x = (T) p->first;
    for (q = p->rest, j = i+1; q != NULL; q = q->rest, j++) {
      y = q->first;
      if (Matchpair_equal(&x,&y) == true) {
	eliminate[j] = true;
      }
    }
  }

  for (p = matchpairlist, i = 0; p != NULL; p = p->rest, i++) {
    if (eliminate[i] == false) {
      debug(matchpair = p->first);
      debug(printf("  Keeping %u-%u (watsonp = %d)\n",matchpair->low,matchpair->high,matchpair->watsonp));
      unique = List_push(unique,(void *) p->first);
    } else {
      matchpair = (T) p->first;
      debug(printf("  Eliminating %u-%u (watsonp = %d)\n",matchpair->low,matchpair->high,matchpair->watsonp));
      Matchpair_free(&matchpair);
    }
  }

  FREE(eliminate);
  List_free(&matchpairlist);

  debug(
	for (p = unique, i = 0; p != NULL; p = p->rest, i++) {
	  matchpair = (T) p->first;
	  printf("  Final %d: %u-%u (watsonp = %d)\n",
		 i,matchpair->low,matchpair->high,matchpair->watsonp);
	}
	);

  return unique;
}



List_T
Matchpair_filter_unique (List_T matchpairlist) {
  List_T unique = NULL, p, q;
  T x, y, matchpair;
  int n, i, j, cmp;
  bool *eliminate;

  n = List_length(matchpairlist);
  if (n == 0) {
    return NULL;
  }

  debug(
	for (p = matchpairlist, i = 0; p != NULL; p = p->rest, i++) {
	  matchpair = (T) p->first;
	  printf("  Initial %d: %u-%u (watsonp = %d)\n",
		 i,matchpair->low,matchpair->high,matchpair->watsonp);
	}
	);

  eliminate = (bool *) CALLOC(n,sizeof(bool));
  /* Check for subsumption */
  for (p = matchpairlist, i = 0; p != NULL; p = p->rest, i++) {
    x = (T) p->first;
    for (q = p->rest, j = i+1; q != NULL; q = q->rest, j++) {
      y = q->first;
      if ((cmp = Matchpair_dominate(&x,&y)) == -1) {
	eliminate[j] = true;
      } else if (cmp == 1) {
	eliminate[i] = true;
      }
    }
  }
    
  for (p = matchpairlist, i = 0; p != NULL; p = p->rest, i++) {
    if (eliminate[i] == false) {
      debug(matchpair = p->first);
      debug(printf("  Keeping %u-%u (watsonp = %d)\n",matchpair->low,matchpair->high,matchpair->watsonp));
      unique = List_push(unique,(void *) p->first);
    } else {
      matchpair = (T) p->first;
      debug(printf("  Eliminating %u-%u (watsonp = %d)\n",matchpair->low,matchpair->high,matchpair->watsonp));
      Matchpair_free(&matchpair);
    }
  }

  FREE(eliminate);
  List_free(&matchpairlist);

  debug(
	for (p = unique, i = 0; p != NULL; p = p->rest, i++) {
	  matchpair = (T) p->first;
	  printf("  Final %d: %u-%u (watsonp = %d)\n",
		 i,matchpair->low,matchpair->high,matchpair->watsonp);
	}
	);

  return unique;
}

void
Matchpair_get_coords (Genomicpos_T *chrpos, Genomicpos_T *genomicstart, Genomicpos_T *genomiclength,
		      bool *watsonp, T this, Stage1_T stage1, Genomicpos_T chrlength, int maxextension,
		      bool maponlyp) {
  Match_T match1, match2;
  Genomicpos_T chrpos1, genomicpos1, genomicpos2, left, right;

  if (Match_position(this->bound3) > Match_position(this->bound5)) {
    *watsonp = true;
    Stage1_find_extensions(&left,&right,stage1,this->bound5,this->bound3,maxextension,maponlyp);
    match1 = this->bound5;
    match2 = this->bound3;
  } else if (Match_position(this->bound3) < Match_position(this->bound5)) {
    *watsonp = false;
    Stage1_find_extensions(&right,&left,stage1,this->bound5,this->bound3,maxextension,maponlyp);
    match1 = this->bound3;
    match2 = this->bound5;
  } else {
    /* Special case of a single match */
    if (Match_forwardp(this->bound5) == true) {
      *watsonp = true;
      Stage1_find_extensions(&left,&right,stage1,this->bound5,this->bound3,maxextension,maponlyp);
    } else {
      *watsonp = false;
      Stage1_find_extensions(&right,&left,stage1,this->bound5,this->bound3,maxextension,maponlyp);
    }
    match1 = match2 = this->bound5;
  }

  if (Match_chrpos(match1) < left) {
    debug(printf("Proposed left is negative: %u < %u.  Problem!\n",
		 Match_chrpos(match1),left));
    chrpos1 = 0;		/* Match_chrpos(match1) - Match_chrpos(match1); */
    genomicpos1 = Match_position(match1) - Match_chrpos(match1);
  } else {
    debug(printf("Proposed left is non-negative: %u >= %u.  Okay.\n",
		 Match_chrpos(match1),left));
    chrpos1 = Match_chrpos(match1) - left;
    genomicpos1 = Match_position(match1) - left;
  }

  if (Match_chrpos(match2) + right >= chrlength) {
    debug(printf("Proposed right is too high: %u + %u >= %u.  Problem!\n",
		 Match_chrpos(match2),right,chrlength));
    /* chrpos2 = chrlength - 1; */  /* Match_chrpos(match2) + (chrlength - 1 - Match_chrpos(match2)); */
    genomicpos2 = Match_position(match2) + (chrlength - 1 - Match_chrpos(match2));
  } else {
    debug(printf("Proposed right is not too high: %u + %u < %u.  Okay.\n",
		 Match_chrpos(match2),right,chrlength));
    /* chrpos2 = Match_chrpos(match2) + right; */
    genomicpos2 = Match_position(match2) + right;
  }

  *genomicstart = genomicpos1;
  *genomiclength = genomicpos2 - genomicpos1 + 1U;
  *chrpos = chrpos1;
  debug(printf("Coordinates are %u, length %u\n",*genomicstart,*genomiclength));

  return;
}


/************************************************************************
 *   Clustering for stage 1
 ************************************************************************/

static T
find_best_path (int *size, Match_T *matches, int n, int stage1size, int minsize, 
		Match_T prevstart, Match_T prevend, int trimlength, bool plusp) {
  T matchpair;
  int *prev, *score, j, i, bestj, besti, bestscore, width;
  bool fivep = false, threep = false;
  Matchpairend_T matchpairend;
  Genomicpos_T endpos;

  prev = (int *) CALLOC(n,sizeof(int));
  score = (int *) CALLOC(n,sizeof(int));
  prev[0] = -1;

  debug1(
	 for (j = 0; j < n; j++) {
	   if (plusp == true) {
	     printf("+");
	   } else {
	     printf("-");
	   }
	   printf("#%d:%u (%d,%d,%d)\n",Match_chrnum(matches[j]),
		  Match_chrpos(matches[j]),Match_querypos(matches[j]),
		  Match_forwardp(matches[j]),Match_fivep(matches[j]));
	 }
	 );

  if (plusp == true) {
    for (j = 1; j < n; j++) {
      endpos = matches[j]->querypos;
      bestscore = 0;
      besti = -1;
      for (i = 0; i < j; i++) {
	if (matches[i]->querypos < endpos) {
	  if (score[i] + 1 > bestscore) {
	    bestscore = score[i] + 1;
	    besti = i;
	  }
	}
      }
      score[j] = bestscore;
      prev[j] = besti;
    }
  } else {
    for (j = 1; j < n; j++) {
      endpos = matches[j]->querypos;
      bestscore = 0;
      besti = -1;
      for (i = 0; i < j; i++) {
	if (matches[i]->querypos > endpos) {
	  if (score[i] + 1 > bestscore) {
	    bestscore = score[i] + 1;
	    besti = i;
	  }
	}
      }
      score[j] = bestscore;
      prev[j] = besti;
    }
  }

  bestscore = 0;
  bestj = 0;
  for (j = 0; j < n; j++) {
    if (score[j] > bestscore) {
      bestscore = score[j];
      bestj = j;
    }
  }

  /* Traceback */
  besti = bestj;
  if (Match_fivep(matches[besti]) == true) {
    fivep = true;
  } else {
    threep = true;
  }
  while (prev[besti] >= 0) {
    besti = prev[besti];
    if (Match_fivep(matches[besti]) == true) {
      fivep = true;
    } else {
      threep = true;
    }
  }

  debug1(printf("Best path is %d to %d, size=%d\n",besti,bestj,bestscore+1));

  if ((*size = bestscore + 1) < minsize) {
    debug1(printf("Size is smaller than minsize of %d.  Discarding.\n",minsize));
    matchpair = NULL;
  } else {
    width = bestj - besti + 1;
    if (width > 2*bestscore) {
      /* Penalize for finding a path in a repetitive region */
      debug1(printf("Bestscore being penalized because of width %d from %d to ",width,bestscore));
      bestscore -= (int) sqrt((double) (width - bestscore));
      debug1(printf("%d\n",bestscore));
    }
    if (matches[besti] == prevstart && matches[bestj] == prevend) {
      debug1(printf("Path equal to previous one.  Discarding.\n"));
      matchpair = NULL;
    } else if ((*size = bestscore + 1) < minsize) {
      debug1(printf("Size is smaller than minsize of %d.  Discarding.\n",minsize));
      matchpair = NULL;
    } else {
      if (fivep == true && threep == true) {
	matchpairend = MIXED;
      } else if (fivep == true) {
	matchpairend = FIVEONLY;
      } else if (threep == true) {
	matchpairend = THREEONLY;
      } else {
	abort();
      }
      if (plusp == true) {
	matchpair = Matchpair_new(matches[besti],matches[bestj],stage1size,bestscore+1,trimlength,matchpairend);
      } else {
	matchpair = Matchpair_new(matches[bestj],matches[besti],stage1size,bestscore+1,trimlength,matchpairend);
      }
    }
    debug1(printf("\n"));
  }

  FREE(score);
  FREE(prev);

  return matchpair;
}

static List_T
find_paths_bounded (int *bestsize, List_T clusterlist, Match_T *matches, int npositions, 
		    int maxintronlen, int stage1size, int minclustersize, double sizebound, 
		    int trimlength, bool plusp) {
  int starti, endi;
  Chrnum_T startchrnum;
  Match_T prevstart = NULL, prevend = NULL;
  T matchpair;
  Genomicpos_T startpos;
  int minsize, size;

  minsize = minclustersize;
  if ((int) ((*bestsize) * sizebound) > minsize) {
    minsize = (int) ((*bestsize) * sizebound);
  }
  debug2(printf("minsize = %d\n",minsize));

  endi = -1;
  for (starti = 0; starti < npositions - 1; starti++) {
    startchrnum = Match_chrnum(matches[starti]);
    startpos = Match_position(matches[starti]);
    while (endi+1 < npositions && Match_chrnum(matches[endi+1]) == startchrnum &&
	   Match_position(matches[endi+1]) < startpos+maxintronlen) {
      endi = endi+1;
    }
    if (endi - starti + 1 >= minsize) {
      if ((matchpair = find_best_path(&size,&(matches[starti]),endi-starti+1,stage1size,
				      minsize,prevstart,prevend,trimlength,plusp)) == NULL) {
	prevstart = prevend = NULL;
      } else {
	clusterlist = List_push(clusterlist,(void *) matchpair);
	if (plusp == true) {
	  prevstart = matchpair->bound5;
	  prevend = matchpair->bound3;
	} else {
	  prevstart = matchpair->bound3;
	  prevend = matchpair->bound5;
	}
	if (size > *bestsize) {
	  *bestsize = size;
	  if (((int) (*bestsize) * sizebound) > minsize) {
	    minsize = (int) ((*bestsize) * sizebound);
	    debug2(printf("resetting minsize = %d\n",minsize));
	  }
	}
      }
    }
  }

  return clusterlist;
}


static void
separate_strands (Match_T **plus_matches, int *plus_npositions,
		  Match_T **minus_matches, int *minus_npositions,
		  List_T matches5, List_T matches3, int querylen) {
  List_T p;
  Match_T match, *plus_candidates, *minus_candidates;
  int plus_ncandidates = 0, minus_ncandidates = 0, i = 0, j = 0, nkeep;
  bool *keepp;

  /* Count */
  for (p = matches5; p != NULL; p = p->rest) {
    match = (Match_T) p->first;
    if (Match_forwardp(match) == true) {
      plus_ncandidates++;
    } else {
      minus_ncandidates++;
    }
  }

  for (p = matches3; p != NULL; p = p->rest) {
    match = (Match_T) p->first;
    if (Match_forwardp(match) == true) {
      plus_ncandidates++;
    } else {
      minus_ncandidates++;
    }
  }

  /* Allocate */
  if (plus_ncandidates == 0) {
    plus_candidates = NULL;
  } else {
    plus_candidates = (Match_T *) CALLOC(plus_ncandidates,sizeof(Match_T));
  }

  if (minus_ncandidates == 0) {
    minus_candidates = NULL;
  } else {
    minus_candidates = (Match_T *) CALLOC(minus_ncandidates,sizeof(Match_T));
  }

  /* Fill */
  for (p = matches5; p != NULL; p = p->rest) {
    match = (Match_T) p->first;
    if (Match_forwardp(match) == true) {
      plus_candidates[i++] = match;
    } else {
      minus_candidates[j++] = match;
    }
  }

  for (p = matches3; p != NULL; p = p->rest) {
    match = (Match_T) p->first;
    if (Match_forwardp(match) == true) {
      plus_candidates[i++] = match;
    } else {
      minus_candidates[j++] = match;
    }
  }

  /* Sort */
  if (plus_candidates) {
    qsort(plus_candidates,plus_ncandidates,sizeof(Match_T),Match_cmp);
  }
  if (minus_candidates) {
    qsort(minus_candidates,minus_ncandidates,sizeof(Match_T),Match_cmp);
  }

  /* Filter plus candidates based on queryseq length */
  if (plus_ncandidates <= 1) {
    *plus_npositions = 0;
    *plus_matches = NULL;

  } else {
    keepp = (bool *) CALLOC(plus_ncandidates,sizeof(bool));
    for (i = 0; i < plus_ncandidates - 1; i++) {
      if (Match_chrnum(plus_candidates[i+1]) == Match_chrnum(plus_candidates[i]) &&
	  Match_chrpos(plus_candidates[i+1]) < Match_chrpos(plus_candidates[i]) + querylen) {
	keepp[i] = true;
	keepp[i+1] = true;
      }
    }

    nkeep = 0;
    for (i = 0; i < plus_ncandidates; i++) {
      if (keepp[i] == true) {
	nkeep++;
      }
    }

    if (nkeep == 0) {
      *plus_npositions = 0;
      *plus_matches = NULL;
    } else {
      *plus_npositions = nkeep;
      *plus_matches = (Match_T *) CALLOC(nkeep,sizeof(Match_T));
      j = 0;
      for (i = 0; i < plus_ncandidates; i++) {
	if (keepp[i] == true) {
	  (*plus_matches)[j++] = plus_candidates[i];
	}
      }
    }
    FREE(keepp);
  }

  /* Filter minus candidates based on queryseq length */
  if (minus_ncandidates <= 1) {
    *minus_npositions = 0;
    *minus_matches = NULL;

  } else {
    keepp = (bool *) CALLOC(minus_ncandidates,sizeof(bool));
    for (i = 0; i < minus_ncandidates - 1; i++) {
      if (Match_chrnum(minus_candidates[i+1]) == Match_chrnum(minus_candidates[i]) &&
	  Match_chrpos(minus_candidates[i+1]) < Match_chrpos(minus_candidates[i]) + querylen) {
	keepp[i] = true;
	keepp[i+1] = true;
      }
    }

    nkeep = 0;
    for (i = 0; i < minus_ncandidates; i++) {
      if (keepp[i] == true) {
	nkeep++;
      }
    }

    if (nkeep == 0) {
      *minus_npositions = 0;
      *minus_matches = NULL;
    } else {
      *minus_npositions = nkeep;
      *minus_matches = (Match_T *) CALLOC(nkeep,sizeof(Match_T));
      j = 0;
      for (i = 0; i < minus_ncandidates; i++) {
	if (keepp[i] == true) {
	  (*minus_matches)[j++] = minus_candidates[i];
	}
      }
    }
    FREE(keepp);
  }

  if (plus_candidates) {
    FREE(plus_candidates);
  }
  if (minus_candidates) {
    FREE(minus_candidates);
  }

  return;
}


static List_T
bound_results_testing (List_T clusterlist, int bestsize, double sizebound) {
  List_T boundedlist = NULL, p;
  T matchpair;
  int minsize;

  minsize = (int) (bestsize * sizebound);
  debug2(printf("minsize = %d\n",minsize));
  for (p = clusterlist; p != NULL; p = p->rest) {
    matchpair = (T) List_head(p);
    if (matchpair->clustersize >= minsize) {
      boundedlist = List_push(boundedlist,(void *) matchpair);
    } else {
      Matchpair_free(&matchpair);
    }
  }
  List_free(&clusterlist);

  boundedlist = Matchpair_filter_unique(boundedlist);
  boundedlist = Matchpair_filter_duplicates(boundedlist);

  return boundedlist;
}


static List_T
bound_results_limited (List_T clusterlist, int bestsize, double sizebound) {
  List_T boundedlist = NULL, p;
  T matchpair;
  int *counts, minsize, total;

  clusterlist = Matchpair_filter_unique(clusterlist);
  clusterlist = Matchpair_filter_duplicates(clusterlist);

  counts = (int *) CALLOC(bestsize+1,sizeof(int));
  for (p = clusterlist; p != NULL; p = p->rest) {
    matchpair = (T) List_head(p);
    counts[matchpair->clustersize]++;
  }

  minsize = bestsize-1;
  total = counts[bestsize] + counts[minsize];
  debug2(printf("Setting minsize = %d, total = %d\n",minsize,total));
  while (minsize-1 >= 0 && minsize-1 >= (int) (bestsize * sizebound) &&
	 total+counts[minsize-1] <= MAXCANDIDATES) {
    total = total+counts[minsize-1];
    minsize = minsize-1;
    debug2(printf("Setting minsize = %d, total = %d\n",minsize,total));
  }
  FREE(counts);

  for (p = clusterlist; p != NULL; p = p->rest) {
    matchpair = (T) List_head(p);
    if (matchpair->clustersize >= minsize) {
      boundedlist = List_push(boundedlist,(void *) matchpair);
    } else {
      Matchpair_free(&matchpair);
    }
  }
  List_free(&clusterlist);
  
  return boundedlist;
}


static void
print_clusters (List_T list) {
  List_T p;
  T matchpair;

  for (p = list; p != NULL; p = p->rest) {
    matchpair = (T) List_head(p);
    if (matchpair->watsonp == true) {
      printf("+");
    } else {
      printf("-");
    }
    printf("#%d:%u - %u (%d entries)\n",
	   Match_chrnum(matchpair->bound5),
	   Match_chrpos(matchpair->bound5),Match_chrpos(matchpair->bound3),
	   matchpair->clustersize);
  }
  return;
}


List_T
Matchpair_find_clusters (List_T matches5, List_T matches3, int stage1size, 
			 int maxintronlen, int minclustersize, double sizebound, 
			 int trimlength, Boundmethod_T boundmethod, int querylen) {
  List_T boundedlist = NULL, clusterlist = NULL, p;
  Match_T *plus_matches = NULL, *minus_matches = NULL;
  T matchpair;
  int plus_npositions, minus_npositions, bestsize = 0;

  separate_strands(&plus_matches,&plus_npositions,&minus_matches,&minus_npositions,matches5,matches3,querylen);

  if (plus_npositions > 0) {
    clusterlist = find_paths_bounded(&bestsize,clusterlist,plus_matches,plus_npositions,maxintronlen,
				     stage1size,minclustersize,sizebound,trimlength,/*plusp*/true);
  }

  if (minus_npositions > 0) {
    clusterlist = find_paths_bounded(&bestsize,clusterlist,minus_matches,minus_npositions,maxintronlen,
				     stage1size,minclustersize,sizebound,trimlength,/*plusp*/false);
  }
	
  if (plus_matches != NULL) {
    FREE(plus_matches);
  }
  if (minus_matches != NULL) {
    FREE(minus_matches);
  }

  if (bestsize == 0) {
    return clusterlist;
  } else if (boundmethod == NO_BOUND) {
    return clusterlist;
  } else if (boundmethod == MOVING_THRESHOLD) {
    boundedlist = bound_results_limited(clusterlist,bestsize,sizebound);

    debug2(printf("Bounded clusters (%d):\n",List_length(boundedlist)));
    debug2(print_clusters(boundedlist));
    return boundedlist;

  } else if (boundmethod == BY_CANDIDATES) {
    boundedlist = bound_results_testing(clusterlist,bestsize,sizebound);

    if (List_length(boundedlist) <= MAXCANDIDATES) {
      debug2(printf("Enough clusters (%d)\n",List_length(boundedlist)));
      debug2(print_clusters(boundedlist));
      return boundedlist;

    } else {
      debug2(printf("Too many clusters (%d)\n",List_length(boundedlist)));
      debug2(print_clusters(boundedlist));

      for (p = boundedlist; p != NULL; p = p->rest) {
	matchpair = (T) List_head(p);
	Matchpair_free(&matchpair);
      }
      List_free(&boundedlist);
      
      return NULL;
    }
  } else {
    abort();
  }
}


List_T
Matchpair_salvage_hits (List_T matches5, List_T matches3, int stage1size, int trimlength) {
  List_T list = NULL, p;
  Match_T match;
  T matchpair;

  for (p = matches5; p != NULL; p = p->rest) {
    match = (Match_T) p->first;
    matchpair = Matchpair_new(match,match,stage1size,/*clustersize*/1,trimlength,
			      /*matchpairend*/FIVEONLY);
    if (matchpair != NULL) {
      list = List_push(list,(void *) matchpair);
    }
  }

  for (p = matches3; p != NULL; p = p->rest) {
    match = (Match_T) p->first;
    matchpair = Matchpair_new(match,match,stage1size,/*clustersize*/1,trimlength,
			      /*matchpairend*/THREEONLY);
    if (matchpair != NULL) {
      list = List_push(list,(void *) matchpair);
    }
  }

  return list;
}


List_T
Matchpair_make_path (T this, char *queryseq_ptr, Pairpool_T pairpool, Genome_T genome, 
		     Genomicpos_T chroffset, Genomicpos_T chrpos, char *gbuffer1, char *gbuffer2, int gbufferlen) {
  List_T path = NULL;
  Pair_T pair;
  Match_T start, end;
  char *genomicseg_ptr;
  Sequence_T genomicseg;
  int querypos1, querypos2, queryjump, genomejump, i, j;
  Genomicpos_T position1, position2, genomepos;

  start = this->bound5;
  end = this->bound3;

  querypos1 = start->querypos;
  querypos2 = end->querypos;
  
  position1 = start->position;
  position2 = end->position;

  /* Handle the 5' end */
  if (this->watsonp == true) {
    genomicseg = Genome_get_segment(genome,position1,this->stage1size,/*revcomp*/false,gbuffer1,gbuffer2,gbufferlen);
  } else {
    genomicseg = Genome_get_segment(genome,position1-(this->stage1size-1U),this->stage1size,/*revcomp*/true,gbuffer1,gbuffer2,gbufferlen);
  }
  genomicseg_ptr = Sequence_fullpointer(genomicseg);

  genomepos = position1 - chroffset - chrpos;
  for (i = querypos1, j = 0; i < querypos1 + this->stage1size; i++, j++) {
    /* printf("%c %c\n",queryseq_ptr[i],genomicseg_ptr[j]); */
    path = Pairpool_push(path,pairpool,i,genomepos,queryseq_ptr[i],MATCH_COMP,genomicseg_ptr[j],/*gapp*/false);
    if (this->watsonp == true) {
      genomepos += 1U;
    } else {
      genomepos -= 1U;
    }
  }
  Sequence_free(&genomicseg);

  /* Handle the gap */
  queryjump = querypos2 - querypos1;
  genomejump = position2 - position1;
  path = Pairpool_push_gap(path,pairpool,queryjump,genomejump);
  pair = (Pair_T) path->first;
  pair->comp = DUALBREAK_COMP;
  /* printf("\n"); */

  /* Handle the 3' end */
  if (this->watsonp == true) {
    genomicseg = Genome_get_segment(genome,position2,this->stage1size,/*revcomp*/false,gbuffer1,gbuffer2,gbufferlen);
  } else {
    genomicseg = Genome_get_segment(genome,position2-(this->stage1size-1U),this->stage1size,/*revcomp*/true,gbuffer1,gbuffer2,gbufferlen);
  }
  genomicseg_ptr = Sequence_fullpointer(genomicseg);

  genomepos = position2 - chroffset - chrpos;
  for (i = querypos2, j = 0; i < querypos2 + this->stage1size; i++, j++) {
    /* printf("%c %c\n",queryseq_ptr[i],genomicseg_ptr[j]); */
    path = Pairpool_push(path,pairpool,i,genomepos,queryseq_ptr[i],MATCH_COMP,genomicseg_ptr[j],/*gapp*/false);
    if (this->watsonp == true) {
      genomepos += 1U;
    } else {
      genomepos -= 1U;
    }
  }
  Sequence_free(&genomicseg);

  return path;
}
