#ifndef BIT32_LUA_H
#define BIT32_LUA_H

	#define BIT32_MODULE_NAME		"bit32"

	typedef struct lua_State lua_State;

	class Bit32_lua
	{
		protected:
			static int bitAnd(lua_State *);
			static int bitOr(lua_State *);
			static int lShift(lua_State *);
			static int rShift(lua_State *);

		public:
			static int regmod(lua_State *);
	};

#endif
