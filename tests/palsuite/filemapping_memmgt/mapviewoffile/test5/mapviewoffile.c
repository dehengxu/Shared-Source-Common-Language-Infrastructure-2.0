/*=============================================================
**
** Source:  MapViewOfFile.c
**
** Purpose: Negative test the MapViewOfFile API.
**          Passing invalid values for the hFileMappingObject.
**
** Depends: CreatePipe,
**          CreateFile,
**          CreateFileMapping,
**          CloseHandle.
**          
** 
**  Copyright (c) 2006 Microsoft Corporation.  All rights reserved.
** 
**  The use and distribution terms for this software are contained in the file
**  named license.txt, which can be found in the root of this distribution.
**  By using this software in any fashion, you are agreeing to be bound by the
**  terms of this license.
** 
**  You must not remove this notice, or any other, from this software.
** 
**
**============================================================*/
#include <palsuite.h>

int __cdecl main(int argc, char *argv[])
{

    const   int MAPPINGSIZE = 2048;
    HANDLE  hFileMapping;
    LPVOID  lpMapViewAddress;
    HANDLE  hReadPipe   = NULL;
    HANDLE  hWritePipe  = NULL;
    BOOL    bRetVal;

    SECURITY_ATTRIBUTES lpPipeAttributes;

    /* Initialize the PAL environment.
     */
    if(0 != PAL_Initialize(argc, argv))
    {
        return FAIL;
    }


    /* Attempt to create a MapViewOfFile with a NULL handle.
     */
    hFileMapping = NULL;
    
    lpMapViewAddress = MapViewOfFile(
                            hFileMapping,
                            FILE_MAP_WRITE, /* access code */
                            0,              /* high order offset */
                            0,              /* low order offset */
                            MAPPINGSIZE);   /* number of bytes for map */

    if((NULL != lpMapViewAddress) && 
       (GetLastError() != ERROR_INVALID_HANDLE))
    {
        Trace("ERROR:%u: Able to create a MapViewOfFile with "
              "hFileMapping=0x%lx.\n",
              GetLastError());
        UnmapViewOfFile(lpMapViewAddress);
        Fail("");
    }

    /* Attempt to create a MapViewOfFile with an invalid handle.
     */
    hFileMapping = INVALID_HANDLE_VALUE;
    
    lpMapViewAddress = MapViewOfFile(
                            hFileMapping,
                            FILE_MAP_WRITE, /* access code */
                            0,              /* high order offset */
                            0,              /* low order offset */
                            MAPPINGSIZE);   /* number of bytes for map */

    if((NULL != lpMapViewAddress) && 
       (GetLastError() != ERROR_INVALID_HANDLE))
    {
        Trace("ERROR:%u: Able to create a MapViewOfFile with "
              "hFileMapping=0x%lx.\n",
              GetLastError());
        UnmapViewOfFile(lpMapViewAddress);
        Fail("");
    }

    /* Setup SECURITY_ATTRIBUTES structure for CreatePipe.
     */
    lpPipeAttributes.nLength              = sizeof(lpPipeAttributes); 
    lpPipeAttributes.lpSecurityDescriptor = NULL; 
    lpPipeAttributes.bInheritHandle       = TRUE; 

    /* Create a Pipe.
     */
    bRetVal = CreatePipe(&hReadPipe,       /* read handle*/
                         &hWritePipe,      /* write handle */
                         &lpPipeAttributes,/* security attributes*/
                         0);               /* pipe size*/
    if (bRetVal == FALSE)
    {
        Fail("ERROR: %ld :Unable to create pipe\n", 
             GetLastError());
    }

    /* Attempt creating a MapViewOfFile with a Pipe Handle.
     */
    lpMapViewAddress = MapViewOfFile(
                            hReadPipe,
                            FILE_MAP_WRITE, /* access code */
                            0,              /* high order offset */
                            0,              /* low order offset */
                            MAPPINGSIZE);   /* number of bytes for map */

    if((NULL != lpMapViewAddress) && 
       (GetLastError() != ERROR_INVALID_HANDLE))
    {
        Trace("ERROR:%u: Able to create a MapViewOfFile with "
              "hFileMapping=0x%lx.\n",
              GetLastError());
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        UnmapViewOfFile(lpMapViewAddress);
        Fail("");
    }

    /* Clean-up and Terminate the PAL.
    */
    CloseHandle(hReadPipe);
    CloseHandle(hWritePipe);
    PAL_Terminate();
    return PASS;
}

  
