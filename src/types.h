/******************************************************************************
	Project: 	MicroMacro
	Author: 	SolarStrike Software
	URL:		www.solarstrike.net
	License:	Modified BSD (see license.txt)
******************************************************************************/
#define NETWORKING_ENABLED
using namespace std;

#ifndef TYPES_H
#define TYPES_H

	#include <string>
	#include <vector>
	#include <queue>
	#include "wininclude.h"
	#include "mutex.h"
	#include "event.h"
	#include <stdint.h>

	namespace MicroMacro
	{

		enum Multivar_type{VT_NUMBER, VT_STRING, VT_NIL};
		enum BatchJob_type{MEM_BYTE, MEM_UBYTE, MEM_SHORT, MEM_USHORT, MEM_INT, MEM_UINT,
			MEM_INT64, MEM_UINT64, MEM_FLOAT, MEM_DOUBLE,MEM_LDOUBLE, MEM_STRING,MEM_USTRING, MEM_SKIP};

		typedef unsigned int ALuint;
		class Event;


		struct WindowInfo
		{
			std::string name;
			std::string classname;
			HWND hwnd;
		};

		/* Used in window.findList() */
		struct EnumWindowListInfo
		{
			std::vector<WindowInfo> windows;
			std::string name;
			std::string classname;
		};

		/* Holds a handle to a process and any extra info about an open process */
		struct ProcHandle
		{
			HANDLE handle;
			bool is32bit;
		};

		/* Currently has no use */
		/*
		struct WindowDCPair
		{
			HWND hwnd;
			HDC hdc;
		};*/

		/* Describes a memory read job (type and length) */
		struct BatchJob
		{
			size_t count;
			BatchJob_type type;

			BatchJob &operator=(const BatchJob &);
		};

		/* Used to load and play sound */
		struct AudioResource
		{
			ALuint buffer;
			ALuint source;
		};

		/* Holds data read from a process */
		struct MemoryChunk
		{
			size_t address;
			size_t size;
			char *data;
		};

		template <class T>
		T getChunkVariable(MemoryChunk *pChunk, int64_t offset, int &err)
		{
			err = 0;
			if( (offset+sizeof(T)) > pChunk->size )
			{
				err = -1;
				return (T)0;
			}

			return *(T*)(pChunk->data+offset);
		}

		std::string getChunkString(MemoryChunk *pChunk, int64_t offset, uint64_t length, int &err);

		struct Vector3d
		{
			double x; double y; double z;
			Vector3d() : x(0), y(0), z(0) { };
			Vector3d(double _x, double _y, double _z) : x(_x), y(_y), z(_z) { };
			Vector3d &operator=(const Vector3d &);
			Vector3d operator+(const Vector3d &);
			Vector3d operator-(const Vector3d &);
			Vector3d operator*(const Vector3d &);
			Vector3d operator*(double);
			Vector3d operator/(const Vector3d &);
			Vector3d operator/(double);
			Vector3d cross(const Vector3d &);
			double dot(const Vector3d &);
			Vector3d normal();
			double magnitude();
		};

		struct Quaternion
		{
			double w, x, y, z;
			Quaternion() : w(1), x(0), y(0), z(0) {};
			Quaternion(double _w, double _x, double _y, double _z) : w(_w), x(_x), y(_y), z(_z) {};
			Quaternion(Vector3d &);
			Quaternion(Vector3d &, double);
			Quaternion &operator=(const Quaternion &);
			Quaternion operator*(double);
			Vector3d operator*(const Vector3d &);
			Quaternion operator*(const Quaternion &);
			Quaternion conjugate();
			Quaternion normal();
		};

		#ifdef NETWORKING_ENABLED
		struct Socket
		{
			SOCKET socket;
			HANDLE hThread;
			int protocol;
			bool connected;
			bool open;

			std::queue<Event> eventQueue;
			std::queue<std::string> recvQueue;
			Mutex mutex;
		};
		#endif

	} // End of namespace MicroMacro
#endif
