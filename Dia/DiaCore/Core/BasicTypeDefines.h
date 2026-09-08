#pragma once

#define MAX_INT_64				0xffffffffffffffff

#ifndef NULL
#define NULL (0)
#endif

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

namespace Dia
{
	namespace Core
	{
		typedef bool					Bool;
		typedef unsigned char			UChar;
		typedef int						Int;
		typedef unsigned int			UInt;
		typedef unsigned int			Unsigned;
		typedef long					Long;
		typedef unsigned long			ULong;
		typedef float					Float;
		typedef double					Double;
		typedef unsigned				uint;
		typedef unsigned char			Byte;

		typedef short					Int16;
		typedef unsigned short			UInt16;
		typedef int						Int32;
		typedef unsigned int			UInt32;

		typedef long long				Int64;
		typedef unsigned long long		UInt64;

		typedef char					Int8;
		typedef unsigned char			UInt8;
		typedef char					Char;
		typedef const char				ConstChar;
		typedef char*					Str;
		typedef const char*				ConstStr;

		template <typename T, T v>
		struct IntegralConstant
		{
			typedef IntegralConstant<T,v>	Type;
			typedef T						ValueType;
			static const T					value = v;
		};

		template <typename T>
		struct TypeConstant
		{
			typedef T						Type;
		};

		typedef char yes_t;

		struct null_t {};

		typedef struct { char padding [8]; } no_t;

		enum eNoInitializer
		{
			NoInitializer = 0,
			Initializer
		};
	}
}
