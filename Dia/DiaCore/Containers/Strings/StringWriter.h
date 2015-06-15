#ifndef DIA_STRING_WRITER__
#define DIA_STRING_WRITER__

#include "DiaCore/Type/BasicTypeDefines.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			class StringWriter
			{
			public:
				StringWriter();
				StringWriter(StringWriter& rhs);
				StringWriter( char* buffer, unsigned int bufferLength );

				void AssignBuffer( char* buffer, unsigned int bufferLength );
				void ClearBuffer();

				const char* AsCStr() const;
				
				unsigned int Capacity() const;
				unsigned int CurrentLength() const;

				StringWriter& operator << (const char val);
				StringWriter& operator << (const char* val);
				StringWriter& operator << (const int val);
				StringWriter& operator << (const unsigned int val);

				StringWriter& operator << (const short val);
				StringWriter& operator << (const unsigned short val);

				StringWriter& operator << (const long long val);
				StringWriter& operator << (const unsigned long long val);

				StringWriter& operator << (const float val);
				StringWriter& operator << (const double val);
			private:

				char* mCurrentBufferPtr;
				char* mStartBufferPtr;
				char* mEndBufferPtr;
			};
		}
	}
} 
#endif 

