// This code is heavily based on code from dvd+rw-tools 
// by Andy Polyakov <appro@fy.chalmers.se>
//
// For further details see http://fy.chalmers.se/~appro/linux/DVD+RW/
//
// Explanation of the error messages can be found at:
// http://fy.chalmers.se/~appro/linux/DVD+RW/keys.txt

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE 
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#if defined(__linux)
/* ... and "engage" glibc large file support */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>
#include <glib.h>

#include "transport.hxx"

#ifdef LIBSTDCXX_HACK
/* Some C++ stuff needed when we not link to libstdc++ */
void *operator new (size_t sz)
{
	void *ret = malloc (sz);
	if (ret == NULL)
	{
		fputs ("libnautilus-burn memory allocation failed\n", stderr);
		exit (1);
	}
	return ret;
}

void *operator new[] (size_t sz)
{
	return ::operator new(sz);
}

void
operator delete (void *ptr)
{
	free (ptr);
}
#endif /* LIBSTDCXX_HACK */

/* Returns:
 * -1: not a DVD+RW or DVD+R
 * 0: DVD+R
 * 1: DVD+RW
 * 2: both DVD+R and DVD+RW
 */
extern "C"
int get_dvd_r_rw_profile (const char *name)
{
  Scsi_Command cmd;
  int retval = -1;
  unsigned char page[20];
  unsigned char *sense=cmd.sense();
  int i;

  if (!cmd.associate((char *)name))
      return -1;

  /* For valgrind */
  memset (&page, 0, sizeof (page));

  cmd[0]=0x46;
  cmd[1]=2;
  cmd[8]=8;
  cmd[9]=0;
  if (cmd.transport(READ,page,8))
  {   /* if (sense[0]==0) perror("Unable to ioctl");
	  else             fprintf(stderr,"GET CONFIGURATION failed with "
			  "SK=%xh/ASC=%02xh/ASCQ=%02xh\n",
			  sense[2]&0xF,sense[12],sense[13]); */
	  goto bail;
  }

  // See if it's 2 gen drive by checking if DVD+R profile is an option
  {
    int len=4+(page[0]<<24|page[1]<<16|page[2]<<8|page[3]);
    if (len>264)
    {
      /* fprintf (stderr,"insane profile list length [%d].\n",len); */
      goto bail;
    }
    unsigned char *list=new unsigned char[len];

    /* For valgrind */
    memset (list, 0, len);

    cmd[0]=0x46;
    cmd[1]=2;
    cmd[7]=len>>8;
    cmd[8]=len;
    cmd[9]=0;
    if (cmd.transport(READ,list,len))
    {
      /* fprintf(stderr,"GET CONFIGURATION failed with "
		      "SK=%xh/ASC=%02xh/ASCQ=%02xh\n",
		      sense[2]&0xF,sense[12],sense[13]); */
      goto bail;
    }

    if (len <= 12 || list[11] > len)
    {
      /* fprintf(stderr, "GET CONFIGURATION with len %d, and list[11] %d\n",
                         len, list[11]); */
      goto bail;
    }

    for (i=12;i<list[11];i+=4)
    {
      int profile = (list[i]<<8|list[i+1]);
      /* 0x1B: DVD+R
       * 0x1A: DVD+RW
       */
      if (profile == 0x1B)
      {
        if (retval == 1)
          retval = 2;
	else
          retval = 0;
      } else if (profile == 0x1A) {
        if (retval == 0)
          retval = 2;
	else
          retval = 1;
      }
    }
  }

bail:

  return retval;
}

extern "C"
int get_mmc_profile (int fd)
{
  Scsi_Command cmd(fd);
  int retval = -1;
  unsigned char page[20];
  unsigned char *sense=cmd.sense();
  int i;

  /* For valgrind */
  memset (&page, 0, sizeof (page));

  cmd[0]=0x46;
  cmd[1]=2;
  cmd[8]=8;
  cmd[9]=0;
  if (cmd.transport(READ,page,8))
  {    /* if (sense[0]==0) perror("Unable to ioctl");
	  else             fprintf(stderr,"GET CONFIGURATION failed with "
			  "SK=%xh/ASC=%02xh/ASCQ=%02xh\n",
			  sense[2]&0xF,sense[12],sense[13]); */
	  goto bail;
  }

  // See if it's 2 gen drive by checking if DVD+R profile is an option
  {
    int len=4+(page[0]<<24|page[1]<<16|page[2]<<8|page[3]);
    if (len>264)
    {
      /* fprintf (stderr,"insane profile list length [%d].\n",len); */
      goto bail;
    }
    unsigned char *list=new unsigned char[len];

    cmd[0]=0x46;
    cmd[1]=2;
    cmd[7]=len>>8;
    cmd[8]=len;
    cmd[9]=0;
    if (cmd.transport(READ,list,len))
    {
      /* fprintf(stderr,"GET CONFIGURATION failed with "
		      "SK=%xh/ASC=%02xh/ASCQ=%02xh\n",
		      sense[2]&0xF,sense[12],sense[13]); */
      goto bail;
    }
  }

  return page[6]<<8|page[7];

bail:

  return retval;

}

extern "C"
int get_disc_status (int fd, int *empty, int *is_rewritable)
{
  Scsi_Command cmd(fd);
  int retval = -1;
  unsigned char page[32];
  unsigned char *sense=cmd.sense();
  int i;

  *empty = 0;
  *is_rewritable = 0;

  /* For valgrind */
  memset (&page, 0, sizeof (page));

  cmd[0]=0x51;
  cmd[8]=32;
  cmd[9]=0;
  if (cmd.transport(READ,page,32))
  {     
    if (((sense[2]&0xF) == 2) && 
	(sense[12] == 0x3a)) {
      /* MEDIUM NOT PRESENT */
      *empty = 1;
      return 0;
    }

    /*if (sense[0]==0) perror("Unable to ioctl");
    else             fprintf(stderr,"READ DISC INFORMATION failed with "
			     "SK=%xh/ASC=%02xh/ASCQ=%02xh\n",
			     sense[2]&0xF,sense[12],sense[13]); */
    goto bail;
  }
  
  *is_rewritable = (page[2] & 0x10) != 0;

  return 0;

bail:

  return retval;

}


extern "C"
int get_disc_size_cd (int fd)
{
  Scsi_Command cmd(fd);
  int retval = -1;
  unsigned char toc[4], *atip;
  int len;

  /* For valgrind */
  memset (&toc, 0, sizeof (toc));

  cmd[0]=0x43; /* READ_TOC */
  cmd[2]=4 & 0x0F;  /* FMT_ATIP */
  cmd[6]=0;
  cmd[8]=4;
  cmd[9]=0;

  if (cmd.transport(READ,toc,4))
  {    
	  goto bail;
  }

  len=2+(toc[0] << 8 | toc[1]);
  atip=new unsigned char[len];
  cmd[0]=0x43; /* READ_TOC */
  cmd[2]=4 & 0x0F;  /* FMT_ATIP */
  cmd[6]=0;
  cmd[7]=len >> 8;
  cmd[8]=len;        /* Real length */
  cmd[9]=0;
  if (cmd.transport(READ,atip,len))
  {
    goto bail;
  }

  return atip[12]*60+atip[13]+(atip[14]/75+1);

bail:

  return retval;
}

static unsigned int get_2k_capacity (Scsi_Command &cmd, int mmc_profile)
{ unsigned char	buf[32];
  unsigned int	ret=0;
  unsigned int	nwa,free_blocks;
  unsigned char formats[260];
  int		i,obligatory,len,err;
  /* Wild guess */
  int           next_track = 1;

retry:
  if (mmc_profile == 0x1A || mmc_profile == 0x13 || mmc_profile == 0x12) {
    memset (&formats, 0, sizeof (formats));
    cmd[0] = 0x23;      // READ FORMAT CAPACITIES
    cmd[8] = 12;
    cmd[9] = 0;
    if ((err=cmd.transport (READ,formats,12))) {
      /* sperror ("READ FORMAT CAPACITIES",err),
	 exit (FATAL_START(errno)); */
      return 0;
    }

    len = formats[3];
    if (len&7 || len<16) {
      /* fprintf (stderr,":-( FORMAT allocaion length isn't sane"),
	 exit (FATAL_START(EINVAL)); */
      return 0;
    }

    cmd[0] = 0x23;      // READ FORMAT CAPACITIES
    cmd[7] = (4+len)>>8;
    cmd[8] = (4+len)&0xFF;
    cmd[9] = 0;
    if ((err=cmd.transport (READ,formats,4+len))) {
      /* sperror ("READ FORMAT CAPACITIES",err),
	 exit (FATAL_START(errno)); */
      return 0;
    }

    if (len != formats[3]) {
      /* fprintf (stderr,":-( parameter length inconsistency\n"),
	 exit(FATAL_START(EINVAL)); */
      return 0;
    }
  }

    obligatory=0x00;
    switch (mmc_profile&0xFFFF)
    {	case 0x1A:		// DVD+RW
	    obligatory=0x26;
	case 0x13:		// DVD-RW Restricted Overwrite
	    for (i=8,len=formats[3];i<len;i+=8)
		if ((formats [4+i+4]>>2) == obligatory) break;

	    if (i==len)
	    {	fprintf (stderr,":-( can't locate obligatory format descriptor\n");
		return 0;
	    }

	    ret  = formats[4+i+0]<<24;
	    ret |= formats[4+i+1]<<16;
	    ret |= formats[4+i+2]<<8;
	    ret |= formats[4+i+3];
	    nwa = formats[4+5]<<16|formats[4+6]<<8|formats[4+7];
	    if (nwa>2048)	ret *= nwa/2048;
	    else if (nwa<2048)	ret /= 2048/nwa;
	    break;

	case 0x12:		// DVD-RAM
	    // As for the moment of this writing I don't format DVD-RAM.
	    // Therefore I just pull formatted capacity for now...
	    ret  = formats[4+0]<<24;
	    ret |= formats[4+1]<<16;
	    ret |= formats[4+2]<<8;
	    ret |= formats[4+3];
	    nwa = formats[4+5]<<16|formats[4+6]<<8|formats[4+7];
	    if (nwa>2048)	ret *= nwa/2048;
	    else if (nwa<2048)	ret /= 2048/nwa;
	    break;

	case 0x11:		// DVD-R
	case 0x14:		// DVD-RW Sequential
	case 0x1B:		// DVD+R
	case 0x2B:		// DVD+R Double Layer
	    cmd[0] = 0x52;	// READ TRACK INFORMATION
	    cmd[1] = 1;
	    cmd[4] = next_track>>8;
	    cmd[5] = next_track&0xFF;	// last track, set up earlier
	    cmd[8] = sizeof(buf);
	    cmd[9] = 0;
	    if ((err=cmd.transport (READ,buf,sizeof(buf))))
	    {	if (next_track > 0) {
		    /* sperror ("READ TRACK INFORMATION",err); */
		    return 0;
		} else { next_track = 1;
		    goto retry;
		}
	    }

	    nwa = 0;
	    if (buf[7]&1)	// NWA_V
	    {	nwa  = buf[12]<<24;
		nwa |= buf[13]<<16;
		nwa |= buf[14]<<8;
		nwa |= buf[15];
	    }
	    free_blocks  = buf[16]<<24;
	    free_blocks |= buf[17]<<16;
	    free_blocks |= buf[18]<<8;
	    free_blocks |= buf[19];
	    ret = nwa + free_blocks;
	    break;

	default:
	    break;
    }

  return ret;
}

/* Returns the size of a DVD+R[W] or DVD-R[W] in bytes */
extern "C"
gint64 get_disc_size_dvd (int fd, int mmc_profile)
{
  Scsi_Command cmd(fd);
  return (gint64)get_2k_capacity(cmd, mmc_profile)*2048;
}

extern "C"
int get_read_write_speed (int fd, int *read_speed, int *write_speed)
{
  Scsi_Command cmd(fd);
  int retval = -1;
  unsigned char header[12], *page2A, *p;
  unsigned int len, bdlen, hlen;

  *read_speed = 0;
  *write_speed = 0;

  /* For valgrind */
  memset (&header, 0, sizeof (header));

  cmd[0]=0x5A; /* MODE SENSE */
  cmd[1]=0x08;  /* Disable Block Descriptors */
  cmd[2]=0x2A;  /* Capabilities and Mechanical Status */
  cmd[8]=12;
  cmd[9]=0;

  if (cmd.transport(READ,header,12))
  {    
    goto bail;
  }
  len=(header[0] << 8 | header[1]) + 2;
  bdlen=header[6] << 8 | header[7];

  if (bdlen)
  {
    if (len < (8 + bdlen + 30))
    {
      goto bail;
    }
  } else if (len < (8 + 2 + (unsigned int) header[9]))
  {
    /* SANYO does this. */
    len = 8 + 2 + header[9];
  }

  page2A=new unsigned char[len];
  cmd[0]=0x5A; /* MODE SENSE */
  cmd[1]=0x08;  /* Disable Block Descriptors */
  cmd[2]=0x2A;  /* Capabilities and Mechanical Status */
  cmd[7]=len >> 8;
  cmd[8]=len;        /* Real length */
  cmd[9]=0;
  if (cmd.transport(READ,page2A,len))
  {
      goto bail;
  }
  if (len < ((unsigned int) page2A[0] << 8 | page2A[1]))
  {
    page2A[0] = len >> 8;
    page2A[1] = len;
  }
  len = (page2A[0] << 8 | page2A[1]) + 2;
  hlen = 8 + (page2A[6] << 8 | page2A[7]);
  p = page2A + hlen;

  /* Values guessed from the cd_mode_page_2A struct
  * in cdrecord's libscg/scg/scsireg.h */
  if (len < (hlen + 30) || p[1] < (30 - 2))
  {
    /* no MMC-3 "Current Write Speed" present,
     * try to use the MMC-2 one */
    if (len < (hlen + 20) || p[1] < (20 - 2))
      *write_speed = 0;
    else
      *write_speed = p[18] << 8 | p[19];
  } else {
    *write_speed = p[28] << 8 | p[29];
  }

  *read_speed = p[8] << 8 | p[9];

  return 0;

bail:

  return -1;

}

