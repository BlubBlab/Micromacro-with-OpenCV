#ifndef VERSION_H
#define VERSION_H

namespace AutoVersion{
	
	//Date Version Types
	static const char DATE[] = "18";
	static const char MONTH[] = "02";
	static const char YEAR[] = "2016";
	static const char UBUNTU_VERSION_STYLE[] =  "18.02";
	
	//Software Status
	static const char STATUS[] =  "Alpha";
	static const char STATUS_SHORT[] =  "a";
	
	//Standard Version Type
	static const long MAJOR  = 1;
	static const long MINOR  = 93;
	static const long BUILD  = 1;
	static const long BUILD_SUB  = 27;
	static const long REVISION  = 1006;
	
	//Miscellaneous Version Types
	static const long BUILDS_COUNT  = 1478;
	#define RC_FILEVERSION 1,92,46,1006
	#define RC_FILEVERSION_STRING "1, 92, 47,24, 1005\0"
	static const char FULLVERSION_STRING [] = "1.93.01.27.1006";
	
	//These values are to keep track of your versioning state, don't modify them.
	static const long BUILD_HISTORY  = 120;
	

}
#endif //VERSION_H
