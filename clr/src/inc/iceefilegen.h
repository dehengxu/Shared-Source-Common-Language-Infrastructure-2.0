// ==++==
// 
//   
//    Copyright (c) 2006 Microsoft Corporation.  All rights reserved.
//   
//    The use and distribution terms for this software are contained in the file
//    named license.txt, which can be found in the root of this distribution.
//    By using this software in any fashion, you are agreeing to be bound by the
//    terms of this license.
//   
//    You must not remove this notice, or any other, from this software.
//   
// 
// ==--==
/*****************************************************************************
 **                                                                         **
 ** ICeeFileGen.h - code generator interface.                               **
 **                                                                         **
 ** This interface provides functionality to create a CLR PE executable.    **
 ** This will typically be used by compilers to generate their compiled     **
 ** output executable.                                                      **
 **                                                                         **
 ** The implemenation lives in mscorpe.dll                                  **
 **                                                                         **
 *****************************************************************************/

/*
  This is how this is typically used:

  CreateICeeFileGen(...);       // Get a ICeeFileGen

  CreateCeeFile(...);           // Get a HCEEFILE (called for every output file needed)
  SetOutputFileName(...);       // Set the name for the output file
  pEmit = IMetaDataEmit object; // Get a metadata emitter
  GetSectionBlock(...);, AddSectionReloc(...); ... // Get blocks, write non-metadata information, and add necessary relocation
  EmitMetaDataEx(pEmit);        // Write out the metadata
  GenerateCeeFile(...);         // Write out the file. Implicitly calls LinkCeeFile and FixupCeeFile

  DestroyICeeFileGen(...);      // Release the ICeeFileGen object
*/


#ifndef _ICEEFILEGEN_H_
#define _ICEEFILEGEN_H_

#include <ole2.h>
#include "cor.h"

class ICeeFileGen;

typedef void *HCEEFILE;

EXTERN_C HRESULT __stdcall CreateICeeFileGen(ICeeFileGen **ceeFileGen); // call this to instantiate an ICeeFileGen interface
EXTERN_C HRESULT __stdcall DestroyICeeFileGen(ICeeFileGen **ceeFileGen); // call this to delete an ICeeFileGen

#define ICEE_CREATE_FILE_PE32	       0x00000001  // Create a PE  (32-bit)
#define ICEE_CREATE_FILE_PE64	       0x00000002  // Create a PE+ (64-bit) 
#define ICEE_CREATE_FILE_CORMAIN_STUB  0x00000004  // add a mscoree!_Cor___Main call stub 
#define ICEE_CREATE_FILE_STRIP_RELOCS  0x00000008  // strip the .reloc section
#define ICEE_CREATE_FILE_EMIT_FIXUPS   0x00000010  // emit fixups for use by Vulcan

#define ICEE_CREATE_MACHINE_MASK       0x0000FF00  // space for up to 256 machine targets
#define ICEE_CREATE_MACHINE_ILLEGAL    0x00000000  // An illegal machine name
#define ICEE_CREATE_MACHINE_I386       0x00000100  // Create a IMAGE_FILE_MACHINE_I386 
#define ICEE_CREATE_MACHINE_IA64       0x00000200  // Create a IMAGE_FILE_MACHINE_IA64
#define ICEE_CREATE_MACHINE_AMD64      0x00000400  // Create a IMAGE_FILE_MACHINE_AMD64

    // Pass this to CreateCeeFileEx to create a pure IL Exe or DLL
#define ICEE_CREATE_FILE_PURE_IL  ICEE_CREATE_FILE_PE32         | \
                                  ICEE_CREATE_FILE_CORMAIN_STUB | \
                                  ICEE_CREATE_MACHINE_I386

class ICeeFileGen {
  public:
    virtual HRESULT CreateCeeFile(HCEEFILE *ceeFile); // call this to instantiate a file handle

    virtual HRESULT EmitMetaData (HCEEFILE ceeFile, IMetaDataEmit *emitter, mdScope scope);
    virtual HRESULT EmitLibraryName (HCEEFILE ceeFile, IMetaDataEmit *emitter, mdScope scope);
    virtual HRESULT EmitMethod ();
    virtual HRESULT GetMethodRVA (HCEEFILE ceeFile, ULONG codeOffset, ULONG *codeRVA); 
    virtual HRESULT EmitSignature ();

    virtual HRESULT EmitString (HCEEFILE ceeFile,__in LPWSTR strValue, ULONG *strRef);
    virtual HRESULT GenerateCeeFile (HCEEFILE ceeFile);

    virtual HRESULT SetOutputFileName (HCEEFILE ceeFile, __in LPWSTR outputFileName);
    __success(return == S_OK)
    virtual HRESULT GetOutputFileName (HCEEFILE ceeFile, __out LPWSTR *outputFileName);

    virtual HRESULT SetResourceFileName (HCEEFILE ceeFile, __in LPWSTR resourceFileName);

    __success(return == S_OK)
    virtual HRESULT GetResourceFileName (HCEEFILE ceeFile, __out LPWSTR *resourceFileName);

    virtual HRESULT SetImageBase(HCEEFILE ceeFile, size_t imageBase);

    virtual HRESULT SetSubsystem(HCEEFILE ceeFile, DWORD subsystem, DWORD major, DWORD minor);

    virtual HRESULT SetEntryClassToken ();
    virtual HRESULT GetEntryClassToken ();

    virtual HRESULT SetEntryPointDescr ();
    virtual HRESULT GetEntryPointDescr ();

    virtual HRESULT SetEntryPointFlags ();
    virtual HRESULT GetEntryPointFlags ();

    virtual HRESULT SetDllSwitch (HCEEFILE ceeFile, BOOL dllSwitch);
    virtual HRESULT GetDllSwitch (HCEEFILE ceeFile, BOOL *dllSwitch);

    virtual HRESULT SetLibraryName (HCEEFILE ceeFile, __in LPWSTR LibraryName);
    __success( return == S_OK )
    virtual HRESULT GetLibraryName (HCEEFILE ceeFile, __out LPWSTR *LibraryName);

    virtual HRESULT SetLibraryGuid (HCEEFILE ceeFile, __in LPWSTR LibraryGuid);

    virtual HRESULT DestroyCeeFile(HCEEFILE *ceeFile); // call this to delete a file handle

    virtual HRESULT GetSectionCreate (HCEEFILE ceeFile, const char *name, DWORD flags, HCEESECTION *section);
    virtual HRESULT GetIlSection (HCEEFILE ceeFile, HCEESECTION *section);
    virtual HRESULT GetRdataSection (HCEEFILE ceeFile, HCEESECTION *section);

    virtual HRESULT GetSectionDataLen (HCEESECTION section, ULONG *dataLen);
    virtual HRESULT GetSectionBlock (HCEESECTION section, ULONG len, ULONG align=1, void **ppBytes=0);
    virtual HRESULT TruncateSection (HCEESECTION section, ULONG len);
    virtual HRESULT AddSectionReloc (HCEESECTION section, ULONG offset, HCEESECTION relativeTo, CeeSectionRelocType relocType);

    // deprecated: use SetDirectoryEntry instead
    virtual HRESULT SetSectionDirectoryEntry (HCEESECTION section, ULONG num);

    virtual HRESULT CreateSig ();
    virtual HRESULT AddSigArg ();
    virtual HRESULT SetSigReturnType ();
    virtual HRESULT SetSigCallingConvention ();
    virtual HRESULT DeleteSig ();

    virtual HRESULT SetEntryPoint (HCEEFILE ceeFile, mdMethodDef method);
    virtual HRESULT GetEntryPoint (HCEEFILE ceeFile, mdMethodDef *method);

    virtual HRESULT SetComImageFlags (HCEEFILE ceeFile, DWORD mask);
    virtual HRESULT GetComImageFlags (HCEEFILE ceeFile, DWORD *mask);

    // get IMapToken interface for tracking mapped tokens
    virtual HRESULT GetIMapTokenIface(HCEEFILE ceeFile, IMetaDataEmit *emitter, IUnknown **pIMapToken);
    virtual HRESULT SetDirectoryEntry (HCEEFILE ceeFile, HCEESECTION section, ULONG num, ULONG size, ULONG offset = 0);

    // Write out the metadata in "emitter" to the metadata section in "ceeFile"
    // Use EmitMetaDataAt() for more control
    virtual HRESULT EmitMetaDataEx (HCEEFILE ceeFile, IMetaDataEmit *emitter); 

    virtual HRESULT EmitLibraryNameEx (HCEEFILE ceeFile, IMetaDataEmit *emitter);
    virtual HRESULT GetIMapTokenIfaceEx(HCEEFILE ceeFile, IMetaDataEmit *emitter, IUnknown **pIMapToken);

    virtual HRESULT EmitMacroDefinitions(HCEEFILE ceeFile, void *pData, DWORD cData);
    virtual HRESULT CreateCeeFileFromICeeGen(
        ICeeGen *pFromICeeGen, HCEEFILE *ceeFile, DWORD createFlags = ICEE_CREATE_FILE_PURE_IL); // call this to instantiate a file handle

    virtual HRESULT SetManifestEntry(HCEEFILE ceeFile, ULONG size, ULONG offset);

    virtual HRESULT SetEnCRVABase(HCEEFILE ceeFile, ULONG dataBase, ULONG rdataBase);
    virtual HRESULT GenerateCeeMemoryImage (HCEEFILE ceeFile, void **ppImage);

    virtual HRESULT ComputeSectionOffset(HCEESECTION section, __in char *ptr,
                                         unsigned *offset);
    
    virtual HRESULT ComputeOffset(HCEEFILE file, __in char *ptr,
                                  HCEESECTION *pSection, unsigned *offset);
    
    virtual HRESULT GetCorHeader(HCEEFILE ceeFile, 
                                 IMAGE_COR20_HEADER **header);
    
    // Layout the sections and assign their starting addresses
    virtual HRESULT LinkCeeFile (HCEEFILE ceeFile);     

    // Apply relocations to any pointer data. Also generate PE base relocs
    virtual HRESULT FixupCeeFile (HCEEFILE ceeFile);

    // Base RVA assinged to the section. To be called only after LinkCeeFile()
    virtual HRESULT GetSectionRVA (HCEESECTION section, ULONG *rva);
    
    __success(return == S_OK)
    virtual HRESULT ComputeSectionPointer(HCEESECTION section, ULONG offset,
                                          __out char **ptr);

    virtual HRESULT SetObjSwitch (HCEEFILE ceeFile, BOOL objSwitch);
    virtual HRESULT GetObjSwitch (HCEEFILE ceeFile, BOOL *objSwitch);
    virtual HRESULT SetVTableEntry(HCEEFILE ceeFile, ULONG size, ULONG offset);
    // See the end of interface for another overload of AetVTableEntry

    virtual HRESULT SetStrongNameEntry(HCEEFILE ceeFile, ULONG size, ULONG offset);

    // Emit the metadata from "emitter".
    // If 'section != 0, it will put the data in 'buffer'.  This
    // buffer is assumed to be in 'section' at 'offset' and of size 'buffLen'
    // (should use GetSaveSize to insure that buffer is big enough
    virtual HRESULT EmitMetaDataAt (HCEEFILE ceeFile, IMetaDataEmit *emitter, 
                                    HCEESECTION section, DWORD offset, 
                                    BYTE* buffer, unsigned buffLen);

    virtual HRESULT GetFileTimeStamp (HCEEFILE ceeFile, DWORD *pTimeStamp);

    // Add a notification handler. If it implements an interface that
    // the ICeeFileGen understands, S_OK is returned. Otherwise,
    // E_NOINTERFACE.
    virtual HRESULT AddNotificationHandler(HCEEFILE ceeFile,
                                           IUnknown *pHandler);

    virtual HRESULT SetFileAlignment(HCEEFILE ceeFile, ULONG fileAlignment);

    virtual HRESULT ClearComImageFlags (HCEEFILE ceeFile, DWORD mask);

    // call this to instantiate a PE+ (64-bit PE file)
    virtual HRESULT CreateCeeFileEx(HCEEFILE *ceeFile, ULONG createFlags);
    virtual HRESULT SetImageBase64(HCEEFILE ceeFile, ULONGLONG imageBase);

    virtual HRESULT GetHeaderInfo (HCEEFILE ceeFile, PIMAGE_NT_HEADERS *ppNtHeaders,
                                                     PIMAGE_SECTION_HEADER *ppSections,
                                                     ULONG *pNumSections);

    // Seed file is a base file which is copied over into the output file
    // Note that there are restrictions on the seed file (the sections
    // cannot be relocated), and that the copy is not complete as the new
    // headers overwrite the seed file headers.
    virtual HRESULT CreateCeeFileEx2(HCEEFILE *ceeFile, ULONG createFlags,
                                     LPCWSTR seedFileName = NULL);

    virtual HRESULT SetVTableEntry64(HCEEFILE ceeFile, ULONG size, void* ptr);
};

#endif
