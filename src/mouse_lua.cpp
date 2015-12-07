/******************************************************************************
	Project: 	MicroMacro
	Author: 	SolarStrike Software
	URL:		www.solarstrike.net
	License:	Modified BSD (see license.txt)
******************************************************************************/
#pragma warning( disable : 4800)
#include "mouse_lua.h"
#include "error.h"
#include "macro.h"
#include <math.h>
extern "C"
{
	#include <lua.h>
	#include <lauxlib.h>
	#include <lualib.h>
}

#include <cmath>

int Mouse_lua::regmod(lua_State *L)
{
	static const luaL_Reg _funcs[] = {
		{"pressed", Mouse_lua::pressed},
		{"released", Mouse_lua::released},
		{"isDown", Mouse_lua::isDown},
		{"press", Mouse_lua::press},
		{"hold", Mouse_lua::hold},
		{"release", Mouse_lua::release},
		{"move", Mouse_lua::move},
		{"setPosition", Mouse_lua::setPosition},
		{"setWindowPosition",Mouse_lua::setWindowPosition },
		{"getPosition", Mouse_lua::getPosition},
		{"getWindowPosition",Mouse_lua::getWindowPosition},
		{"getConsolePosition", Mouse_lua::getConsolePosition},
		{"wheelMove", Mouse_lua::wheelMove},
		{"virtualPress", Mouse_lua::virtualPress},
		{"virtualHold", Mouse_lua::virtualHold},
		{"virtualRelease", Mouse_lua::virtualRelease},
		{"virtualMove", Mouse_lua::virtualMove},
		{"virtualWheelMove", Mouse_lua::virtualWheelMove},
		{"setVirtualPosition", Mouse_lua::setVirtualPosition},
		{"getVirtualPosition", Mouse_lua::getVirtualPosition},
		{NULL, NULL}
	};

	luaL_newlib(L, _funcs);
	lua_setglobal(L, MOUSE_MODULE_NAME);

	return MicroMacro::ERR_OK;
}
//double round(double d)
//{
//  return floor(d + 0.5);
//}
/*	mouse.pressed(number vk)
	Returns:	boolean

	If the mouse button identified by vk was
	pressed since last polling, returns true.
	Else, returns false.
*/
int Mouse_lua::pressed(lua_State *L)
{
	if( lua_gettop(L) != 1 )
		wrongArgs(L);
	checkType(L, LT_NUMBER, 1);

	int vk = (int)lua_tointeger(L, 1);
	if( vk <= VK_XBUTTON2 && vk != 0)
		lua_pushboolean(L, Macro::instance()->getHid()->pressed(vk));
	else
		lua_pushboolean(L, false);
	return 1;
}

/*	mouse.released(number vk)
	Returns:	boolean

	If the mouse button identified by vk was
	released since last polling, returns true.
	Else, returns false.
*/
int Mouse_lua::released(lua_State *L)
{
	if( lua_gettop(L) != 1 )
		wrongArgs(L);
	checkType(L, LT_NUMBER, 1);

	int vk = (int)lua_tointeger(L, 1);
	if( vk <= VK_XBUTTON2 && vk != 0 )
		lua_pushboolean(L, Macro::instance()->getHid()->released(vk));
	else
		lua_pushboolean(L, false);
	return 1;
}

/*	mouse.isDown(number vk)
	Returns:	boolean

	If the mouse button identified by vk is
	currently being held down (as of last polling),
	returns true. Else, returns false.
*/
int Mouse_lua::isDown(lua_State *L)
{
	if( lua_gettop(L) != 1 )
		wrongArgs(L);
	checkType(L, LT_NUMBER, 1);

	int vk = (int)lua_tointeger(L, 1);
	if( vk <= VK_XBUTTON2 && vk != 0 )
		lua_pushboolean(L, Macro::instance()->getHid()->isDown(vk));
	else
		lua_pushboolean(L, false);
	return 1;
}

/*	mouse.press(number vk [, boolean async])
	Returns:	nil

	Attempts to send a synthetic press for the given button.
	If async is true (default), it is queued for automatic
	release. Otherwise, execution is blocked while waiting for release.
*/
int Mouse_lua::press(lua_State *L)
{
	int top = lua_gettop(L);
	if( top != 1 && top != 2 )
		wrongArgs(L);
	checkType(L, LT_NUMBER, 1);
	if( top == 2 )
		checkType(L, LT_BOOLEAN, 2);

	int vk = (int)lua_tointeger(L, 1);
	BOOL async = true;
	if( top == 2 )
		async = (bool)lua_toboolean(L, 2);
	if( vk <= VK_XBUTTON2 && vk != 0 )
		Macro::instance()->getHid()->press(vk, async);
	return 0;
}

/*	mouse.hold(number vk)
	Returns:	nil

	Attempts to send a synthetic hold for the given button.
*/
int Mouse_lua::hold(lua_State *L)
{
	if( lua_gettop(L) != 1 )
		wrongArgs(L);
	checkType(L, LT_NUMBER, 1);

	int vk = (int)lua_tointeger(L, 1);
	if( vk <= VK_XBUTTON2 && vk != 0 )
		Macro::instance()->getHid()->hold(vk);
	return 0;
}

/*	mouse.release(number vk)
	Returns:	nil

	Attempts to send a synthetic release for the given button.
*/
int Mouse_lua::release(lua_State *L)
{
	if( lua_gettop(L) != 1 )
		wrongArgs(L);
	checkType(L, LT_NUMBER, 1);

	int vk = (int)lua_tointeger(L, 1);
	if( vk <= VK_XBUTTON2 && vk != 0 )
		Macro::instance()->getHid()->release(vk);
	return 0;
}

/*	mouse.move(number dx, number dy)
	Returns:	nil

	Attempts to move the physical mouse cursor.
	'dx' and 'dy' are the amount to move in pixels in
	the x and y axis, respectively.
*/
int Mouse_lua::move(lua_State *L)
{
	if( lua_gettop(L) != 2 )
		wrongArgs(L);
	checkType(L, LT_NUMBER, 1);
	checkType(L, LT_NUMBER, 2);

	int x = (int)lua_tointeger(L, 1);
	int y = (int)lua_tointeger(L, 2);

	INPUT inp;
	inp.type = INPUT_MOUSE;
	inp.mi.dx = x;
	inp.mi.dy = y;
	inp.mi.dwFlags = MOUSEEVENTF_MOVE;
	inp.mi.time = 0;
	SendInput(1, &inp, sizeof(INPUT));

	return 0;
}

/*	mouse.wheelMove(number delta)
	Returns:	nil

	Attempts to move the physical mouse wheel.
	'delta' specified the amount to move; 120 = 1 wheel click.
	If 'delta' is positive, moves the wheel up (away from user).
	If 'delta' is negative, moves the wheel down (towards user).
*/
int Mouse_lua::wheelMove(lua_State *L)
{
	if( lua_gettop(L) != 1 )
		wrongArgs(L);
	checkType(L, LT_NUMBER, 1);
	int delta = (int)lua_tointeger(L, 1);

	INPUT inp;
	inp.type = INPUT_MOUSE;
	inp.mi.dwFlags = MOUSEEVENTF_WHEEL;
	inp.mi.mouseData = (DWORD)(delta);
	SendInput(1, &inp, sizeof(INPUT));
	return 0;
}

/*	mouse.setPosition(number x, number y)
	Returns:	nil

	Attempts to set the physical mouse wheel to
	the given coordinates. 'x' and 'y' are
	specified in pixels.

	NOTE: Microsoft, in their infinite wisdom,
	decided to use some goofy, brain-dead system
	for this and we lose accuracy when normalizing
	the screen coordinates. As a result, the actual
	cursor may be 1 or 2 pixels off our target.
*/
int Mouse_lua::setPosition(lua_State *L)
{
	if( lua_gettop(L) != 2 )
		wrongArgs(L);
	checkType(L, LT_NUMBER, 1);
	checkType(L, LT_NUMBER, 2);

	double fScreenWidth = ::GetSystemMetrics(SM_CXSCREEN)-1;
	double fScreenHeight = ::GetSystemMetrics(SM_CYSCREEN)-1;
	int x = (int)lua_tointeger(L, 1);
	int y = (int)lua_tointeger(L, 2);

	// Normalize coords to expected value
	x = (int)round(x * (65535/fScreenWidth));
	y = (int)round(y * (65535/fScreenHeight));

	INPUT inp;
	inp.type = INPUT_MOUSE;
	inp.mi.dx = (DWORD)x;
	inp.mi.dy = (DWORD)y;
	inp.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
	inp.mi.time = 0;
	SendInput(1, &inp, sizeof(INPUT));

	return 0;
}
/*	mouse.setWindowPosition(hwnd, number x, number y)
Returns:	nil

Attempts to set the physical mouse wheel inside the windows to
the given coordinates. 'x' and 'y' are
specified in pixels.

*/
int Mouse_lua::setWindowPosition( lua_State *L ) {
	if ( lua_gettop( L ) != 3 )
		wrongArgs( L );
	checkType( L, LT_NUMBER, 1 );
	checkType( L, LT_NUMBER, 2 );
	checkType( L, LT_NUMBER, 3 );
	
	HWND hwnd = (HWND) lua_tointeger( L, 1 );
	int x = (int) lua_tointeger( L, 2 );
	int y = (int) lua_tointeger( L, 3 );
	POINT p;
	p.x = x;
	p.y = y;
	ClientToScreen( hwnd, &p );
	SetCursorPos( p.x, p.y );

	return 0;
}
/*	mouse.getPosition()
	Returns:	number x
				number y

	Returns the position of the physical mouse cursor, in pixels.
*/
int Mouse_lua::getPosition(lua_State *L)
{
	if( lua_gettop(L) != 0 )
		wrongArgs(L);

	POINT p;
	GetCursorPos(&p);
	lua_pushnumber(L, p.x);
	lua_pushnumber(L, p.y);
	return 2;
}
/*	mouse.getWindowPosition(hwnd)
Returns:	number x
number y

Returns the position of the physical mouse cursor, in pixels insode the window.
*/
int Mouse_lua::getWindowPosition( lua_State *L ) {
	if ( lua_gettop( L ) != 1 )
		wrongArgs( L );
	checkType( L, LT_NUMBER, 1 );

	HWND hwnd = (HWND) lua_tointeger( L, 1 );

	POINT p;
	GetCursorPos( &p );
	if ( ScreenToClient( hwnd, &p ) )
	{
		lua_pushnumber( L, p.x );
		lua_pushnumber( L, p.y );

		return 2;
	}
	return 0;
	
}

/*	mouse.getConsolePosition()
	Returns:	number x
				number y

	Returns the position of the physical mouse cursor, in console characters.
	NOTE: This returns the position inside the window, not globally.
	Therefor, if the mouse is to the left or above the console window,
	you can receive negative numbers.
*/

int Mouse_lua::getConsolePosition(lua_State *L)
{
	if( lua_gettop(L) != 0 )
		wrongArgs(L);

	POINT mousePos;
	POINT winPos;
	winPos.x = 0; winPos.y = 0;
	GetCursorPos(&mousePos);
	ClientToScreen(Macro::instance()->getAppHwnd(), &winPos);

	int cx = (mousePos.x - winPos.x) / Macro::instance()->getConsoleFontWidth();
	int cy = (mousePos.y - winPos.y) / Macro::instance()->getConsoleFontHeight();

	lua_pushinteger(L, cx);
	lua_pushinteger(L, cy);
	return 2;
}

/*	mouse.virtualPress(number hwnd, number vk [, boolean async])
	Returns:	nil

	Attempts to send a synthetic press for the given button, and sends
	that input directly to the given window.
	If async is true (default), it is queued for automatic
	release. Otherwise, execution is blocked while waiting for release.
*/
int Mouse_lua::virtualPress(lua_State *L)
{
	int top = lua_gettop(L);
	if( top != 2 && top != 3 )
		wrongArgs(L);
	checkType(L, LT_NUMBER, 1);
	checkType(L, LT_NUMBER, 2);
	if( top == 3 )
		checkType(L, LT_BOOLEAN, 3);

	HWND hwnd = (HWND)lua_tointeger(L, 1);
	int vk = (int)lua_tointeger(L, 2);
	BOOL async = true;
	if( top == 3 )
		async = (BOOL)lua_toboolean(L, 3);
	if( vk <= VK_XBUTTON2 && vk != 0 )
		Macro::instance()->getHid()->virtualPress(hwnd, vk, async);
	return 0;
}

/*	mouse.virtualHold(number hwnd, number vk)
	Returns:	nil

	Attempts to send a synthetic hold for the given button, and sends
	that input directly to the given window.
*/
int Mouse_lua::virtualHold(lua_State *L)
{
	int top = lua_gettop(L);
	if( top != 2 )
		wrongArgs(L);
	checkType(L, LT_NUMBER, 1);
	checkType(L, LT_NUMBER, 2);

	HWND hwnd = (HWND)lua_tointeger(L, 1);
	int vk = (int)lua_tointeger(L, 2);
	if( vk <= VK_XBUTTON2 && vk != 0 )
		Macro::instance()->getHid()->virtualHold(hwnd, vk);
	return 0;
}

/*	mouse.virtualRelease(number hwnd, number vk)
	Returns:	nil

	Attempts to send a synthetic release for the given button, and sends
	that input directly to the given window.
*/
int Mouse_lua::virtualRelease(lua_State *L)
{
	int top = lua_gettop(L);
	if( top != 2 )
		wrongArgs(L);
	checkType(L, LT_NUMBER, 1);
	checkType(L, LT_NUMBER, 2);

	HWND hwnd = (HWND)lua_tointeger(L, 1);
	int vk = (int)lua_tointeger(L, 2);
	if( vk <= VK_XBUTTON2 && vk != 0 )
		Macro::instance()->getHid()->virtualRelease(hwnd, vk);
	return 0;
}

/*	mouse.virtualMove(number dx, number dy)
	Returns:	nil

	Moves the virtual mouse cursor by dx, dy.
	This does not affect the physical mouse cursor.
	See mouse.move() for more details.
*/
int Mouse_lua::virtualMove(lua_State *L)
{
	if( lua_gettop(L) != 2 )
		wrongArgs(L);
	checkType(L, LT_NUMBER, 1);
	checkType(L, LT_NUMBER, 2);
	checkType(L, LT_NUMBER, 3);

	int dx, dy;
	int cx, cy;
	HWND hwnd = (HWND)lua_tointeger(L, 1);
	dx = (int)lua_tointeger(L, 2);
	dy = (int)lua_tointeger(L, 3);


	// Update the virtual mouse's position
	Macro::instance()->getHid()->getVirtualMousePos(cx, cy);
	Macro::instance()->getHid()->setVirtualMousePos(cx+dx, cy+dy);

	// Post a message about its movement
	PostMessage(hwnd, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(cx+dx, cy+dy));

	return 0;
}

/*	mouse.virtualWheelMove(number delta)
	Returns:	nil

	Moves the virtual mouse wheel by 'delta'.
	See mouse.wheelMove() for more details.
*/
int Mouse_lua::virtualWheelMove(lua_State *L)
{
	if( lua_gettop(L) != 2 )
		wrongArgs(L);
	checkType(L, LT_NUMBER, 1);
	checkType(L, LT_NUMBER, 2);

	int mx, my;
	Macro::instance()->getHid()->getVirtualMousePos(mx, my);

	HWND hwnd = (HWND)lua_tointeger(L, 1);
	int delta = (int)lua_tointeger(L, 2);
	WPARAM wparam = MAKEWPARAM(0, delta);
	LPARAM lparam = MAKELPARAM(mx, my);
	PostMessage(hwnd, WM_MOUSEWHEEL, wparam, lparam);

	return 0;
}

/*	mouse.setVirtualPosition(number hwnd, number x, number y)
	Returns:	nil

	Sets the virtual mouse cursor to x, y.
	This does not affect the physical mouse cursor.
	See mouse.setPosition() for more details.
*/
int Mouse_lua::setVirtualPosition(lua_State *L)
{
	if( lua_gettop(L) != 3 )
		wrongArgs(L);
	checkType(L, LT_NUMBER, 1);
	checkType(L, LT_NUMBER, 2);
	checkType(L, LT_NUMBER, 3);

	unsigned int dx, dy;
	HWND hwnd = (HWND)lua_tointeger(L, 1);
	dx = (unsigned int)lua_tointeger(L, 2);
	dy = (unsigned int)lua_tointeger(L, 3);
	Macro::instance()->getHid()->setVirtualMousePos(dx, dy);

	// Post a message about its movement
	PostMessage(hwnd, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(dx, dy));

	return 0;
}

/*	mouse.getVirtualPosition()
	Returns:	number x
				number y

	Returns the position of the virtual mouse cursor.
	See mouse.getPosition() for more details.
*/
int Mouse_lua::getVirtualPosition(lua_State *L)
{
	if( lua_gettop(L) != 0 )
		wrongArgs(L);

	int cx, cy;
	Macro::instance()->getHid()->getVirtualMousePos(cx, cy);
	lua_pushnumber(L, cx);
	lua_pushnumber(L, cy);
	return 2;
}
