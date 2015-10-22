/******************************************************************************
Project: 	MicroMacro
Author: 	SolarStrike Software
URL:		www.solarstrike.net
License:	Modified BSD (see license.txt)
******************************************************************************/

#ifndef CV_LUA
#define CV_LUA

#define CV_MODULE_NAME        "cv"


typedef struct lua_State lua_State;

class CV_lua
{
protected:
	static int lines(lua_State *);
	static int cycles(lua_State *);
	static int objects(lua_State *);
	static int setFilter_colour(lua_State *);
	static int setFilter_colour2(lua_State *);
	static int setFilter_rect(lua_State *);
	static int clearFilter(lua_State *);
	static int lines_next(lua_State *);
	static int cycles_next(lua_State *);
	static int objects_next(lua_State *);
	static int clearBuffer(lua_State *);
	static int loadImage(lua_State *);
	static int setFilter( lua_State * L );
	static int saveImage(lua_State *);
	static int motions(lua_State *);
	static int motions2(lua_State *);
	static int motions_next(lua_State *);
	static int getPixel(lua_State *);
	static int findCorners( lua_State * );
	static int setMaskFilter(lua_State *);
	static int clearMaskFilter(lua_State *);

public:
	static int regmod(lua_State *);
};

#endif
