#pragma once

#include "DiaCore/Type/BasicTypeDefines.h"

namespace Dia
{
	namespace Core
	{
		class StackWalkerInternal;  // forward
		struct CallstackEntry;

		class StackWalker
		{
		public:
			static const unsigned long msInvalidProcessId = 99999;

			enum { STACKWALK_MAX_NAMELEN = 1024 }; // max name length for found symbols

			typedef enum StackWalkOptions
			{
				RetrieveNone = 0,					// No addition info will be retrieved 		
				RetrieveSymbol = 1,					// Try to get the symbol-name
				RetrieveLine = 2,					// Try to get the line for this symbol		
				RetrieveModuleInfo = 4,				// Try to retrieve the module-infos
				RetrieveFileVersion = 8,			// Also retrieve the version for the DLL/EXE			
				RetrieveVerbose = 0xF,				// Contains all the above		
				SymBuildPath = 0x10,				// Generate a "good" symbol-search-path		
				SymUseSymSrv = 0x20,				// Also use the public Microsoft-Symbol-Server			
				SymAll = 0x30,						// Contains all the above "Sym"-options		
				OptionsAll = 0x3F					// Contains all options (default)
			} StackWalkOptions;

			StackWalker(	int options = OptionsAll, 
							const char* szSymPath = NULL, 
							unsigned long dwProcessId = msInvalidProcessId, 
							void* hProcess = NULL );

			StackWalker(unsigned long dwProcessId, void* hProcess);
			virtual ~StackWalker();

			int LoadModules();

			int ShowCallstack( void* hThread = NULL, const void *context = NULL);
		protected:		

			enum CallstackEntryType {firstEntry, nextEntry, lastEntry};

			virtual void OnSymInit(const char* szSearchPath, unsigned long symOptions, const char* szUserName);
			virtual void OnLoadModule(const char* img, const char* mod, unsigned long long baseAddr, unsigned long size, unsigned long result, const char* symType, const char* pdbName, unsigned long long fileVersion);
			virtual void OnCallstackEntry(CallstackEntryType eType, CallstackEntry &entry);
			virtual void OnDbgHelpErr(const char* szFuncName, unsigned long gle, unsigned long long addr);
			virtual void OnOutput(const char* szText);

			StackWalkerInternal *m_sw;
			void* m_hProcess;
			unsigned long m_dwProcessId;
			int m_modulesLoaded;
			char* m_szSymPath;

			int m_options;

			friend StackWalkerInternal;
		};
	}
} 

