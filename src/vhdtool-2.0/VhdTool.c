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

/*++

Module Name:

    VhdTool.c -- Linux port, derived from VhdTool.cpp

Abstract:

    This file implements the VhdTool functionality

Author:

    Christopher Eck - 23-Mar-2009

Contributors:

    Dan Wilder - 22-Feb-2012 (Linux build, adapt for XtmV) 

--*/

#include "wg_vhdtool.h"

//
// VHD definitions.
// Taken from the public VHD spec, current link is
// http://technet.microsoft.com/en-us/virtualserver/bb676673.aspx
//

#define VHD_CURRENT_FILE_FORMAT_VERSION    0x00010000
#define VHD_TYPE_BIG_FIXED                 0x02000000
#define VHD_TYPE_BIG_SPARSE                0x03000000
#define VHD_TYPE_BIG_DIFF                  0x04000000
#define VHD_SECTOR_SIZE                    0x00000200ULL

//
// Other constants added by WatchGuard. 
//

#define	KB		(1024L)
#define	MB		(1024L*KB)
#define	GB		(1024L*MB)

#define	DISK_SECTORS	(63L)
#define	DISK_HEADS	(255L)
#define	SECTOR_SIZE	(512L)
#define	TRACK_SIZE	(DISK_HEADS * DISK_SECTORS * SECTOR_SIZE)

#define	MAX_BLOCK_SIZE	(2*MB)
#define	MIN_BLOCK_SIZE	(1*MB)

// Data structures

static union {
    CHAR data[4];
    ULONG result;
} vhd_host_os = {{ 'W', 'i', '2', 'k' }};

typedef struct
{
    UCHAR   Cookie[8];
    ULONG   Features;
    ULONG   FileFormatVersion;
    ULONG64 DataOffset;
    ULONG   TimeStamp;
    UCHAR   CreatorApplication[4];
    ULONG   CreatorVersion;
    ULONG   CreatorHostOS;
    ULONG64 OriginalSize;
    ULONG64 CurrentSize;

    struct
    {
        USHORT Cylinders;
        UCHAR  Heads;
        UCHAR  SectorsPerTrack;
    } Geometry;

    ULONG   DiskType;
    ULONG   Checksum;
    UCHAR   UniqueId[16];
    UCHAR   SavedState;
    UCHAR   Reserved[427];
} VHD_DISK_FOOTER;


typedef struct
{
    ULONG PlatformCode;
    ULONG PlatformDataAllocatedSpace;
    ULONG PlatformDataLength;
    ULONG Reserved;
    ULONGLONG PlatformDataOffset;
} PARENT_DRIVE_LOCATOR;


typedef struct
{
    ULONGLONG Signature;
    ULONGLONG Reserved1;
    ULONGLONG TableOffset;
    ULONG HeaderVersion;
    ULONG MaxTableEntries;
    ULONG BlockSize;
    ULONG CheckSum;

    UCHAR ParentUniqueId[16];
    ULONG ParentTimeStamp;
    ULONG Reserved2;
    USHORT ParentName[256];
    PARENT_DRIVE_LOCATOR ParentLocatorTable[8];
    UCHAR Reserved3[256];
} VHD_SPARSE_HEADER;


//
// Tool definitions
//

/* Seconds from start of UNIX epoch 1970-01-01 00:00:00 UTC 
 * to start of VHD epoch 2000-01-01 00:00:00 UTC
 */

const LARGE_INTEGER SecondsToStartOf2000    = 946684800L;

const int           VHD_CREATE_TOOL_VERSION = 0x00020000;

enum PROGRAM_MODE
{
    ProgramModeUnspecified = 0,
    ProgramModeCreate      = 1,
    ProgramModeConvert     = 2,
    ProgramModeExtend      = 3,
    ProgramModeRepair      = 4
};


//
// Global state
//
BOOL g_QuietMode = FALSE;


//
// Input/Output handlers
//


VOID
PrintUsage()
/*++

Routine Description:

    Prints the command line usage to the console.

Arguments:

    None.

Return Value:

    None.

--*/
{
    if (g_QuietMode)
    {
        goto Cleanup;
    }

    printf("\n");
    printf("VhdTool create <FileName> <Size> [-quiet]\n");
    printf("VhdTool convert <FileName> [-quiet]\n");
    printf("VhdTool extend <FileName> <NewSize> [-quiet]\n");
    printf("VhdTool repair <BaseVhdFileName> <FirstSnapshotAVhdFileName> [-quiet]\n");
    printf("\n");
    printf("create: Creates a new fixed format VHD of size <Size>.\n");
    printf("        WARNING - this function is admin only and bypasses\n");
    printf("        file system security.  The resulting VHD file will\n");
    printf("        contain data which currently exists on the physical disk.\n");
    printf("\n");
    printf("convert: Converts an existing file to a fixed-format VHD.\n");
    printf("         The existing file length, rounded up, will contain block data\n");
    printf("         A VHD footer is appended to the current end of file.\n");
    printf("\n");
    printf("extend: Extends an existing fixed format VHD to a larger size <Size>.\n");
    printf("        WARNING - this function is admin only and bypasses\n");
    printf("        file system security.  The resulting VHD file will\n");
    printf("        contain data which currently exists on the physical disk.\n");
    printf("\n");
    printf("repair: Repairs a broken Hyper-V snapshot chain where an administrator\n");
    printf("        has expanded the size of the root VHD.  The base VHD will be\n");
    printf("        returned to its original size. THIS MAY CAUSE DATA LOSS if the\n");
    printf("        contents of the base VHD were changed after expansion.\n");
    printf("\n");

Cleanup:
    return;
}


BOOL
ParseCommandLine(
    __in  int           ArgCount,
    __in  _TCHAR*       ArgArray[],
    __out enum PROGRAM_MODE* ProgramMode,
    __out LPTSTR*       Filename,
    __out UINT64*       Filesize,
    __out LPTSTR*       ChildFilename
    )
/*++

Routine Description:

    Parses the command line

Arguments:

    ArgCount - Number of command line arguments

    ArgArray - Array of command line arguments

    ProgramMode - Mode the program should run in.

    Filename - Name of the file to take action upon.

    Filesize - Desired size of the file.

    ChildFilename - Name of the child VHD if present.

Return Value:

    BOOL - TRUE on success, FALSE on failure

--*/
{
    LPTSTR  tempString        = NULL;
    UINT64  filenameLength    = 0;
    BOOL    status            = FALSE;
    int     currentArg        = 1;

    //
    // Initialize out params
    //
    *ProgramMode = ProgramModeUnspecified;
    *Filename = NULL;
    *Filesize = 0;

    //
    // Validate the command line arguments
    //
    if (ArgCount < 3)
    {
        PrintUsage();
        goto Cleanup;
    }

    //
    // Retrieve mode
    //
    tempString = ArgArray[currentArg];
    currentArg++;

    if (_tcsicmp(tempString, TEXT("create")) == 0)
    {
        *ProgramMode = ProgramModeCreate;
    }
    else if (_tcsicmp(tempString, TEXT("convert")) == 0)
    {
        *ProgramMode = ProgramModeConvert;
    }
    else if (_tcsicmp(tempString, TEXT("extend")) == 0)
    {
        *ProgramMode = ProgramModeExtend;
    }
    else if (_tcsicmp(tempString, TEXT("repair")) == 0)
    {
        *ProgramMode = ProgramModeRepair;
    }
    else
    {
        PrintUsage();
        goto Cleanup;
    }

    //
    // Retrieve file name
    //
    tempString = ArgArray[currentArg];
    currentArg++;

    // Add space for the null-terminating character
    filenameLength = _tcslen(tempString) + 1;

    *Filename = (LPTSTR)malloc(filenameLength * sizeof(TCHAR));
    _tcscpy_s(*Filename, filenameLength, tempString);

    //
    // Retrieve file size if in create or extend mode.
    //
    if ((*ProgramMode == ProgramModeCreate) ||
        (*ProgramMode == ProgramModeExtend))
    {
        if ((currentArg + 1) > ArgCount)
        {
            PrintUsage();
            goto Cleanup;
        }

        tempString = ArgArray[currentArg];
        currentArg++;

        *Filesize = _ttoi64(tempString);
        if (*Filesize == 0)
        {
            PrintUsage();
            goto Cleanup;
        }
    }

    //
    // Retrieve child VHD name if in repair mode.
    //
    if (*ProgramMode == ProgramModeRepair)
    {
        if ((currentArg + 1) > ArgCount)
        {
            PrintUsage();
            goto Cleanup;
        }

        tempString = ArgArray[currentArg];
        currentArg++;

        filenameLength = _tcslen(tempString) + 1;

        *ChildFilename = (LPTSTR)malloc(filenameLength * sizeof(TCHAR));
        _tcscpy_s(*ChildFilename, filenameLength, tempString);
    }


    //
    // Retrieve quiet mode if present
    //
    if ((currentArg + 1) <= ArgCount)
    {
        tempString = ArgArray[currentArg];
        currentArg++;

        if (_tcsicmp(tempString, TEXT("-quiet")) == 0)
        {
            g_QuietMode = TRUE;
        }
        else
        {
            PrintUsage();
            goto Cleanup;
        }
    }

    status = TRUE;

Cleanup:
    return status;
}


VOID
PrintError(
    DWORD LastErrorValue,
    __in_z const char* FormatString,
    ...
    )
/*++

Routine Description:

    Prints tool errors.

Arguments:
    
    LastErrorValue - error code returned from a system call.

    FormatString - the printf format string.

    ... - VarArgs passed to printf.

Return Value:

    None.

--*/
{
    va_list argumentPointer;
    LPTSTR  errorMessage    = NULL;

    if (g_QuietMode)
    {
        goto Cleanup;
    }

    va_start(argumentPointer, FormatString);

    printf("\tError: ");
    vprintf(FormatString, argumentPointer);
    va_end(argumentPointer);


    errorMessage = strerror(LastErrorValue);
    if (errorMessage == NULL)
    {
        printf(" with error: %d", LastErrorValue);
    }
    else
    {
        printf(" with error: %s", errorMessage);
    }

    printf("\n");

Cleanup:
    return;
}


VOID
PrintStatus(
    __in_z const char* FormatString,
    ...
    )
/*++

Routine Description:

    Prints tool status.

Arguments:

    FormatString - the printf format string.

    ... - VarArgs passed to printf.

Return Value:

    None.

--*/
{
    va_list argumentPointer;

    if (g_QuietMode)
    {
        goto Cleanup;
    }

    va_start(argumentPointer, FormatString);

    printf("\tStatus: ");
    vprintf(FormatString, argumentPointer);
    printf("\n");

    va_end(argumentPointer);

Cleanup:
    return;
}


VOID
PrintVhdIdentifier(
    __in GUID Identifier,
    __in_z const char* FormatString
    )
/*++

Routine Description:

    Prints a VHD identifier as status

Arguments:

    FormatString - the printf format string.

    ... - VarArgs passed to printf.

Return Value:

    None.

--*/
{
    CHAR parentIdString[48];

    if (g_QuietMode)
    {
        goto Cleanup;
    }

    uuid_unparse(Identifier, parentIdString);

    printf("\tStatus: ");
    printf(FormatString, parentIdString);
    printf("\n");

Cleanup:
    return;
}


//
// Utility functions
//


ULONG64
RoundUpUlong64(
    __in ULONG64 Value,
    __in ULONG64 PowerOf2
    )
/*++

Routine Description:

    Rounds a 64-bit Value up to the nearest given PowerOf2.

Arguments:

    Value - Supplies the value to round.

    PowerOf2 - Supplies the value to round to.

Return Value:

    The rounded value.

--*/
{
    return (Value + (PowerOf2 - 1)) & ~(PowerOf2 - 1);
}


ULONG
CalculateChecksum(
    __in_bcount(Length) PVOID Buffer,
    __in                ULONG Length
    )
/*++

Routine Description:

    This routine calculates the one's complement of the checksum of all the
    bytes with the given range, excluding the address to ignore.

Arguments:

    Buffer - Pointer to the buffer to calculate the checksum for.

    Length - Length of the buffer in bytes.

Return Value:

    Checksum.

--*/
{
    PUCHAR address    = NULL;
    ULONG  checksum   = 0;

    checksum = 0;
    address = (PUCHAR)Buffer;

    while (Length != 0)
    {
        checksum += *address;
        Length -= 1;
        address += 1;
    }

    return ~checksum;
}


BOOL
ZeroFileContents(
    __in HANDLE           FileHandle,
    __in LARGE_INTEGER    FileOffset,
    __in DWORD            BytesToClear
    )
/*++

Routine Description:

    Zeros out a section of a file.

Arguments:

    FileHandle - Handle to the open file.

    FileOffset - Offset in the file to begin clearing.

    BytesToClear - Number of bytes of space to clear.

Return Value:

    BOOL - TRUE on success, FALSE on failure.

--*/
{
    LPVOID  buffer          = NULL;
    DWORD   bytesWritten    = 0;
    DWORD   bufferSize      = 1024 * 1024;
    DWORD   bytesToWrite    = 0;
    BOOL    success         = FALSE;

    // Validate input
    if (FileHandle == INVALID_HANDLE_VALUE)
    {
        goto Cleanup;
    }

    //
    // Allocate and clear the 1 MB zeroing buffer
    //
    buffer = malloc(bufferSize);
    if (buffer == NULL)
    {
        PrintError(ERROR_OUTOFMEMORY, "Unable to allocate zeroing buffer");
        goto Cleanup;
    }
    RtlZeroMemory(buffer, bufferSize);

    //
    // Seek to the file offset
    //
    if (!SetFilePointerEx(FileHandle, FileOffset, NULL, FILE_BEGIN))
    {
        PrintError(GetLastError(), "Unable to seek to file offset %lld", FileOffset);
        goto Cleanup;
    }
    
    //
    // Clear file contents.
    // This isn't particularly fast, but maintainability is more important here.
    //
    while (BytesToClear > 0)
    {
        if (BytesToClear > bufferSize)
        {
            bytesToWrite = bufferSize;
        }
        else
        {
            bytesToWrite = BytesToClear;
        }
        BytesToClear -= bytesToWrite;

        if (!WriteFile(FileHandle, buffer, bytesToWrite, &bytesWritten, NULL))
        {
            PrintError(GetLastError(), "Unable to write data to file");
            goto Cleanup;
        }

        if (bytesWritten != bytesToWrite)
        {
            PrintError(ERROR_WRITE_FAULT, "Bytes written count unexpected");
            goto Cleanup;
        }
    }

    success = TRUE;

Cleanup:

    if (buffer != NULL)
    {
        free(buffer);
        buffer = NULL;
    }

    return success;
}

//
// VHD format functions
//

VOID
GetVirtualDiskGeometry(
    __in  ULONGLONG TotalSectors,
    __out PUSHORT   DiskCylinders,
    __out PUCHAR    DiskHeads,
    __out PUCHAR    DiskSectorsPerTrack
    )
/*++

Routine Description:

    This routine returns the disk geometry values given the total number of
    sectors on the disk.

Arguments:

    TotalSectors - Supplies the total number of sectors on the disk.

    DiskCylinders - Supplies a pointer to a value that will hold the number of
        cylinders on the disk.

    DiskHeads - Supplies a pointer to a value that will hold the number of
        heads on the disk.

    DiskSectorsPerTrack - Supplies a pointer to a value that will hold the
        number sectors per track on the disk.

Return Value:

    None.

--*/
{
    USHORT cylinders            = 0;
    ULONG  cylinderTimesHeads   = 0;
    UCHAR  heads                = 0;
    UCHAR  sectorsPerTrack      = 0;

    // Upper bound the total sector count.
    if (TotalSectors > (65535 * 16 * 255))
    {
        TotalSectors = 65535 * 16 * 255;
    }

    if (TotalSectors >= (65535 * 16 * 63))
    {
        sectorsPerTrack = 255;
        heads = 16;
        cylinderTimesHeads = (ULONG)(TotalSectors / sectorsPerTrack);
    }
    else
    {
        sectorsPerTrack = 17;
        cylinderTimesHeads = (ULONG)(TotalSectors / sectorsPerTrack);
        heads = (UCHAR)((cylinderTimesHeads + 1023) / 1024);
        
        if (heads < 4)
        {
            heads = 4;
        }

        if ((cylinderTimesHeads >= ((ULONG)heads * 1024)) || (heads > 16))
        {
            sectorsPerTrack = 31;
            heads = 16;
            cylinderTimesHeads = (ULONG)(TotalSectors / sectorsPerTrack);
        }

        if (cylinderTimesHeads >= ((ULONG)heads * 1024))
        {
            sectorsPerTrack = 63;
            heads = 16;
            cylinderTimesHeads = (ULONG)(TotalSectors / sectorsPerTrack);
        }
    }

    cylinders = (USHORT)(cylinderTimesHeads / heads);

    *DiskCylinders = cylinders;
    *DiskHeads = heads;
    *DiskSectorsPerTrack = sectorsPerTrack;
}


BOOL
GetTimestamp(
    __in ULONG* Timestamp
    )
/*++

Routine Description:

    This routine calculates the VHD timestamp, based on the number
    of seconds since 1/1/2000.

Arguments:

    Timestamp - returns the VHD timestamp value, based on the current time.

Return Value:

    BOOL - TRUE upon success, FALSE upon failure.

--*/
{
    LARGE_INTEGER   seconds;
    SYSTEMTIME      systemTime;

    // Get current time, UNIX epoch (1970) based
    time(&systemTime);
    seconds = systemTime;

    // Subtract number of seconds from 1970 to 2000
    seconds = seconds - SecondsToStartOf2000;

    //
    //  If the result is negative then the date was before 2000 or if
    //  the result is greater than a ULONG then it's too far in the
    //  future so we return FALSE.
    //
    if ((seconds & 0xffffffff00000000LL) != 0)
    {
        return FALSE;
    }

    *Timestamp = seconds;
    
    return TRUE;
}


BOOL
CreateVhdFooter(
    __in  ULONG64          VirtualSize,
    __in  GUID             UniqueId,
    __in  BOOLEAN          FixedType,
    __in  UINT64           SparseHeaderOffset,
    __out VHD_DISK_FOOTER* Footer
    )
/*++

Routine Description:

    This routine creates a valid fixed VHD footer given a virtual disk size.

Arguments:

    VirtualSize - Size of the virtual storage device in bytes.

    UniqueId - the unique identifier to place in the footer.

    FixedType - TRUE if a fixed VHD footer should be generated,
        FALSE for a sparse VHD footer.

    SparseHeaderOffset - for sparse VHD footers, the offset to the
        sparse header.

    Footer - Upon return, filled with a valid VHD 1.0 footer for the 
        virtual storage device.

Return Value:

    BOOL - TRUE upon success, FALSE upon failure.

--*/
{
    ULONG64 totalSectors     = 0;
    USHORT  cylinders        = 0;
    ULONG   timestamp        = 0;
    UCHAR   heads            = 0;
    UCHAR   sectorsPerTrack  = 0;
    BOOL    success          = FALSE;

    RtlZeroMemory(Footer, sizeof(VHD_DISK_FOOTER));

    //
    // Gather data
    //

    assert((VirtualSize % VHD_SECTOR_SIZE) == 0);
    totalSectors = VirtualSize / VHD_SECTOR_SIZE;

    GetVirtualDiskGeometry(totalSectors, &cylinders, &heads, &sectorsPerTrack);

    if (!GetTimestamp(&timestamp))
    {
        PrintError(ERROR_OUTOFMEMORY, "Unable to determine timestamp");
        goto Cleanup;
    }

    //
    // Initialize the VHD 1.0 Footer
    //

    Footer->Cookie[0] = 'c';
    Footer->Cookie[1] = 'o';
    Footer->Cookie[2] = 'n';
    Footer->Cookie[3] = 'e';
    Footer->Cookie[4] = 'c';
    Footer->Cookie[5] = 't';
    Footer->Cookie[6] = 'i';
    Footer->Cookie[7] = 'x';

    Footer->Features = _byteswap_ulong(0x00000002);
    Footer->FileFormatVersion = _byteswap_ulong(VHD_CURRENT_FILE_FORMAT_VERSION);

    if (FixedType)
    {
        Footer->DiskType = VHD_TYPE_BIG_FIXED;
        Footer->DataOffset = 0xFFFFFFFFFFFFFFFFULL;
    }
    else
    {
        Footer->DiskType = VHD_TYPE_BIG_SPARSE;
        Footer->DataOffset = SparseHeaderOffset;
    }

    Footer->TimeStamp = _byteswap_ulong(timestamp);

    Footer->CreatorApplication[0] = 'h';
    Footer->CreatorApplication[1] = 'a';
    Footer->CreatorApplication[2] = 'c';
    Footer->CreatorApplication[3] = 'k';

    Footer->CreatorVersion = _byteswap_ulong(VHD_CREATE_TOOL_VERSION);

    Footer->CreatorHostOS = vhd_host_os.result;

    Footer->OriginalSize = _byteswap_uint64(VirtualSize);
    Footer->CurrentSize = _byteswap_uint64(VirtualSize);

    Footer->Geometry.Cylinders = _byteswap_ushort(cylinders);
    Footer->Geometry.Heads = heads;
    Footer->Geometry.SectorsPerTrack = sectorsPerTrack;

    RtlCopyMemory(&Footer->UniqueId, UniqueId, 16);

    Footer->SavedState = 0;

    Footer->Checksum = _byteswap_ulong(CalculateChecksum((PVOID)Footer, sizeof(VHD_DISK_FOOTER)));

    PrintStatus("VHD footer generated");

    success = TRUE;

Cleanup:
    return success;
}


BOOL
OpenVhdFile(
    __in  LPTSTR            Filename,
    __out HANDLE*           FileHandle,
    __out LARGE_INTEGER*    FileSize
    )
/*++

Routine Description:

    Opens a file for IO & retrieves the physical file size

Arguments:

    Filename - name of the file to open.

    FileHandle - Upon successful return, contains the handle to the file.

    FileSize - Upon successful return, contains the physical size of the file.

Return Value:

    BOOL - TRUE on success, FALSE on failure

--*/
{
    BOOL status = FALSE;

    //
    // Initialize out parameters
    //
    *FileHandle = INVALID_HANDLE_VALUE;
    *FileSize = 0;

    //
    // Open the file
    //
    PrintStatus("Attempting to open file \"%s\"", Filename);
    *FileHandle = fopen(Filename, "a");

    if (*FileHandle == INVALID_HANDLE_VALUE)
    {
        PrintError(GetLastError(), "Unable to open file \"%s\"", Filename);
        goto Cleanup;
    }

    //
    // Determine the file length
    //
    if (!GetFileSizeEx(*FileHandle, FileSize))
    {
        PrintError(GetLastError(), "Unable to retrieve file size");
        goto Cleanup;
    }
    PrintStatus("File opened, current size is %lld", *FileSize);

    status = TRUE;

Cleanup:

    if (!status && *FileHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(*FileHandle);
        *FileHandle = INVALID_HANDLE_VALUE;
    }

    return status;
}


VOID
RoundUpToSectorSize(
    __inout LARGE_INTEGER* FileSize
    )
/*++

Routine Description:

    Rounds the input file size up to the nearest sector

Arguments:

    FileSize - Size to round up.

Return Value:

    None.

--*/
{
    ULONG64 size = *FileSize;
    size = RoundUpUlong64(size, VHD_SECTOR_SIZE);

    if (size != *FileSize)
    {
        PrintStatus("Warning: Rounding file size up to nearest sector boundary.");
    }

    *FileSize = size;
}


BOOL
ReadFooter(
    __in  HANDLE           FileHandle,
    __in  UINT64           FooterOffset,
    __out VHD_DISK_FOOTER* Footer
    )
/*++

Routine Description:

    Reads the VHD footer from an open VHD file

Arguments:

    FileHandle - handle to the open VHD file.

    FooterOffset - file offset of the footer object within the file.

    Footer - Upon successful return, contains the footer object in big-endian format.

Return Value:

    BOOL - TRUE on success, FALSE on failure

--*/
{
    LARGE_INTEGER savedPosition;
    LARGE_INTEGER footerPosition;
    DWORD bytesRead;
    BOOL savedCurrentPosition = FALSE;
    BOOL status = FALSE;

    memset(Footer, 0, sizeof(VHD_DISK_FOOTER));

    //
    // Save the current file pointer
    //
    savedPosition = 0;
    if (!SetFilePointerEx(FileHandle, savedPosition, &savedPosition, FILE_CURRENT))
    {
        PrintError(GetLastError(), "Unable to get current file pointer");
        goto Cleanup;
    }

    savedCurrentPosition = TRUE;

    //
    // Read the existing footer.
    //
    footerPosition = FooterOffset;
    if (!SetFilePointerEx(FileHandle, footerPosition, NULL, FILE_BEGIN))
    {
        PrintError(GetLastError(), "Unable to set file pointer to footer offset");
        goto Cleanup;
    }

    if (!ReadFile(FileHandle, Footer, sizeof(VHD_DISK_FOOTER), &bytesRead, NULL))
    {
        PrintError(GetLastError(), "Cannot read existing footer");
        goto Cleanup;
    }

    if (bytesRead != sizeof(VHD_DISK_FOOTER))
    {
        PrintError(ERROR_HANDLE_EOF, "Cannot read existing footer");
        goto Cleanup;
    }

    status = TRUE;

Cleanup:

    if (savedCurrentPosition)
    {
        SetFilePointerEx(FileHandle, savedPosition, NULL, FILE_BEGIN);
    }

    return status;
}


BOOL
ReadSparseHeader(
    __in  HANDLE             FileHandle,
    __in  VHD_DISK_FOOTER*   Footer,
    __out VHD_SPARSE_HEADER* SparseHeader
    )
/*++

Routine Description:

    Reads the VHD sparse header from a VHD file.

Arguments:

    FileHandle - handle to the open VHD file.

    Footer - Contains the footer object in big-endian format.

    SparseHeader - Upon successful return, contains the sparse header object in big-endian format.

Return Value:

    BOOL - TRUE on success, FALSE on failure

--*/
{
    LARGE_INTEGER savedPosition;
    LARGE_INTEGER headerPosition;
    DWORD bytesRead;
    BOOL savedCurrentPosition = FALSE;
    BOOL status = FALSE;

    memset(SparseHeader, 0, sizeof(VHD_SPARSE_HEADER));

    //
    // Save the current file pointer
    //
    savedPosition = 0;
    if (!SetFilePointerEx(FileHandle, savedPosition, &savedPosition, FILE_CURRENT))
    {
        PrintError(GetLastError(), "Unable to get current file pointer");
        goto Cleanup;
    }
    savedCurrentPosition = TRUE;

    //
    // Move the file pointer to the start of the sparse header.
    //
    headerPosition = _byteswap_uint64(Footer->DataOffset);
    if (!SetFilePointerEx(FileHandle, headerPosition, NULL, FILE_BEGIN))
    {
        PrintError(GetLastError(), "Unable to set file pointer");
        goto Cleanup;
    }

    //
    // Read the sparse header.
    //
    if (!ReadFile(FileHandle, SparseHeader, sizeof(VHD_SPARSE_HEADER), &bytesRead, NULL))
    {
        PrintError(GetLastError(), "Cannot read existing sparse header");
        goto Cleanup;
    }

    if (bytesRead != sizeof(VHD_SPARSE_HEADER))
    {
        PrintError(ERROR_HANDLE_EOF, "Cannot read existing sparse header");
        goto Cleanup;
    }
    status = TRUE;

Cleanup:

    if (savedCurrentPosition)
    {
        SetFilePointerEx(FileHandle, savedPosition, NULL, FILE_BEGIN);
    }

    return status;
}


BOOL
WriteFooter(
    __in  HANDLE           FileHandle,
    __in  UINT64           FooterOffset,
    __in  VHD_DISK_FOOTER* Footer
    )
/*++

Routine Description:

    Writes a VHD footer to an open VHD file

Arguments:

    FileHandle - handle to the open VHD file.

    FooterOffset - file offset to write the footer object within the file.

    Footer - Footer object in big-endian format to write to the file.

Return Value:

    BOOL - TRUE on success, FALSE on failure

--*/
{
    LARGE_INTEGER savedPosition;
    LARGE_INTEGER footerPosition;
    DWORD bytesWritten;
    BOOL savedCurrentPosition = FALSE;
    BOOL status = FALSE;

    //
    // Save the current file pointer
    //
    savedPosition = 0;
    if (!SetFilePointerEx(FileHandle, savedPosition, &savedPosition, FILE_CURRENT))
    {
        PrintError(GetLastError(), "Unable to get current file pointer");
        goto Cleanup;
    }
    savedCurrentPosition = TRUE;


    //
    // Seek to the offset in the file
    //
    footerPosition = FooterOffset;
    if (!SetFilePointerEx(FileHandle, footerPosition, NULL, FILE_BEGIN))
    {
        PrintError(GetLastError(), "Unable to set file pointer to footer offset");
        goto Cleanup;
    }

    //
    // Write the footer
    //
    if (!WriteFile(FileHandle, (PVOID)Footer, sizeof(VHD_DISK_FOOTER), &bytesWritten, NULL))
    {
        PrintError(GetLastError(), "Unable to write VHD footer");
        goto Cleanup;
    }

    if (bytesWritten != sizeof(VHD_DISK_FOOTER))
    {
        PrintError(ERROR_WRITE_FAULT, "Bytes written count unexpected");
        goto Cleanup;
    }

    PrintStatus("VHD footer written to file.");

    status = TRUE;

Cleanup:
    
    if (savedCurrentPosition)
    {
        SetFilePointerEx(FileHandle, savedPosition, NULL, FILE_BEGIN);
    }

    return status;
}


BOOL
SetFileLength(
    __in HANDLE FileHandle,
    __in UINT64 FileLength
    )
/*++

Routine Description:

    Sets the file length of the given file, potentially
    truncating it.  It does not set the valid data length.

Arguments:

    FileHandle - handle to the open VHD file.

    FileLength - file length to set.

Return Value:

    BOOL - TRUE on success, FALSE on failure

--*/
{
    LARGE_INTEGER savedPosition;
    LARGE_INTEGER fileSize;
    BOOL savedCurrentPosition = FALSE;
    BOOL status = FALSE;

    //
    // Save the current file pointer
    //
    savedPosition = 0;
    if (!SetFilePointerEx(FileHandle, savedPosition, &savedPosition, FILE_CURRENT))
    {
        PrintError(GetLastError(), "Unable to get current file pointer");
        goto Cleanup;
    }
    savedCurrentPosition = TRUE;

    //
    // If the saved file pointer is beyond the new end-of-file, don't
    // restore the saved position on exit.  Instead, leave the file
    // pointer at the new EOF.
    //
    if ((UINT64)savedPosition > FileLength)
    {
        savedCurrentPosition = FALSE;
    }

    //
    // Move the file pointer to the new end-of-file.
    //
    fileSize = FileLength;

    if (!SetFilePointerEx(FileHandle, fileSize, NULL, FILE_BEGIN))
    {
        PrintError(GetLastError(), "Unable to set file pointer to new end of file");
        goto Cleanup;
    }

    //
    // Set EOF
    //
    if (!SetEndOfFile(FileHandle))
    {
        PrintError(GetLastError(), "Unable to set EOF");
        goto Cleanup;
    }

    PrintStatus("Set the file length");

    status = TRUE;

Cleanup:

    if (savedCurrentPosition)
    {
        SetFilePointerEx(FileHandle, savedPosition, NULL, FILE_BEGIN);
    }

    return status;
}


//
// Main handler functions
//


BOOL
ConvertToVhd(
    __in LPTSTR Filename
    )
/*++

Routine Description:

    Pads to some convenient length and Adds a VHD footer 

Arguments:

    Filename - name of the file to convert to a VHD.

Return Value:

    BOOL - TRUE on success, FALSE on failure

--*/
{
    VHD_DISK_FOOTER footer;
    LARGE_INTEGER   fileSize;
    LARGE_INTEGER   fileSizePadded;
    UINT64          padIncrement    = 2 * 1024 * 1024;  // must be a power of 2
    UINT64          size;
    HANDLE          fileHandle      = INVALID_HANDLE_VALUE;
    GUID            uniqueId;
    BOOL            status          = FALSE;

    PrintStatus("Converting \"%s\" to a fixed format VHD.", Filename);

    //
    // Open the file
    //
    if (!OpenVhdFile(Filename, &fileHandle, &fileSize))
    {
        goto Cleanup;
    }

    fileSizePadded = fileSize;

    //
    // Pad a little -- some place we're losing one cylinder (of our heads/sectors,
    // = 255/63, abt 8Mb, in translation on the way into HyperV. No other virtualization
    // bumps into this so maybe it's a boundary error in HyperV.  Compensate by
    // adding two sectors here.
    //
    fileSizePadded += 1024;

    // 
    // Pad up to increment of 2MB.  This also incidentally puts on
    // a sector boundary, so we've removed the round-up-to-sectory boundary
    // code here.
    //
    size = fileSizePadded;
    fileSizePadded = RoundUpUlong64(size, padIncrement);

    // 
    // Expand file to match padding
    //
    if (fileSizePadded != fileSize) {
        if (!SetFileValidData(fileHandle, fileSizePadded)) {
	    goto Cleanup;
	}
	if (!SetFilePointerEx(fileHandle, 0, NULL, SEEK_END)) {
	    goto Cleanup;
	}
	fileSize = fileSizePadded;
    }

    //
    // Error if file is too small
    //
    if (fileSize < (1024 * 1024 * 12))
    {
        PrintError(ERROR_HANDLE_EOF, "File is smaller than 12 MB.");
        goto Cleanup;
    }


    //
    // Create the footer
    //
    uuid_generate(uniqueId);

    if (!CreateVhdFooter(fileSize, uniqueId, TRUE, 0, &footer))
    {
        goto Cleanup;
    }

    //
    // Append the footer
    //
    if (!WriteFooter(fileHandle, fileSize, &footer))
    {
        goto Cleanup;
    }

    status = TRUE;

Cleanup:

    if (fileHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(fileHandle);
        fileHandle = INVALID_HANDLE_VALUE;
    }

    return status;
}


BOOL
CreateNewVhd(
    __in LPTSTR Filename,
    __in UINT64 Filesize
    )
/*++

Routine Description:

    Adds a VHD footer and clears the first few MB of an existing file.

Arguments:

    Filename - name of the file to create.

    Filesize - size of the resulting data section.

Return Value:

    BOOL - TRUE on success, FALSE on failures

--*/
{
    VHD_DISK_FOOTER footer;
    LARGE_INTEGER   filePointer;
    HANDLE          fileHandle      = INVALID_HANDLE_VALUE;
    GUID            uniqueId;
    BOOL            status          = FALSE;

    PrintStatus("Creating new fixed format VHD with name \"%s\"", Filename);

    //
    // Error if file is too small
    //
    if (Filesize < (1024 * 1024 * 12))
    {
        PrintError(ERROR_HANDLE_EOF, "File is smaller than 12 MB.");
        goto Cleanup;
    }

    //
    // Create the file
    //
    PrintStatus("Attempting to create file \"%s\"", Filename);

    fileHandle = fopen(Filename, "w");
    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        PrintError(GetLastError(), "Unable to create file \"%s\"", Filename);
        goto Cleanup;
    }

    PrintStatus("Created file \"%s\"", Filename);

    //
    // Round up to the nearest sector size
    //
    filePointer = Filesize;
    RoundUpToSectorSize(&filePointer);

    //
    // Set the file length
    //
    if (!SetFileLength(fileHandle, filePointer))
    {
        goto Cleanup;
    }

    //
    // Set the valid data length
    //
    if (!SetFileValidData(fileHandle, filePointer))
    {
        PrintError(GetLastError(), "Unable to set valid data length");
        goto Cleanup;
    }

    PrintStatus("Set the valid data length");

    //
    // Create the footer
    //
    uuid_generate(uniqueId);

    if (!CreateVhdFooter(filePointer, uniqueId, TRUE, 0, &footer))
    {
        goto Cleanup;
    }

    //
    // Append the footer
    //
    if (!WriteFooter(fileHandle, filePointer, &footer))
    {
        goto Cleanup;
    }

    //
    // Clear out first 12 MB of file.
    //
    filePointer = 0;
    if (!ZeroFileContents(fileHandle, filePointer, 12 * 1024 * 1024))
    {
        goto Cleanup;
    }

    PrintStatus("VHD header area cleared.");

    status = TRUE;

Cleanup:

    if (fileHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(fileHandle);
        fileHandle = INVALID_HANDLE_VALUE;
    }

    return status;
}


BOOL
ExtendVhd(
    __in LPTSTR Filename,
    __in UINT64 Filesize
    )
/*++

Routine Description:

    Adds a VHD footer and clears the first few MB of an existing file.

Arguments:

    Filename - name of the VHD file to extend.

    Filesize - new virtual size of the VHD.

Return Value:

    BOOL - TRUE on success, FALSE on failure

--*/
{
    VHD_DISK_FOOTER footer;
    LARGE_INTEGER   originalFileSize;
    LARGE_INTEGER   newFileSize;
    HANDLE          fileHandle      = INVALID_HANDLE_VALUE;
    GUID            uniqueId;
    BOOL            status          = FALSE;

    PrintStatus("Extending \"%s\" to a larger size.", Filename);

    //
    // Open the file
    //
    if (!OpenVhdFile(Filename, &fileHandle, &originalFileSize))
    {
        goto Cleanup;
    }

    //
    // Error if file is too small to operate on.
    //
    if (originalFileSize < (1024 * 1024 * 12))
    {
        PrintError(ERROR_HANDLE_EOF, "File is smaller than 12 MB.");
        goto Cleanup;
    }

    //
    // Error if a shrink operation is attempted
    //
    if ((UINT64)originalFileSize > Filesize)
    {
        PrintError(ERROR_HANDLE_EOF, "Cannot extend a VHD smaller than its current size");
        goto Cleanup;
    }

    //
    // Read the existing footer.
    //
    originalFileSize -= VHD_SECTOR_SIZE;
    if (!ReadFooter(fileHandle, originalFileSize, &footer))
    {
        goto Cleanup;
    }

    RtlCopyMemory(&uniqueId, footer.UniqueId, sizeof(GUID));

    //
    // Round the extension up to the nearest sector size
    //
    newFileSize = Filesize;
    RoundUpToSectorSize(&newFileSize);
    
    //
    // Set the file length
    //
    if (!SetFileLength(fileHandle, newFileSize))
    {
        goto Cleanup;
    }

    //
    // Set the valid data length
    //
    if (!SetFileValidData(fileHandle, newFileSize))
    {
        PrintError(GetLastError(), "Unable to set valid data length");
        goto Cleanup;
    }

    PrintStatus("Set the valid data length");

    //
    // Create the footer
    //
    if (!CreateVhdFooter(newFileSize, uniqueId, TRUE, 0, &footer))
    {
        goto Cleanup;
    }

    //
    // Append the footer
    //
    if (!WriteFooter(fileHandle, newFileSize, &footer))
    {
        goto Cleanup;
    }

    //
    // Clear out original footer.
    //
    if (!ZeroFileContents(fileHandle, originalFileSize, VHD_SECTOR_SIZE))
    {
        goto Cleanup;
    }

    PrintStatus("Existing footer cleared");

    status = TRUE;

Cleanup:

    if (fileHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(fileHandle);
        fileHandle = INVALID_HANDLE_VALUE;
    }

    return status;
}


BOOL
RepairVhd(
    __in LPTSTR Filename,
    __in LPTSTR ChildFilename
    )
/*++

Routine Description:

    Repairs a VHD chain broken by an extend operation on the base VHD.
    Resets the base VHD size to match the child.

Arguments:

    Filename - name of the base VHD file to repair.

    ChildFilename - name of the next child VHD in the disk chain.

Return Value:

    BOOL - TRUE on success, FALSE on failure

--*/
{
    VHD_SPARSE_HEADER   childSparseHeader;
    VHD_DISK_FOOTER     footer;
    VHD_DISK_FOOTER     childFooter;
    LARGE_INTEGER       fileSize;
    LARGE_INTEGER       childFileSize;
    HANDLE              fileHandle      = INVALID_HANDLE_VALUE;
    HANDLE              childFileHandle = INVALID_HANDLE_VALUE;
    BOOL                status          = FALSE;
    GUID parentIdentifier;
    GUID baseIdentifier;

    PrintStatus("Resizing base VHD \"%s\" to match the size indicated in child VHD \"%s\".", Filename, ChildFilename);
    
    //
    // Open the base file
    //
    if (!OpenVhdFile(Filename, &fileHandle, &fileSize))
    {
        goto Cleanup;
    }

    //
    // Open the child file
    //
    if (!OpenVhdFile(ChildFilename, &childFileHandle, &childFileSize))
    {
        goto Cleanup;
    }

    //
    // Read the base VHD footer.
    //
    fileSize -= VHD_SECTOR_SIZE;
    if (!ReadFooter(fileHandle, fileSize, &footer))
    {
        goto Cleanup;
    }

    //
    // Verify we have a fixed or dynamic VHD as the base.
    //
    switch (footer.DiskType)
    {
    case VHD_TYPE_BIG_FIXED:
        PrintStatus("Opened \"%s\" as base VHD file, type is fixed-sized.", Filename);
        break;

    case VHD_TYPE_BIG_SPARSE:
        PrintStatus("Opened \"%s\" as base VHD file, type is dynamic-sized.", Filename);
        break;

    case VHD_TYPE_BIG_DIFF:
        PrintError(ERROR_BAD_FILE_TYPE, "VHD \"%s\" is a differencing disk.  You must enter the base VHD for this task.", Filename);
        goto Cleanup;

    default:
        PrintError(ERROR_BAD_FILE_TYPE, "VHD \"%s\" is an unsupported type.", Filename);
        goto Cleanup;
    }

    memcpy(baseIdentifier, footer.UniqueId, sizeof(GUID));
    PrintVhdIdentifier(baseIdentifier, "Base VHD's identifier is \"%s\"");

    //
    // Read the child VHD's footer.
    //
    childFileSize -= VHD_SECTOR_SIZE;
    if (!ReadFooter(childFileHandle, childFileSize, &childFooter))
    {
        goto Cleanup;
    }

    //
    // Verify we have a differencing disk as the child.
    //
    if (childFooter.DiskType != VHD_TYPE_BIG_DIFF)
    {
        PrintError(ERROR_BAD_FILE_TYPE, "Child VHD \"%s\" is not a differencing VHD.", ChildFilename);
        goto Cleanup;
    }

    PrintStatus("Opened \"%s\" as child VHD file.", ChildFilename);

    //
    // Retrieve the child's sparse header and parent pointer information
    //
    if (!ReadSparseHeader(childFileHandle, &childFooter, &childSparseHeader))
    {
        goto Cleanup;
    }
    
    memcpy(parentIdentifier, childSparseHeader.ParentUniqueId, sizeof(GUID));
    PrintVhdIdentifier(parentIdentifier, "Child VHD's parent identifier is \"%s\"");

    //
    // Verify that the child's parent pointer matches the parent id.
    //

    if (0 != memcmp(parentIdentifier, footer.UniqueId, sizeof(GUID)))
    {
        PrintError(ERROR_HANDLE_EOF, "Child's parent identifier doesn't match the base VHD's identifier.");
        goto Cleanup;
    }

    //
    // Determine the correct size for the base VHD.
    //

    PrintStatus("Resizing base VHD to match child size of %llu bytes", _byteswap_uint64(childFooter.CurrentSize));

    //
    // Resize the parent VHD to match the expected size.
    //

    if (footer.DiskType == VHD_TYPE_BIG_FIXED)
    {
        //
        // Fixed VHDs only contain a footer.  Truncate the file
        // and write the new footer.
        //

        LARGE_INTEGER newFileSize;
        newFileSize = _byteswap_uint64(childFooter.CurrentSize);

        //
        // Set the file length
        //
        if (!SetFileLength(fileHandle, newFileSize))
        {
            goto Cleanup;
        }
        
        PrintStatus("Base VHD truncated to new size %llu.", newFileSize);

        //
        // Create the footer
        //
        if (!CreateVhdFooter(newFileSize, parentIdentifier, TRUE, 0, &footer))
        {
            goto Cleanup;
        }

        //
        // Write the footer
        //
        if (!WriteFooter(fileHandle, newFileSize, &footer))
        {
            goto Cleanup;
        }
    }
    else
    {
        //
        // Sparse VHDs contain a footer and a duplicate header.
        // Update both of these, but leave the file size intact.
        //

        //
        // Create the footer
        //
        VHD_DISK_FOOTER newFooter;
        if (!CreateVhdFooter(_byteswap_uint64(childFooter.CurrentSize), parentIdentifier, FALSE, footer.DataOffset, &newFooter))
        {
            goto Cleanup;
        }

        //
        // Write the footer to the beginning and end of the VHD.
        //
        if (!GetFileSizeEx(fileHandle, &fileSize))
        {
            PrintError(GetLastError(), "Unable to retrieve file size");
            goto Cleanup;
        }

        if (!WriteFooter(fileHandle, fileSize - VHD_SECTOR_SIZE, &newFooter))
        {
            goto Cleanup;
        }

        if (!WriteFooter(fileHandle, 0, &newFooter))
        {
            goto Cleanup;
        }
    }

    PrintStatus("Operation complete.");
    status = TRUE;

Cleanup:

    if (fileHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(fileHandle);
        fileHandle = INVALID_HANDLE_VALUE;
    }

    if (childFileHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(childFileHandle);
        childFileHandle = INVALID_HANDLE_VALUE;
    }

    return status;
}


int 
main(
    __in int     argc,
    __in _TCHAR* argv[]
    )
/*++

Routine Description:

    Entrypoint for the tool

Arguments:

    argc - Number of command line arguments

    argv - Array of command line arguments

Return Value:

    int - 0 on success, >0 on failure

--*/
{
    enum PROGRAM_MODE programMode;
    LPTSTR       filename        = NULL;
    LPTSTR       childFilename   = NULL;
    UINT64       filesize        = 0;
    int          status          = 1;

    //
    // Parse command line
    //
    if (!ParseCommandLine(argc, argv, &programMode, &filename, &filesize, &childFilename))
    {
        goto Cleanup;
    }

    switch(programMode)
    {
    case ProgramModeCreate:
        if (!CreateNewVhd(filename, filesize))
        {
            goto Cleanup;
        }
        break;

    case ProgramModeConvert:
        if (!ConvertToVhd(filename))
        {
            goto Cleanup;
        }
        break;

    case ProgramModeExtend:
        if (!ExtendVhd(filename, filesize))
        {
            goto Cleanup;
        }
        break;

    case ProgramModeRepair:
        if (!RepairVhd(filename, childFilename))
        {
            goto Cleanup;
        }
        break;

    default:
        PrintStatus("Unknown Command");
        goto Cleanup;
    }

    PrintStatus("Complete");

    status = 0;

Cleanup:
    
    if (filename != NULL)
    {
        free(filename);
        filename = NULL;
    }

    if (childFilename != NULL)
    {
        free(childFilename);
        childFilename = NULL;
    }

    return status;
}

