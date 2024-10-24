// MICROSOFT PUBLIC LICENSE (Ms-PL)
//
// This license governs use of the accompanying software. If you use the
// software, you accept this license. If you do not accept the license, do not
// use the software.
//
// 1. Definitions
//
// The terms "reproduce," "reproduction," "derivative works," and
// "distribution" have the same meaning here as under U.S. copyright law.
//
// A "contribution" is the original software, or any additions or changes to
// the software.
//
// A "contributor" is any person that distributes its contribution under this
// license.  "Licensed patents" are a contributor's patent claims that read
// directly on its contribution. 
//
// 2. Grant of Rights
//
// (A) Copyright Grant- Subject to the terms of this license, including the
// license conditions and limitations in section 3, each contributor grants
// you a non-exclusive, worldwide, royalty-free copyright license to reproduce
// its contribution, prepare derivative works of its contribution, and
// distribute its contribution or any derivative works that you create.
//
// (B) Patent Grant- Subject to the terms of this license, including the
// license conditions and limitations in section 3, each contributor grants
// you a non-exclusive, worldwide, royalty-free license under its licensed
// patents to make, have made, use, sell, offer for sale, import, and/or
// otherwise dispose of its contribution in the software or derivative works
// of the contribution in the software. 
//
// 3. Conditions and Limitations
//
// (A) No Trademark License- This license does not grant you rights to use any
// contributors' name, logo, or trademarks.
//
// (B) If you bring a patent claim against any contributor over patents that
// you claim are infringed by the software, your patent license from such
// contributor to the software ends automatically.
//
// (C) If you distribute any portion of the software, you must retain all
// copyright, patent, trademark, and attribution notices that are present in
// the software.
//
// (D) If you distribute any portion of the software in source code form, you
// may do so only under this license by including a complete copy of this
// license with your distribution. If you distribute any portion of the
// software in compiled or object code form, you may only do so under a
// license that complies with this license.
//
// (E) The software is licensed "as-is." You bear the risk of using it. The
// contributors give no express warranties, guarantees or conditions. You may
// have additional consumer rights under your local laws which this license
// cannot change. To the extent permitted under your local laws, the
// contributors exclude the implied warranties of merchantability, fitness for
// a particular purpose and non-infringement. 
//

#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <endian.h>
#include <assert.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <uuid/uuid.h>

/* ==================================================================================
 * Definitions in good enough agreement with
 * http://msdn.microsoft.com/en-us/library/cc230343.aspx 
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa383751%28v=vs.85%29.aspx
 * for use with gcc
 */

/* Constants */
#define ERROR_OUTOFMEMORY    ENOMEM
#define ERROR_WRITE_FAULT    EIO
#define ERROR_HANDLE_EOF     EIO
#define ERROR_BAD_FILE_TYPE  EINVAL
#define TRUE                 1
#define FALSE                0
#define INVALID_HANDLE_VALUE NULL
#define FILE_BEGIN           SEEK_SET
#define FILE_CURRENT         SEEK_CUR
#define FILE_END             SEEK_END

/* Typedefs */
typedef char               CHAR, *LPTSTR;
typedef char               _TCHAR;
typedef char               TCHAR;
typedef unsigned char      UCHAR, *PUCHAR;
typedef int32_t            LONG;
typedef uint32_t           ULONG;
typedef int64_t            LONGLONG;
typedef uint64_t           ULONGLONG;
typedef uint64_t           UINT64;
typedef uint64_t           ULONG64;
typedef int16_t            SHORT;
typedef uint16_t           USHORT, *PUSHORT;
typedef uint32_t           DWORD;
typedef int32_t            BOOL;
typedef uint8_t            BOOLEAN;
typedef void               VOID, *PVOID, *LPVOID;
typedef uuid_t             GUID;
typedef FILE               *HANDLE;
typedef time_t             SYSTEMTIME;
typedef int64_t            LARGE_INTEGER, *PLARGE_INTEGER;
typedef uint64_t           ULARGE_INTEGER, *PULARGE_INTEGER;

/* Macros */

#define __in 
#define __out
#define __inout
#define __in_z
#define __in_bcount(x)

/* Don't worry about multibyte characters for purposes of this port. */

#define _tcsicmp(pa, pb) strcasecmp((pa), (pb))

#define _tcslen(pa) strlen(pa)

#define _tcscpy_s(dst, len, src) \
  ({char *_d = (dst);\
  int _l = (len);\
  char *_s = (src);\
  char * _r;\
  _r = strncpy(_d, _s, _l);\
  *(_d+_l-1) = '\0';\
  _r;})

#define _ttoi64(pa) atoll(pa)

#define RtlZeroMemory(buffer, bufferSize) memset((buffer), 0, (bufferSize))

#define RtlCopyMemory(dst, src, len) memcpy((dst), (src), (len))

#define SetFilePointerEx(FileHandle, FileOffset, newOffset, whence) ({\
  int rc; \
  LARGE_INTEGER *res = (LARGE_INTEGER *)newOffset;\
  rc = (fseeko((FileHandle), (FileOffset), (whence)) == 0);\
  if (rc && res && *res) *res = ftello(FileHandle);\
  rc;\
  })

#define GetFileSizeEx(fh, psize) ({\
  int rc;\
  struct stat statbuf;\
  rc = (fstat(fileno(fh), &statbuf) == 0);\
  if (rc) *psize = statbuf.st_size;\
  rc;\
  })

#define ReadFile(fh, buf, len, pread, overlap_flag) ({\
  int rc = 1;\
  size_t readlen;\
  readlen = fread(buf, 1, len, fh);\
  if (readlen == 0 && errno) rc = 0;\
  else *(pread) = readlen;\
  rc;\
  })

#define WriteFile(fh, buf, len, pwrite, overlap_flag) ({\
  int rc = 1;\
  size_t writelen;\
  writelen = fwrite(buf, 1, len, fh);\
  if (writelen == 0 && errno) rc = 0;\
  else *(pwrite) = writelen;\
  rc;\
  })

#define SetEndOfFile(fh) ({\
  (ftruncate(fileno(fh), ftell(fh)) == 0);\
  })

#define SetFileValidData(fh, len) ({\
  (ftruncate(fileno(fh), len) == 0);\
  })

#define CloseHandle(fp) ({\
  (fclose(fp) == 0);\
  })

#define GetLastError() errno

#define TEXT(t) t

#define _byteswap_ushort(x) htobe16(x)

#define _byteswap_ulong(x)  htobe32(x)

#define _byteswap_uint64(x) htobe64(x)
