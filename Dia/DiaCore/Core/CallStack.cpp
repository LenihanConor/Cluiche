#include "DiaCore/Core/CallStack.h"

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <dbghelp.h>

#include "DiaCore/Memory/Memory.h"

#pragma comment(lib, "version.lib")  // for "VerQueryValue"

// Normally it should be enough to use 'CONTEXT_FULL' (better would be 'CONTEXT_ALL')
#define USED_CONTEXT_FLAGS CONTEXT_FULL

namespace Dia
{
	namespace Core
	{
		// The "ugly" assembler-implementation is needed for systems before XP
		// If you have a new PSDK and you only compile for XP and later, then you can use 
		// the "RtlCaptureContext"
		// Currently there is no define which determines the PSDK-Version... 
		// So we just use the compiler-version (and assumes that the PSDK is 
		// the one which was installed by the VS-IDE)

		// INFO: If you want, you can use the RtlCaptureContext if you only target XP and later...
		//       But I currently use it in x64/IA64 environments...
		//#if defined(_M_IX86) && (_WIN32_WINNT <= 0x0500) && (_MSC_VER < 1400)

#if defined(_M_IX86)
#ifdef CURRENT_THREAD_VIA_EXCEPTION
		// TODO: The following is not a "good" implementation, 
		// because the callstack is only valid in the "__except" block...
#define GET_CURRENT_CONTEXT(c, contextFlags) \
	do { \
	memset(&c, 0, sizeof(CONTEXT)); \
	EXCEPTION_POINTERS *pExp = NULL; \
	__try { \
	throw 0; \
	} __except( ( (pExp = GetExceptionInformation()) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_EXECUTE_HANDLER)) {} \
	if (pExp != NULL) \
	memcpy(&c, pExp->ContextRecord, sizeof(CONTEXT)); \
	c.ContextFlags = contextFlags; \
	} while(0);
#else
		// The following should be enough for walking the callstack...
#define GET_CURRENT_CONTEXT(c, contextFlags) \
	do { \
	memset(&c, 0, sizeof(CONTEXT)); \
	c.ContextFlags = contextFlags; \
	__asm    call x \
	__asm x: pop eax \
	__asm    mov c.Eip, eax \
	__asm    mov c.Ebp, ebp \
	__asm    mov c.Esp, esp \
	} while(0);
#endif

#else

		// The following is defined for x86 (XP and higher), x64 and IA64:
#define GET_CURRENT_CONTEXT(c, contextFlags) \
	do { \
	memset(&c, 0, sizeof(CONTEXT)); \
	c.ContextFlags = contextFlags; \
	RtlCaptureContext(&c); \
	} while(0);
#endif

		// Entry for each Callstack-Entry
		struct CallstackEntry
		{
			unsigned long long offset;  // if 0, we have no valid entry
			char name[StackWalker::STACKWALK_MAX_NAMELEN];
			char undName[StackWalker::STACKWALK_MAX_NAMELEN];
			char undFullName[StackWalker::STACKWALK_MAX_NAMELEN];
			unsigned long long offsetFromSmybol;
			unsigned long offsetFromLine;
			unsigned long lineNumber;
			char lineFileName[StackWalker::STACKWALK_MAX_NAMELEN];
			unsigned long symType;
			const char* symTypeString;
			char moduleName[StackWalker::STACKWALK_MAX_NAMELEN];
			unsigned long long baseOfImage;
			char loadedImageName[StackWalker::STACKWALK_MAX_NAMELEN];
		};

		class StackWalkerInternal
		{
		public:
			StackWalkerInternal(StackWalker *parent, void* hProcess)
			{
				m_parent = parent;
				m_hDbhHelp = NULL;
				pSC = NULL;
				m_hProcess = hProcess;
				m_szSymPath = NULL;
				pSFTA = NULL;
				pSGLFA = NULL;
				pSGMB = NULL;
				pSGMI = NULL;
				pSGO = NULL;
				pSGSFA = NULL;
				pSI = NULL;
				pSLM = NULL;
				pSSO = NULL;
				pSW = NULL;
				pUDSN = NULL;
				pSGSP = NULL;
			}
			~StackWalkerInternal()
			{
				if (pSC != NULL)
					pSC(m_hProcess);  // SymCleanup
				if (m_hDbhHelp != NULL)
					FreeLibrary(m_hDbhHelp);
				m_hDbhHelp = NULL;
				m_parent = NULL;
				if(m_szSymPath != NULL)
					free(m_szSymPath);
				m_szSymPath = NULL;
			}
			int Init(const char* szSymPath)
			{
				if (m_parent == NULL)
					return FALSE;
				// Dynamically load the Entry-Points for dbghelp.dll:
				// First try to load the newsest one from
				TCHAR szTemp[4096];
				// But before wqe do this, we first check if the ".local" file exists
				if (GetModuleFileName(NULL, szTemp, 4096) > 0)
				{
					_tcscat_s(szTemp, _T(".local"));
					if (GetFileAttributes(szTemp) == INVALID_FILE_ATTRIBUTES)
					{
						// ".local" file does not exist, so we can try to load the dbghelp.dll from the "Debugging Tools for Windows"
						if (GetEnvironmentVariable(_T("ProgramFiles"), szTemp, 4096) > 0)
						{
							_tcscat_s(szTemp, _T("\\Debugging Tools for Windows\\dbghelp.dll"));
							// now check if the file exists:
							if (GetFileAttributes(szTemp) != INVALID_FILE_ATTRIBUTES)
							{
								m_hDbhHelp = LoadLibrary(szTemp);
							}
						}
						// Still not found? Then try to load the 64-Bit version:
						if ( (m_hDbhHelp == NULL) && (GetEnvironmentVariable(_T("ProgramFiles"), szTemp, 4096) > 0) )
						{
							_tcscat_s(szTemp, _T("\\Debugging Tools for Windows 64-Bit\\dbghelp.dll"));
							if (GetFileAttributes(szTemp) != INVALID_FILE_ATTRIBUTES)
							{
								m_hDbhHelp = LoadLibrary(szTemp);
							}
						}
					}
				}
				if (m_hDbhHelp == NULL)  // if not already loaded, try to load a default-one
					m_hDbhHelp = LoadLibrary( _T("dbghelp.dll") );
				if (m_hDbhHelp == NULL)
					return FALSE;
				pSI = (tSI) GetProcAddress(m_hDbhHelp, "SymInitialize" );
				pSC = (tSC) GetProcAddress(m_hDbhHelp, "SymCleanup" );

				pSW = (tSW) GetProcAddress(m_hDbhHelp, "StackWalk64" );
				pSGO = (tSGO) GetProcAddress(m_hDbhHelp, "SymGetOptions" );
				pSSO = (tSSO) GetProcAddress(m_hDbhHelp, "SymSetOptions" );

				pSFTA = (tSFTA) GetProcAddress(m_hDbhHelp, "SymFunctionTableAccess64" );
				pSGLFA = (tSGLFA) GetProcAddress(m_hDbhHelp, "SymGetLineFromAddr64" );
				pSGMB = (tSGMB) GetProcAddress(m_hDbhHelp, "SymGetModuleBase64" );
				pSGMI = (tSGMI) GetProcAddress(m_hDbhHelp, "SymGetModuleInfo64" );
				//pSGMI_V3 = (tSGMI_V3) GetProcAddress(m_hDbhHelp, "SymGetModuleInfo64" );
				pSGSFA = (tSGSFA) GetProcAddress(m_hDbhHelp, "SymGetSymFromAddr64" );
				pUDSN = (tUDSN) GetProcAddress(m_hDbhHelp, "UnDecorateSymbolName" );
				pSLM = (tSLM) GetProcAddress(m_hDbhHelp, "SymLoadModule64" );
				pSGSP =(tSGSP) GetProcAddress(m_hDbhHelp, "SymGetSearchPath" );

				if ( pSC == NULL || pSFTA == NULL || pSGMB == NULL || pSGMI == NULL ||
					pSGO == NULL || pSGSFA == NULL || pSI == NULL || pSSO == NULL ||
					pSW == NULL || pUDSN == NULL || pSLM == NULL )
				{
					FreeLibrary(m_hDbhHelp);
					m_hDbhHelp = NULL;
					pSC = NULL;
					return FALSE;
				}

				// SymInitialize
				if (szSymPath != NULL)
					m_szSymPath = _strdup(szSymPath);
				if (this->pSI(m_hProcess, m_szSymPath, FALSE) == FALSE)
					this->m_parent->OnDbgHelpErr("SymInitialize", GetLastError(), 0);

				unsigned long symOptions = this->pSGO();  // SymGetOptions
				symOptions |= SYMOPT_LOAD_LINES;
				symOptions |= SYMOPT_FAIL_CRITICAL_ERRORS;
				//symOptions |= SYMOPT_NO_PROMPTS;
				// SymSetOptions
				symOptions = this->pSSO(symOptions);

				char buf[StackWalker::STACKWALK_MAX_NAMELEN] = {0};
				if (this->pSGSP != NULL)
				{
					if (this->pSGSP(m_hProcess, buf, StackWalker::STACKWALK_MAX_NAMELEN) == FALSE)
						this->m_parent->OnDbgHelpErr("SymGetSearchPath", GetLastError(), 0);
				}
				char szUserName[1024] = {0};
				unsigned long dwSize = 1024;
				GetUserNameA(szUserName, &dwSize);
				this->m_parent->OnSymInit(buf, symOptions, szUserName);

				return TRUE;
			}

			StackWalker *m_parent;

			HMODULE m_hDbhHelp;
			void* m_hProcess;
			LPSTR m_szSymPath;

			typedef struct IMAGEHLP_MODULE64_V2 {
				unsigned long    SizeOfStruct;           // set to sizeof(IMAGEHLP_MODULE64)
				unsigned long long  BaseOfImage;            // base load address of module
				unsigned long    ImageSize;              // virtual size of the loaded module
				unsigned long    TimeDateStamp;          // date/time stamp from pe header
				unsigned long    CheckSum;               // checksum from the pe header
				unsigned long    NumSyms;                // number of symbols in the symbol table
				SYM_TYPE SymType;                // type of symbols loaded
				char     ModuleName[32];         // module name
				char     ImageName[256];         // image name
				char     LoadedImageName[256];   // symbol file name
			};


			// SymCleanup()
			typedef int (__stdcall *tSC)( IN void* hProcess );
			tSC pSC;

			// SymFunctionTableAccess64()
			typedef void* (__stdcall *tSFTA)( void* hProcess, unsigned long long AddrBase );
			tSFTA pSFTA;

			// SymGetLineFromAddr64()
			typedef int (__stdcall *tSGLFA)( IN void* hProcess, IN unsigned long long dwAddr,
				OUT PDWORD pdwDisplacement, OUT PIMAGEHLP_LINE64 Line );
			tSGLFA pSGLFA;

			// SymGetModuleBase64()
			typedef unsigned long long (__stdcall *tSGMB)( IN void* hProcess, IN unsigned long long dwAddr );
			tSGMB pSGMB;

			// SymGetModuleInfo64()
			typedef int (__stdcall *tSGMI)( IN void* hProcess, IN unsigned long long dwAddr, OUT IMAGEHLP_MODULE64_V2 *ModuleInfo );
			tSGMI pSGMI;

			//  // SymGetModuleInfo64()
			//  typedef int (__stdcall *tSGMI_V3)( IN void* hProcess, IN unsigned long long dwAddr, OUT IMAGEHLP_MODULE64_V3 *ModuleInfo );
			//  tSGMI_V3 pSGMI_V3;

			// SymGetOptions()
			typedef unsigned long (__stdcall *tSGO)( VOID );
			tSGO pSGO;

			// SymGetSymFromAddr64()
			typedef int (__stdcall *tSGSFA)( IN void* hProcess, IN unsigned long long dwAddr,
				OUT PDWORD64 pdwDisplacement, OUT PIMAGEHLP_SYMBOL64 Symbol );
			tSGSFA pSGSFA;

			// SymInitialize()
			typedef int (__stdcall *tSI)( IN void* hProcess, IN PSTR UserSearchPath, IN int fInvadeProcess );
			tSI pSI;

			// SymLoadModule64()
			typedef unsigned long long (__stdcall *tSLM)( IN void* hProcess, IN void* hFile,
				IN PSTR ImageName, IN PSTR ModuleName, IN unsigned long long BaseOfDll, IN unsigned long SizeOfDll );
			tSLM pSLM;

			// SymSetOptions()
			typedef unsigned long (__stdcall *tSSO)( IN unsigned long SymOptions );
			tSSO pSSO;

			// StackWalk64()
			typedef int (__stdcall *tSW)( 
				unsigned long MachineType, 
				void* hProcess,
				void* hThread, 
				LPSTACKFRAME64 StackFrame, 
				void* ContextRecord,
				PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
				PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
				PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
				PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress );
			tSW pSW;

			// UnDecorateSymbolName()
			typedef unsigned long (__stdcall WINAPI *tUDSN)( PCSTR DecoratedName, PSTR UnDecoratedName,
				unsigned long UndecoratedLength, unsigned long Flags );
			tUDSN pUDSN;

			typedef int (__stdcall WINAPI *tSGSP)(void* hProcess, PSTR SearchPath, unsigned long SearchPathLength);
			tSGSP pSGSP;


		private:
			// **************************************** ToolHelp32 ************************
#define MAX_MODULE_NAME32 255
#define TH32CS_SNAPMODULE   0x00000008
#pragma pack( push, 8 )
			typedef struct tagMODULEENTRY32
			{
				unsigned long   dwSize;
				unsigned long   th32ModuleID;       // This module
				unsigned long   th32ProcessID;      // owning process
				unsigned long   GlblcntUsage;       // Global usage count on the module
				unsigned long   ProccntUsage;       // Module usage count in th32ProcessID's context
				BYTE  * modBaseAddr;        // Base address of module in th32ProcessID's context
				unsigned long   modBaseSize;        // Size in bytes of module starting at modBaseAddr
				HMODULE hModule;            // The hModule of this module in th32ProcessID's context
				char    szModule[MAX_MODULE_NAME32 + 1];
				char    szExePath[MAX_PATH];
			} MODULEENTRY32;
			typedef MODULEENTRY32 *  PMODULEENTRY32;
			typedef MODULEENTRY32 *  LPMODULEENTRY32;
#pragma pack( pop )

			int GetModuleListTH32(void* hProcess, unsigned long pid)
			{
				// CreateToolhelp32Snapshot()
				typedef void* (__stdcall *tCT32S)(unsigned long dwFlags, unsigned long th32ProcessID);
				// Module32First()
				typedef int (__stdcall *tM32F)(void* hSnapshot, LPMODULEENTRY32 lpme);
				// Module32Next()
				typedef int (__stdcall *tM32N)(void* hSnapshot, LPMODULEENTRY32 lpme);

				// try both dlls...
				const TCHAR *dllname[] = { _T("kernel32.dll"), _T("tlhelp32.dll") };
				HINSTANCE hToolhelp = NULL;
				tCT32S pCT32S = NULL;
				tM32F pM32F = NULL;
				tM32N pM32N = NULL;

				void* hSnap;
				MODULEENTRY32 me;
				me.dwSize = sizeof(me);
				int keepGoing;
				size_t i;

				for (i = 0; i<(sizeof(dllname) / sizeof(dllname[0])); i++ )
				{
					hToolhelp = LoadLibrary( dllname[i] );
					if (hToolhelp == NULL)
						continue;
					pCT32S = (tCT32S) GetProcAddress(hToolhelp, "CreateToolhelp32Snapshot");
					pM32F = (tM32F) GetProcAddress(hToolhelp, "Module32First");
					pM32N = (tM32N) GetProcAddress(hToolhelp, "Module32Next");
					if ( (pCT32S != NULL) && (pM32F != NULL) && (pM32N != NULL) )
						break; // found the functions!
					FreeLibrary(hToolhelp);
					hToolhelp = NULL;
				}

				if (hToolhelp == NULL)
					return FALSE;

				hSnap = pCT32S( TH32CS_SNAPMODULE, pid );
				if (hSnap == (void*) -1)
					return FALSE;

				keepGoing = !!pM32F( hSnap, &me );
				int cnt = 0;
				while (keepGoing)
				{
					this->LoadModule(hProcess, me.szExePath, me.szModule, (unsigned long long) me.modBaseAddr, me.modBaseSize);
					cnt++;
					keepGoing = !!pM32N( hSnap, &me );
				}
				CloseHandle(hSnap);
				FreeLibrary(hToolhelp);
				if (cnt <= 0)
					return FALSE;
				return TRUE;
			}  // GetModuleListTH32

			// **************************************** PSAPI ************************
			typedef struct _MODULEINFO {
				void* lpBaseOfDll;
				unsigned long SizeOfImage;
				void* EntryPoint;
			} MODULEINFO, *LPMODULEINFO;

			int GetModuleListPSAPI(void* hProcess)
			{
				// EnumProcessModules()
				typedef int (__stdcall *tEPM)(void* hProcess, HMODULE *lphModule, unsigned long cb, LPDWORD lpcbNeeded );
				// GetModuleFileNameEx()
				typedef unsigned long (__stdcall *tGMFNE)(void* hProcess, HMODULE hModule, LPSTR lpFilename, unsigned long nSize );
				// GetModuleBaseName()
				typedef unsigned long (__stdcall *tGMBN)(void* hProcess, HMODULE hModule, LPSTR lpFilename, unsigned long nSize );
				// GetModuleInformation()
				typedef int (__stdcall *tGMI)(void* hProcess, HMODULE hModule, LPMODULEINFO pmi, unsigned long nSize );

				HINSTANCE hPsapi;
				tEPM pEPM;
				tGMFNE pGMFNE;
				tGMBN pGMBN;
				tGMI pGMI;

				unsigned long i;
				//ModuleEntry e;
				unsigned long cbNeeded;
				MODULEINFO mi;
				HMODULE *hMods = 0;
				char *tt = NULL;
				char *tt2 = NULL;
				const SIZE_T TTBUFLEN = 8096;
				int cnt = 0;

				hPsapi = LoadLibrary( _T("psapi.dll") );
				if (hPsapi == NULL)
					return FALSE;

				pEPM = (tEPM) GetProcAddress( hPsapi, "EnumProcessModules" );
				pGMFNE = (tGMFNE) GetProcAddress( hPsapi, "GetModuleFileNameExA" );
				pGMBN = (tGMFNE) GetProcAddress( hPsapi, "GetModuleBaseNameA" );
				pGMI = (tGMI) GetProcAddress( hPsapi, "GetModuleInformation" );
				if ( (pEPM == NULL) || (pGMFNE == NULL) || (pGMBN == NULL) || (pGMI == NULL) )
				{
					// we couldn´t find all functions
					FreeLibrary(hPsapi);
					return FALSE;
				}

				hMods = (HMODULE*) malloc(sizeof(HMODULE) * (TTBUFLEN / sizeof HMODULE));
				tt = (char*) malloc(sizeof(char) * TTBUFLEN);
				tt2 = (char*) malloc(sizeof(char) * TTBUFLEN);
				if ( (hMods == NULL) || (tt == NULL) || (tt2 == NULL) )
					goto cleanup;

				if ( ! pEPM( hProcess, hMods, TTBUFLEN, &cbNeeded ) )
				{
					//_ftprintf(fLogFile, _T("%lu: EPM failed, GetLastError = %lu\n"), g_dwShowCount, gle );
					goto cleanup;
				}

				if ( cbNeeded > TTBUFLEN )
				{
					//_ftprintf(fLogFile, _T("%lu: More than %lu module handles. Huh?\n"), g_dwShowCount, lenof( hMods ) );
					goto cleanup;
				}

				for ( i = 0; i < cbNeeded / sizeof hMods[0]; i++ )
				{
					// base address, size
					pGMI(hProcess, hMods[i], &mi, sizeof mi );
					// image file name
					tt[0] = 0;
					pGMFNE(hProcess, hMods[i], tt, TTBUFLEN );
					// module name
					tt2[0] = 0;
					pGMBN(hProcess, hMods[i], tt2, TTBUFLEN );

					unsigned long dwRes = this->LoadModule(hProcess, tt, tt2, (unsigned long long) mi.lpBaseOfDll, mi.SizeOfImage);
					if (dwRes != ERROR_SUCCESS)
						this->m_parent->OnDbgHelpErr("LoadModule", dwRes, 0);
					cnt++;
				}

cleanup:
				if (hPsapi != NULL) FreeLibrary(hPsapi);
				if (tt2 != NULL) free(tt2);
				if (tt != NULL) free(tt);
				if (hMods != NULL) free(hMods);

				return cnt != 0;
			}  // GetModuleListPSAPI

			unsigned long LoadModule(void* hProcess, const char* img, const char* mod, unsigned long long baseAddr, unsigned long size)
			{
				char *szImg = _strdup(img);
				char *szMod = _strdup(mod);
				unsigned long result = ERROR_SUCCESS;
				if ( (szImg == NULL) || (szMod == NULL) )
					result = ERROR_NOT_ENOUGH_MEMORY;
				else
				{
					if (pSLM(hProcess, 0, szImg, szMod, baseAddr, size) == 0)
						result = GetLastError();
				}
				unsigned long long fileVersion = 0;
				if ( (m_parent != NULL) && (szImg != NULL) )
				{
					// try to retrive the file-version:
					if ( (this->m_parent->m_options & StackWalker::RetrieveFileVersion) != 0)
					{
						VS_FIXEDFILEINFO *fInfo = NULL;
						unsigned long dwHandle;
						unsigned long dwSize = GetFileVersionInfoSizeA(szImg, &dwHandle);
						if (dwSize > 0)
						{
							void* vData = malloc(dwSize);
							if (vData != NULL)
							{
								if (GetFileVersionInfoA(szImg, dwHandle, dwSize, vData) != 0)
								{
									UINT len;
									TCHAR szSubBlock[] = _T("\\");
									if (VerQueryValue(vData, szSubBlock, (void**) &fInfo, &len) == 0)
										fInfo = NULL;
									else
									{
										fileVersion = ((unsigned long long)fInfo->dwFileVersionLS) + ((unsigned long long)fInfo->dwFileVersionMS << 32);
									}
								}
								free(vData);
							}
						}
					}

					// Retrive some additional-infos about the module
					IMAGEHLP_MODULE64_V2 Module;
					const char *szSymType = "-unknown-";
					if (this->GetModuleInfo(hProcess, baseAddr, &Module) != FALSE)
					{
						switch(Module.SymType)
						{
						case SymNone:
							szSymType = "-nosymbols-";
							break;
						case SymCoff:
							szSymType = "COFF";
							break;
						case SymCv:
							szSymType = "CV";
							break;
						case SymPdb:
							szSymType = "PDB";
							break;
						case SymExport:
							szSymType = "-exported-";
							break;
						case SymDeferred:
							szSymType = "-deferred-";
							break;
						case SymSym:
							szSymType = "SYM";
							break;
						case 8: //SymVirtual:
							szSymType = "Virtual";
							break;
						case 9: // SymDia:
							szSymType = "DIA";
							break;
						}
					}
					this->m_parent->OnLoadModule(img, mod, baseAddr, size, result, szSymType, Module.LoadedImageName, fileVersion);
				}
				if (szImg != NULL) free(szImg);
				if (szMod != NULL) free(szMod);
				return result;
			}
		public:
			int LoadModules(void* hProcess, unsigned long dwProcessId)
			{
				// first try toolhelp32
				if (GetModuleListTH32(hProcess, dwProcessId))
					return true;
				// then try psapi
				return GetModuleListPSAPI(hProcess);
			}


			int GetModuleInfo(void* hProcess, unsigned long long baseAddr, IMAGEHLP_MODULE64_V2 *pModuleInfo)
			{
				if(this->pSGMI == NULL)
				{
					SetLastError(ERROR_DLL_INIT_FAILED);
					return FALSE;
				}
		
				// could not retrive the bigger structure, try with the smaller one (as defined in VC7.1)...
				pModuleInfo->SizeOfStruct = sizeof(IMAGEHLP_MODULE64_V2);
				void *pData = malloc(4096); // reserve enough memory, so the bug in v6.3.5.1 does not lead to memory-overwrites...
				if (pData == NULL)
				{
					SetLastError(ERROR_NOT_ENOUGH_MEMORY);
					return FALSE;
				}
				memcpy(pData, pModuleInfo, sizeof(IMAGEHLP_MODULE64_V2));
				if (this->pSGMI(hProcess, baseAddr, (IMAGEHLP_MODULE64_V2*) pData) != FALSE)
				{
					// only copy as much memory as is reserved...
					memcpy(pModuleInfo, pData, sizeof(IMAGEHLP_MODULE64_V2));
					pModuleInfo->SizeOfStruct = sizeof(IMAGEHLP_MODULE64_V2);
					free(pData);
					return TRUE;
				}
				free(pData);
				SetLastError(ERROR_DLL_INIT_FAILED);
				return FALSE;
			}
		};

		// #############################################################
		StackWalker::StackWalker(unsigned long dwProcessId, void* hProcess)
		{
			this->m_options = OptionsAll;
			this->m_modulesLoaded = FALSE;
			this->m_hProcess = hProcess;
			this->m_sw = DIA_NEW( StackWalkerInternal(this, this->m_hProcess) );
			this->m_dwProcessId = dwProcessId;
			this->m_szSymPath = NULL;
		}
		StackWalker::StackWalker(int options, const char* szSymPath, unsigned long dwProcessId, void* hProcess)
		{
			if (hProcess == NULL)
			{
				hProcess = GetCurrentProcess();
			}
			
			if (dwProcessId == msInvalidProcessId)
			{
				dwProcessId = GetCurrentProcessId();
			}

			this->m_options = options;
			this->m_modulesLoaded = FALSE;
			this->m_hProcess = hProcess;
			this->m_sw = DIA_NEW( StackWalkerInternal(this, this->m_hProcess) );
			this->m_dwProcessId = dwProcessId;
			if (szSymPath != NULL)
			{
				this->m_szSymPath = _strdup(szSymPath);
				this->m_options |= SymBuildPath;
			}
			else
			{
				this->m_szSymPath = NULL;
			}			
		}

		StackWalker::~StackWalker()
		{
			if (m_szSymPath != NULL)
				free(m_szSymPath);
			m_szSymPath = NULL;
			if (this->m_sw != NULL)
				delete this->m_sw;
			this->m_sw = NULL;
		}

		int StackWalker::LoadModules()
		{
			if (this->m_sw == NULL)
			{
				SetLastError(ERROR_DLL_INIT_FAILED);
				return FALSE;
			}
			if (m_modulesLoaded != FALSE)
				return TRUE;

			// Build the sym-path:
			char *szSymPath = NULL;
			if ( (this->m_options & SymBuildPath) != 0)
			{
				const size_t nSymPathLen = 4096;
				szSymPath = (char*) malloc(nSymPathLen);
				if (szSymPath == NULL)
				{
					SetLastError(ERROR_NOT_ENOUGH_MEMORY);
					return FALSE;
				}
				szSymPath[0] = 0;
				// Now first add the (optional) provided sympath:
				if (this->m_szSymPath != NULL)
				{
					strcat_s(szSymPath, nSymPathLen, this->m_szSymPath);
					strcat_s(szSymPath, nSymPathLen, ";");
				}

				strcat_s(szSymPath, nSymPathLen, ".;");

				const size_t nTempLen = 1024;
				char szTemp[nTempLen];
				// Now add the current directory:
				if (GetCurrentDirectoryA(nTempLen, szTemp) > 0)
				{
					szTemp[nTempLen-1] = 0;
					strcat_s(szSymPath, nSymPathLen, szTemp);
					strcat_s(szSymPath, nSymPathLen, ";");
				}

				// Now add the path for the main-module:
				if (GetModuleFileNameA(NULL, szTemp, nTempLen) > 0)
				{
					szTemp[nTempLen-1] = 0;
					for (char *p = (szTemp+strlen(szTemp)-1); p >= szTemp; --p)
					{
						// locate the rightmost path separator
						if ( (*p == '\\') || (*p == '/') || (*p == ':') )
						{
							*p = 0;
							break;
						}
					}  // for (search for path separator...)
					if (strlen(szTemp) > 0)
					{
						strcat_s(szSymPath, nSymPathLen, szTemp);
						strcat_s(szSymPath, nSymPathLen, ";");
					}
				}
				if (GetEnvironmentVariableA("_NT_SYMBOL_PATH", szTemp, nTempLen) > 0)
				{
					szTemp[nTempLen-1] = 0;
					strcat_s(szSymPath, nSymPathLen, szTemp);
					strcat_s(szSymPath, nSymPathLen, ";");
				}
				if (GetEnvironmentVariableA("_NT_ALTERNATE_SYMBOL_PATH", szTemp, nTempLen) > 0)
				{
					szTemp[nTempLen-1] = 0;
					strcat_s(szSymPath, nSymPathLen, szTemp);
					strcat_s(szSymPath, nSymPathLen, ";");
				}
				if (GetEnvironmentVariableA("SYSTEMROOT", szTemp, nTempLen) > 0)
				{
					szTemp[nTempLen-1] = 0;
					strcat_s(szSymPath, nSymPathLen, szTemp);
					strcat_s(szSymPath, nSymPathLen, ";");
					// also add the "system32"-directory:
					strcat_s(szTemp, nTempLen, "\\system32");
					strcat_s(szSymPath, nSymPathLen, szTemp);
					strcat_s(szSymPath, nSymPathLen, ";");
				}

				if ( (this->m_options & SymBuildPath) != 0)
				{
					if (GetEnvironmentVariableA("SYSTEMDRIVE", szTemp, nTempLen) > 0)
					{
						szTemp[nTempLen-1] = 0;
						strcat_s(szSymPath, nSymPathLen, "SRV*");
						strcat_s(szSymPath, nSymPathLen, szTemp);
						strcat_s(szSymPath, nSymPathLen, "\\websymbols");
						strcat_s(szSymPath, nSymPathLen, "*http://msdl.microsoft.com/download/symbols;");
					}
					else
						strcat_s(szSymPath, nSymPathLen, "SRV*c:\\websymbols*http://msdl.microsoft.com/download/symbols;");
				}
			}

			// First Init the whole stuff...
			int bRet = this->m_sw->Init(szSymPath);
			if (szSymPath != NULL) free(szSymPath); szSymPath = NULL;
			if (bRet == FALSE)
			{
				this->OnDbgHelpErr("Error while initializing dbghelp.dll", 0, 0);
				SetLastError(ERROR_DLL_INIT_FAILED);
				return FALSE;
			}

			bRet = this->m_sw->LoadModules(this->m_hProcess, this->m_dwProcessId);
			if (bRet != FALSE)
				m_modulesLoaded = TRUE;
			return bRet;
		}

		int __stdcall myReadProcMem(
			void*      hProcess,
			unsigned long long     qwBaseAddress,
			void*       lpBuffer,
			unsigned long       nSize,
			LPDWORD     lpNumberOfBytesRead
			)
		{
			SIZE_T st;
			int bRet = ReadProcessMemory(hProcess, (void*) qwBaseAddress, lpBuffer, nSize, &st);
			*lpNumberOfBytesRead = (unsigned long) st;
			return bRet;
		}

		int StackWalker::ShowCallstack(void* hThread, const void *context)
		{
			if (hThread == NULL)
			{
				hThread = GetCurrentThread();
			}

			CONTEXT c;
			CallstackEntry csEntry;
			IMAGEHLP_SYMBOL64 *pSym = NULL;
			StackWalkerInternal::IMAGEHLP_MODULE64_V2 Module;
			IMAGEHLP_LINE64 Line;
			int frameNum;

			if (m_modulesLoaded == FALSE)
				this->LoadModules();  // ignore the result...

			if (this->m_sw->m_hDbhHelp == NULL)
			{
				SetLastError(ERROR_DLL_INIT_FAILED);
				return FALSE;
			}

			if (context == NULL)
			{
				// If no context is provided, capture the context
				if (hThread == GetCurrentThread())
				{
					GET_CURRENT_CONTEXT(c, USED_CONTEXT_FLAGS);
				}
				else
				{
					SuspendThread(hThread);
					memset(&c, 0, sizeof(CONTEXT));
					c.ContextFlags = USED_CONTEXT_FLAGS;
					if (GetThreadContext(hThread, &c) == FALSE)
					{
						ResumeThread(hThread);
						return FALSE;
					}
				}
			}
			else
			{
				c = *reinterpret_cast<const CONTEXT*>(context);
			}

			// init STACKFRAME for first call
			STACKFRAME64 s; // in/out stackframe
			memset(&s, 0, sizeof(s));
			unsigned long imageType;
#ifdef _M_IX86
			// normally, call ImageNtHeader() and use machine info from PE header
			imageType = IMAGE_FILE_MACHINE_I386;
			s.AddrPC.Offset = c.Eip;
			s.AddrPC.Mode = AddrModeFlat;
			s.AddrFrame.Offset = c.Ebp;
			s.AddrFrame.Mode = AddrModeFlat;
			s.AddrStack.Offset = c.Esp;
			s.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
			imageType = IMAGE_FILE_MACHINE_AMD64;
			s.AddrPC.Offset = c.Rip;
			s.AddrPC.Mode = AddrModeFlat;
			s.AddrFrame.Offset = c.Rsp;
			s.AddrFrame.Mode = AddrModeFlat;
			s.AddrStack.Offset = c.Rsp;
			s.AddrStack.Mode = AddrModeFlat;
#elif _M_IA64
			imageType = IMAGE_FILE_MACHINE_IA64;
			s.AddrPC.Offset = c.StIIP;
			s.AddrPC.Mode = AddrModeFlat;
			s.AddrFrame.Offset = c.IntSp;
			s.AddrFrame.Mode = AddrModeFlat;
			s.AddrBStore.Offset = c.RsBSP;
			s.AddrBStore.Mode = AddrModeFlat;
			s.AddrStack.Offset = c.IntSp;
			s.AddrStack.Mode = AddrModeFlat;
#else
#error "Platform not supported!"
#endif

			pSym = (IMAGEHLP_SYMBOL64 *) malloc(sizeof(IMAGEHLP_SYMBOL64) + STACKWALK_MAX_NAMELEN);
			if (!pSym) goto cleanup;  // not enough memory...
			memset(pSym, 0, sizeof(IMAGEHLP_SYMBOL64) + STACKWALK_MAX_NAMELEN);
			pSym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
			pSym->MaxNameLength = STACKWALK_MAX_NAMELEN;

			memset(&Line, 0, sizeof(Line));
			Line.SizeOfStruct = sizeof(Line);

			memset(&Module, 0, sizeof(Module));
			Module.SizeOfStruct = sizeof(Module);

			for (frameNum = 0; ; ++frameNum )
			{
				// get next stack frame (StackWalk64(), SymFunctionTableAccess64(), SymGetModuleBase64())
				// if this returns ERROR_INVALID_ADDRESS (487) or ERROR_NOACCESS (998), you can
				// assume that either you are done, or that the stack is so hosed that the next
				// deeper frame could not be found.
				// CONTEXT need not to be suplied if imageTyp is IMAGE_FILE_MACHINE_I386!
				if ( ! this->m_sw->pSW(imageType, this->m_hProcess, hThread, &s, &c, myReadProcMem, this->m_sw->pSFTA, this->m_sw->pSGMB, NULL) )
				{
					this->OnDbgHelpErr("StackWalk64", GetLastError(), s.AddrPC.Offset);
					break;
				}

				csEntry.offset = s.AddrPC.Offset;
				csEntry.name[0] = 0;
				csEntry.undName[0] = 0;
				csEntry.undFullName[0] = 0;
				csEntry.offsetFromSmybol = 0;
				csEntry.offsetFromLine = 0;
				csEntry.lineFileName[0] = 0;
				csEntry.lineNumber = 0;
				csEntry.loadedImageName[0] = 0;
				csEntry.moduleName[0] = 0;
				if (s.AddrPC.Offset == s.AddrReturn.Offset)
				{
					this->OnDbgHelpErr("StackWalk64-Endless-Callstack!", 0, s.AddrPC.Offset);
					break;
				}
				if (s.AddrPC.Offset != 0)
				{
					// we seem to have a valid PC
					// show procedure info (SymGetSymFromAddr64())
					if (this->m_sw->pSGSFA(this->m_hProcess, s.AddrPC.Offset, &(csEntry.offsetFromSmybol), pSym) != FALSE)
					{
						// TODO: Mache dies sicher...!
						strcpy_s(csEntry.name, pSym->Name);
						// UnDecorateSymbolName()
						this->m_sw->pUDSN( pSym->Name, csEntry.undName, STACKWALK_MAX_NAMELEN, UNDNAME_NAME_ONLY );
						this->m_sw->pUDSN( pSym->Name, csEntry.undFullName, STACKWALK_MAX_NAMELEN, UNDNAME_COMPLETE );
					}
					else
					{
						this->OnDbgHelpErr("SymGetSymFromAddr64", GetLastError(), s.AddrPC.Offset);
					}

					// show line number info, NT5.0-method (SymGetLineFromAddr64())
					if (this->m_sw->pSGLFA != NULL )
					{ // yes, we have SymGetLineFromAddr64()
						if (this->m_sw->pSGLFA(this->m_hProcess, s.AddrPC.Offset, &(csEntry.offsetFromLine), &Line) != FALSE)
						{
							csEntry.lineNumber = Line.LineNumber;
							// TODO: Mache dies sicher...!
							strcpy_s(csEntry.lineFileName, Line.FileName);
						}
						else
						{
							this->OnDbgHelpErr("SymGetLineFromAddr64", GetLastError(), s.AddrPC.Offset);
						}
					} // yes, we have SymGetLineFromAddr64()

					// show module info (SymGetModuleInfo64())
					if (this->m_sw->GetModuleInfo(this->m_hProcess, s.AddrPC.Offset, &Module ) != FALSE)
					{ // got module info OK
						switch ( Module.SymType )
						{
						case SymNone:
							csEntry.symTypeString = "-nosymbols-";
							break;
						case SymCoff:
							csEntry.symTypeString = "COFF";
							break;
						case SymCv:
							csEntry.symTypeString = "CV";
							break;
						case SymPdb:
							csEntry.symTypeString = "PDB";
							break;
						case SymExport:
							csEntry.symTypeString = "-exported-";
							break;
						case SymDeferred:
							csEntry.symTypeString = "-deferred-";
							break;
						case SymSym:
							csEntry.symTypeString = "SYM";
							break;
#if API_VERSION_NUMBER >= 9
						case SymDia:
							csEntry.symTypeString = "DIA";
							break;
#endif
						case 8: //SymVirtual:
							csEntry.symTypeString = "Virtual";
							break;
						default:
							//_snprintf( ty, sizeof ty, "symtype=%ld", (long) Module.SymType );
							csEntry.symTypeString = NULL;
							break;
						}

						// TODO: Mache dies sicher...!
						strcpy_s(csEntry.moduleName, Module.ModuleName);
						csEntry.baseOfImage = Module.BaseOfImage;
						strcpy_s(csEntry.loadedImageName, Module.LoadedImageName);
					} // got module info OK
					else
					{
						this->OnDbgHelpErr("SymGetModuleInfo64", GetLastError(), s.AddrPC.Offset);
					}
				} // we seem to have a valid PC

				CallstackEntryType et = nextEntry;
				if (frameNum == 0)
					et = firstEntry;
				this->OnCallstackEntry(et, csEntry);

				if (s.AddrReturn.Offset == 0)
				{
					this->OnCallstackEntry(lastEntry, csEntry);
					SetLastError(ERROR_SUCCESS);
					break;
				}
			} // for ( frameNum )

cleanup:
			if (pSym) free( pSym );

			if (context == NULL)
				ResumeThread(hThread);

			return TRUE;
		}

		void StackWalker::OnLoadModule(const char* img, const char* mod, unsigned long long baseAddr, unsigned long size, unsigned long result, const char* symType, const char* pdbName, unsigned long long fileVersion)
		{
			char buffer[STACKWALK_MAX_NAMELEN];
			if (fileVersion == 0)
				_snprintf_s(buffer, STACKWALK_MAX_NAMELEN, "%s:%s (%p), size: %d (result: %d), SymType: '%s', PDB: '%s'\n", img, mod, (void*) baseAddr, size, result, symType, pdbName);
			else
			{
				unsigned long v4 = (unsigned long) fileVersion & 0xFFFF;
				unsigned long v3 = (unsigned long) (fileVersion>>16) & 0xFFFF;
				unsigned long v2 = (unsigned long) (fileVersion>>32) & 0xFFFF;
				unsigned long v1 = (unsigned long) (fileVersion>>48) & 0xFFFF;
				_snprintf_s(buffer, STACKWALK_MAX_NAMELEN, "%s:%s (%p), size: %d (result: %d), SymType: '%s', PDB: '%s', fileVersion: %d.%d.%d.%d\n", img, mod, (void*) baseAddr, size, result, symType, pdbName, v1, v2, v3, v4);
			}
			OnOutput(buffer);
		}

		void StackWalker::OnCallstackEntry(CallstackEntryType eType, CallstackEntry &entry)
		{
			char buffer[STACKWALK_MAX_NAMELEN];
			if ( (eType != lastEntry) && (entry.offset != 0) )
			{
				if (entry.name[0] == 0)
				{
					strcpy_s(entry.name, "(function-name not available)");
				}
				
				if (entry.undName[0] != 0)
				{
					strcpy_s(entry.name, entry.undName);
				}
				
				if (entry.undFullName[0] != 0)
				{
					strcpy_s(entry.name, entry.undFullName);
				}
				
				if (entry.lineFileName[0] == 0)
				{
					strcpy_s(entry.lineFileName, "(filename not available)");
					if (entry.moduleName[0] == 0)
					{
						strcpy_s(entry.moduleName, "(module-name not available)");
					}

					_snprintf_s(buffer, STACKWALK_MAX_NAMELEN, "%p (%s): %s: %s\n", (void*) entry.offset, entry.moduleName, entry.lineFileName, entry.name);
				}
				else
				{
					_snprintf_s(buffer, STACKWALK_MAX_NAMELEN, "%s (%d): %s\n", entry.lineFileName, entry.lineNumber, entry.name);
				}
				
				int temp = strcmp(entry.name, "Dia::Core::StackWalker::ShowCallstack");
				if (temp != 0)
				{
					OnOutput(buffer);
				}			
			}
		}

		void StackWalker::OnDbgHelpErr(const char* szFuncName, unsigned long gle, unsigned long long addr)
		{
			char buffer[STACKWALK_MAX_NAMELEN];
			_snprintf_s(buffer, STACKWALK_MAX_NAMELEN, "ERROR: %s, GetLastError: %d (Address: %p)\n", szFuncName, gle, (void*) addr);
			OnOutput(buffer);
		}

		void StackWalker::OnSymInit(const char* szSearchPath, unsigned long symOptions, const char* szUserName)
		{
			char buffer[STACKWALK_MAX_NAMELEN];
			_snprintf_s(buffer, STACKWALK_MAX_NAMELEN, "SymInit: Symbol-SearchPath: '%s', symOptions: %d, UserName: '%s'\n", szSearchPath, symOptions, szUserName);
			OnOutput(buffer);
			// Also display the OS-version
#if _MSC_VER <= 1200
			OSVERSIONINFOA ver;
			ZeroMemory(&ver, sizeof(OSVERSIONINFOA));
			ver.dwOSVersionInfoSize = sizeof(ver);
			if (GetVersionExA(&ver) != FALSE)
			{
				_snprintf_s(buffer, STACKWALK_MAX_NAMELEN, "OS-Version: %d.%d.%d (%s)\n", 
					ver.dwMajorVersion, ver.dwMinorVersion, ver.dwBuildNumber,
					ver.szCSDVersion);
				OnOutput(buffer);
			}
#else
			OSVERSIONINFOEXA ver;
			ZeroMemory(&ver, sizeof(OSVERSIONINFOEXA));
			ver.dwOSVersionInfoSize = sizeof(ver);
			if (GetVersionExA( (OSVERSIONINFOA*) &ver) != FALSE)
			{
				_snprintf_s(buffer, STACKWALK_MAX_NAMELEN, "OS-Version: %d.%d.%d (%s) 0x%x-0x%x\n", 
					ver.dwMajorVersion, ver.dwMinorVersion, ver.dwBuildNumber,
					ver.szCSDVersion, ver.wSuiteMask, ver.wProductType);
				OnOutput(buffer);
			}
#endif
		}

		void StackWalker::OnOutput(const char* buffer)
		{
			OutputDebugStringA(buffer);
		}
	}
}