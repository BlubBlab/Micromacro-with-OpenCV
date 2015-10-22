/******************************************************************************
Project: 	MicroMacro
Author: 	SolarStrike Software
URL:		www.solarstrike.net
License:	Modified BSD (see license.txt)
******************************************************************************/
#pragma warning( disable : 4800)
#include "process_lua.h"
#include "memorychunk_lua.h"
#include "luatypes.h"
#include "error.h"
#include "wininclude.h"
#include "macro.h"
#include "strl.h"
#include "settings.h"
#include "debugmessages.h"
#include "logger.h"
#include <wchar.h>
#include <string.h>
extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <tlhelp32.h>

const char *Process_lua::szInvalidHandleError = "Invalid process handle.";
const char *Process_lua::szInvalidDataType = "Invalid data type given. Cannot read/write memory without a proper type.";

std::vector<DWORD> Process_lua::attachedThreadIds;


LPFN_ISWOW64PROCESS fnIsWow64Process = NULL;




using MicroMacro::BatchJob;
using MicroMacro::ProcHandle;
using MicroMacro::MemoryChunk;

struct EnumFilterWindows
{
	DWORD dwPid;
	std::vector<HWND> hwndList;
};


/* These are mostly just helper functions and are not actually registered
into the Lua state. They are, however, used by functions that are
accessible by Lua.
*/
int isWindows32()
{
	return !isWindows64();
}

int isWindows64()
{
#ifdef _WIN64
	return true;
#else
	// If not already assigned, read it, then check again
	if( !fnIsWow64Process )
		fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");

	if( !fnIsWow64Process ) // Function not found; assume 32-bit
		return false;

	BOOL iswow64;
	bool success = fnIsWow64Process(GetCurrentProcess(), &iswow64);

	if( !success )
		Logger::instance()->add("Failed to call IsWow64Process() in %s. Error code: %d\n",
		__FUNCTION__, GetLastError());

	return iswow64;
#endif
}
bool ReadPMemory(HANDLE process, LPCVOID curAddr, LPVOID buffer, SIZE_T  bufferLen, SIZE_T *bytesRead)
{

	SIZE_T bytesread = 0;

	int success = 0;

	success = ReadProcessMemory(process, curAddr,
		buffer, bufferLen, bytesRead);

	if( success == 0 ){
		int trys_counter = 0;


		NtSuspendProcess pfnNtSuspendProcess = (NtSuspendProcess)GetProcAddress(GetModuleHandle("ntdll"), "NtSuspendProcess");
		NtResumeProcess pfnNtResumeProcess = (NtResumeProcess)GetProcAddress(GetModuleHandle("ntdll"), "NtResumeProcess");

		while(success == 0 && trys_counter < 25){
			pfnNtSuspendProcess(process);
			success = ReadProcessMemory(process, curAddr,
				buffer, bufferLen, bytesRead);
			pfnNtResumeProcess(process);
			// if it still fail the only reason could be we looked it in a state where it acess itself the memory
			// so give it sometime and try again
			if( success == 0 )
				Sleep(10);
			trys_counter ++;
		}

	}


	return success;
}
static BOOL CALLBACK EnumFilterWindowsByProcIdProc(HWND hwnd,  LPARAM lParam)
{
	EnumFilterWindows *pEfw = (EnumFilterWindows *)lParam;
	DWORD oPid;

	// Get the 'other' process ID
	GetWindowThreadProcessId(hwnd, &oPid);


	// Check if they match (this window belongs to our target process)
	if( oPid == pEfw->dwPid )
	{
		pEfw->hwndList.push_back(hwnd); // Push it to our list.
	}

	return true;
}

std::string Process_lua::narrowString(std::wstring instr)
{
	std::string holder(instr.begin(), instr.end());
	return holder;
}
std::wstring Process_lua::charToWChar(std::string text)
{
	wchar_t* wide_string = new wchar_t[ text.length() + 1 ];
	// size_t size = text.length + 1;
	// std::wstring wa = new wchar_t[size];
	std::copy( text.begin(), text.end(), wide_string );
	wide_string[ text.length() ] = NULL;
	// mbstowcs(wa,text,size);
	return wide_string;
}
std::string Process_lua::readString(HANDLE handle, size_t address, int &err, size_t len)
{
	std::string fullstr;
	//unsigned char buffer = 0;
	SIZE_T bytesread;
	err = 0;
	int success;
	size_t memoryReadBufferSize =
		(size_t)Macro::instance()->getSettings()->getInt(CONFVAR_MEMORY_STRING_BUFFER_SIZE,
		CONFDEFAULT_MEMORY_STRING_BUFFER_SIZE);

	char *readBuffer = 0;
	try{ readBuffer = new char[memoryReadBufferSize]; }
	catch( std::bad_alloc &ba ) { badAllocation(); }

	memset(readBuffer, 0, memoryReadBufferSize);

	int64_t stroffset = 0;
	size_t count = 0;
	bool done = false;
	while( !done ) // read until we hit a NULL
	{
		success = ReadPMemory(handle, (LPVOID)(address + stroffset),
			(void*)readBuffer, memoryReadBufferSize, &bytesread);

		if( success == 0 || bytesread == 0 ) {
			fullstr.push_back('\0');
			err = MEMORY_READ_FAIL;
			break; }

		for(size_t i = 0; i < bytesread; i++)
		{
			stroffset++;
			count++;

			if( (len && count > len) || readBuffer[i] == '\0' )
			{
				fullstr.push_back('\0');
				done = true;
				break;
			}
			fullstr.push_back(readBuffer[i]);
		} // END FOR
	} // END WHILE

	delete []readBuffer;

	return fullstr;
}

std::wstring Process_lua::readUString(HANDLE handle, size_t address, int &err,
									  size_t len)
{
	std::wstring fullstr;
	//unsigned char buffer = 0;
	SIZE_T bytesread;
	err = 0;
	int success;

	size_t memoryReadBufferSize =
		(size_t)Macro::instance()->getSettings()->getInt(CONFVAR_MEMORY_WSTRING_BUFFER_SIZE,
		CONFDEFAULT_MEMORY_WSTRING_BUFFER_SIZE);


	wchar_t *readBuffer = 0;
	try{ readBuffer = new wchar_t[memoryReadBufferSize]; }
	catch( std::bad_alloc &ba ) { badAllocation(); }

	memset(readBuffer, 0, sizeof(wchar_t)*memoryReadBufferSize);

	int64_t stroffset = 0;
	size_t count = 0;
	bool done = false;
	while( !done ) // read until we hit a NULL
	{
		 success = ReadPMemory(handle, (LPVOID)(address + stroffset),
			(void*)readBuffer, sizeof(wchar_t)*memoryReadBufferSize, &bytesread);

		if( success == 0 || bytesread == 0 ) {
			fullstr.push_back((wchar_t)'\0');
			err = MEMORY_READ_FAIL;
			break; }

		for(size_t i = 0; i < bytesread/sizeof(wchar_t); i++)
		{
			stroffset += sizeof(wchar_t);
			count++;

			if( (len && count > len) || readBuffer[i] == 0 )
			{
				fullstr.push_back((wchar_t)'\0');
				done = true;
				break;
			}
			fullstr.push_back(readBuffer[i]);
		} // END FOR
	} // END WHILE

	delete []readBuffer;

	return fullstr;
}

void Process_lua::writeString(HANDLE process, size_t address, char *data, int &err, size_t len)
{
	SIZE_T byteswritten = 0;
	err = 0;
	int success = 0;
	DWORD old;

	VirtualProtectEx(process, (void *)address, (size_t)len, PAGE_READWRITE, &old);
	success = WriteProcessMemory(process, (void *)address,
		(void*)data, (size_t)len, &byteswritten);
	VirtualProtectEx(process, (void *)address, (size_t)len, old, &old);


	if( success == 0 ){
		int trys_counter = 0;

		NtSuspendProcess pfnNtSuspendProcess = (NtSuspendProcess)GetProcAddress(GetModuleHandle("ntdll"), "NtSuspendProcess");
		NtResumeProcess pfnNtResumeProcess = (NtResumeProcess)GetProcAddress(GetModuleHandle("ntdll"), "NtResumeProcess");

		while(success == 0 && trys_counter < 25){
			pfnNtSuspendProcess(process);
			VirtualProtectEx(process, (void *)address, (size_t)len, PAGE_READWRITE, &old);
			success = WriteProcessMemory(process, (void *)address,
				(void*)&data,(size_t)len, &byteswritten);
			VirtualProtectEx(process, (void *)address, (size_t)len, old, &old);
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
void Process_lua::writeUString(HANDLE process, size_t address, wchar_t *data, int &err, size_t len)
{
	SIZE_T byteswritten = 0;
	err = 0;
	int success = 0;
	DWORD old;

	VirtualProtectEx(process, (void *)address, (size_t)len, PAGE_READWRITE, &old);
	success = WriteProcessMemory(process, (void *)address,
		(void*)data, (size_t)len, &byteswritten);
	VirtualProtectEx(process, (void *)address, (size_t)len, old, &old);

	if( success == 0 ){
		int trys_counter = 0;

		NtSuspendProcess pfnNtSuspendProcess = (NtSuspendProcess)GetProcAddress(GetModuleHandle("ntdll"), "NtSuspendProcess");
		NtResumeProcess pfnNtResumeProcess = (NtResumeProcess)GetProcAddress(GetModuleHandle("ntdll"), "NtResumeProcess");

		while(success == 0 && trys_counter < 25){
			pfnNtSuspendProcess(process);
			VirtualProtectEx(process, (void *)address, (size_t)len, PAGE_READWRITE, &old);
			success = WriteProcessMemory(process, (void *)address,
				(void*)&data,(size_t)len, &byteswritten);
			VirtualProtectEx(process, (void *)address, (size_t)len, old, &old);
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

size_t Process_lua::readBatch_parsefmt(const char *fmt, std::vector<BatchJob> &out)
{
	size_t length = 0;
	out.clear();

	BatchJob job;

#pragma loop(hint_parallel(2))
	for(size_t i = 0; i < strlen(fmt); i++)
	{
		char c = fmt[i];

		// See if we are setting the varcount
		if( c >= '0' && c <= '9' )
		{
			// Read the number as a string, convert to int, advance
			char buffer[16];
			memset(&buffer, 0, sizeof(buffer));
			for(size_t j = 0; j < 15; j++)
			{
				char p = fmt[i + j];
				if( p < '0' || p > '9' )
				{ // End
					buffer[j] = 0;
					job.count = atoi((char*)&buffer);
					i += j - 1;
					break;
				}

				buffer[j] = p;
			}

			continue;
		}
		else // Setting a variable type
		{
			switch(c)
			{
			case 'b':
				job.type = MicroMacro::MEM_BYTE;
				length += sizeof(char) * job.count;
				out.push_back(job);
				break;
			case 'B':
				job.type = MicroMacro::MEM_UBYTE;
				length += sizeof(unsigned char) * job.count;
				out.push_back(job);
				break;
			case 's':
				job.type = MicroMacro::MEM_SHORT;
				length += sizeof(short) * job.count;
				out.push_back(job);
				break;
			case 'S':
				job.type = MicroMacro::MEM_USHORT;
				length += sizeof(unsigned short) * job.count;
				out.push_back(job);
				break;
			case 'i':
				job.type = MicroMacro::MEM_INT;
				length += sizeof(int) * job.count;
				out.push_back(job);
				break;
			case 'I':
				job.type = MicroMacro::MEM_UINT;
				length += sizeof(unsigned int) * job.count;
				out.push_back(job);
				break;
			case 'h':
				job.type = MicroMacro::MEM_INT64;
				length += sizeof(int64_t) * job.count;
				out.push_back(job);
				break;
			case 'H':
				job.type = MicroMacro::MEM_UINT64;
				length += sizeof(uint64_t) * job.count;
				out.push_back(job);
				break;
			case 'f':
				job.type = MicroMacro::MEM_FLOAT;
				length += sizeof(float) * job.count;
				out.push_back(job);
				break;
			case 'F':
				job.type = MicroMacro::MEM_DOUBLE;
				length += sizeof(double) * job.count;
				out.push_back(job);
				break;
			case 'D':
				job.type = MicroMacro::MEM_LDOUBLE;
				length += sizeof(long double) * job.count;
				out.push_back(job);
				break;
			case 'c':
				job.type = MicroMacro::MEM_STRING;
				length += sizeof(char) * job.count;
				out.push_back(job);
				break;
			case 'C':
				job.type = MicroMacro::MEM_USTRING;
				length += sizeof(char) * job.count;
				out.push_back(job);
				break;
			case '_':
				job.type =  MicroMacro::MEM_SKIP;
				length += sizeof(char) * job.count;
				out.push_back(job);
				break;
			}

			job.count = 1; // Reset count
		}
	}

	return length;
}

bool Process_lua::procDataCompare(const char *data, const char *bmask, const char *szMask)
{
	for(; *szMask; ++szMask, ++data, ++bmask)
	{
		if( *szMask == 'x' && *data!= *bmask )
			return false;
	}
	return (*szMask) == 0;
}





/* These functions actually *are* used by Lua */
int Process_lua::regmod(lua_State *L)
{
	static const luaL_Reg _funcs[] = {
		{"open", Process_lua::open},
		{"close", Process_lua::close},
		{"read", Process_lua::read},
		{"readPtr", Process_lua::readPtr},
		{"readBatch", Process_lua::readBatch},
		{"readChunk", Process_lua::readChunk},
		{"write", Process_lua::write},
		{"writePtr", Process_lua::writePtr},
		{"findPattern", Process_lua::findPattern},
		{"findByWindow", Process_lua::findByWindow},
		{"findByExe", Process_lua::findByExe},
		{"getModuleAddress", Process_lua::getModuleAddress},
		{"getModules", Process_lua::getModules},
		{"attachInput", Process_lua::attachInput},
		{"detachInput", Process_lua::detachInput},
		{"is32bit", Process_lua::is32bit},
		{"is64bit", Process_lua::is64bit},
		{"terminate", Process_lua::terminate},
		{"getWindows", Process_lua::getWindows},
		{"suspend", Process_lua::suspend},
		{"resume", Process_lua::resume},
		{NULL, NULL}
	};

	luaL_newlib(L, _funcs);
	lua_setglobal(L, PROCESS_MODULE_NAME);

	// Assign isWow64Process function for later use;
	if( !fnIsWow64Process )
		fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(TEXT("kernel32")),"IsWow64Process");

	/*if( !pfnNtSuspendProcess )
	pfnNtSuspendProcess = (NtSuspendProcess)GetProcAddress(GetModuleHandle("ntdll"), "NtSuspendProcess");

	if( !pfnNtResumeProcess )
	pfnNtResumeProcess = (NtResumeProcess)GetProcAddress(GetModuleHandle("ntdll"), "NtResumeProcess");*/

	return MicroMacro::ERR_OK;
}

int Process_lua::cleanup(lua_State *)
{
	// Detatach all processes
	DWORD thisThreadId = GetCurrentThreadId();
	for(size_t i = 0; i < attachedThreadIds.size(); i++)
	{
		AttachThreadInput(thisThreadId, attachedThreadIds.at(i), false);
	}

	attachedThreadIds.clear(); // Empty it out

	return MicroMacro::ERR_OK;
}


/*	process.open(number processId)
Returns (on success):	userdata (handle)
Returns (on failure):	nil

Attempt to open a handle to a process for reading/writing.
*/
int Process_lua::open(lua_State *L)
{
	if( lua_gettop(L) != 1 )
		wrongArgs(L);
	checkType(L, LT_NUMBER, 1);

	HANDLE handle;
	DWORD procId = (DWORD)lua_tointeger(L, 1);
	DWORD access = PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE |  PROCESS_SUSPEND_RESUME ;

	handle = OpenProcess(access, false, procId);

	if( handle == NULL )
	{ // An error occurred!
		pushLuaErrorEvent(L, "Error opening process.");
		return 0;
	}

	bool is32bit = true;
#ifdef _WIN64
	// We only make use of this when MicroMacro is compiled as 64-bit
	if( fnIsWow64Process )
	{
		BOOL iswow64;
		bool success = fnIsWow64Process(handle, &iswow64);
		if( success )
		{
			if( iswow64 || isWindows32() )
				is32bit = true;
			else
				is32bit = false;
		}
	}
#endif

	ProcHandle *pHandle = static_cast<ProcHandle *>(lua_newuserdata(L, sizeof(ProcHandle)));
	luaL_getmetatable(L, LuaType::metatable_handle);
	lua_setmetatable(L, -2);
	pHandle->handle = handle;
	pHandle->is32bit = is32bit;


	return 1;
}

/*	process.close(handle proc)
Returns:	nil

Closes an opened handle.
*/
int Process_lua::close(lua_State *L)
{
	if( lua_gettop(L) != 1 )
		wrongArgs(L);
	checkType(L, LT_USERDATA, 1);
	ProcHandle *pHandle = static_cast<ProcHandle *>(lua_touserdata(L, 1));

	CloseHandle(pHandle->handle);
	return 0;
}

/*	process.read(handle proc, string type, number address [, length])
Returns (on success):	number|string
Returns (on failure):	nil

Attempt to read memory from process 'proc' at the given address.
'type' should be "byte", "ubyte", "short", "ushort", etc.
When reading a string, a maximum bytes to read should be given as 'length'.
*/
int Process_lua::read(lua_State *L)
{
	int top = lua_gettop(L);
	if( top != 3 && top != 4 )
		wrongArgs(L);
	checkType(L, LT_USERDATA, 1);
	checkType(L, LT_STRING, 2);
	checkType(L, LT_NUMBER, 3);

	int err = 0;
	ProcHandle *pHandle = static_cast<ProcHandle *>(lua_touserdata(L, 1));
	std::string type = (char *)lua_tostring(L, 2);
	size_t address = (size_t)lua_tointeger(L, 3);
	if( pHandle->handle == 0 )
		luaL_error(L, szInvalidHandleError);

	if( type == "byte" )
	{
		char value = readMemory<char>(pHandle->handle, address, err);
		if( !err )
			lua_pushinteger(L, value);
		else
			lua_pushnil(L);
	} else if( type == "ubyte" )
	{
		unsigned char value = readMemory<unsigned char>(pHandle->handle, address, err);
		if( !err )
			lua_pushinteger(L, (lua_Unsigned)value);
		else
			lua_pushnil(L);
	} else if( type == "short" )
	{
		short value = readMemory<short>(pHandle->handle, address, err);
		if( !err )
			lua_pushinteger(L, value);
		else
			lua_pushnil(L);
	} else if( type == "ushort" )
	{
		unsigned short value = readMemory<unsigned short>(pHandle->handle, address, err);
		if( !err )
			lua_pushinteger(L, (lua_Unsigned)value);
		else
			lua_pushnil(L);
	} else if( type == "int" )
	{
		int value = readMemory<int>(pHandle->handle, address, err);
		if( !err )
			lua_pushinteger(L, value);
		else
			lua_pushnil(L);
	} else if( type == "uint" )
	{
		unsigned int value = readMemory<unsigned int>(pHandle->handle, address, err);
		if( !err )
			lua_pushinteger(L, (lua_Unsigned)value);
		else
			lua_pushnil(L);
	} else if( type == "int64" )
	{
		int64_t value = readMemory<int64_t>(pHandle->handle, address, err);
		if( !err )
			lua_pushinteger(L, value);
		else
			lua_pushnil(L);
	} else if( type == "uint64" )
	{
		uint64_t value = readMemory<uint64_t>(pHandle->handle, address, err);
		if( !err )
			lua_pushinteger(L, (lua_Unsigned)value);
		else
			lua_pushnil(L);
	} else if( type == "float" )
	{
		float value = readMemory<float>(pHandle->handle, address, err);
		if( !err )
			lua_pushnumber(L, value);
		else
			lua_pushnil(L);
	} else if( type == "double" )
	{
		double value = readMemory<double>(pHandle->handle, address, err);
		if( !err )
			lua_pushnumber(L, value);
		else
			lua_pushnil(L);
	}

	else if( type == "ldouble" )
	{
		long double value = readMemory<long double>(pHandle->handle, address, err);
		if( !err )
			lua_pushnumber(L, value);
		else
			lua_pushnil(L);
	}
	else if( type == "string" )
	{
		checkType(L, LT_NUMBER, 4);
		size_t len = (size_t)lua_tointeger(L, 4);
		std::string value = readString(pHandle->handle, address, err, len);
		if( !err )
			lua_pushstring(L, value.c_str());
		else
			lua_pushnil(L);
	}
	else if( type == "ustring" )
	{
		checkType(L, LT_NUMBER, 4);
		size_t len = (size_t)lua_tointeger(L, 4);
		std::wstring value = readUString(pHandle->handle, address, err, len);
		if( !err )
			lua_pushstring(L, narrowString(value).c_str());
		else
			lua_pushnil(L);
	} else
	{ // Not a valid type
		luaL_error(L, szInvalidDataType);
		return 0;
	}

	if( err )
	{ // Throw an error
		int errCode = GetLastError();
		pushLuaErrorEvent(L, "Failure reading memory from 0x%p at 0x%p. "\
			"Error code %i (%s).",
			pHandle->handle, address, errCode, getWindowsErrorString(errCode).c_str());
		return 0;
	}

	return 1;
}

/*	process.readPtr(handle proc, string type, number address, number|table offsets [, length])
Returns (on success):	number|string
Returns (on failure):	nil

Attempt to read memory from process 'proc' pointed to by address + offsets.
'type' should be "byte", "ubyte", "short", "ushort", etc.
When reading a string, a maximum bytes to read should be given as 'length'.

'offsets' can be a number (single offset) or a table (multiple offsets).
If a table is given for 'offsets', each value should be of type number.
*/
int Process_lua::readPtr(lua_State *L)
{
	int top = lua_gettop(L);
	if( top != 4 && top != 5 && top != 6 )
		wrongArgs(L);
	checkType(L, LT_USERDATA, 1);
	checkType(L, LT_STRING, 2);
	checkType(L, LT_NUMBER, 3);
	checkType(L, LT_NUMBER | LT_TABLE, 4);

	bool isn64 = false;
	bool using_table = false;
	int err = 0;
	std::vector<int64_t> offsets;
	std::vector<bool> readrules;;
	ProcHandle *pHandle = static_cast<ProcHandle *>(lua_touserdata(L, 1));
	std::string type = (char *)lua_tostring(L, 2);
	size_t address = (size_t)lua_tointeger(L, 3);
	if( pHandle->is32bit ){
		isn64 = true;
	}

	if( top == 5){
		checkType(L, LT_NUMBER | LT_BOOLEAN | LT_TABLE, 5);
		if( true == lua_isboolean(L, 5)){
			isn64 = lua_toboolean(L, 5);
		}
		if( true == lua_istable(L, 5)){
			using_table = true;
		}
	}

	if( top == 6){
		checkType(L, LT_BOOLEAN | LT_TABLE, 6);
		if( true == lua_isboolean(L, 6)){
			isn64 = lua_toboolean(L, 6);
		}
		if( true == lua_istable(L, 6)){
			using_table = true;
		}
	}





	if( pHandle->handle == 0 )
		luaL_error(L, szInvalidHandleError);

	if( lua_isnumber(L, 4) )
		offsets.push_back(lua_tointeger(L, 4));
	else
	{
		// Read the table, store into offsets
		lua_pushnil(L);
		while( lua_next(L, 4) )
		{
			if( lua_isnumber(L, -1) )
			{
				offsets.push_back(lua_tointeger(L, -1));
				lua_pop(L, 1); // Pop value off stack
			}
			else
			{ // Throw error (invalid type)
				lua_pop(L, 1); // Pop value off stack

				std::string key;
				if( lua_isnumber(L, -1) )
				{
					char buffer[32];
					slprintf(buffer, sizeof(buffer)-1, "%d", lua_tointeger(L, -1));
					key = buffer;
				}
				else
					key = lua_tostring(L, -1);

				char buffer[1024];
				slprintf(buffer, sizeof(buffer)-1,
					"Received invalid type (non-number) in offset list; key: %s.", key.c_str());
				luaL_error(L, buffer);
				return 0;
			}
		}
	}
	if( using_table ){
		int index = top;
		// Read the table, store into offsets
		lua_pushnil(L);
		while( lua_next(L, index) )
		{
			if( lua_isboolean(L, -1) )
			{
				readrules.push_back(lua_toboolean(L, -1));
				lua_pop(L, 1); // Pop value off stack
			}
			else
			{ // Throw error (invalid type)
				lua_pop(L, 1); // Pop value off stack

				std::string key;
				if( lua_isboolean(L, -1) )
				{
					char buffer[32];
					slprintf(buffer, sizeof(buffer)-1, "%d", lua_toboolean(L, -1));
					key = buffer;
				}
				else
					key = lua_tostring(L, -1);

				char buffer[1024];
				slprintf(buffer, sizeof(buffer)-1,
					"Received invalid type (non-boolean) in readrules list; key: %s.", key.c_str());
				luaL_error(L, buffer);
				return 0;
			}
		}
	}
	if (using_table &&  offsets.size() > 1 && readrules.size() > 1){
		if(readrules.size() != offsets.size()){
			char buffer[1024];
			slprintf(buffer, sizeof(buffer)-1,
				"Received  lists which are uneven of size from readrules and offsets");
			luaL_error(L, buffer);
			return 0;
		}
	}
	if(readrules.size() == 1){
		isn64 = readrules.at(0);
	}


	size_t realAddress;
	if( offsets.size() == 1 )
	{
#ifdef _WIN64
		if( isn64 )	// Must read as 32-bit pointer
			realAddress = readMemory<unsigned long>(pHandle->handle, address, err) + offsets.at(0);
		else					// 64-bit pointers are OK!
			realAddress = readMemory<size_t>(pHandle->handle, address, err) + offsets.at(0);
#else
		realAddress = readMemory<size_t>(pHandle->handle, address, err) + (size_t)offsets.at(0);
#endif
	}
	else
	{
		realAddress = address;
		for(size_t i = 0; i < offsets.size() - 1; i++)
		{
#ifdef _WIN64
			if( (using_table && readrules.at(i)) || (!using_table && isn64) )	// Must read as 32-bit pointer
				realAddress = readMemory<unsigned long>(pHandle->handle, realAddress + offsets.at(i), err); // Get value
			else					// 64-bit pointers are OK!
				realAddress = readMemory<size_t>(pHandle->handle, realAddress + offsets.at(i), err); // Get value
#else
			realAddress = readMemory<size_t>(pHandle->handle, realAddress + (size_t)offsets.at(i), err); // Get value
#endif

			if( err )
				break;
		}
		realAddress = realAddress + offsets.back(); // Add in the last offset
	}

	if( !err )
	{ // Read value by type.
		if( type == "byte" )
		{
			char value = readMemory<char>(pHandle->handle, realAddress, err);
			if( !err )
				lua_pushinteger(L, value);
			else
				lua_pushnil(L);
		} else if( type == "ubyte" )
		{
			unsigned char value = readMemory<unsigned char>(pHandle->handle, realAddress, err);
			if( !err )
				lua_pushinteger(L, value);
			else
				lua_pushnil(L);
		} else if( type == "short" )
		{
			short value = readMemory<short>(pHandle->handle, realAddress, err);
			if( !err )
				lua_pushinteger(L, value);
			else
				lua_pushnil(L);
		} else if( type == "ushort" )
		{
			unsigned short value = readMemory<unsigned short>(pHandle->handle, realAddress, err);
			if( !err )
				lua_pushinteger(L, (lua_Unsigned)value);
			else
				lua_pushnil(L);
		} else if( type == "int" )
		{
			int value = readMemory<int>(pHandle->handle, realAddress, err);
			if( !err )
				lua_pushinteger(L, value);
			else
				lua_pushnil(L);
		} else if( type == "uint" )
		{
			unsigned int value = readMemory<unsigned int>(pHandle->handle, realAddress, err);
			if( !err )
				lua_pushinteger(L, value);
			else
				lua_pushnil(L);
		} else if( type == "int64" )
		{
			int64_t value = readMemory<int64_t>(pHandle->handle, realAddress, err);
			if( !err )
				lua_pushinteger(L, value);
			else
				lua_pushnil(L);
		} else if( type == "uint64" )
		{
			uint64_t value = readMemory<uint64_t>(pHandle->handle, realAddress, err);
			if( !err )
				lua_pushinteger(L, (lua_Unsigned)value);
			else
				lua_pushnil(L);
		} else if( type == "float" )
		{
			float value = readMemory<float>(pHandle->handle, realAddress, err);
			if( !err )
				lua_pushnumber(L, value);
			else
				lua_pushnil(L);
		} else if( type == "double" )
		{
			double value = readMemory<double>(pHandle->handle, realAddress, err);
			if( !err )
				lua_pushnumber(L, value);
			else
				lua_pushnil(L);
		}
		else if( type == "ldouble" )
		{
			long double value = readMemory<long double>(pHandle->handle, realAddress, err);
			if( !err )
				lua_pushnumber(L, value);
			else
				lua_pushnil(L);
		}
		else if( type == "string" )
		{
			checkType(L, LT_NUMBER, 5);
			size_t len = (size_t)lua_tointeger(L, 5);
			std::string value = readString(pHandle->handle, realAddress, err, len);
			if( !err )
				lua_pushstring(L, value.c_str());
			else
				lua_pushnil(L);
		}
		else if( type == "ustring" )
		{
			checkType(L, LT_NUMBER, 4);
			size_t len = (size_t)lua_tointeger(L, 4);
			std::wstring value = readUString(pHandle->handle, realAddress, err, len);
			if( !err )
				lua_pushstring(L, narrowString(value).c_str());
			else
				lua_pushnil(L);
		} else
		{ // Not a valid type
			luaL_error(L, szInvalidDataType);
			return 0;
		}
	}

	if( err )
	{ // Throw an error
		int errCode = GetLastError();
		pushLuaErrorEvent(L, "Failure reading memory from 0x%p at 0x%p. "\
			"Error code %i (%s)",
			pHandle->handle, address, errCode, getWindowsErrorString(errCode).c_str());
		return 0;
	}

	return 1;
}

/*	process.readBatch(handle proc, number address, string mask)
Returns (on success):	table
Returns (on failure):	nil

Attempt to read memory from process 'proc' at the given address.
'mask' dictates what type(s) and how many variables should be read.
Each character in 'mask' specifies the type to read or skip. A number
prefixing the type can dictate the number to read (number types) or
the length of a string.

Character		Type
b				byte
B				unsigned byte
s				short
S				unsigned short
i				int
I				unsigned int
h               int64(int64_t)
H               unsigend int64 (uint64_t)
f				float
F				double
D               long double(ldouble)
c				string
C				unicode string
_				(skip ahead; do not return this)
*/
int Process_lua::readBatch(lua_State *L)
{
	if( lua_gettop(L) != 3 )
		wrongArgs(L);
	checkType(L, LT_USERDATA, 1);
	checkType(L, LT_NUMBER, 2);
	checkType(L, LT_STRING, 3);

	ProcHandle *pHandle = static_cast<ProcHandle *>(lua_touserdata(L, 1));
	size_t address = (size_t)lua_tointeger(L, 2);
	const char *fmt = lua_tostring(L, 3);

	std::vector<BatchJob> jobs;
	size_t readLen = readBatch_parsefmt(fmt, jobs);

	char *readBuffer = 0;
	try {
		readBuffer = new char[readLen+1];
	} catch( std::bad_alloc &ba ) { badAllocation(); }

	SIZE_T bytesRead = 0;
	int success = ReadPMemory(pHandle->handle, (LPVOID)address, (void *)readBuffer, readLen, &bytesRead);

	if( !success || bytesRead != readLen )
	{ // Throw error
		delete []readBuffer;
		int errCode = GetLastError();
		pushLuaErrorEvent(L, "Failure reading memory from 0x%p at 0x%p. "\
			"Error code %i (%s)",
			pHandle->handle, address, errCode, getWindowsErrorString(errCode).c_str());
		return 0;
	}

	size_t cursorPos = 0;
	size_t tableIndex = 1;

	lua_newtable(L);
	for(size_t i = 0; i < jobs.size(); i++)
	{
		if( jobs.at(i).type ==  MicroMacro::MEM_SKIP )
		{ // We skip over it. Duh.
			cursorPos += sizeof(char) * jobs.at(i).count;
			continue;
		}

		switch(jobs.at(i).type)
		{
		case MicroMacro::MEM_BYTE:
			for(size_t j = 0; j < jobs.at(i).count; j++)
			{
				char data = readBuffer[cursorPos];
				cursorPos += sizeof(char);
				lua_pushinteger(L, tableIndex); // Push key
				lua_pushinteger(L, data); // Push value
				lua_settable(L, -3); // Set it
				++tableIndex;
			}
			break;
		case MicroMacro::MEM_UBYTE:
			for(size_t j = 0; j < jobs.at(i).count; j++)
			{
				unsigned char data = readBuffer[cursorPos];
				cursorPos += sizeof(unsigned char);
				lua_pushinteger(L, tableIndex); // Push key
				lua_pushinteger(L, (lua_Unsigned)data); // Push value
				lua_settable(L, -3); // Set it
				++tableIndex;
			}
			break;
		case MicroMacro::MEM_SHORT:
			for(size_t j = 0; j < jobs.at(i).count; j++)
			{
				short data = *(short*)&readBuffer[cursorPos];
				cursorPos += sizeof(short);
				lua_pushinteger(L, tableIndex); // Push key
				lua_pushinteger(L, data); // Push value
				lua_settable(L, -3); // Set it
				++tableIndex;
			}
			break;
		case MicroMacro::MEM_USHORT:
			for(size_t j = 0; j < jobs.at(i).count; j++)
			{
				unsigned short data = *(unsigned short*)&readBuffer[cursorPos];
				cursorPos += sizeof(unsigned short);
				lua_pushinteger(L, tableIndex); // Push key
				lua_pushinteger(L, (lua_Unsigned)data); // Push value
				lua_settable(L, -3); // Set it
				++tableIndex;
			}
			break;
		case MicroMacro::MEM_INT:
			for(size_t j = 0; j < jobs.at(i).count; j++)
			{
				int data = *(int*)&readBuffer[cursorPos];
				cursorPos += sizeof(int);
				lua_pushinteger(L, tableIndex); // Push key
				lua_pushinteger(L, (lua_Unsigned)data); // Push value
				lua_settable(L, -3); // Set it
				++tableIndex;
			}
			break;
		case MicroMacro::MEM_UINT:
			for(size_t j = 0; j < jobs.at(i).count; j++)
			{
				unsigned int data = *(unsigned int*)&readBuffer[cursorPos];
				cursorPos += sizeof(unsigned int);
				lua_pushinteger(L, tableIndex); // Push key
				lua_pushinteger(L, (lua_Unsigned)data); // Push value
				lua_settable(L, -3); // Set it
				++tableIndex;
			}
			break;
		case MicroMacro::MEM_INT64:
			for(size_t j = 0; j < jobs.at(i).count; j++)
			{
				int64_t data = *(int64_t*)&readBuffer[cursorPos];
				cursorPos += sizeof(int64_t);
				lua_pushinteger(L, tableIndex); // Push key
				lua_pushinteger(L, data); // Push value
				lua_settable(L, -3); // Set it
				++tableIndex;
			}
			break;
		case MicroMacro::MEM_UINT64:
			for(size_t j = 0; j < jobs.at(i).count; j++)
			{
				uint64_t data = *(uint64_t*)&readBuffer[cursorPos];
				cursorPos += sizeof(uint64_t);
				lua_pushinteger(L, tableIndex); // Push key
				lua_pushinteger(L, (lua_Unsigned) data); // Push value
				lua_settable(L, -3); // Set it
				++tableIndex;
			}
			break;
		case MicroMacro::MEM_FLOAT:
			for(size_t j = 0; j < jobs.at(i).count; j++)
			{
				float data = *(float*)&readBuffer[cursorPos];
				cursorPos += sizeof(float);
				//printf("Do float... %f\n", data);
				lua_pushinteger(L, tableIndex); // Push key
				lua_pushnumber(L, data); // Push value
				lua_settable(L, -3); // Set it
				++tableIndex;
			}
			break;
		case MicroMacro::MEM_DOUBLE:
			for(size_t j = 0; j < jobs.at(i).count; j++)
			{
				double data = *(double*)&readBuffer[cursorPos];
				cursorPos += sizeof(double);
				lua_pushinteger(L, tableIndex); // Push key
				lua_pushnumber(L, data); // Push value
				lua_settable(L, -3); // Set it
				++tableIndex;
			}
			break;
		case MicroMacro::MEM_LDOUBLE:
			for(size_t j = 0; j < jobs.at(i).count; j++)
			{
				long double data = *(long double*)&readBuffer[cursorPos];
				cursorPos += sizeof(long double);
				lua_pushinteger(L, tableIndex); // Push key
				lua_pushnumber(L, data); // Push value
				lua_settable(L, -3); // Set it
				++tableIndex;
			}
			break;
		case MicroMacro::MEM_STRING:
			{
				size_t len = jobs.at(i).count;
				char *buffer = 0;
				try {
					buffer = new char[len+2]; // Make sure we have room for terminator
				} catch( std::bad_alloc &ba ) { badAllocation(); }

				// Copy this string segment into our buffer, push it, delete it
				strlcpy(buffer, (char*)&readBuffer[cursorPos], len);
				cursorPos += len;
				lua_pushinteger(L, tableIndex); // Push key
				lua_pushstring(L, buffer);
				lua_settable(L, -3); // Set it
				++tableIndex;
				delete []buffer;
			} break;
		case MicroMacro::MEM_USTRING:
			{
				size_t len = jobs.at(i).count;
				wchar_t *buffer = 0;
				try {
					buffer = new wchar_t[len+2]; // Make sure we have room for terminator
				} catch( std::bad_alloc &ba ) { badAllocation(); }

				// Copy this string segment into our buffer, push it, delete it
				wstrlcpy(buffer, (wchar_t*)&readBuffer[cursorPos], len);
				cursorPos += len;
				lua_pushinteger(L, tableIndex); // Push key
				lua_pushstring(L,  narrowString(buffer).c_str());
				lua_settable(L, -3); // Set it
				++tableIndex;
				delete []buffer;
			} break;
		default:
			/* Hmmm... this shouldn't have happened.
			Oh well. Advance? */
			cursorPos += sizeof(char) * jobs.at(i).count;
			break;
		}
	}

	return 1;
}

/*	process.readChunk(handle proc, number address, number size)
Returns:	chunk (class)

Read a chunk of memory.

Returns a chunk on success, nil on failure.
*/
int Process_lua::readChunk(lua_State *L)
{
	if( lua_gettop(L) != 3 )
		wrongArgs(L);
	checkType(L, LT_USERDATA, 1);
	checkType(L, LT_NUMBER, 2);
	checkType(L, LT_NUMBER, 3);

	ProcHandle *pHandle = static_cast<ProcHandle *>(lua_touserdata(L, 1));
	size_t address = (size_t)lua_tointeger(L, 2);
	size_t size = (size_t)lua_tointeger(L, 3);

	MemoryChunk *pChunk = static_cast<MemoryChunk *>(lua_newuserdata(L, sizeof(MemoryChunk)));
	try {
		pChunk->data = new char[size+1];
	} catch( std::bad_alloc &ba ) { badAllocation(); }
	pChunk->address = address;
	pChunk->size = size;

	SIZE_T bytesRead;
	int success = ReadPMemory(pHandle->handle, (LPVOID)address, (void *)pChunk->data, size, &bytesRead);

	if( !success || bytesRead != size )
	{ // Error
		lua_pop(L, 1); // Pop that memory chunk... looks like we don't need it!
		int errCode = GetLastError();
		pushLuaErrorEvent(L, "Failure read memory from 0x%p at 0x%p. "\
			"Error code %i (%s)",
			pHandle->handle, address, errCode, getWindowsErrorString(errCode).c_str());
		return 0;
	}

	// Set all the things
	luaL_getmetatable(L, LuaType::metatable_memorychunk);
	lua_setmetatable(L, -2);

	return 1;
}

/*	process.write(handle proc, string type, number address, string|number data)
Returns:	boolean

Attempt to write memory to process 'proc' at the given address.
'type' does not need to indicate signedness. (do not includes 'u' prefix)
Strings also do not require length to be given.

Returns true on success, false on failure.
*/
int Process_lua::write(lua_State *L)
{
	if( lua_gettop(L) != 3 && lua_gettop(L) != 4 )
		wrongArgs(L);
	checkType(L, LT_USERDATA, 1);
	checkType(L, LT_STRING, 2);
	checkType(L, LT_NUMBER, 3);

	int err = 0;
	ProcHandle *pHandle = (ProcHandle *)lua_touserdata(L, 1);
	std::string type = (char *)lua_tostring(L, 2);
	size_t address = (size_t)lua_tointeger(L, 3);

	if( pHandle->handle == 0 )
		luaL_error(L, szInvalidHandleError);

	if( type == "byte" )
	{
		checkType(L, LT_NUMBER, 4);
		char data = (char)lua_tointeger(L, 4);
		writeMemory<char>(pHandle->handle, address, data, err);
	} else if( type == "short" )
	{
		checkType(L, LT_NUMBER, 4);
		short data = (short)lua_tointeger(L, 4);
		writeMemory<short>(pHandle->handle, address, data, err);
	} else if( type == "int" )
	{
		checkType(L, LT_NUMBER, 4);
		int data = (int)lua_tointeger(L, 4);
		writeMemory<int>(pHandle->handle, address, data, err);
	} else if( type == "int64" )
	{
		checkType(L, LT_NUMBER, 4);
		int64_t data = lua_tointeger(L, 4);
		writeMemory<int64_t>(pHandle->handle, address, data, err);

	} else if( type == "uint64" )
	{
		checkType(L, LT_NUMBER, 4);
		uint64_t data = (uint64_t)lua_tointeger(L, 4);
		writeMemory<uint64_t>(pHandle->handle, address, data, err);

	} else if( type == "float" )
	{
		checkType(L, LT_NUMBER, 4);
		float data = (float)lua_tonumber(L, 4);
		writeMemory<float>(pHandle->handle, address, data, err);
	} else if( type == "double" )
	{
		checkType(L, LT_NUMBER, 4);
		double data = (double)lua_tonumber(L, 4);
		writeMemory<double>(pHandle->handle, address, data, err);
	} else if( type == "ldouble" )
	{
		checkType(L, LT_NUMBER, 4);
		long double data = (long double)lua_tonumber(L, 4);
		writeMemory<long double>(pHandle->handle, address, data, err);
	} else if( type == "string" )
	{
		checkType(L, LT_STRING, 4);
		size_t maxlen = 0;
		char *data = (char *)lua_tolstring(L, 4, &maxlen);
		writeString(pHandle->handle, address, data, err, (size_t)maxlen);
	} else if( type == "ustring" )
	{
		checkType(L, LT_STRING, 4);
		size_t maxlen = 0;
		char *data = (char *)lua_tolstring(L, 4, &maxlen);
		writeUString(pHandle->handle, address, const_cast<wchar_t*>(charToWChar(data).c_str()), err, (size_t)maxlen);
	} else
	{ // Not a valid type
		luaL_error(L, szInvalidDataType);
		return 0;
	}

	if( err )
	{ // Throw an error
		int errCode = GetLastError();
		pushLuaErrorEvent(L, "Failure writing memory to 0x%p at 0x%p. "\
			"Error code %i (%s)",
			pHandle->handle, address, errCode, getWindowsErrorString(errCode).c_str());
	}

	lua_pushboolean(L, err == 0);
	return 1;
}

/*	process.writePtr(handle proc, string type, number address, number|table offsets, number|string data)
Returns:	boolean

Attempt to write memory to process 'proc' at the given address + offsets.

'offsets' can be a number (single offset) or a table (multiple offsets).
If a table is given for 'offsets', each value should be of type number.

See process.write() for more details.

Returns true on success, false on failure.
*/
int Process_lua::writePtr(lua_State *L)
{
	int top = lua_gettop(L);
	if( top != 4 && top != 5  && top != 6 )
		wrongArgs(L);
	checkType(L, LT_USERDATA, 1);
	checkType(L, LT_STRING, 2);
	checkType(L, LT_NUMBER, 3);
	checkType(L, LT_NUMBER | LT_TABLE, 4);
	bool isn64 = true;
	bool using_table = false;
	int err = 0;
	std::vector<int64_t> offsets;
	std::vector<bool> readrules;
	ProcHandle *pHandle = static_cast<ProcHandle *>(lua_touserdata(L, 1));
	std::string type = (char *)lua_tostring(L, 2);
	size_t address = (size_t)lua_tointeger(L, 3);
	if( pHandle->is32bit ){
		isn64 = false;
	}

	if( top == 5){
		checkType(L, LT_NUMBER | LT_BOOLEAN | LT_TABLE, 5);
		if( true == lua_isboolean(L, 5)){
			isn64 =lua_toboolean(L, 5);
		}
		if( true == lua_istable(L, 5)){
			using_table = true;
		}
	}

	if( top == 6){
		checkType(L, LT_BOOLEAN | LT_TABLE, 6);
		if( true == lua_isboolean(L, 6)){
			isn64 =lua_toboolean(L, 6);
		}
		if( true == lua_istable(L, 6)){
			using_table = true;
		}
	}


	if( pHandle->handle == 0 )
		luaL_error(L, szInvalidHandleError);

	if( lua_isnumber(L, 4) )
		offsets.push_back(lua_tointeger(L, 4));
	else
	{
		// Read the table, store into offsets
		lua_pushnil(L);
		while( lua_next(L, 4) )
		{
			if( lua_isnumber(L, -1) )
			{
				offsets.push_back(lua_tointeger(L, -1));
				lua_pop(L, 1);
			}
			else
			{ // Throw error (invalid type)
				lua_pop(L, 1); // Pop value off stack

				std::string key;
				if( lua_isnumber(L, -1) )
				{
					char buffer[32];
					slprintf(buffer, sizeof(buffer)-1, "%d", lua_tointeger(L, -1));
					key = buffer;
				}
				else
					key = lua_tostring(L, -1);

				char buffer[1024];
				slprintf(buffer, sizeof(buffer)-1,
					"Received invalid type (non-number) in offset list; key: %s.", key.c_str());
				luaL_error(L, buffer);
				return 0;
			}
		}
	}
	if( using_table ){
		int index = top;
		// Read the table, store into offsets
		lua_pushnil(L);
		while( lua_next(L, index) )
		{
			if( lua_isboolean(L, -1) )
			{
				readrules.push_back(lua_toboolean(L, -1));
				lua_pop(L, 1); // Pop value off stack
			}
			else
			{ // Throw error (invalid type)
				lua_pop(L, 1); // Pop value off stack

				std::string key;
				if( lua_isboolean(L, -1) )
				{
					char buffer[32];
					slprintf(buffer, sizeof(buffer)-1, "%d", lua_toboolean(L, -1));
					key = buffer;
				}
				else
					key = lua_tostring(L, -1);

				char buffer[1024];
				slprintf(buffer, sizeof(buffer)-1,
					"Received invalid type (non-boolean) in readrules list; key: %s.", key.c_str());
				luaL_error(L, buffer);
				return 0;
			}
		}
	}
	if (using_table &&  offsets.size() > 1  && readrules.size() > 1){
		if(readrules.size() != offsets.size()){
			char buffer[1024];
			slprintf(buffer, sizeof(buffer)-1,
				"Received  lists which are uneven of size from readrules and offsets");
			luaL_error(L, buffer);
			return 0;
		}
	}
	if(readrules.size() == 1){
		isn64 = readrules.at(0);
	}

	size_t realAddress;
	if( offsets.size() == 1 )
#ifdef _WIN64
		if( isn64 )
			realAddress = readMemory<unsigned long>(pHandle->handle, address, err) + offsets.at(0);
		else
			realAddress = readMemory<size_t>(pHandle->handle, address, err) + offsets.at(0);
#else
		realAddress = readMemory<size_t>(pHandle->handle, address, err) + offsets.at(0);
#endif
	else
	{
		realAddress = address;
		for(size_t i = 0; i < offsets.size() - 1; i++)
		{
#ifdef _WIN64
			if( (using_table && readrules.at(i)) || (!using_table && isn64) )
				realAddress = readMemory<unsigned long>(pHandle->handle, realAddress + offsets.at(i), err); // Get value
			else
				realAddress = readMemory<size_t>(pHandle->handle, realAddress + offsets.at(i), err); // Get value
#else
			realAddress = readMemory<size_t>(pHandle->handle, realAddress + (size_t)offsets.at(i), err); // Get value
#endif

			if( err )
				break;
		}
		realAddress = realAddress + offsets.back(); // Add in the last offset
	}

	if( !err )
	{ // Write value by type.
		if( type == "byte" )
		{
			checkType(L, LT_NUMBER, 5);
			char data = (char)lua_tointeger(L, 5);
			writeMemory<char>(pHandle->handle, realAddress, data, err);
		}else if( type == "ubyte" )
		{
			checkType(L, LT_NUMBER, 5);
			unsigned char data = (unsigned char)lua_tointeger(L, 5);
			writeMemory<unsigned char>(pHandle->handle, realAddress, data, err);
		}else if( type == "short" )
		{
			checkType(L, LT_NUMBER, 5);
			short data = (short)lua_tointeger(L, 5);
			writeMemory<short>(pHandle->handle, realAddress, data, err);
		}else if( type == "ushort" )
		{
			checkType(L, LT_NUMBER, 5);
			unsigned short data = (unsigned short)lua_tointeger(L, 5);
			writeMemory<unsigned short>(pHandle->handle, realAddress, data, err);
		}else if( type == "int" )
		{
			checkType(L, LT_NUMBER, 5);
			int data = (int)lua_tointeger(L, 5);
			writeMemory<int>(pHandle->handle, realAddress, data, err);
		}else if( type == "uint" )
		{
			checkType(L, LT_NUMBER, 5);
			unsigned int data = (unsigned int)lua_tointeger(L, 5);
			writeMemory<unsigned int>(pHandle->handle, realAddress, data, err);
		}else if( type == "int64" )
		{
			checkType(L, LT_NUMBER, 5);
			int64_t data = lua_tointeger(L, 5);
			writeMemory<int64_t>(pHandle->handle, realAddress, data, err); 
		}else if( type == "uint64" )
		{
			checkType(L, LT_NUMBER, 5);
			uint64_t data = (uint64_t)lua_tointeger(L, 5);
			writeMemory<uint64_t>(pHandle->handle, realAddress, data, err);
		}else if( type == "float" )
		{
			checkType(L, LT_NUMBER, 5);
			float data = (float)lua_tonumber(L, 5);
			writeMemory<float>(pHandle->handle, realAddress, data, err);
		}else if( type == "double" )
		{
			checkType(L, LT_NUMBER, 5);
			double data = (double)lua_tonumber(L, 5);
			writeMemory<double>(pHandle->handle, realAddress, data, err);
		}else if( type == "ldouble" )
		{
			checkType(L, LT_NUMBER, 5);
			long double data = (long double)lua_tonumber(L, 5);
			writeMemory<long double>(pHandle->handle, realAddress, data, err);
		}else if( type == "string" )
		{
			checkType(L, LT_STRING, 5);
			size_t dataLen;
			char *data = (char *)lua_tolstring(L, 5, &dataLen);
			writeString(pHandle->handle, realAddress, data, err, (size_t)dataLen);
		}else if( type == "ustring" )
		{
			checkType(L, LT_STRING, 5);
			size_t dataLen;
			char *data = (char *)lua_tolstring(L, 5, &dataLen);
			writeUString(pHandle->handle, address, const_cast<wchar_t*>(charToWChar(data).c_str()), err, (size_t)dataLen);
		} else
		{ // Not a valid type
			luaL_error(L, szInvalidDataType);
			return 0;
		}
	}

	if( err )
	{ // Throw an error
		int errCode = GetLastError();
		pushLuaErrorEvent(L, "Failure writing memory to 0x%p at 0x%p. "\
			"Error code %i (%s)",
			pHandle->handle, address, errCode, getWindowsErrorString(errCode).c_str());
	}

	lua_pushboolean(L, err == 0);
	return 1;
}

/*	process.findPattern(handle proc, number address, number length, string bitmask, string szmask)
Returns (on success):	number address
Returns (on failure):	nil

Attempt to find a pattern within a process, beginning at memory address 'address',
with a max scan length of 'length'.
'bitmask' should contain an 'x' for a match, and '?' for wildcard.
'szmask' should contain the actual data we are checking against for a match.
*/
int Process_lua::findPattern(lua_State *L)
{
	if( lua_gettop(L) != 5 )
		wrongArgs(L);
	checkType(L, LT_USERDATA, 1);
	checkType(L, LT_NUMBER, 2);
	checkType(L, LT_NUMBER, 3);
	checkType(L, LT_STRING, 4);
	checkType(L, LT_STRING, 5);

	// Get data, create buffers, etc.
	size_t bmaskLen;
	size_t szMaskLen;
	ProcHandle *pHandle = static_cast<ProcHandle *>(lua_touserdata(L, 1));
	size_t address = (size_t)lua_tointeger(L, 2);
	size_t scanLen = (size_t)lua_tointeger(L, 3);
	const char *bmask = lua_tolstring(L, 4, &bmaskLen);
	const char *szMask = lua_tolstring(L, 5, &szMaskLen);

	bool found = false;
	size_t foundAddr = 0;
	size_t bufferLen = szMaskLen * 50;
	if( bufferLen < 1024 ) // Minimum of 1kb
		bufferLen = 1024;
	size_t addLen = bufferLen % szMaskLen;
	if( addLen > 0 )
		bufferLen = bufferLen + addLen;
	size_t bufferStart = 0;
	size_t curBufferLen = bufferLen;
	unsigned char *buffer = 0;
	try {
		buffer = new unsigned char[bufferLen + 1];
	} catch( std::bad_alloc &ba ) { badAllocation(); }

	// Iterate through up until we reach the length or find something
	for(size_t i = 0; i < scanLen; i++)
	{
		// Read a chunk (if needed)
		size_t curAddr = address + i;
		if( (curAddr - bufferStart + szMaskLen) >= curBufferLen )
		{
			memset(buffer, 0, bufferLen);
			SIZE_T bytesRead;
			bool success = ReadPMemory(pHandle->handle, (LPCVOID)curAddr, buffer, bufferLen, &bytesRead);

			bufferStart = curAddr;
			if( !success && bytesRead == 0 )
				continue;

			if( !success && bytesRead < bufferLen )
			{ // An error might have occurred, or we just couldn't complete the read. Ignore it.
				debugMessage("findPattern() did not read full length. Got %d bytes, expected %d.",
					(int)bytesRead, bufferLen);
			}

			curBufferLen = bytesRead;
		}

		// Check for matches
		if( procDataCompare((const char *)&buffer[curAddr - bufferStart], bmask, szMask) )
		{
			found = true;
			foundAddr = address + i;
			break;
		}
	}

	delete []buffer;

	if( !found ) // If we didn't find anything, don't return anything
		return 0;

	lua_pushinteger(L, (lua_Unsigned)foundAddr);
	return 1;
}

/*	process.findByWindow(number hwnd)
Returns (on success):	number procId
Returns (on failure):	nil

Look up the process ID that owns the given window, return it.
*/
int Process_lua::findByWindow(lua_State *L)
{
	if( lua_gettop(L) != 1 )
		wrongArgs(L);
	checkType(L, LT_NUMBER, 1);

	HWND hwnd = (HWND)lua_tointeger(L, 1);
	DWORD procId;
	GetWindowThreadProcessId(hwnd, &procId);

	if( procId == 0 ) // Nothing found
	{ // Throw warning
		int errCode = GetLastError();
		pushLuaErrorEvent(L, "Failure to find process ID by window. Error code %i (%s)",
			hwnd, errCode, getWindowsErrorString(errCode).c_str());
		return 0;
	}

	lua_pushinteger(L, (lua_Unsigned)procId);
	return 1;
}

/*	process.findByExe(string exeName)
Returns (on success):	number procId
Returns (on failure):	nil

Look up a process ID by checking for its running executable.
*/
int Process_lua::findByExe(lua_State *L)
{
	if( lua_gettop(L) != 1 )
		wrongArgs(L);
	checkType(L, LT_STRING, 1);

	size_t nameLen = 0;
	const char *name = lua_tolstring(L, 1, &nameLen);

	DWORD processes[8192];
	DWORD bytesReturned;
	int success = EnumProcesses(processes, sizeof(processes), &bytesReturned);
	if( !success ) {
		// Throw error
		int errCode = GetLastError();
		pushLuaErrorEvent(L, "Failure to enumerate processes. Error code %i (%s)",
			errCode, getWindowsErrorString(errCode).c_str());
		return 0;
	}

	DWORD foundProcId = 0;
	DWORD proccount = bytesReturned / sizeof(DWORD);
	for(size_t i = 0; i < proccount; i++)
	{
		if( processes[i] == 0 ) // Skip invalid entries
			continue;

		TCHAR szProcName[MAX_PATH] = TEXT("");
		DWORD access = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;
		HANDLE handle = OpenProcess(access, false, processes[i]);

		if( handle == NULL )
		{
			int errCode = GetLastError();
			if( errCode != ERROR_ACCESS_DENIED ) // Skip over processes we cannot access
			{

			}
		}

		// Check process's module names
		HMODULE hModule;
		DWORD bytesReturned2;
		success = EnumProcessModules(handle, &hModule, sizeof(HMODULE), &bytesReturned2);
		if( success )
			GetModuleBaseName(handle, hModule, szProcName, sizeof(szProcName)/sizeof(TCHAR));

		// Close handle now, we're done with this one
		CloseHandle(handle);

		// Now try to match it... but first, convert to lowercase
		char *name_lower = 0;
		char *found_lower = 0;
		size_t foundLen = strlen(szProcName);
		try {
			name_lower = new char[nameLen+1];
			found_lower = new char[foundLen+1];
		} catch( std::bad_alloc &ba ) { badAllocation(); }

		sztolower(name_lower, name, nameLen);
		sztolower(found_lower, szProcName, foundLen);
		bool found = wildfind(name_lower, found_lower);

		// Free memory
		delete []name_lower;
		delete []found_lower;

		if( found )
		{ // We have a match
			foundProcId = processes[i];
			break;
		}
	}

	if( foundProcId == 0 )
		return 0;

	lua_pushinteger(L, (lua_Unsigned)foundProcId);
	return 1;
}

/*	process.getModuleAddress(number procId, string moduleName)
Returns (on success):	number address
Returns (on failure):	nil

Look up the address of a module within a process and return
its origin address.
*/
int Process_lua::getModuleAddress(lua_State *L)
{
	if( lua_gettop(L) != 2 )
		wrongArgs(L);
	checkType(L, LT_NUMBER, 1);
	checkType(L, LT_STRING, 2);

	size_t modnameLen;
	DWORD procId = (DWORD)lua_tointeger(L, 1);
	const char *modname = lua_tolstring(L, 2, &modnameLen);

	char *modname_lower = 0;
	try {
		modname_lower = new char[modnameLen+1];
	} catch( std::bad_alloc &ba ) { badAllocation(); }
	sztolower(modname_lower, modname, modnameLen);

#ifdef _WIN64
	DWORD cthFlags = TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32;
#else
	DWORD cthFlags = TH32CS_SNAPMODULE;
#endif

	HANDLE snapshot = CreateToolhelp32Snapshot(cthFlags, procId);
	if( snapshot == INVALID_HANDLE_VALUE )
	{ // Throw error
		int errCode = GetLastError();
		pushLuaErrorEvent(L, "Failure to find window. Error code %i (%s)",
			errCode, getWindowsErrorString(errCode).c_str());
	}

	bool found = false;
	size_t addrFound = 0;
	MODULEENTRY32 mod;
	mod.dwSize = sizeof(MODULEENTRY32);
	if( Module32First(snapshot, &mod) )
	{
		char modname_snap[256];
		sztolower(modname_snap, mod.szModule, sizeof(modname_snap)-1);

		if( strcmp(modname_snap, modname_lower) == 0 )
		{
			found = true;
			addrFound = (size_t)mod.modBaseAddr;
		}

		while( !found && Module32Next(snapshot, &mod) )
		{
			sztolower(modname_snap, mod.szModule, sizeof(modname_snap)-1);

			if( strcmp(modname_snap, modname_lower) == 0 )
			{
				found = true;
				addrFound = (size_t)mod.modBaseAddr;
				break;
			}
		}
	}

	if( !found )
		return 0;
	lua_pushinteger(L, (lua_Unsigned)addrFound);
	return 1;
}

/*	process.getModules(number procId)
Returns (on success):	table(name => address pairs)
Returns (on failure):	nil

Returns a list of all available modules in a process as key/value pairs.
*/
int Process_lua::getModules(lua_State *L)
{
	if( lua_gettop(L) != 1 )
		wrongArgs(L);
	checkType(L, LT_NUMBER, 1);

	DWORD procId = (DWORD)lua_tointeger(L, 1);

#ifdef _WIN64
	DWORD cthFlags = TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32;
#else
	DWORD cthFlags = TH32CS_SNAPMODULE;
#endif

	HANDLE snapshot = CreateToolhelp32Snapshot(cthFlags, procId);
	if( snapshot == INVALID_HANDLE_VALUE )
	{ // Throw error
		int errCode = GetLastError();
		pushLuaErrorEvent(L, "Failure to find window. Error code %i (%s)",
			errCode, getWindowsErrorString(errCode).c_str());
	}

	bool found = false;
	MODULEENTRY32 mod;
	mod.dwSize = sizeof(MODULEENTRY32);
	if( Module32First(snapshot, &mod) )
	{
		found = true;

		lua_newtable(L);
		int newtab_index = lua_gettop(L);

		lua_pushstring(L, mod.szModule);					// Key
		lua_pushinteger(L, (size_t)mod.modBaseAddr);		// Value
		lua_settable(L, newtab_index);						// Set

		while( Module32Next(snapshot, &mod) )
		{
			lua_pushstring(L, mod.szModule);				// Key
			lua_pushinteger(L, (size_t)mod.modBaseAddr);	// Value
			lua_settable(L, newtab_index);					// Set
		}
	}

	return found;
}


/*	process.attachInput(number hwnd)
Returns:	boolean

Attach our input thread to the target window.
Returns true on success, false on failure.
*/
int Process_lua::attachInput(lua_State *L)
{
	if( lua_gettop(L) != 1 )
		wrongArgs(L);
	DWORD threadId;
	HWND hwnd = (HWND)lua_tointeger(L, 1);

	threadId = GetWindowThreadProcessId(hwnd, NULL);
	bool success = AttachThreadInput(GetCurrentThreadId(), threadId, true);

	if( success ) // Add it to our list
	{
		attachedThreadIds.push_back(threadId);
	}
	else
	{ // Throw error
		int errCode = GetLastError();
		pushLuaErrorEvent(L, "Failure attaching input thread to process. "\
			"Error code %i (%s)", errCode, getWindowsErrorString(errCode).c_str());
	}

	lua_pushboolean(L, success);
	return 1;
}

/*	process.detachInput(number hwnd)
Returns:	boolean

Detach our input thread from the target window.
Returns true on success, false on failure.
*/
int Process_lua::detachInput(lua_State *L)
{
	if( lua_gettop(L) != 1 )
		wrongArgs(L);
	DWORD threadId;
	HWND hwnd = (HWND)lua_tointeger(L, 1);

	threadId = GetWindowThreadProcessId(hwnd, NULL);

	bool success = AttachThreadInput(GetCurrentThreadId(), threadId, false);

	if( success ) // Remove it from the list
	{
		for(size_t i = 0; i < attachedThreadIds.size(); i++)
		{
			if( threadId == attachedThreadIds.at(i) )
			{
				attachedThreadIds.erase(attachedThreadIds.begin() + i);
				break;
			}
		}
	}
	else
	{ // Throw error
		int errCode = GetLastError();
		pushLuaErrorEvent(L, "Failure attaching input thread to process. "\
			"Error code %i (%s)", errCode, getWindowsErrorString(errCode).c_str());
	}

	lua_pushboolean(L, success);
	return 1;
}

/*	process.is32bit(handle proc)
Returns:	boolean

Determines if the target process is 32 bit.
*/
int Process_lua::is32bit(lua_State *L)
{
	if( lua_gettop(L) != 1 )
		wrongArgs(L);

	checkType(L, LT_USERDATA, 1);
	ProcHandle *pHandle = static_cast<ProcHandle *>(lua_touserdata(L, 1));

	BOOL iswow64 = false;
	bool success;

	if( fnIsWow64Process )
		success = fnIsWow64Process(pHandle->handle, &iswow64);
	else
	{	// Assume 32-bit
		lua_pushboolean(L, true);
		return 1;
	}

	if( !success )
		Logger::instance()->add("Failed to call IsWow64Process() in %s. Error code: %d\n",
		__FUNCTION__, GetLastError());

	if( iswow64 )
	{	// 32 bit process, running under 64-bit Windows
		lua_pushboolean(L, true);
	}
	else
	{
		lua_pushboolean(L, isWindows32());
	}
	return 1;
}

/*	process.is64bit(handle proc)
Returns:	boolean

Determines if the target process is 64 bit.
*/
int Process_lua::is64bit(lua_State *L)
{
	if( lua_gettop(L) != 1 )
		wrongArgs(L);

	checkType(L, LT_USERDATA, 1);
	ProcHandle *pHandle = static_cast<ProcHandle *>(lua_touserdata(L, 1));


	BOOL iswow64 = false;
	bool success;

	if( fnIsWow64Process )
		success = fnIsWow64Process(pHandle->handle, &iswow64);
	else
	{
		lua_pushboolean(L, false);	// Assume 32-bit
		return 1;
	}

	if( !success )
		Logger::instance()->add("Failed to call IsWow64Process() in %s. Error code: %d\n",
		__FUNCTION__, GetLastError());

	if( iswow64 )
	{	// 32 bit process, running under 64-bit Windows
		lua_pushboolean(L, false);
	}
	else
	{
		lua_pushboolean(L, isWindows64());
	}
	return 1;
}

/*	process.terminate(handle proc[, number exitCode])
Returns:	boolean

Terminates a process with the given exit code.
Returns whether or not the call was successful
*/
int Process_lua::terminate(lua_State *L)
{
	int top = lua_gettop(L);
	if( top != 1 && top != 2 )
		wrongArgs(L);

	checkType(L, LT_USERDATA, 1);
	ProcHandle *pHandle = static_cast<ProcHandle *>(lua_touserdata(L, 1));
	UINT exitCode = 0;

	if( top >= 2 )
	{
		checkType(L, LT_NUMBER, 2);
		exitCode = (UINT)lua_tointeger(L, 2);
	}

	bool success = TerminateProcess(pHandle->handle, exitCode);

	lua_pushboolean(L, success);
	return 1;
}

/*	process.getWindows(number procId)
Returns:	table

Returns a table of all window handles belonging to the process.
If this function fails, it will return nil.
*/
int Process_lua::getWindows(lua_State *L)
{
	int top = lua_gettop(L);
	if( top != 1 )
		wrongArgs(L);

	checkType(L, LT_NUMBER, 1);
	DWORD dwPid = (DWORD)lua_tointeger(L, 1);

	EnumFilterWindows efw;
	efw.dwPid = dwPid;

	EnumWindows(EnumFilterWindowsByProcIdProc, (LPARAM)&efw);

	if( efw.hwndList.empty() )
		return 0;

	lua_newtable(L);
	for(unsigned int i = 0; i < efw.hwndList.size(); i++)
	{
		lua_pushinteger(L, i+1); // Push key

		char namestring[2048];
		GetWindowText(efw.hwndList.at(i), (char *)&namestring, sizeof(namestring)-1);

		char classstring[256];
		GetClassName(efw.hwndList.at(i), (char*)&classstring, sizeof(classstring)-1);

		lua_newtable(L); // Push value (table)
		lua_pushstring(L, "hwnd");
		lua_pushinteger(L, (lua_Integer)efw.hwndList.at(i));
		lua_settable(L, -3);

		lua_pushstring(L, "name");
		lua_pushstring(L, namestring);
		lua_settable(L, -3);

		lua_pushstring(L, "class");
		lua_pushstring(L, classstring);
		lua_settable(L, -3);
		lua_settable(L, -3); // Set it
	}

	return 1;
}
/*	process.suspend(handle proc])
Returns:	boolean

Terminates a process with the given exit code.
Returns whether or not the call was successful
*/
int Process_lua::suspend(lua_State *L)
{
	int top = lua_gettop(L);
	if( top != 1 )
		wrongArgs(L);

	checkType(L, LT_USERDATA, 1);
	ProcHandle *pHandle = static_cast<ProcHandle *>(lua_touserdata(L, 1));
	UINT exitCode = 0;
	
	NtSuspendProcess pfnNtSuspendProcess = (NtSuspendProcess)GetProcAddress(GetModuleHandle("ntdll"), "NtSuspendProcess");
	NtResumeProcess pfnNtResumeProcess = (NtResumeProcess)GetProcAddress(GetModuleHandle("ntdll"), "NtResumeProcess");

	int successn = pfnNtResumeProcess(pHandle->handle);
	int success = pfnNtSuspendProcess(pHandle->handle);
	
	lua_pushinteger(L, successn);
	lua_pushinteger(L, success);
	return 2;
}
/*	process.resume(handle proc)
Returns:	boolean

Terminates a process with the given exit code.
Returns whether or not the call was successful
*/
int Process_lua::resume(lua_State *L)
{
	int top = lua_gettop(L);
	if( top != 1 )
		wrongArgs(L);

	checkType(L, LT_USERDATA, 1);
	ProcHandle *pHandle = static_cast<ProcHandle *>(lua_touserdata(L, 1));
	UINT exitCode = 0;

	//NtSuspendProcess pfnNtSuspendProcess = (NtSuspendProcess)GetProcAddress(GetModuleHandle("ntdll"), "NtSuspendProcess");
	NtResumeProcess pfnNtResumeProcess = (NtResumeProcess)GetProcAddress(GetModuleHandle("ntdll"), "NtResumeProcess");


	int success = pfnNtResumeProcess(pHandle->handle);

	lua_pushinteger(L, success);
	return 1;
}