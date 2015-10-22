/******************************************************************************
	Project: 	MicroMacro
	Author: 	SolarStrike Software
	URL:		www.solarstrike.net
	License:	Modified BSD (see license.txt)
******************************************************************************/

#ifndef DEBUGMESSAGES_H
#define DEBUGMESSAGES_H

	#ifdef DISPLAY_DEBUG_MESSAGES
		#define debugMessage(fmt, ...)		debugMessageReal(__FILE__, __LINE__, fmt, ##args)
	#else
		#define debugMessage(fmt, ...)
	#endif

	void debugMessageReal(const char *, int, const char *, ...);

#endif
