/******************************************************************************
Project: 	MicroMacro
Author: 	SolarStrike Software
URL:		www.solarstrike.net
License:	Modified BSD (see license.txt)
******************************************************************************/
#pragma warning( disable : 4800)
#include "map_lua.h"
#include "error.h"
#include <iostream>       // std::cout
#include <thread>         // std::thread
#include <mutex>          // std::mutex
#include <map> 
#include <string.h>

std::mutex mtx;           // mutex for critical section
std::map<std::string,std::string> map_string;
std::map<std::string,int64_t> map_int;
std::map<std::string,uint64_t> map_uint;
std::map<std::string,bool> map_bool;
std::map<std::string,double> map_double;
std::map<std::string,const void*> map_pointer;
std::map<std::string,lua_State *> map_thread; // lua states
extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

int Map_lua::regmod(lua_State *L)
{
	static const luaL_Reg _funcs[] = {
		{"get", Map_lua::get},
		{"set", Map_lua::set},
		{"has", Map_lua::has},
		{"clear", Map_lua::clear},
		{"remove", Map_lua::remove},
		{"getu", Map_lua::getu},
		{"setu", Map_lua::setu},
		{"hasu", Map_lua::hasu},
		{"clearu", Map_lua::clearu},
		{"removeu", Map_lua::removeu},
		{NULL, NULL}
	};

	luaL_newlib(L, _funcs);
	lua_setglobal(L, MAP_MODULE_NAME);

	return MicroMacro::ERR_OK;
}

/*	map.get(string key)
Returns:	string or int value


*/
int Map_lua::get(lua_State *L)
{

	mtx.lock();
	if( lua_gettop(L) != 1 )
		wrongArgs(L);
	checkType(L, LT_STRING, 1);

	size_t strlen;
	const char *str = lua_tolstring(L, 1, &strlen);

	std::string key = std::string(str);

	if(map_string.count(key) > 0){
		std::string value = map_string.at(key);
		lua_pushstring(L, value.c_str());
	}
	else
	{
		if(map_int.count(key) > 0){
			int64_t value = map_int.at(key);
			lua_pushinteger(L, value);
		}
		else
		{
			if(map_double.count(key) > 0){
				double value = map_double.at(key);
				lua_pushnumber(L, value);
			}
			else
			{
				if(map_pointer.count(key) > 0){
					const void* value = map_pointer.at(key);
					//void* v2 = value;
					lua_pushlightuserdata(L,  const_cast<void*>(value));
				}
				else
				{
					if(map_thread.count(key) > 0){
						lua_State * value = map_thread.at(key);
						lua_pushthread(value);
					}
					else
					{
						if(map_bool.count(key) >0){
							bool value = map_bool.at(key);
							lua_pushboolean(L,value);
						}
						else
						{
							lua_pushnil(L); 
						}
					}
				}

			}
		}
	}
	mtx.unlock();
	return 1;
}

/*	map.set(string key, string value)
Returns:	nil

Returns 
*/
int Map_lua::set(lua_State *L)
{
	mtx.lock();

	if( lua_gettop(L) != 2 )
		wrongArgs(L);

	checkType(L, LT_STRING, 1);
	//checkType(L, LT_STRING, 2);
	std::string key = (char *)lua_tostring(L, 1);
	int type = lua_type (L, 2);
	//DBEBUG
	//	std::cout << "Here comes the kex: "<< key << std::endl;
	//	std::cout << "Here comes the type" << type << std::endl;
	if(map_int.count(key) > 0){
		map_int.erase(key);
	}
	if(map_double.count(key) > 0){
		map_double.erase(key);
	}
	if(map_string.count(key) > 0){
		map_string.erase(key);
	}
	if(map_pointer.count(key) > 0){
		map_pointer.erase(key);
	}
	if(map_thread.count(key) > 0){
		map_thread.erase(key);
	}
	if(map_bool.count(key) > 0){
		map_bool.erase(key);
	}

	if(type == LUA_TNUMBER){
		//checkType(L, LT_NUMBER, 2);
		double value = lua_tonumber(L, 2);
		if (value == (int64_t)value) {

			map_int[key] = (int64_t)value;
		} 
		else
		{

			map_double[key] = value;
		}

	}
	else
	{
		if(type == LUA_TSTRING){
			//size_t strlen;
			//std::string key = (char *)lua_tostring(L, 1);
			//checkType(L, LT_STRING, 2);
			std::string value = (char *)lua_tostring(L, 2);

			map_string[key] = value;
		}
		else
		{


			if(type == LUA_TFUNCTION || type == LUA_TTABLE || type == LUA_TUSERDATA  || type == LUA_TLIGHTUSERDATA ){
				
				//std::string key = (char *)lua_tostring(L, 1);
				const void* value = lua_topointer(L, 2);

				map_pointer[key] = value;
			}
			else
			{
				if(type== LUA_TTHREAD){
					//	std::string key = (char *)lua_tostring(L, 1);
					lua_State * value = lua_tothread(L, 2);

					map_thread[key] = value;
				}

				else
				{
					if(type == LUA_TBOOLEAN){
						bool value = (bool)(lua_toboolean(L, 2));
						map_bool[key] = value;
					}
					else
					{
						wrongArgs(L);
					}
				}
			}
		}
	}

	mtx.unlock();

	return 0;
}
/*	map.clear()

clear the whole map;


*/
int Map_lua::clear(lua_State *L)
{
	mtx.lock();

	if( lua_gettop(L) != 0 )
		wrongArgs(L);

	map_double.clear();
	map_int.clear();
	map_string.clear();
	map_pointer.clear();
	map_thread.clear();
	map_bool.clear();
	mtx.unlock();
	return 0;
}
/*	map.has(string key)
Returns: bool if the map has a value with this key


*/
int Map_lua::has(lua_State *L){

	mtx.lock();
	if( lua_gettop(L) != 1 )
		wrongArgs(L);
	checkType(L, LT_STRING, 1);
	std::string key = (char *)lua_tostring(L, 1);

	if(map_double.count(key) > 0){
		lua_pushboolean(L,true);
		mtx.unlock();
		return 1;
	}
	if(map_string.count(key) > 0){
		lua_pushboolean(L,true);
		mtx.unlock();
		return 1;
	}
	if(map_pointer.count(key) > 0){
		lua_pushboolean(L,true);
		mtx.unlock();
		return 1;
	}
	if(map_int.count(key) > 0){
		lua_pushboolean(L,true);
		mtx.unlock();
		return 1;
	}
	if(map_thread.count(key) > 0){
		lua_pushboolean(L,true);
		mtx.unlock();
		return 1;
	}
	if(map_bool.count(key) > 0){
		lua_pushboolean(L,true);
		mtx.unlock();
		return 1;
	}
	lua_pushboolean(L,false);
	mtx.unlock();


	return 1;
}
/*	map.remove(string key)
Removes the value with this key


*/
int Map_lua::remove(lua_State *L){


	mtx.lock();
	if( lua_gettop(L) != 1 )
		wrongArgs(L);
	checkType(L, LT_STRING, 1);
	std::string key = (char *)lua_tostring(L, 1);


	if(map_int.count(key) > 0){
		map_int.erase(key);
	}
	if(map_double.count(key) > 0){
		map_double.erase(key);
	}
	if(map_string.count(key) > 0){
		map_string.erase(key);
	}
	if(map_pointer.count(key) > 0){
		map_pointer.erase(key);
	}
	if(map_thread.count(key) > 0){
		map_thread.erase(key);
	}
	if(map_bool.count(key) > 0){
		map_bool.erase(key);
	}

	mtx.unlock();
	return 0;
}

int Map_lua::getu(lua_State *L)
{

	mtx.lock();
	if( lua_gettop(L) != 1 )
		wrongArgs(L);
	checkType(L, LT_STRING, 1);
	//checkType(L, LT_STRING, 2);
	size_t strlen;
	const char *str = lua_tolstring(L, 1, &strlen);

	std::string key = std::string(str);

	//int type = lua_type (L, 2);
	//DBEBUG
	//	std::cout << "Here comes the kex: "<< key << std::endl;
	//	std::cout << "Here comes the type" << type << std::endl;
	if(map_uint.count(key) > 0){
		lua_pushinteger(L, (lua_Unsigned)map_uint[key]);
		//	map_uint.erase(key);
	}
	else
	{
		lua_pushnil(L);
	}


	mtx.unlock();
	return 1;
}
int Map_lua::setu(lua_State *L)
{
	mtx.lock();

	if( lua_gettop(L) != 2 )
		wrongArgs(L);

	checkType(L, LT_STRING, 1);
	checkType(L, LT_NUMBER, 2);
	std::string key = (char *)lua_tostring(L, 1);
	//int type = lua_type (L, 2);
	if(map_uint.count(key) > 0){
		map_uint.erase(key);
	}
	uint64_t value = (uint64_t)lua_tointeger(L, 2);

	map_uint[key]= value;

	mtx.unlock();

	return 0;
}
int Map_lua::clearu(lua_State *L)
{
	mtx.lock();

	if( lua_gettop(L) != 0 )
		wrongArgs(L);

	map_uint.clear();

	mtx.unlock();

	return 0;
}
int Map_lua::removeu(lua_State *L){


	mtx.lock();
	if( lua_gettop(L) != 1 )
		wrongArgs(L);
	checkType(L, LT_STRING, 1);
	std::string key = (char *)lua_tostring(L, 1);


	if(map_uint.count(key) > 0){
		map_uint.erase(key);
	}
	mtx.unlock();

	return 0;
}
int Map_lua::hasu(lua_State *L){

	mtx.lock();
	if( lua_gettop(L) != 1 )
		wrongArgs(L);
	checkType(L, LT_STRING, 1);
	std::string key = (char *)lua_tostring(L, 1);

	if(map_uint.count(key) > 0){
		lua_pushboolean(L,true);
		mtx.unlock();
		return 1;
	}

	lua_pushboolean(L,false);

	mtx.unlock();
	return 1;

}