/*
 * Copyright 2012 WatchGuard Technologies, Inc.
 *
 * WatchGuard Technologies, Inc. grants you the right to use this code under the terms of the 
 * MICROSOFT PUBLIC LICENSE (Ms-PL), a copy of which should accompany this file.
 *
 * Author: John Borchek, 03 March 2013
 */

/* large file support in 32-bit binaries */

#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <endian.h>

#define	INT64		int64_t
#define	DWORD		uint32_t

#define	KB		(1024LL)
#define	MB		(1024LL*KB)
#define	GB		(1024LL*MB)

#define	DISK_SECTORS	(63LL)
#define	DISK_HEADS	(255LL)
#define	SECTOR_SIZE	(512LL)
#define	TRACK_SIZE	(DISK_HEADS * DISK_SECTORS * SECTOR_SIZE)

#define	MAX_BLOCK_SIZE	(2*MB)
#define	MIN_BLOCK_SIZE	(1*MB)

#define	MAX_FILE_SIZE	(128*GB-MAX_BLOCK_SIZE)

#define	ENTRIES		(MAX_FILE_SIZE/MIN_BLOCK_SIZE)

int64_t	BLOCK_SIZE	= MAX_BLOCK_SIZE;
int     verbose         = 0;

DWORD check(unsigned char* p)
{
  int   j;
  DWORD sum = 0;

  for (j = 0; j < SECTOR_SIZE; j++) sum += *p++;

  return htobe32(~sum);
}

struct	{
  struct	{
  char		Cookie[8];
  DWORD		Features;
  DWORD		File_Format_Version;
  INT64		Data_Offset;
  DWORD		Time_Stamp;
  char		Creator_Application[4];
  DWORD		Creator_Version;
  char		Creator_Host_OS[4];
  INT64		Original_Size;
  INT64		Current_Size;
  short		Cylinders;
  char		Heads;
  char		Track_Size;
  DWORD		Disk_Type;
  DWORD		Checksum;
  char		Unique_Id[16];
  char		Saved_State;
  char		Reserved[427];
  } Footer;

  struct	{
  char		Cookie[8];
  INT64		Data_Offset;
  INT64		Table_Offset;
  DWORD		Header_Version;
  DWORD		Max_Table_Entries;
  DWORD		Block_Size;
  DWORD		Checksum;
  char		Reserved[984];
  } Header;

  DWORD		BAT[ENTRIES+1];
} VHD;

char	BitMap[SECTOR_SIZE];

char	Data[MAX_BLOCK_SIZE];
char	Zero[MAX_BLOCK_SIZE];

int	fdi	= -1;
int	fdo	= -1;

struct	stat	stats;

INT64	fix_size;
INT64	pad_size;
INT64	org_size;
INT64	cur_size;

void bp(void) {}

/* Macro containing exit doesn't leave gcc -Wall hollering 
 * about return from non-void function at end of main()
 */
#define done(ec, str) \
  ({char *_str = (char*)(str); \
  int _errno = errno; \
  if (fdo >= 0) close(fdo); \
  if (fdi >= 0) close(fdi); \
  errno = _errno; \
  if (errno) perror(_str); \
  else if (*_str) printf("%s\n", _str); \
  exit(ec);})

int copy_block(DWORD n)
{
  memset(Data, 0, sizeof(Data));

  if (n > BLOCK_SIZE) n = BLOCK_SIZE;

  if (read( fdi, Data, n)           != n)
    done(-2, "Read data failure");

  if (memcmp(Data, Zero, n) == 0) return -1;

  n += (SECTOR_SIZE - 1);
  n &= -SECTOR_SIZE;

  if (n < BLOCK_SIZE) n = BLOCK_SIZE;
  
  if (write(fdo, &BitMap, sizeof(BitMap)) != sizeof(BitMap))
    done(-3, "Write map failure");

  if (write(fdo, Data, n)           != n)
    done(-4, "Write data failure");

  return ((n + sizeof(BitMap)) / SECTOR_SIZE);
}

void usage(char *us) {
  char *pa = {0};

  pa = strrchr(us, '/');
  if (pa == NULL) {
    pa = us;
  }
  fprintf(stderr, 
  "Usage: %s [-v] [-1] infile outfile\n"
  "       %s -h\n"
  "       %s -s\n"
  "\n"
  "       -v -- enable verbose output\n"
  "       -1 -- use 1MB blocksize instead of default 2MB\n"
  "       -s -- print sizes of header/footer structures and exit\n"
  "       -h -- print this help and exit\n"
  "\n"
  "       infile must exist, will not be altered\n"
  "       outfile is created if it does not exist,\n"
  "       outfile is overwritten if it does exist.\n"
  "\n", pa, pa, pa);
}

void print_sizes(void)
{
  printf("VHD dynamic header size   : %6u Bytes\n", 
    (unsigned)(sizeof(VHD.Header) + sizeof VHD.Footer));
  printf("VHD fixed disk footer size: %6u Bytes\n", 
    (unsigned)sizeof(VHD.Footer));
}

int main(int argc, char** argv)
{
  int   rc   = 0;
  DWORD j    = 0;
  DWORD blk  = 0;
  DWORD ent  = 0;
  INT64 off  = 0;
  DWORD sum  = 0;
  INT64 left = 0;
  char* inp  = {0};
  char* out  = {0};
  char* us   = {0};

  us = *argv;

  if (argc == 1) {
    usage(us);
    done(0, "");
  }

  while (--argc && *(++argv)) {
    if (**argv == '-') {
      switch (*(*argv+1)) {
      case '1':
        BLOCK_SIZE = MIN_BLOCK_SIZE;
        break;
      case 'v':
        verbose = 1;
        break;
      case 'h':
        usage(us);
        done(0, "");
        break;
      case 's':
        print_sizes();
        done(0, "");
        break;
      default:
        usage(us);
        fprintf(stderr, "Error: unknown option %s\n", *argv);
        done(-1, "");
        break;
      }
      continue;
    }

    if (!inp) {
      inp = *argv;
    } else if (!out) {
      out = *argv;
    } else {
      usage(us);
      fprintf(stderr, "Error: too many input parameters\n");
      done(-1, "");
    }
  }

  if (!inp) {
    usage(us);
    fprintf(stderr, "Error: must specify input file\n");
    done(-1, "");
  }

  if (!out) {
    usage(us);
    fprintf(stderr, "Error: must specify output file\n");
    done(-1, "");
  }

  fdi = open(inp, O_RDONLY);
  if (fdi <  0)  done(-1, "Error: can't open input file");

  rc = fstat(fdi, &stats);
  if (rc !=  0)  done(-1, "Error: can't read input file size");

  fix_size = stats.st_size;

  if (fix_size > MAX_FILE_SIZE) done(-1, "Error: input file over 128GB");

  rc = lseek64(fdi, fix_size-sizeof(VHD.Footer), SEEK_CUR);
  if (rc == -1)  done(-1, "Error: reading input file");

  rc = read(fdi, &VHD.Footer, sizeof(VHD.Footer));
  if (rc != sizeof(VHD.Footer))  done(-1, "Error: can't find input file .vhd footer");

  sum = VHD.Footer.Checksum;
  VHD.Footer.Checksum =   0;

  if (memcmp(&VHD.Footer.Cookie, "conectix", 8)) {
   /* Subtract seconds from start of UNIX epoch 1970-01-01 00:00:00 UTC 
    * to start of VHD epoch 2000-01-01 00:00:00 UTC */
    DWORD now = time(0) - (((30 * 365) + 7) * 24 * 60 * 60);
    DWORD cyl = (fix_size + (TRACK_SIZE - 1)) /  TRACK_SIZE;
    
    pad_size  = (fix_size + (BLOCK_SIZE - 1)) & -BLOCK_SIZE;

    memcpy(VHD.Footer.Cookie,    "conectix", 8);

    VHD.Footer.Features            = htobe32(2);
    VHD.Footer.File_Format_Version = htobe32(0x10000);
    VHD.Footer.Time_Stamp          = htobe32(now);
    VHD.Footer.Creator_Version     = htobe32(0x60001);
    VHD.Footer.Original_Size       = htobe64(pad_size);
    VHD.Footer.Current_Size        = htobe64(pad_size);

    if (cyl <= UINT16_MAX) {
      VHD.Footer.Heads             = DISK_HEADS;
      VHD.Footer.Track_Size        = DISK_SECTORS;
      VHD.Footer.Cylinders         = htobe16(cyl);
    }

    memcpy(VHD.Footer.Creator_Application, "win ",     4);
    memcpy(VHD.Footer.Creator_Host_OS,     "Wi2k",     4);
    memcpy(VHD.Footer.Unique_Id,           &VHD.Footer.Time_Stamp, 4);

  } else {
    fix_size -= sizeof(VHD.Footer);
    pad_size = (fix_size + (BLOCK_SIZE - 1)) & -BLOCK_SIZE;

    if (sum != check((unsigned char*)&VHD.Footer))
      done(8, "Bad checksum");
  }

  VHD.Footer.Data_Offset       = htobe64(sizeof(VHD.Footer));
  VHD.Footer.Disk_Type         = htobe32(3);
  VHD.Footer.Checksum          = check((unsigned char*)&VHD.Footer);

  org_size = be64toh(VHD.Footer.Original_Size);
  cur_size = be64toh(VHD.Footer.Current_Size);

  printf("\n");

  printf("VHD Block Size %9dMB\n",  (int)(BLOCK_SIZE / MB));

  printf("\n");

  printf("Fixed VHD Size %11lld\n", (long long)fix_size);
  printf("Original  Size %11lld\n", (long long)org_size);
  printf("Current   Size %11lld\n", (long long)cur_size);
  printf("Padded    Size %11lld\n", (long long)pad_size);

  ent = pad_size / BLOCK_SIZE;

  rc = lseek64(fdi, 0, SEEK_SET);
  if (rc == -1)  done(9, "Error: can't find data in input file");

  off  = (sizeof(VHD.Footer) + sizeof(VHD.Header) + (ent * sizeof(DWORD)));
  off += (SECTOR_SIZE - 1);
  off &= -SECTOR_SIZE;

  blk  = off / SECTOR_SIZE;

  printf("\n");
  printf("BAT    Entries %11d\n", (int)ent);
  printf("BAT    Offset  %11d\n", (int)off);
  printf("Data   Offset  %11d\n", (int)blk);

  memcpy(&VHD.Header.Cookie, "cxsparse", 8);

  VHD.Header.Checksum          =  0;
  VHD.Header.Data_Offset       = -1;
  VHD.Header.Table_Offset      = htobe64(sizeof(VHD.Footer)+sizeof(VHD.Header));
  VHD.Header.Header_Version    = htobe32(0x00010000);
  VHD.Header.Max_Table_Entries = htobe32(ent);
  VHD.Header.Block_Size        = htobe32(BLOCK_SIZE);
  VHD.Header.Checksum          = check((unsigned char*)&VHD.Header);

  fdo = creat(out, 0644);
  if (fdo == -1) {
    usage(us);
    fprintf(stderr, "Error: cannot create output file %s\n", out);
    done(-1, "");
  }

  rc = lseek64(fdo, off, SEEK_SET);
  if (rc == -1)  done(10, "Seek failure in input file");

  memset(VHD.BAT, -1, sizeof(VHD.BAT));
  memset(BitMap,  -1, (BLOCK_SIZE / SECTOR_SIZE) / 8);

  printf("\n");
  for (j = 0, left = fix_size; left > 0; left -= BLOCK_SIZE, j++) {
    int k = copy_block((left < BLOCK_SIZE) ? left : BLOCK_SIZE);

    if (k != -1) {
      if (verbose) printf("%5dMB Sector %11d\n", 
      (int)(j * (BLOCK_SIZE / MB)), (int)blk);
      VHD.BAT[j] = htobe32(blk);
      blk += k;
    }
  }

  rc = sizeof(VHD.Footer);
  if (write(fdo, &VHD.Footer, rc)  != rc)  done(11, "Error: can't write output file footer");

  rc = lseek64(fdo, 0, SEEK_SET);
  if (rc == -1)  done(12, "Error can't find input file header");

  if (write(fdo, &VHD.Footer, off) != off) done(13, "Error: can't write output file header");

  bp();

  done(0, "");
}

// vim: ai sw=2 ts=2 et
