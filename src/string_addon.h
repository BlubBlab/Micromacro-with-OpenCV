/******************************************************************************
	Project: 	MicroMacro
	Author: 	SolarStrike Software
	URL:		www.solarstrike.net
	License:	Modified BSD (see license.txt)
******************************************************************************/

#ifndef STRING_ADDON_H
#define STRING_ADDON_H

	#define STRING_MODULE_NAME			"string"
	typedef struct lua_State lua_State;

	class String_addon
	{
		protected:
			static int explode(lua_State *);
			static int trim(lua_State *);
			static int random(lua_State *);
			static int toUnicode(lua_State *);
			static int regexmatch( lua_State * );
			static int regexsearch( lua_State * );
			static int regexsearchall( lua_State * );
			static int regexreplaceall( lua_State * L );
			static int regexreplace( lua_State * L );
		public:
			static int regmod(lua_State *);
	};

#endif
