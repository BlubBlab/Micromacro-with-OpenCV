#include "bit32_lua.h"
#include "error.h"

extern "C"
{
	#include "lua.h"
	#include "lauxlib.h"
	#include "lualib.h"
}


int Bit32_lua::regmod(lua_State *L)
{
	static const luaL_Reg _funcs[] = {
		{"bitAnd", Bit32_lua::bitAnd},
		{"bitOr", Bit32_lua::bitOr},
		{"lShift", Bit32_lua::lShift},
		{"rShift", Bit32_lua::rShift},
		{NULL, NULL}
	};

	luaL_newlib(L, _funcs);
	lua_setglobal(L, BIT32_MODULE_NAME);

	return MicroMacro::ERR_OK;
}

int Bit32_lua::bitAnd(lua_State *L)
{
	unsigned int top = lua_gettop(L);
	if( top < 2 )
		wrongArgs(L);

	unsigned int val = 0; // Holder var
	for(unsigned int i = 1; i <= top; i++)
	{
		checkType(L, LT_NUMBER, i);	// Ensure all given arguments are numbers
		unsigned int val2 = (unsigned int)lua_tonumber(L, i);
		val = val & val2;
	}

	lua_pushnumber(L, val);
	return 1;
}

int Bit32_lua::bitOr(lua_State *L)
{
	unsigned int top = lua_gettop(L);
	if( top < 2 )
		wrongArgs(L);

	unsigned int val = 0; // Holder var
	for(unsigned int i = 1; i <= top; i++)
	{
		checkType(L, LT_NUMBER, i);	// Ensure all given arguments are numbers
		unsigned int val2 = (unsigned int)lua_tonumber(L, i);
		val = val | val2;
	}

	lua_pushnumber(L, val);
	return 1;
}

int Bit32_lua::lShift(lua_State *L)
{
	if( lua_gettop(L) != 2 )
		wrongArgs(L);
	checkType(L, LT_NUMBER, 1);
	checkType(L, LT_NUMBER, 2);

	lua_Integer val1 = lua_tointeger(L, 1);
	lua_Integer val2 = lua_tointeger(L, 2);

	lua_pushinteger(L, val1 << val2);
	return 1;
}

int Bit32_lua::rShift(lua_State *L)
{
	if( lua_gettop(L) != 2 )
		wrongArgs(L);
	checkType(L, LT_NUMBER, 1);
	checkType(L, LT_NUMBER, 2);

	lua_Integer val1 = lua_tointeger(L, 1);
	lua_Integer val2 = lua_tointeger(L, 2);

	lua_pushinteger(L, val1 >> val2);
	return 1;
}
