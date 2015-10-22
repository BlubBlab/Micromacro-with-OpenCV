/******************************************************************************
	Project: 	MicroMacro
	Author: 	SolarStrike Software
	URL:		www.solarstrike.net
	License:	Modified BSD (see license.txt)
******************************************************************************/

#ifndef PROCESS_LUA_H
#define PROCESS_LUA_H

//#include <Windows.h>
#include "wininclude.h"
#include <Dbghelp.h>
#include <string>
#include <vector>
#include "types.h"

	#define PROCESS_MODULE_NAME			"process"
	#define MEMORY_READ_FAIL			0x00000001 // cannot read memory
	#define MEMORY_WRITE_FAIL			0x00000010 // cannot write memory

	typedef struct lua_State lua_State;
	//special functions
	typedef LONG (NTAPI *NtSuspendProcess)(IN HANDLE ProcessHandle);
	typedef LONG (NTAPI *NtResumeProcess)(IN HANDLE ProcessHandle);
	typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
	


	int isWindows32();
	int isWindows64();
	bool ReadPMemory(HANDLE process, LPCVOID curAddr, LPVOID buffer, SIZE_T  bufferLen, SIZE_T &bytesRead);
	class Process_lua
	{
		protected:
			// Error strings
			static const char *szInvalidHandleError;
			static const char *szInvalidDataType;
			
			static NtResumeProcess pfnNtResumeProcess;
			// Necessary data
			static std::vector<DWORD> attachedThreadIds;

			// Helper functions
			static std::string narrowString(std::wstring);
			static std::wstring charToWChar(std::string);
			static std::string readString(HANDLE, size_t, int &, size_t);
			static std::wstring readUString(HANDLE, size_t, int &, size_t);
			static void writeString(HANDLE, size_t, char *, int &, size_t);
			static void writeUString(HANDLE, size_t, wchar_t *, int &, size_t);

			template <class T>
			static T readMemory(HANDLE process, size_t address, int &err)
			{
				T buffer;
				SIZE_T bytesread = 0;
				err = 0;
				int success = 0;

				success = ReadProcessMemory(process, (LPCVOID)address,
				(void *)&buffer, sizeof(T), &bytesread);

				if( success == 0 ){
					int trys_counter = 0;
					
					
					NtSuspendProcess pfnNtSuspendProcess = (NtSuspendProcess)GetProcAddress(GetModuleHandle("ntdll"), "NtSuspendProcess");
					NtResumeProcess pfnNtResumeProcess = (NtResumeProcess)GetProcAddress(GetModuleHandle("ntdll"), "NtResumeProcess");

					while(success == 0 && trys_counter < 25){
						pfnNtSuspendProcess(process);
						success = ReadProcessMemory(process, (LPCVOID)address,
						(void *)&buffer, sizeof(T), &bytesread);
						pfnNtResumeProcess(process);
						// if it still fail the only reason could be we looked it in a state where it acess itself the memory
						// so give it sometime and try again
						if( success == 0 )
							Sleep(10);
						trys_counter ++;
					}
					if( success == 0 )
						err = MEMORY_READ_FAIL;
				}


				return buffer;
			}

			template <class T>
			static void writeMemory(HANDLE process, size_t address, T data, int &err)
			{
				SIZE_T byteswritten = 0;
				err = 0;
				int success = 0;
				DWORD old;

				VirtualProtectEx(process, (void *)address, sizeof(T), PAGE_READWRITE, &old);
				success = WriteProcessMemory(process, (void *)address,
				(void*)&data, sizeof(T), &byteswritten);
				VirtualProtectEx(process, (void *)address, sizeof(T), old, &old);

				if( success == 0 ){
					int trys_counter = 0;
					
					NtSuspendProcess pfnNtSuspendProcess = (NtSuspendProcess)GetProcAddress(GetModuleHandle("ntdll"), "NtSuspendProcess");
					NtResumeProcess pfnNtResumeProcess = (NtResumeProcess)GetProcAddress(GetModuleHandle("ntdll"), "NtResumeProcess");
	
					while(success == 0 && trys_counter < 25){
						pfnNtSuspendProcess(process);
						VirtualProtectEx(process, (void *)address, sizeof(T), PAGE_READWRITE, &old);
						success = WriteProcessMemory(process, (void *)address,
						(void*)&data, sizeof(T), &byteswritten);
						VirtualProtectEx(process, (void *)address, sizeof(T), old, &old);
						// if it still fail the only reason could be we looked it in a state where it acess itself the memory
						// so give it sometime and try again
						pfnNtResumeProcess(process);
						if( success == 0 )
							Sleep(10);
						trys_counter ++;
					}

					if( success == 0 )
						err = MEMORY_WRITE_FAIL;
				}
					
			}

			static size_t readBatch_parsefmt(const char *, std::vector<MicroMacro::BatchJob> &);
			static bool procDataCompare(const char *, const char *, const char *);

			// Actual Lua functions
			static int open(lua_State *);
			static int close(lua_State *);
			static int read(lua_State *);
			static int readPtr(lua_State *);
			static int readBatch(lua_State *);
			static int readChunk(lua_State *);
			static int write(lua_State *);
			static int writePtr(lua_State *);
			static int findPattern(lua_State *);
			static int findByWindow(lua_State *);
			static int findByExe(lua_State *);
			static int getModuleAddress(lua_State *);
			static int getModules(lua_State *);
			static int attachInput(lua_State *);
			static int detachInput(lua_State *);
			static int is32bit(lua_State *);
			static int is64bit(lua_State *);
			static int terminate(lua_State *);
			static int getWindows(lua_State *);
			static int suspend(lua_State *);
			static int resume(lua_State *);
		public:
			static int regmod(lua_State *);
			static int cleanup(lua_State *);
	};
#endif
