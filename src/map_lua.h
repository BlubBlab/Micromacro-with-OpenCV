/******************************************************************************
	Project: 	MicroMacro
	Author: 	SolarStrike Software
	URL:		www.solarstrike.net
	License:	Modified BSD (see license.txt)
******************************************************************************/

#ifndef MAP_LUA
#define MAP_LUA

    #define MAP_MODULE_NAME        "map"
 

    typedef struct lua_State lua_State;

    class Map_lua
    {
        protected:
            static int set(lua_State *);
            static int get(lua_State *);
			static int clear(lua_State *);
			static int has(lua_State *);
			static int remove(lua_State *);
			static int setu(lua_State *);
            static int getu(lua_State *);
			static int clearu(lua_State *);
			static int hasu(lua_State *);
			static int removeu(lua_State *);

        public:
            static int regmod(lua_State *);
    };

#endif
