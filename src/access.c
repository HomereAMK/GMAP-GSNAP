static char rcsid[] = "$Id: access.c,v 1.8 2005/12/02 22:57:21 twu Exp $";
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "access.h"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

/* <unistd.h> and <sys/types.h> included in access.h */
#include <sys/mman.h>		/* For mmap */
#ifdef HAVE_FCNTL_H
#include <fcntl.h>		/* For open */
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>		/* For open and fstat */
#endif
/* Not sure why this was included
#include <errno.h>
*/
#ifdef PAGESIZE_VIA_SYSCTL
#include <sys/sysctl.h>
#endif

#include "assert.h"
#include "mem.h"
#include "types.h"
#include "fopen.h"
#include "stopwatch.h"

#ifdef WORDS_BIGENDIAN
#include "bigendian.h"
#else
#include "littleendian.h"
#endif


#ifdef DEBUG
#define debug(x) x
#else
#define debug(x)
#endif


off_t
Access_filesize (char *filename) {
  struct stat sb;

  stat(filename,&sb);
  return sb.st_size;
}

int
Access_fileio (char *filename) {
  int fd;

  if ((fd = open(filename,O_RDONLY,0764)) < 0) {
    fprintf(stderr,"Error: can't open file %s\n",filename);
    exit(9);
  }
  return fd;
}

int
Access_fileio_rw (char *filename) {
  int fd;

  if ((fd = open(filename,O_RDWR | O_CREAT | O_TRUNC,0764)) < 0) {
    fprintf(stderr,"Error: can't open file %s for reading/writing\n",filename);
    exit(9);
  }
  return fd;
}

/* Bigendian conversion not needed */
void *
Access_allocated (size_t *len, double *seconds, char *filename, size_t eltsize) {
  void *memory;
  int fd;
  FILE *fp;

  *len = (size_t) Access_filesize(filename);

  if ((fp = FOPEN_READ_BINARY(filename)) == NULL) {
    fprintf(stderr,"Error: can't open file %s\n",filename);
    exit(9);
  }

  Stopwatch_start();
  memory = (void *) MALLOC(*len);
  FREAD_UINTS(memory,(*len)/eltsize,fp);

  /* Note: the following (old non-batch mode) requires conversion to bigendian later, as needed */
  /* fread(new->offsets,eltsize,sb.st_size/eltsize,fp); */

  *seconds = Stopwatch_stop();

  fclose(fp);

  return memory;
}



#ifdef HAVE_MMAP
/* Returns NULL if mmap fails.  Bigendian conversion required */
#ifdef HAVE_CADDR_T
caddr_t
#else
void *
#endif
Access_mmap (int *fd, size_t *len, char *filename, size_t eltsize, bool randomp) {
  off_t length;
#ifdef HAVE_CADDR_T
  caddr_t memory;
#else
  void *memory;
#endif

  if ((*len = length = Access_filesize(filename)) == 0U) {
    fprintf(stderr,"Error: file %s is empty\n",filename);
    exit(9);
  } 

  if ((*fd = open(filename,O_RDONLY,0764)) < 0) {
    fprintf(stderr,"Error: can't open file %s\n",filename);
    exit(9);
  }

  if (sizeof(size_t) <= 4 && length > MAX32BIT) {
    debug(printf("Too big to mmap\n"));
    *len = 0;
    memory = NULL;
  } else {
    *len = (size_t) length;
    memory = mmap(NULL,length,PROT_READ,0
#ifdef HAVE_MMAP_MAP_SHARED
		  |MAP_SHARED
#endif
#ifdef HAVE_MMAP_MAP_FILE
		  |MAP_FILE
#endif
#ifdef HAVE_MMAP_MAP_VARIABLE
		  |MAP_VARIABLE
#endif
		  ,*fd,0);
    if (memory == MAP_FAILED) {
      debug(printf("Got MAP_FAILED on len %lu from length %lu\n",*len,length));
      memory = NULL;
    } else if (randomp == true) {
      debug(printf("Got mmap of %d bytes at %p to %p\n",length,memory,memory+length-1));
#ifdef HAVE_MADVISE
#ifdef HAVE_MADVISE_MADV_RANDOM
      madvise(memory,*len,MADV_RANDOM);
#endif
#endif
    } else {
      debug(printf("Got mmap of %d bytes at %p to %p\n",length,memory,memory+length-1));
#ifdef HAVE_MADVISE
#ifdef HAVE_MADVISE_MADV_DONTNEED
      madvise(memory,*len,MADV_DONTNEED);
#endif
#endif
    }
  }

  return memory;
}
#endif


#ifdef HAVE_MMAP
/* Returns NULL if mmap fails.  Bigendian conversion required */
#ifdef HAVE_CADDR_T
caddr_t
#else
void *
#endif
Access_mmap_rw (int *fd, size_t *len, char *filename, size_t eltsize, bool randomp) {
  off_t length;
#ifdef HAVE_CADDR_T
  caddr_t memory;
#else
  void *memory;
#endif

  if ((*len = length = Access_filesize(filename)) == 0U) {
    fprintf(stderr,"Error: file %s is empty\n",filename);
    exit(9);
  }

  if ((*fd = open(filename,O_RDWR,0764)) < 0) {
    fprintf(stderr,"Error: can't open file %s\n",filename);
    exit(9);
  }

  if (sizeof(size_t) <= 4 && length > MAX32BIT) {
    debug(printf("Too big to mmap\n"));
    *len = 0;
    memory = NULL;
  } else {
    *len = (size_t) length;
    memory = mmap(NULL,length,PROT_READ|PROT_WRITE,0
#ifdef HAVE_MMAP_MAP_SHARED
		  |MAP_SHARED
#endif
#ifdef HAVE_MMAP_MAP_FILE
		  |MAP_FILE
#endif
#ifdef HAVE_MMAP_MAP_VARIABLE
		  |MAP_VARIABLE
#endif
		  ,*fd,0);
    if (memory == MAP_FAILED) {
      debug(printf("Got MAP_FAILED on len %lu from length %lu\n",*len,length));
      memory = NULL;
    } else if (randomp == true) {
      debug(printf("Got mmap of %d bytes at %p to %p\n",length,memory,memory+length-1));
#ifdef HAVE_MADVISE
#ifdef HAVE_MADVISE_MADV_RANDOM
      madvise(memory,*len,MADV_RANDOM);
#endif
#endif
    } else {
      debug(printf("Got mmap of %d bytes at %p to %p\n",length,memory,memory+length-1));
#ifdef HAVE_MADVISE
#ifdef HAVE_MADVISE_MADV_DONTNEED
      madvise(memory,*len,MADV_DONTNEED);
#endif
#endif
    }
  }

  return memory;
}
#endif


#ifdef HAVE_MMAP

#define PAGESIZE 1024*4

#ifdef HAVE_CADDR_T
caddr_t
#else
void *
#endif
Access_mmap_and_preload (int *fd, size_t *len, int *npages, double *seconds, char *filename, size_t eltsize) {
  off_t length;
#ifdef HAVE_CADDR_T
  caddr_t memory;
#else
  void *memory;
#endif
  int pagesize, indicesperpage;
  size_t totalindices, i;	/* Needs to handle uncompressed genomes > 2 gigabytes */
  int nzero = 0, npos = 0;

#ifdef PAGESIZE_VIA_SYSCTL
  size_t pagelen;
  int mib[2];
#endif

  if ((*len = length = Access_filesize(filename)) == 0U) {
    fprintf(stderr,"Error: file %s is empty\n",filename);
    exit(9);
  }

  if ((*fd = open(filename,O_RDONLY,0764)) < 0) {
    fprintf(stderr,"Error: can't open file %s\n",filename);
    exit(9);
  }

  if (sizeof(size_t) <= 4 && *len > MAX32BIT) {
    debug(printf("Too big to mmap\n"));
    *len = 0;
    *npages = 0;
    *seconds = 0.0;
    memory = NULL;

  } else {

#ifdef __STRICT_ANSI__
    pagesize = PAGESIZE;
#elif defined(HAVE_GETPAGESIZE)
    pagesize = getpagesize();
#elif defined(PAGESIZE_VIA_SYSCONF)
    pagesize = (int) sysconf(_SC_PAGESIZE);
#elif defined(PAGESIZE_VIA_SYSCTL)
    pagelen = sizeof(pagesize);
    mib[0] = CTL_HW;
    mib[1] = HW_PAGESIZE;
    sysctl(mib,2,&pagesize,&pagelen,NULL,0);
#else
    pagesize = PAGESIZE;
#endif

    indicesperpage = pagesize/eltsize;
    Stopwatch_start();

    memory = mmap(NULL,length,PROT_READ,0
#ifdef HAVE_MMAP_MAP_SHARED
		  |MAP_SHARED
#endif
#ifdef HAVE_MMAP_MAP_FILE
		  |MAP_FILE
#endif
#ifdef HAVE_MMAP_MAP_VARIABLE
		  |MAP_VARIABLE
#endif
		  ,*fd,0);
    if (memory == MAP_FAILED) {
      debug(printf("Got MAP_FAILED on len %lu from length %lu\n",*len,length));
      memory = NULL;
      Stopwatch_stop();
    } else {
      /* Touch all pages */
      debug(printf("Got mmap of %d bytes at %p to %p\n",length,memory,memory+length-1));
#ifdef HAVE_MADVISE
#ifdef HAVE_MADVISE_MADV_WILLNEED
      madvise(memory,*len,MADV_WILLNEED);
#endif
#endif
      totalindices = (*len)/eltsize;
      for (i = 0; i < totalindices; i += indicesperpage) {
	if (memory[i] == 0U) {
	  nzero++;
	} else {
	  npos++;
	}
	if (i % 10000 == 0) {
	  fprintf(stderr,".");
	}
      }
      *npages = nzero + npos;
      *seconds = Stopwatch_stop();
    }
  }

  return memory;
}
#endif

