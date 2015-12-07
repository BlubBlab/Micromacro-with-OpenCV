/******************************************************************************
Project: 	MicroMacro
Author: 	SolarStrike Software
URL:		www.solarstrike.net
License:	Modified BSD (see license.txt)
******************************************************************************/
#pragma warning( disable : 4800)
#include "cv_lua.h"
#include "error.h"
#include "opencv2/core/core.hpp"
#include "opencv2/video/background_segm.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/objdetect/objdetect.hpp"
#include "objectdll.h"
#include "window_lua.h"
#include "types.h"
#include "strl.h"
#include "event.h"
#include "macro.h"
#include "luatypes.h"
#include "types.h"
#include <iostream>
#include <map> 



extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
//filter on heap
cv::Mat filter;
bool filter_flag = false;
bool mask_flag = false;
int number_args = 0;
// save info on heap
std::queue<int> buffer;
std::queue<std::map<std::string, int>> mask;
/*
Helper function to which made screenshots and convert it into an Mat object
In this function also the filterMask are applied to cut of regions.
*/
cv::Mat hwnd2mat( HWND hwnd ) {

	HDC hwindowDC, hwindowCompatibleDC;

	int height, width, srcheight, srcwidth;
	HBITMAP hbwindow;
	cv::Mat src;
	BITMAPINFOHEADER  bi;

	hwindowDC = GetDC( hwnd );
	hwindowCompatibleDC = CreateCompatibleDC( hwindowDC );
	SetStretchBltMode( hwindowCompatibleDC, COLORONCOLOR );

	RECT windowsize;    // get the height and width of the screen
	GetClientRect( hwnd, &windowsize );

	srcheight = windowsize.bottom;
	srcwidth = windowsize.right;
	height = windowsize.bottom;  //change this to whatever size you want to resize to
	width = windowsize.right;

	src.create( height, width, CV_8UC4 );

	// create a bitmap
	hbwindow = CreateCompatibleBitmap( hwindowDC, width, height );
	bi.biSize = sizeof( BITMAPINFOHEADER );    //http://msdn.microsoft.com/en-us/library/windows/window/dd183402%28v=vs.85%29.aspx
	bi.biWidth = width;
	bi.biHeight = -height;  //this is the line that makes it draw upside down or not
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	// use the previously created device context with the bitmap
	SelectObject( hwindowCompatibleDC, hbwindow );
	// copy from the window device context to the bitmap device context
	StretchBlt( hwindowCompatibleDC, 0, 0, width, height, hwindowDC, 0, 0, srcwidth, srcheight, SRCCOPY ); //change SRCCOPY to NOTSRCCOPY for wacky colors !
	GetDIBits( hwindowCompatibleDC, hbwindow, 0, height, src.data, (BITMAPINFO *) &bi, DIB_RGB_COLORS );  //copy from hwindowCompatibleDC to hbwindow

	// avoid memory leak
	DeleteObject( hbwindow ); DeleteDC( hwindowCompatibleDC ); ReleaseDC( hwnd, hwindowDC );
	if ( src.empty() ) {
		printf( "possible wrong HWD no picture \n" );
	}
	std::queue<std::map<std::string, int>> rebuffer;

	if ( mask_flag ) {
		//seek the the list through



		while ( !mask.empty() ) {
			// find the corners inside the queue than inside the map
			if ( mask.front().count( "x1" ) == 0 ) {
				printf( "missing key for mask x1 breaking loop \n" );
			}
			int x1 = mask.front()[ "x1" ];

			if ( mask.front().count( "y1" ) == 0 ) {
				printf( "missing key for mask y1 breaking loop \n" );
			}
			int y1 = mask.front()[ "y1" ];

			if ( mask.front().count( "x2" ) == 0 ) {
				printf( "missing key for mask x2 breaking loop \n" );
			}
			int x2 = mask.front()[ "x2" ];

			if ( mask.front().count( "y2" ) == 0 ) {
				printf( "missing key for mask y2 breaking loop \n" );
			}
			int y2 = mask.front()[ "y2" ];
			//draw a rectangle above the picture to mask ist
			//rectangle(src,Point(x1,y1),Point(x2,y2),Scalar(0,0,255),FILLED,8);
#pragma loop(hint_parallel(4))
			for ( int i = 0 + y1; i < src.rows && i < y2; i++ ) { //i=y
				for ( int j = 0 + x1; j < src.cols && j < x2; j++ ) { // j= x
					if ( i >= 0 && j >= 0 ) {
						src.at<cv::Vec3b>( i, j )[ 0 ] = 0;
						src.at<cv::Vec3b>( i, j )[ 1 ] = 0;
						src.at<cv::Vec3b>( i, j )[ 2 ] = 0;
						src.at<cv::Vec3b>( i, j )[ 3 ] = 0;
					}
				}
			}


			// this
			rebuffer.push( mask.front() );
			mask.pop();

		}
	}
	//refill the global buffer
	while ( !rebuffer.empty() ) {
		mask.push( rebuffer.front() );
		rebuffer.pop();
	}


	return src;
}

int CV_lua::regmod( lua_State *L ) {
	static const luaL_Reg _funcs[] = {
		{"lines", CV_lua::lines},
		{"cycles", CV_lua::cycles},
		{"objects", CV_lua::objects},
		{"motions", CV_lua::motions},
		{"motions2", CV_lua::motions2},
		{"setFilter_colour", CV_lua::setFilter_colour},
		{"setFilter_colour2", CV_lua::setFilter_colour2},
		{"setFilter_rect", CV_lua::setFilter_rect},
		{"setFilter", CV_lua::setFilter },
		{"getFilterPixel", CV_lua::getPixel},
		{"getFilterPixelSearch", CV_lua::getPixelSearch},
		{"getFilterBufferDimension",CV_lua::getFilterBufferDimension},
		{"hasFilterBuffer",CV_lua::hasFilterBuffer},
		{"findCorners", CV_lua::findCorners},
		{"lines_next", CV_lua::lines_next},
		{"cycles_next", CV_lua::cycles_next},
		{"objects_next", CV_lua::objects_next},
		{"motions_next", CV_lua::motions_next},
		{"clearFilter", CV_lua::clearFilter},
		{"clearBuffer", CV_lua::clearBuffer},
		{"loadImage", CV_lua::loadImage},
		{"saveImage", CV_lua::saveImage},
		{"setMaskFilter", CV_lua::setMaskFilter},
		{"clearMaskFilter", CV_lua::clearMaskFilter},
		{NULL, NULL}
	};

	luaL_newlib( L, _funcs );
	lua_setglobal( L, CV_MODULE_NAME );

	return MicroMacro::ERR_OK;
}
/*	cv.setFilter_rect(hwd, int x, int y, int width, int heigh[,bool debugwindow]))
This function copy only a part of the screen you choose
@pre x,y are top left.

@pre if a filter is definded already it will be used otherweise hwd for the application
from which a screenshot will be made.

@pre width and heigh must be greater than 0

@pre hwd must be valid or 0

@modify The results will be saved in the filter buffer


Returns nil
*/
int CV_lua::setFilter_rect( lua_State *L ) {
	int argsn = lua_gettop( L );
	if ( argsn != 5 && argsn != 6 )
		wrongArgs( L );
	checkType( L, LT_NUMBER, 1 );
	checkType( L, LT_NUMBER, 2 );
	checkType( L, LT_NUMBER, 3 );
	checkType( L, LT_NUMBER, 4 );
	checkType( L, LT_NUMBER, 5 );


	HWND hwnd = (HWND) lua_tointeger( L, 1 );

	int x = (int) lua_tointeger( L, 2 );
	int y = (int) lua_tointeger( L, 3 );
	int width = (int) lua_tointeger( L, 4 );
	int heigh = (int) lua_tointeger( L, 5 );

	if ( width == 0 || heigh == 0 ) {
		wrongArgs( L );
	}


	if ( filter_flag == false ) {
		//filter your screen/window
		if ( hwnd == 0 ) {
			hwnd = GetDesktopWindow();
		}

		cv::Mat src = hwnd2mat( hwnd );
		//clear the old filter
		filter = cv::Scalar( 0, 0, 0 );
		cvtColor( src, src, CV_8UC3 );

		cv::Rect myROI( x, y, width, heigh );
		filter = src( myROI );
		filter_flag = true;
	}
	else
	{
		cv::Rect myROI( x, y, width, heigh );
		filter = filter( myROI );
		//filter the result again

	}
	if ( argsn == 6 ) {
		checkType( L, LT_BOOLEAN, 6 );
		bool showwindows = lua_toboolean( L, 6 );
		if ( showwindows ) {
			cv::namedWindow( "Micro Macro 2:cv_filter", cv::WINDOW_NORMAL );
			imshow( "Micro Macro 2:cv_filter", filter );
			cv::waitKey( 0 );

		}
	}
	return 0;
}
/*	cv.setFilter_colour(hwd, int min_red, int min_green, int min_blue, int max_red, int max_green, int max_blue[,bool debugwindow]))
This filter give out only the part of the the picture which is in between the values you gave it

@pre if a filter is definded already it will be used otherweise hwd for the application
from which a screenshot will be made.

@pre all max parameters must be greater than 0

@pre hwd must be valid or 0

@modify The results will be saved in the filter buffer


Returns nil
*/
int CV_lua::setFilter_colour( lua_State *L ) {
	int argsn = lua_gettop( L );
	if ( argsn != 7 && argsn != 8 )
		wrongArgs( L );
	checkType( L, LT_NUMBER, 1 );
	checkType( L, LT_NUMBER, 2 );
	checkType( L, LT_NUMBER, 3 );
	checkType( L, LT_NUMBER, 4 );
	checkType( L, LT_NUMBER, 5 );
	checkType( L, LT_NUMBER, 6 );
	checkType( L, LT_NUMBER, 7 );
	if ( argsn == 8 )
		checkType( L, LT_BOOLEAN, 8 );


	HWND hwnd = (HWND) lua_tointeger( L, 1 );

	int min_red = (int) lua_tointeger( L, 2 );
	int min_green = (int) lua_tointeger( L, 3 );
	int min_blue = (int) lua_tointeger( L, 4 );

	int max_red = (int) lua_tointeger( L, 5 );
	int max_green = (int) lua_tointeger( L, 6 );
	int max_blue = (int) lua_tointeger( L, 7 );

	if ( max_red == 0 || max_blue == 0 || max_green == 0 ) {

		wrongArgs( L );
		return 0;
	}

	if ( filter_flag == false ) {
		//filter your screen/window
		if ( hwnd == 0 ) {
			hwnd = GetDesktopWindow();
		}

		cv::Mat src = hwnd2mat( hwnd );
		//clear the old filter
		filter = cv::Scalar( 0, 0, 0 );

		//	cvtColor(src,src,CV_8UC3);
		//cvtColor(filter,filter,CV_8UC3);
		//Mat dst,cdst;
		inRange( src, cv::Scalar( min_blue, min_green, min_red ), cv::Scalar( max_blue, max_green, max_red ), filter );
		filter_flag = true;
		//filter.copyTo(filter,target);
	}
	else
	{
		//filter the result again
		inRange( filter, cv::Scalar( min_blue, min_green, min_red ), cv::Scalar( max_blue, max_green, max_red ), filter );
	}
	if ( argsn == 8 ) {
		checkType( L, LT_BOOLEAN, 8 );
		bool showwindows = lua_toboolean( L, 8 );
		if ( showwindows ) {
			cv::namedWindow( "Micro Macro 2:cv_filter_colour", cv::WINDOW_NORMAL );
			imshow( "Micro Macro 2:cv_filter_colour", filter );
			cv::waitKey( 0 );

		}
	}
	return 0;
}
/*	cv.setFilter_colour2(hwd, int min_red, int min_green, int min_blue, int max_red, int max_green, int max_blue[,bool debugwindow]))
This filter give out only the part of the the picture which is in between the values you gave it

@pre if a filter is definded already it will be used otherweise hwd for the application
from which a screenshot will be made.

@pre all max parameters must be greater than 0

@pre hwd must be valid or 0

@modify The results will be saved in the filter buffer


Returns nil
*/
int CV_lua::setFilter_colour2( lua_State *L ) {
	int argsn = lua_gettop( L );
	if ( argsn != 7 && argsn != 8 )
		wrongArgs( L );
	checkType( L, LT_NUMBER, 1 );
	checkType( L, LT_NUMBER, 2 );
	checkType( L, LT_NUMBER, 3 );
	checkType( L, LT_NUMBER, 4 );
	checkType( L, LT_NUMBER, 5 );
	checkType( L, LT_NUMBER, 6 );
	checkType( L, LT_NUMBER, 7 );
	if ( argsn == 8 )
		checkType( L, LT_BOOLEAN, 8 );


	HWND hwnd = (HWND) lua_tointeger( L, 1 );

	int min_red = (int) lua_tointeger( L, 2 );
	int min_green = (int) lua_tointeger( L, 3 );
	int min_blue = (int) lua_tointeger( L, 4 );

	int max_red = (int) lua_tointeger( L, 5 );
	int max_green = (int) lua_tointeger( L, 6 );
	int max_blue = (int) lua_tointeger( L, 7 );

	if ( max_red == 0 || max_blue == 0 || max_green == 0 ) {

		wrongArgs( L );
	}

	if ( filter_flag == false ) {
		//filter your screen/window
		if ( hwnd == 0 ) {
			hwnd = GetDesktopWindow();
		}

		cv::Mat src = hwnd2mat( hwnd );
		//clear the old filter
		filter = cv::Scalar( 0, 0, 0 );

		//	cvtColor(src,src,CV_8UC3);
		//cvtColor(filter,filter,CV_8UC3);
		//Mat dst,cdst;
#pragma loop(hint_parallel(4))
		for ( int i = 0; i < src.rows; i++ ) { //i=y
			for ( int j = 0; j < src.cols; j++ ) { // j= x
				if ( !( src.at<cv::Vec3b>( i, j )[ 0 ] <= max_blue &&  src.at<cv::Vec3b>( i, j )[ 0 ] >= min_blue &&
					src.at<cv::Vec3b>( i, j )[ 1 ] <= max_green && src.at<cv::Vec3b>( i, j )[ 1 ] >= min_green &&
					src.at<cv::Vec3b>( i, j )[ 2 ] <= max_red && src.at<cv::Vec3b>( i, j )[ 2 ] >= min_red ) ) {
					src.at<cv::Vec3b>( i, j )[ 0 ] = 0;
					src.at<cv::Vec3b>( i, j )[ 1 ] = 0;
					src.at<cv::Vec3b>( i, j )[ 2 ] = 0;
					src.at<cv::Vec3b>( i, j )[ 3 ] = 0;
				}
			}
		}
		filter = src;
		filter_flag = true;
		//filter.copyTo(filter,target);
	}
	else
	{
#pragma loop(hint_parallel(4))
		for ( int i = 0; i < filter.rows; i++ ) { //i=y
			for ( int j = 0; j < filter.cols; j++ ) { // j= x
				if ( !( filter.at<cv::Vec3b>( i, j )[ 0 ] <= max_blue &&  filter.at<cv::Vec3b>( i, j )[ 0 ] >= min_blue &&
					filter.at<cv::Vec3b>( i, j )[ 1 ] <= max_green && filter.at<cv::Vec3b>( i, j )[ 1 ] >= min_green &&
					filter.at<cv::Vec3b>( i, j )[ 2 ] <= max_red && filter.at<cv::Vec3b>( i, j )[ 2 ] >= min_red ) ) {
					filter.at<cv::Vec3b>( i, j )[ 0 ] = 0;
					filter.at<cv::Vec3b>( i, j )[ 1 ] = 0;
					filter.at<cv::Vec3b>( i, j )[ 2 ] = 0;
					filter.at<cv::Vec3b>( i, j )[ 3 ] = 0;
				}
			}
		}
		//filter the result again
		inRange( filter, cv::Scalar( min_blue, min_green, min_red ), cv::Scalar( max_blue, max_green, max_red ), filter );
	}
	if ( argsn == 8 ) {
		checkType( L, LT_BOOLEAN, 8 );
		bool showwindows = lua_toboolean( L, 8 );
		if ( showwindows ) {
			cv::namedWindow( "Micro Macro 2:cv_filter_colour", cv::WINDOW_NORMAL );
			imshow( "Micro Macro 2:cv_filter_colour", filter );
			cv::waitKey( 0 );

		}
	}
	return 0;
}
/*	cv.clearFilter()
This empty the filter buffer in which all pictures are stored between calls

Returns nil
*/
int CV_lua::clearFilter( lua_State *L ) {
	if ( lua_gettop( L ) != 0 )
		wrongArgs( L );
	filter_flag = false;
	filter = cv::Scalar( 0, 0, 0 );
	filter.resize( 0 );
	return 0;
}
/*	cv.lines(hwd[, int min_lenght][, max_gap][,bool debugwindow]))
This call will search lines in the picture/screen

@pre if a filter is definded already it will be used otherweise hwd for the application
from which a screenshot will be made.

@pre hwd must be valid or 0

@modify The results will be saved in the result buffer, you can get it with cv.lines_next()

@post The filter buffer will be flushed

Returns int number of results
*/
int CV_lua::lines( lua_State *L ) {
	int argsn = lua_gettop( L );
	if ( argsn > 4 || argsn < 1 )
		wrongArgs( L );
	checkType( L, LT_NUMBER, 1 );

	if ( argsn > 1 )
		checkType( L, LT_NUMBER, 2 );
	if ( argsn > 2 )
		checkType( L, LT_NUMBER, 3 );
	if ( argsn > 3 )
		checkType( L, LT_BOOLEAN, 4 );

	while ( !buffer.empty() ) {
		buffer.pop();
	}


	//global setup for return values
	number_args = 4;
	//size_t filenameLen;
	HWND hwnd = (HWND) lua_tointeger( L, 1 );
	//const char *filename = lua_tolstring(L, 2, &filenameLen);
	cv::Mat src;

	if ( hwnd == 0 )
		hwnd = GetDesktopWindow();

	if ( filter_flag == false ) {
		src = hwnd2mat( hwnd );
	}
	else
	{
		src = filter;

	}
	cv::Mat dst, cdst;



	Canny( src, dst, 50, 200, 3 );
	cv::cvtColor( dst, cdst, cv::COLOR_GRAY2BGR );

	int min_length;
	int max_gap;
	bool debug_image;
	int show_insec = 30;
	if ( argsn >= 2 ) {
		min_length = (int) lua_tonumber( L, 2 );
	}
	else
	{
		//default value
		min_length = 50;
	}

	if ( argsn >= 3 ) {
		max_gap = (int) lua_tonumber( L, 3 );
	}
	else
	{
		//default value
		max_gap = 10;
	}
	//debug window?
	if ( argsn >= 4 ) {
		debug_image = lua_toboolean( L, 4 );
	}
	else
	{
		//default value
		debug_image = false;
	}

	std::vector<cv::Vec4i> lines;
	//HoughLinesP(cdst, lines, 1, CV_PI/180, 50, 50, 10 );
	HoughLinesP( dst, lines, 1, CV_PI / 180, 50, min_length, max_gap );
	int counter = 0;
	for ( size_t i = 0; i < lines.size(); i++ )
	{
		cv::Vec4i l = lines[ i ];

		buffer.push( l[ 0 ] );
		buffer.push( l[ 1 ] );
		buffer.push( l[ 2 ] );
		buffer.push( l[ 3 ] );

		if ( debug_image == true ) {
			line( cdst, cv::Point( l[ 0 ], l[ 1 ] ), cv::Point( l[ 2 ], l[ 3 ] ), cv::Scalar( 0, 0, 255 ), 3, 8 );
		}
	}
	//std::cout << "Here comes the counter: "<< counter << std::endl;
	//std::cout << "Here comes the lines" << lines.size() << std::endl;

	//imshow("source", src);
	//imshow("detected lines", cdst);

	//waitKey();
	if ( debug_image ) {
		cv::namedWindow( "Micro Macro 2:cv_lines", cv::WINDOW_NORMAL );
		imshow( "Micro Macro 2:cv_lines", cdst );
		cv::waitKey( 0 );
	}
	// avoid memory leak

	if ( filter_flag == false ) {
		// we can't relese it when then pointer show to the filer on heap.
		src.release();
	}
	else
	{
		//clear buffer;
		filter.resize( 0 );
	}

	dst.release();
	cdst.release();

	if ( filter_flag == true ) {
		filter_flag = false;
		filter = cv::Scalar( 0, 0, 0 );
		filter.resize( 0 );
	}
	//push number of objects
	lua_pushinteger( L, (lua_Unsigned) lines.size() );

	return 1;

}
/*	cv.findcorners(hwd[, int min_lenght][, max_gap][,bool debugwindow]))
This call will search lines in the picture/screen

@pre if a filter is definded already it will be used otherweise hwd for the application
from which a screenshot will be made.

@pre hwd must be valid or 0

@modify The results will be saved in the result buffer, you can get it with cv.lines_next()

@post The filter buffer will be flushed

Returns int number of results
*/
int CV_lua::findCorners( lua_State *L ) {
	int argsn = lua_gettop( L );
	if ( argsn != 2 && argsn != 3 ) {
		wrongArgs( L );
		return 0;
	}
	checkType( L, LT_NUMBER, 1 );
	checkType( L, LT_NUMBER, 2 );


	while ( !buffer.empty() ) {
		buffer.pop();
	}


	//global setup for return values
	number_args = 4;
	//size_t filenameLen;
	HWND hwnd = (HWND) lua_tointeger( L, 1 );
	int corners = (int) lua_tointeger( L, 2 );
	//const char *filename = lua_tolstring(L, 2, &filenameLen);
	cv::Mat src;

	if ( hwnd == 0 )
		hwnd = GetDesktopWindow();

	if ( filter_flag == false ) {
		src = hwnd2mat( hwnd );
	}
	else
	{
		src = filter;

	}

	bool result = ObjectScan::ObjectScan::scan( src, corners, 2, 0, 73 );


	// avoid memory leak

	if ( filter_flag == false ) {
		// we can't relese it when then pointer show to the filer on heap.
		src.release();
	}
	else
	{
		//clear buffer;
		filter.resize( 0 );
	}



	if ( filter_flag == true ) {
		filter_flag = false;
		filter = cv::Scalar( 0, 0, 0 );
		filter.resize( 0 );
	}
	//push number of objects
	lua_pushboolean( L, result );

	return 1;

}
/*	cv.cycles(hwd[, int min_radius][, max_radius][,bool debugwindow]))
This call will search cycles in the picture/screen

@pre if a filter is definded already it will be used otherweise hwd for the application
from which a screenshot will be made.

@pre hwd must be valid or 0

@modify The results will be saved in the result buffer, you can get it with cv.cycles_next()

@post The filter buffer will be flushed

Returns int number of results
*/
int CV_lua::cycles( lua_State *L ) {
	int argsn = lua_gettop( L );
	if ( argsn > 5 || argsn < 1 )
		wrongArgs( L );
	checkType( L, LT_NUMBER, 1 );

	if ( argsn > 1 )
		checkType( L, LT_NUMBER, 2 );
	if ( argsn > 2 )
		checkType( L, LT_NUMBER, 3 );
	if ( argsn > 3 )
		checkType( L, LT_BOOLEAN, 4 );

	while ( !buffer.empty() ) {
		buffer.pop();
	}


	number_args = 3;
	HWND hwnd = (HWND) lua_tointeger( L, 1 );
	//const char *filename = lua_tolstring(L, 2, &filenameLen);
	cv::Mat src;

	if ( hwnd == 0 )
		hwnd = GetDesktopWindow();

	if ( filter_flag == false ) {
		src = hwnd2mat( hwnd );
	}
	else
	{
		src = filter;

	}
	cv::Mat dst, cdst;


	Canny( src, dst, 50, 200, 3 );
	cvtColor( dst, cdst, cv::COLOR_GRAY2BGR );

	int min_radius;
	int max_radius;
	bool debug_image;
	int show_insec = 30;
	if ( argsn >= 2 ) {
		min_radius = (int) lua_tonumber( L, 2 );
	}
	else
	{
		//default value
		min_radius = 0;
	}

	if ( argsn >= 3 ) {
		max_radius = (int) lua_tonumber( L, 3 );
	}
	else
	{
		//default value
		max_radius = 0;
	}
	//debug window?
	if ( argsn >= 4 ) {
		debug_image = lua_toboolean( L, 4 );
	}
	else
	{
		//default value
		debug_image = false;
	}


	std::vector<cv::Vec3f> circles;

	/// Apply the Hough Transform to find the circles
	HoughCircles( dst, circles, cv::HOUGH_GRADIENT, 1, dst.rows / 8, 200, 100, min_radius, max_radius );
	//int counter = 0;
	for ( size_t i = 0; i < circles.size(); i++ )
	{


		buffer.push( (int) circles[ i ][ 0 ] );
		buffer.push( (int) circles[ i ][ 1 ] );
		buffer.push( (int) circles[ i ][ 2 ] );
		if ( debug_image == true ) {
			cv::Point center( cvRound( circles[ i ][ 0 ] ), cvRound( circles[ i ][ 1 ] ) );
			int radius = cvRound( circles[ i ][ 2 ] );
			// circle center
			circle( src, center, 3, cv::Scalar( 0, 255, 0 ), -1, 8, 0 );
			// circle outline
			circle( src, center, radius, cv::Scalar( 0, 0, 255 ), 3, 8, 0 );
		}

	}



	//waitKey();
	if ( debug_image ) {
		cv::namedWindow( "Micro Macro 2:cv_cycles", cv::WINDOW_NORMAL );
		imshow( "Micro Macro 2:cv_cycles", src );
		cv::waitKey( 0 );
	}
	// avoid memory leak

	if ( filter_flag == false ) {
		// we can't relese it when then pointer show to the filer on heap.
		src.release();
	}
	else
	{
		//clear buffer;
		filter.resize( 0 );
	}

	dst.release();
	cdst.release();

	if ( filter_flag == true ) {
		filter_flag = false;
		filter = cv::Scalar( 0, 0, 0 );
		filter.resize( 0 );
	}
	//push number of objects
	lua_pushinteger( L, (lua_Unsigned) circles.size() );

	return 1;


}
/*	cv.objects(hwd, string xml_path[,int min_width, int min_height][,int max_width, int max_height][,bool debugwindow]))
This call will search objects in the picture/screen

@pre if a filter is definded already it will be used otherweise hwd for the application
from which a screenshot will be made.

@pre max_width and max_height must be not 0

@pre hwd must be valid or 0

@modify The results will be saved in the result buffer, you can get it with cv.object_next()

@post The filter buffer will be flushed

Returns int number of results
*/
int CV_lua::objects( lua_State *L ) {
	int top = lua_gettop( L );
	if ( top < 2 || top > 7 )
		wrongArgs( L );



	checkType( L, LT_NUMBER, 1 );
	checkType( L, LT_STRING, 2 );

	if ( top >= 4 ) {
		checkType( L, LT_NUMBER, 3 );
		checkType( L, LT_NUMBER, 4 );
	}

	if ( top >= 6 ) {
		checkType( L, LT_NUMBER, 5 );
		checkType( L, LT_NUMBER, 6 );
	}

	if ( top >= 7 ) {
		checkType( L, LT_BOOLEAN, 7 );
	}


	//RECT winRect;
	//HDC hdc;
	//HDC tmpDc;
	//HBITMAP hBmp;
	while ( !buffer.empty() ) {
		buffer.pop();
	}

	HWND hwnd = (HWND) lua_tointeger( L, 1 );


	if ( hwnd == 0 )
		hwnd = GetDesktopWindow();


	cv::Mat src;

	//global setup for return values
	number_args = 4;

	if ( hwnd == 0 )
		hwnd = GetDesktopWindow();

	if ( filter_flag == false ) {
		src = hwnd2mat( hwnd );
	}
	else
	{
		src = filter;

	}
	cv::Mat dst;

	std::string xml_string = lua_tostring( L, 2 );
	cv::CascadeClassifier seekobject;

	if ( !seekobject.load( xml_string ) ) {
		printf( "--(!)Error loading\n" ); return 0;
	};

	std::vector<cv::Rect> faces;

	bool debug_image;
	int show_insec = 30;
	if ( top >= 7 ) {
		debug_image = lua_toboolean( L, 7 );
	}
	else
	{
		//default value
		debug_image = false;
	}
	//any time for the window?

	cv::Mat frame_gray;
	frame_gray.create( src.size(), cv::COLOR_BGR2GRAY );
	frame_gray = cv::Scalar( 0, 0, 0 );

	cvtColor( src, frame_gray, cv::COLOR_BGR2GRAY );
	equalizeHist( frame_gray, frame_gray );


	//-- Detect what you seek
	if ( top == 2 ) {
		seekobject.detectMultiScale( frame_gray, faces, 1.1, 2, 0 );
	}
	else
	{
		if ( top == 4 ) {
			int min_width = (int) lua_tointeger( L, 3 );
			int min_height = (int) lua_tointeger( L, 4 );

			seekobject.detectMultiScale( frame_gray, faces, 1.1, 2, 0, cv::Size( min_width, min_height ) );
		}
		else
		{
			int min_width = (int) lua_tointeger( L, 3 );
			int min_height = (int) lua_tointeger( L, 4 );
			int max_width = (int) lua_tointeger( L, 5 );
			int max_height = (int) lua_tointeger( L, 6 );

			seekobject.detectMultiScale( frame_gray, faces, 1.1, 2, 0, cv::Size( min_width, min_height ), cv::Size( max_width, max_height ) );
		}
	}

	//int counter = 0;
	for ( size_t i = 0; i < faces.size(); i++ ) {

		// push in queue 
		buffer.push( faces[ i ].x );
		buffer.push( faces[ i ].y );
		buffer.push( faces[ i ].width );
		buffer.push( faces[ i ].height );

		if ( debug_image == true ) {
			cv::Point center( (int) ( faces[ i ].x + faces[ i ].width*0.5 ), (int) ( faces[ i ].y + faces[ i ].height*0.5 ) );
			ellipse( src, center, cv::Size( (int) ( faces[ i ].width*0.5 ), (int) ( faces[ i ].height*0.5 ) ), 0, 0, 360, cv::Scalar( 255, 0, 255 ), 4, 8, 0 );
		}
	}




	//waitKey();
	if ( debug_image ) {
		cv::namedWindow( "Micro Macro 2:cv_object", cv::WINDOW_NORMAL );
		imshow( "Micro Macro 2:cv_object", src );
		cv::waitKey( 0 );
	}
	// avoid memory leak

	if ( filter_flag == false ) {
		// we can't relese it when then pointer show to the filer on heap.
		src.release();
	}
	else
	{
		//clear buffer;
		filter.resize( 0 );
	}

	frame_gray.release();
	dst.release();


	if ( filter_flag == true ) {
		filter_flag = false;
		filter = cv::Scalar( 0, 0, 0 );
		filter.resize( 0 );
	}
	//push number of objects
	lua_pushinteger( L, (lua_Unsigned) faces.size() );

	return 1;

}
/*	cv.lines_next()
This will be return the results of cv.lines()

@pre cv.lines(..) must be called previously

Returns nil or tables {tab.x1,tab.y1,tab.x2,tab.y2},{..} until 10 times
each table has stored the coordinates for a line to get all results
you must call that function muliply times until you got less than 10 arguments or nil.

if you have all the results before the result buffer is empty please flush it with:
cv.clearbuffer()
*/
int CV_lua::lines_next( lua_State *L ) {
	if ( lua_gettop( L ) != 0 )
		wrongArgs( L );
	int size = 0;

	if ( buffer.empty() == true ) {
		lua_pushnil( L );
		return 1;
	}

	for ( size_t i = 0; i < buffer.size() && i < 10; i++ ) {
		lua_createtable( L, 0, 4 );

		lua_pushinteger( L, buffer.front() );
		lua_setfield( L, -2, "x1" );
		buffer.pop();//delete value 

		lua_pushinteger( L, buffer.front() );
		lua_setfield( L, -2, "y1" );
		buffer.pop();//delete value 

		lua_pushinteger( L, buffer.front() );
		lua_setfield( L, -2, "x2" );
		buffer.pop();//delete value 

		lua_pushinteger( L, buffer.front() );
		lua_setfield( L, -2, "y2" );
		buffer.pop();//delete value 

		size = size + 1;

	}
	return size;
}
/*	cv.lines_cycles()
This will be return the results of cv.lines()

@pre cv.cycles(..) must be called previously

Returns nil or tables {tab.x,tab.y,tab.radius},{..} until 10 times
each table has stored the coordinates for a cycle to get all results
you must call that function muliply times until you got less than 10 arguments or nil.

if you have all the results before the result buffer is empty please flush it with:
cv.clearbuffer()
*/
int CV_lua::cycles_next( lua_State *L ) {
	if ( lua_gettop( L ) != 0 )
		wrongArgs( L );
	int size = 0;

	if ( buffer.size() == 0 ) {
		lua_pushnil( L );
		return 1;
	}

	for ( size_t i = 0; i < buffer.size() && i < 10; i++ ) {
		lua_createtable( L, 0, 3 );

		lua_pushinteger( L, buffer.front() );
		lua_setfield( L, -2, "x" );
		buffer.pop();//delete value 

		lua_pushinteger( L, buffer.front() );
		lua_setfield( L, -2, "y" );
		buffer.pop();//delete value 

		lua_pushinteger( L, buffer.front() );
		lua_setfield( L, -2, "radius" );
		buffer.pop();//delete value 



		size = size + 1;

	}
	return size;
}
/*	cv.objects_next()
This will be return the results of cv.objects()

@pre cv.objects(..) must be called previously

Returns nil or tables {tab.x,tab.y,tab.width,tab.height},{..} until 10 times
each table has stored the coordinates for a object to get all results
you must call that function muliply times until you got less than 10 arguments or nil.

if you have all the results before the result buffer is empty please flush it with:
cv.clearbuffer()
*/
int CV_lua::objects_next( lua_State *L ) {
	if ( lua_gettop( L ) != 0 )
		wrongArgs( L );
	int size = 0;

	if ( buffer.size() == 0 ) {
		lua_pushnil( L );
		return 1;
	}

	for ( size_t i = 0; i < buffer.size() && i <= 10; i++ ) {
		lua_createtable( L, 0, 4 );

		lua_pushinteger( L, buffer.front() );
		lua_setfield( L, -2, "x" );
		buffer.pop();//delete value 

		lua_pushinteger( L, buffer.front() );
		lua_setfield( L, -2, "y" );
		buffer.pop();//delete value 

		lua_pushinteger( L, buffer.front() );
		lua_setfield( L, -2, "width" );
		buffer.pop();//delete value 

		lua_pushinteger( L, buffer.front() );
		lua_setfield( L, -2, "height" );
		buffer.pop();//delete value 

		size = size + 1;

	}
	return size;
}
/*	cv.clearBuffer()
Clear the result buffer

Return nil
*/
int CV_lua::clearBuffer( lua_State *L ) {
	if ( lua_gettop( L ) != 0 )
		wrongArgs( L );

	while ( !buffer.empty() ) {
		buffer.pop();
	}

	return 0;
}
/*	cv.loadImage(string file)
Load an image in the filter buffer

Return nil
*/
int CV_lua::loadImage( lua_State *L ) {
	/// Read the image
	if ( lua_gettop( L ) != 1 )
		wrongArgs( L );
	checkType( L, LT_STRING, 1 );

	filter = cv::imread( lua_tostring( L, 1 ), 1 );

	if ( !filter.data )
		wrongArgs( L );

	filter_flag = true;

	return 0;
}
/*	cv.setFilter(hwnd)
Load an image in the filter buffer

Return nil
*/
int CV_lua::setFilter( lua_State *L ) {
	/// Read the image
	if ( lua_gettop( L ) != 1 )
		wrongArgs( L );
	checkType( L, LT_STRING, 1 );
	HWND hwnd = (HWND) lua_tointeger( L, 1 );

		//filter your screen/window
	if ( hwnd == 0 ) {
		hwnd = GetDesktopWindow();
	}

	cv::Mat src = hwnd2mat( hwnd );
	//clear the old filter
	filter = cv::Scalar( 0, 0, 0 );

	filter = src;
	filter_flag = true;


	if ( !filter.data )
		wrongArgs( L );

	filter_flag = true;

	return 0;
}

/*	cv.saveImage(string file)
Save  an image from the filter buffer

Return nil
*/
int CV_lua::saveImage( lua_State *L ) {
	if ( lua_gettop( L ) != 1 )
		wrongArgs( L );
	checkType( L, LT_STRING, 1 );

	//check if something is in the filterbuffer
	if ( filter_flag == false ) {
		printf( "The filter is empty it make no sense to save an image \n" );
	}
	//write image
	imwrite( lua_tostring( L, 1 ), filter );
	//clear filter buffer
	filter_flag = false;
	filter = cv::Scalar::all( 0 );
	filter.resize( 0 );

	return 0;
}
/*	cv.motions(hwd[, int min_found_in_a_row][,int pause_in_msec] [, int min_diff_pixel][, int history size][, bool shadows][,bool debugwindow]))
This function seeks in the hwnd you gave  motion object


@pre hwd must be valid or 0

@modify The results will be saved in the filter buffer and can be taken by motions_next()


Returns number of objects
*/
int CV_lua::motions( lua_State *L ) {
	int argsn = lua_gettop( L );
	if ( argsn > 7 || argsn < 1 )
		wrongArgs( L );

	checkType( L, LT_NUMBER, 1 );

	if ( argsn > 1 )
		checkType( L, LT_NUMBER, 2 );
	if ( argsn > 2 )
		checkType( L, LT_NUMBER, 3 );
	if ( argsn > 3 )
		checkType( L, LT_NUMBER, 4 );
	if ( argsn > 4 )
		checkType( L, LT_NUMBER, 5 );
	if ( argsn > 5 )
		checkType( L, LT_BOOLEAN, 6 );
	if ( argsn > 6 )
		checkType( L, LT_BOOLEAN, 7 );

	while ( !buffer.empty() ) {
		buffer.pop();
	}
	//global setup for return values

	HWND hwnd = (HWND) lua_tointeger( L, 1 );

	cv::Mat src;

	if ( hwnd == 0 )
		hwnd = GetDesktopWindow();

	src = hwnd2mat( hwnd );


	cv::Mat back;
	cv::Mat fore;
	//MOG2 Background subtractor
	cv::Ptr<cv::BackgroundSubtractor> pMOG2;
	std::vector<std::vector<cv::Point> > contours;
	int min_found = 3;
	DWORD pause = 100;
	int threshold = 16;
	int history = 15;


	bool shadows = true;
	bool debug_image = false;


	if ( argsn > 1 )
		min_found = (int) lua_tointeger( L, 2 );
	if ( argsn > 2 )
		pause = (DWORD) lua_tointeger( L, 3 );
	if ( argsn > 3 )
		threshold = (int) lua_tointeger( L, 4 );
	if ( argsn > 4 )
		history = (int) lua_tointeger( L, 5 );
	if ( argsn > 5 )
		shadows = (bool) lua_toboolean( L, 6 );
	if ( argsn > 6 )
		debug_image = (bool) lua_toboolean( L, 7 );

	//create Background Subtractor objects
	pMOG2 = cv::createBackgroundSubtractorMOG2( history, threshold, shadows ); //MOG2 approach


	int foundcounter = 0;

	while ( foundcounter < min_found ) {
		src = hwnd2mat( hwnd );

		pMOG2->apply( src, fore );
		//pMOG2->getBackgroundImage(back);

		erode( fore, fore, cv::Mat() );
		dilate( fore, fore, cv::Mat() );
		//Earlier this was CHAIN_APPROX_NONE but for speeding up I removed all unnessary point
		//with CHAIN_APPROX_SIMPLE
		findContours( fore, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE );

		if ( contours.size() > 0 ) {
			foundcounter = foundcounter + 1;
			if ( debug_image == true ) {
				drawContours( src, contours, -1, cv::Scalar( 0, 0, 255 ), 1 );
			}

		}
		// don't work too hard
		Sleep( pause );


	}
	//int p1,p2,p3,p4;
	int centerx, centery, width, height = 0;
	int min_x, max_x, min_y, max_y = 0;
	int vcounter = 0;
	std::vector<std::map<std::string, int>> vec_b( contours.size() );
	for ( size_t i = 0; i < contours.size(); i++ ) {

		centerx = 0;
		centery = 0;
		min_x = 0;
		min_y = 0;
		max_x = 0;
		max_y = 0;
		std::vector<cv::Point> vec = contours[ i ];
		if ( vec.size() > 0 ) {

			min_x = vec[ 0 ].x;
			min_y = vec[ 0 ].y;

		}
		for ( size_t j = 0; j < vec.size(); j++ ) {
			cv::Point point = vec[ j ];

			centerx = centerx + point.x;
			centery = centery + point.y;

			if ( point.x > max_x ) {
				max_x = point.x;
			}
			if ( point.x < min_x ) {
				min_x = point.x;
			}
			if ( point.y > max_y ) {
				max_y = point.y;
			}
			if ( point.y < min_y ) {
				min_y = point.y;
			}

		}
		width = max_x - min_x;
		height = max_y - min_y;
		//printf("size. %d",vec.size());
		centerx = centerx / (int) vec.size();
		centery = centery / (int) vec.size();



		std::map<std::string, int> map_t;
		//prepare for compare
		map_t[ "centerx" ] = centerx;
		map_t[ "centery" ] = centery;
		map_t[ "width" ] = width;
		map_t[ "height" ] = height;
		vec_b[ vcounter ] = map_t;
		vcounter++;
	}
	// we aren't finshed yet
	int results = 0;
	bool redo = false;
	for ( size_t i = 0; i < vec_b.size(); i++ ) {

		for ( size_t j = 0; j < vec_b.size(); j++ ) {
			std::map<std::string, int> k = vec_b[ j ];
			std::map<std::string, int> l = vec_b[ i ];
			if ( k != l ) {
				//most awesome way to calculate the distance 
				double dist = sqrt( pow( ( (float) k[ "centerx" ] - l[ "centerx" ] ), 2.0 ) + pow( ( (float) k[ "centery" ] - l[ "centery" ] ), 2.0 ) );
				//int touch1 = abs(dist - (k["width"]/2 +  l["width"]/2));
				//int touch2 = abs(dist - (k["height"]/2 + l["height"]/2));

				if ( dist <= 16.0 ) {
					//fuse points 
					int left, right, bottom, top = 0;

					//calculate new points
					if ( k[ "centerx" ] < l[ "centerx" ] ) {
						left = k[ "centerx" ] - k[ "width" ] / 2;
					}
					else
					{
						left = l[ "centerx" ] - l[ "width" ] / 2;
					}

					if ( k[ "centerx" ] > l[ "centerx" ] ) {
						right = k[ "centerx" ] + k[ "width" ] / 2;
					}
					else
					{
						right = l[ "centerx" ] + l[ "width" ] / 2;
					}

					if ( k[ "centery" ] < l[ "centery" ] ) {
						bottom = k[ "centery" ] - k[ "width" ] / 2;
					}
					else
					{
						bottom = l[ "centery" ] - l[ "width" ] / 2;
					}

					if ( k[ "centery" ] > l[ "centery" ] ) {
						top = k[ "centery" ] + k[ "width" ] / 2;
					}
					else
					{
						top = l[ "centery" ] + l[ "width" ] / 2;
					}

					int newwidth = right - left;
					int newheight = top - bottom;
					int newcenterx = ( k[ "centerx" ] + l[ "centerx" ] ) / 2;
					int newcentery = ( k[ "centery" ] + l[ "centery" ] ) / 2;
					std::map<std::string, int> map_t;
					map_t[ "centerx" ] = newcenterx;
					map_t[ "centery" ] = newcentery;
					map_t[ "width" ] = newwidth;
					map_t[ "height" ] = newheight;
					vec_b[ i ] = map_t;

					vec_b.erase( vec_b.begin() + j );
					vec_b.shrink_to_fit();

					j = j - 1;
					redo = true;

				}
				if ( redo ) {
					i = i - 1;
					redo = false;
				}
			}
		}
	}
	for ( size_t i = 0; i < vec_b.size(); i++ ) {
		buffer.push( vec_b[ i ][ "centerx" ] );
		buffer.push( vec_b[ i ][ "centery" ] );
		buffer.push( vec_b[ i ][ "width" ] );
		buffer.push( vec_b[ i ][ "height" ] );
		results = results + 1;
	}

	//waitKey();
	if ( debug_image ) {
		cv::namedWindow( "Micro Macro 2:cv_motion", cv::WINDOW_NORMAL );
		imshow( "Micro Macro 2:cv_motion", src );
		cv::waitKey( 0 );
	}
	// avoid memory leak
	vec_b.shrink_to_fit();
	//push number of objects
	lua_pushinteger( L, results );

	return 1;
}
/*	cv.motions2(hwd[, int max_time_without_found][,int pause_in_msec] [, int min_diff_pixel][, int history size][, bool shadows][,bool debugwindow]))
This function seeks in the hwnd you gave  motion object


@pre hwd must be valid or 0

@modify The results will be saved in the filter buffer and can be taken by motions_next()


Returns number of objects
*/
int CV_lua::motions2( lua_State *L ) {
	int argsn = lua_gettop( L );
	if ( argsn > 7 || argsn < 1 )
		wrongArgs( L );

	checkType( L, LT_NUMBER, 1 );

	if ( argsn > 1 )
		checkType( L, LT_NUMBER, 2 );
	if ( argsn > 2 )
		checkType( L, LT_NUMBER, 3 );
	if ( argsn > 3 )
		checkType( L, LT_NUMBER, 4 );
	if ( argsn > 4 )
		checkType( L, LT_NUMBER, 5 );
	if ( argsn > 5 )
		checkType( L, LT_BOOLEAN, 6 );
	if ( argsn > 6 )
		checkType( L, LT_BOOLEAN, 7 );


	while ( !buffer.empty() ) {
		buffer.pop();
	}

	//global setup for return values

	HWND hwnd = (HWND) lua_tointeger( L, 1 );

	cv::Mat src;

	if ( hwnd == 0 )
		hwnd = GetDesktopWindow();

	src = hwnd2mat( hwnd );


	cv::Mat back;
	cv::Mat fore;
	//MOG2 Background subtractor
	cv::Ptr<cv::BackgroundSubtractor> pMOG2;
	std::vector<std::vector<cv::Point> > contours;
	DWORD max_time = 500;
	DWORD pause = 100;
	int threshold = 16;
	int history = 15;

	int breakpoint = 3;

	bool shadows = true;
	bool debug_image = false;


	if ( argsn > 1 )
		max_time = (DWORD) lua_tointeger( L, 2 );
	if ( argsn > 2 )
		pause = (DWORD) lua_tointeger( L, 3 );
	if ( argsn > 3 )
		threshold = (int) lua_tointeger( L, 4 );
	if ( argsn > 4 )
		history = (int) lua_tointeger( L, 5 );
	if ( argsn > 5 )
		shadows = (bool) lua_toboolean( L, 6 );
	if ( argsn > 6 )
		debug_image = (bool) lua_toboolean( L, 7 );

	//create Background Subtractor objects
	pMOG2 = cv::createBackgroundSubtractorMOG2( history, threshold, shadows ); //MOG2 approach

	int foundcounter = 0;
	DWORD starttime = GetTickCount();

	while ( ( GetTickCount() - starttime ) <= max_time && !( foundcounter > breakpoint && contours.size() > 0 ) ) {
		src = hwnd2mat( hwnd );

		pMOG2->apply( src, fore );
		//pMOG2->getBackgroundImage(back);

		erode( fore, fore, cv::Mat() );
		dilate( fore, fore, cv::Mat() );
		//Earlier this was CHAIN_APPROX_NONE but for speeding up I removed all unnessary point
		//with CHAIN_APPROX_SIMPLE
		findContours( fore, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE );

		if ( contours.size() > 0 ) {
			foundcounter = foundcounter + 1;
			if ( debug_image == true ) {
				drawContours( src, contours, -1, cv::Scalar( 0, 0, 255 ), 1 );
			}

			if ( foundcounter > breakpoint && contours.size() > 0 ) {
				break;
			}
		}
		// don't work too hard
		Sleep( pause );


	}
	//int p1,p2,p3,p4;
	int centerx, centery, width, height = 0;
	int min_x, max_x, min_y, max_y = 0;
	int vcounter = 0;
	std::vector<std::map<std::string, int>> vec_b( contours.size() );

	for ( size_t i = 0; i < contours.size(); i++ ) {

		centerx = 0;
		centery = 0;
		min_x = 0;
		min_y = 0;
		max_x = 0;
		max_y = 0;
		std::vector<cv::Point> vec = contours[ i ];
		if ( vec.size() > 0 ) {

			min_x = vec[ 0 ].x;
			min_y = vec[ 0 ].y;

		}
		for ( size_t j = 0; j < vec.size(); j++ ) {
			cv::Point point = vec[ j ];

			centerx = centerx + point.x;
			centery = centery + point.y;

			if ( point.x > max_x ) {
				max_x = point.x;
			}
			if ( point.x < min_x ) {
				min_x = point.x;
			}
			if ( point.y > max_y ) {
				max_y = point.y;
			}
			if ( point.y < min_y ) {
				min_y = point.y;
			}

		}
		width = max_x - min_x;
		height = max_y - min_y;
		//printf("size. %d",vec.size());
		centerx = centerx / (int) vec.size();
		centery = centery / (int) vec.size();


		std::map<std::string, int> map_t;
		//prepare for compare
		map_t[ "centerx" ] = centerx;
		map_t[ "centery" ] = centery;
		map_t[ "width" ] = width;
		map_t[ "height" ] = height;
		vec_b[ vcounter ] = map_t;
		vcounter++;
	}
	int results = 0;
	bool redo = false;
	// we aren't finshed yet
	if ( foundcounter > breakpoint ) {

		for ( size_t i = 0; i < vec_b.size(); i++ ) {

			for ( size_t j = 0; j < vec_b.size(); j++ ) {
				std::map<std::string, int> k = vec_b[ j ];
				std::map<std::string, int> l = vec_b[ i ];
				if ( k != l ) {
					// calculate the distance 
					double dist = sqrt( pow( ( (float) k[ "centerx" ] - l[ "centerx" ] ), 2.0 ) + pow( ( (float) k[ "centery" ] - l[ "centery" ] ), 2.0 ) );
					//int touch1 = abs(dist - (k["width"]/2 +  l["width"]/2));
					//int touch2 = abs(dist - (k["height"]/2 + l["height"]/2));

					if ( dist <= 16.0 ) {
						//fuse points 
						int left, right, bottom, top = 0;

						//calculate new points
						if ( k[ "centerx" ] < l[ "centerx" ] ) {
							left = k[ "centerx" ] - k[ "width" ] / 2;
						}
						else
						{
							left = l[ "centerx" ] - l[ "width" ] / 2;
						}

						if ( k[ "centerx" ] > l[ "centerx" ] ) {
							right = k[ "centerx" ] + k[ "width" ] / 2;
						}
						else
						{
							right = l[ "centerx" ] + l[ "width" ] / 2;
						}

						if ( k[ "centery" ] < l[ "centery" ] ) {
							bottom = k[ "centery" ] - k[ "width" ] / 2;
						}
						else
						{
							bottom = l[ "centery" ] - l[ "width" ] / 2;
						}

						if ( k[ "centery" ] > l[ "centery" ] ) {
							top = k[ "centery" ] + k[ "width" ] / 2;
						}
						else
						{
							top = l[ "centery" ] + l[ "width" ] / 2;
						}

						int newwidth = right - left;
						int newheight = top - bottom;
						int newcenterx = ( k[ "centerx" ] + l[ "centerx" ] ) / 2;
						int newcentery = ( k[ "centery" ] + l[ "centery" ] ) / 2;
						std::map<std::string, int> map_t;
						map_t[ "centerx" ] = newcenterx;
						map_t[ "centery" ] = newcentery;
						map_t[ "width" ] = newwidth;
						map_t[ "height" ] = newheight;
						vec_b[ i ] = map_t;

						vec_b.erase( vec_b.begin() + j );
						vec_b.shrink_to_fit();
						j = j - 1;
						redo = true;
					}
					if ( redo ) {
						i = i - 1;
						redo = false;
					}
				}
			}
		}
		for ( size_t i = 0; i < vec_b.size(); i++ ) {
			if ( !( (float) vec_b[ i ][ "width" ] / src.cols > 0.90 && (float) vec_b[ i ][ "height" ] / src.rows > 0.90 ) ) {
				buffer.push( vec_b[ i ][ "centerx" ] );
				buffer.push( vec_b[ i ][ "centery" ] );
				buffer.push( vec_b[ i ][ "width" ] );
				buffer.push( vec_b[ i ][ "height" ] );
				results = results + 1;
			}
		}
	}

	//waitKey();
	if ( debug_image ) {
		cv::namedWindow( "Micro Macro 2:cv_motion", cv::WINDOW_NORMAL );
		imshow( "Micro Macro 2:cv_motion", src );
		cv::waitKey( 0 );
	}
	// avoid memory leak
	vec_b.shrink_to_fit();
	//push number of objects
	if ( foundcounter > breakpoint ) {
		lua_pushinteger( L, results );
	}
	else
	{
		lua_pushinteger( L, 0 );
	}

	return 1;
}
/*	cv.motions_next()
This will be return the results of cv.motions()

@pre cv.motions(..) must be called previously

Returns nil or tables {tab.x,tab.y,tab.width,tab.height},{..} until 10 times
each table has stored the coordinates for a cycle to get all results
you must call that function muliply times until you got less than 10 arguments or nil.

if you have all the results before the result buffer is empty please flush it with:
cv.clearbuffer()
*/
int CV_lua::motions_next( lua_State *L ) {
	if ( lua_gettop( L ) != 0 )
		wrongArgs( L );
	int size = 0;

	if ( buffer.size() == 0 ) {
		lua_pushnil( L );
		return 1;
	}

	for ( size_t i = 0; i < buffer.size() && i <= 10; i++ ) {
		lua_createtable( L, 0, 4 );

		lua_pushinteger( L, buffer.front() );
		lua_setfield( L, -2, "x" );
		buffer.pop();//delete value 

		lua_pushinteger( L, buffer.front() );
		lua_setfield( L, -2, "y" );
		buffer.pop();//delete value 

		lua_pushinteger( L, buffer.front() );
		lua_setfield( L, -2, "width" );
		buffer.pop();//delete value 

		lua_pushinteger( L, buffer.front() );
		lua_setfield( L, -2, "height" );
		buffer.pop();//delete value 

		size = size + 1;

	}
	return size;
}
/*	cv.pixelFilterSearch(number hwnd, number r, number g, number b,
number x1, number y1, number x2, number y2,
number accuracy)

Returns (on success):	number x
number y

Returns (on failure):	nil

Search the given window for a pixel that matches r,g,b
within the rectangle outlined by (x1,y1) -> (x2,y2)

'accuracy' is how many units each channel must be within
the given color to generate a match. Default: 1

'step' is the step size (distance between pixels to search).
*/
int CV_lua::getPixelSearch( lua_State *L ) {
	int top = lua_gettop( L );
	if ( top < 8 || top > 10 )
		wrongArgs( L );
	checkType( L, LT_NUMBER, 1 ); // HWND
	checkType( L, LT_NUMBER, 2 ); // R
	checkType( L, LT_NUMBER, 3 ); // G
	checkType( L, LT_NUMBER, 4 ); // B
	checkType( L, LT_NUMBER, 5 ); // x1
	checkType( L, LT_NUMBER, 6 ); // y1
	checkType( L, LT_NUMBER, 7 ); // x2
	checkType( L, LT_NUMBER, 8 ); // y2
	checkType( L, LT_NUMBER, 9 ); // accuracy
	checkType( L, LT_NUMBER, 10 ); // step

	/*POINT retval;
	POINT offset;
	RECT winRect;
	RECT clientRect;*/
	int r1, g1, b1, r2, g2, b2;
	int x1, y1, x2, y2;
	int resultx, resulty;
	int startx, endx, starty, endy;
	int step = 1;
	int accuracy = 1;
	HWND hwnd;
	bool reversex;
	bool reversey;
	cv::Mat local;
	bool found = false;

	hwnd = (HWND) lua_tointeger( L, 1 );
	r1 = (int) lua_tointeger( L, 2 );
	g1 = (int) lua_tointeger( L, 3 );
	b1 = (int) lua_tointeger( L, 4 );
	x1 = (int) lua_tointeger( L, 5 );
	y1 = (int) lua_tointeger( L, 6 );
	x2 = (int) lua_tointeger( L, 7 );
	y2 = (int) lua_tointeger( L, 8 );

	if ( top >= 9 )
		accuracy = (int) lua_tointeger( L, 9 );

	reversex = ( x2 < x1 );
	reversey = ( y2 < y1 );
	int steps_x = abs( x2 - x1 ) / step;
	int steps_y = abs( y2 - y1 ) / step;


	if(reversex ){ 
		startx = x2;
		endx = x1;
	}
	else
	{
		startx = x1;
		endx = x2;
	}
	if ( reversey ) {
		starty = y2;
		endy = y1;
	}
	else
	{
		starty = y1;
		endy = y2;
	}
	// The number of steps across each axis
	int width = abs( x2 - x1 );
	int height = abs( y2 - y1 );


	if ( hwnd == 0 )
		hwnd = GetDesktopWindow();

	if ( filter_flag == false ) {
		local = hwnd2mat( hwnd );
	}
	else
	{
		local = filter;

	}



	for ( int x = startx; x <= endx && x < local.cols; x++ )
	{
		for ( int y = starty; y <= endy && y < local.rows ; y++ )
		{
			int b = local.at<cv::Vec3b>( y, x )[ 0 ];
			int g = local.at<cv::Vec3b>( y, x )[ 1 ];
			int r = local.at<cv::Vec3b>( y, x )[ 2 ];
			
			/*if ( local.at<Vec3b>( y, x ).channels > 3 ) {
				a = local.at<Vec3b>( y, x )[ 3 ];
			}*/
			if ( abs( r1 - r ) <= accuracy && abs( g1 - g ) <= accuracy && abs( b1 - b ) <= accuracy ) {
				found = true;
				resultx = x;
				resulty = y;
			}
		}
	}
	

	if ( !found )
		return 0;

	lua_pushinteger( L, resultx );
	lua_pushinteger( L, resulty );
	return 2;

}
/*	cv.getFilterBufferDimension()
This function return the dimension of the filterbuffer 


@pre  a filter is definded already


Returns int cols(x),rows(y) dimension
*/
int CV_lua::getFilterBufferDimension( lua_State *L ) {
	if ( filter_flag == false ) {
		lua_pushinteger( L, filter.cols );
		lua_pushinteger( L, filter.rows );
		return 2;
	}
	else
	{
		return 0;
	}
}
/*	cv.hasFilterBuffer()
This function return weather a filterbuffer was 


@pre  a filter is definded already


Returns bool true or false
*/
int CV_lua::hasFilterBuffer( lua_State *L ) {
	if ( filter_flag == false ) {
		lua_pushboolean(L, true );
		return 1;
	}
	else
	{
		lua_pushboolean( L, false );
		return 1;
	}
}
/*	cv.getPixel(hwnd, int x, int y,)
This function return the RGBA values of the part you set up previously with a filter


@pre  a filter is definded already



Returns int red, green ,blue, alpha;
*/
int CV_lua::getPixel( lua_State *L ) {
	int top = lua_gettop( L );
	if ( top != 2 )
		wrongArgs( L );

	checkType( L, LT_NUMBER, 1 );
	checkType( L, LT_NUMBER, 2 );

	HWND hwnd = (HWND) lua_tointeger( L, 1 );
	int x = (int) lua_tonumber( L, 2 );
	int y = (int) lua_tonumber( L, 3 );
	cv::Mat local;

	if ( hwnd == 0 )
		hwnd = GetDesktopWindow();

	if ( filter_flag == false ) {
		local = hwnd2mat( hwnd );
	}
	else
	{
		local = filter;

	}
	if ( y >= 0 && x >= 0 && y < local.rows && x < local.cols ) {
		//values from filter
		int b = local.at<cv::Vec3b>( y, x )[ 0 ];
		int g = local.at<cv::Vec3b>( y, x )[ 1 ];
		int r = local.at<cv::Vec3b>( y, x )[ 2 ];
		int a = 0;
		if ( local.at<cv::Vec3b>( y, x ).channels > 3 ) {
			a = local.at<cv::Vec3b>( y, x )[ 3 ];
		}
		//push values
		lua_pushinteger( L, r );
		lua_pushinteger( L, g );
		lua_pushinteger( L, b );
		lua_pushinteger( L, a );
	}
	else
	{
		//we are out of range
		lua_pushnil( L );
		return 1;
	}

	return 3;
}
/*	cv.setMaskFilter(tab,...)
tab = {x1,y1,x2,y2}

This function set up a filter which block certain parts of the screen.
It apply to all function which use a screenshot. P1 is top left and P2 is bottom right
The function can also use multiple region you can setup by givin more than 1 table.

Returns nil
*/
int CV_lua::setMaskFilter( lua_State *L ) {

	int tabIndex = lua_gettop( L );
	checkType( L, LT_TABLE, tabIndex );

	//we are going through all tables
	while ( tabIndex > 0 ) {
		// a map for the values because I don't know in which order they come.
		std::map<std::string, int> p0;
		// Now iterate this table
		lua_pushnil( L );

		while ( lua_next( L, tabIndex ) ) {
			checkType( L, LT_STRING, -2 );
			checkType( L, LT_NUMBER, -1 );
			p0[ lua_tostring( L, -2 ) ] = (int) lua_tonumber( L, -1 );

			lua_pop( L, 1 );
		}
		mask.push( p0 );

		tabIndex = tabIndex - 1;
	}
	mask_flag = true;

	return 0;
}
/*	cv.clearMaskFilter()

Clear the mask which are given by setMaskFilter
so that all scrrenshot are clear again.

Returns nil
*/
int CV_lua::clearMaskFilter( lua_State *L ) {

	if ( lua_gettop( L ) != 0 )
		wrongArgs( L );

	while ( !buffer.empty() ) {
		mask.pop();
	}
	mask_flag = false;
	return 0;
}