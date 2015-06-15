#include "DiaCore/Containers/Strings/StringWriter.h"

#include "DiaCore/Strings/stringutils.h"
#include "DiaCore/Strings/String256.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			StringWriter::StringWriter()
				: mCurrentBufferPtr(NULL)
				, mStartBufferPtr(NULL)
				, mEndBufferPtr(NULL)
			{}

			StringWriter::StringWriter(StringWriter& rhs)
				: mCurrentBufferPtr(rhs.mCurrentBufferPtr)
				, mStartBufferPtr(rhs.mStartBufferPtr)
				, mEndBufferPtr(rhs.mEndBufferPtr)
			{}

			StringWriter::StringWriter(char* buffer, unsigned int bufferLength)
				: mCurrentBufferPtr(buffer)
				, mStartBufferPtr(buffer)
				, mEndBufferPtr(buffer + (sizeof(char) * bufferLength))
			{
				DIA_ASSERT(bufferLength > 0, "Buffer needs to larger then  zero");

				*mCurrentBufferPtr = NULL;
			}

			void StringWriter::AssignBuffer( char* buffer, unsigned int bufferLength )
			{
				DIA_ASSERT(mStartBufferPtr == NULL, "Buffer already been allocated");
				DIA_ASSERT(bufferLength > 0, "Buffer needs to larger then  zero");

				mCurrentBufferPtr = buffer;
				mStartBufferPtr = buffer;
				mEndBufferPtr = buffer + (sizeof(char) * bufferLength);

				*mCurrentBufferPtr = NULL;
			}		

			const char* StringWriter::AsCStr() const
			{
				DIA_ASSERT(mStartBufferPtr != NULL, "Not been assigned buffer");
				return mStartBufferPtr;
			}

			unsigned int StringWriter::Capacity() const
			{
				DIA_ASSERT(mStartBufferPtr != NULL, "Not been assigned buffer");
				return (mEndBufferPtr - mStartBufferPtr);
			}

			unsigned int StringWriter::CurrentLength() const
			{
				DIA_ASSERT(mStartBufferPtr != NULL, "Not been assigned buffer");
				return StringLength(mStartBufferPtr);
			}

	 		StringWriter& StringWriter::operator << (const char val)
	 		{
				DIA_ASSERT(mStartBufferPtr != NULL, "Not been assigned buffer");

	 			*mCurrentBufferPtr = val;

	 			mCurrentBufferPtr++;
				
				*mCurrentBufferPtr = NULL;
				
	 			return *this;
	 		}

			StringWriter& StringWriter::operator << (const char* val)
			{
				DIA_ASSERT(mStartBufferPtr != NULL, "Not been assigned buffer");

				unsigned int stringLength = StringLength(val);

				DIA_ASSERT((stringLength + (mCurrentBufferPtr - mStartBufferPtr)) < Capacity(), "Overbouding the string buffer");

				StringConcat(mCurrentBufferPtr, val, stringLength);

				mCurrentBufferPtr += (sizeof(char) * stringLength);

				*mCurrentBufferPtr = NULL;
 				
				return *this;
			}

			StringWriter& StringWriter::operator << (const int val)
			{
				String256 buffer;
				StringConvertFromInt(buffer.AsCStr(), val);
			
				*this << buffer.AsCStr();

				return *this;
			}

			StringWriter& StringWriter::operator << (const unsigned int val)
			{
				String256 buffer;
				StringConvertFromUInt(buffer.AsCStr(), val);

				*this << buffer.AsCStr();

				return *this;
			}

			StringWriter& StringWriter::operator << (const short val)
			{
				String256 buffer;
				StringConvertFromShort(buffer.AsCStr(), 256, val);

				*this << buffer.AsCStr();

				return *this;
			}

			StringWriter& StringWriter::operator << (const unsigned short val)
			{
				String256 buffer;
				StringConvertFromUShort(buffer.AsCStr(), 256,val);

				*this << buffer.AsCStr();

				return *this;
			}

			StringWriter& StringWriter::operator << (const long long val)
			{
				String256 buffer;
				StringConvertFromInt64(buffer.AsCStr(), val);

				*this << buffer.AsCStr();

				return *this;
			}
				
			StringWriter& StringWriter::operator << (const unsigned long long val)
			{
				String256 buffer;
				StringConvertFromUInt64(buffer.AsCStr(), val);

				*this << buffer.AsCStr();

				return *this;
			}

			StringWriter& StringWriter::operator << (const float val)
			{
				String256 buffer;
				StringConvertFromFloat(buffer.AsCStr(), 256,val);

				*this << buffer.AsCStr();

				return *this;
			}

			StringWriter& StringWriter::operator << (const double val)
			{
				String256 buffer;
				StringConvertFromDouble(buffer.AsCStr(), 256, val);

				*this << buffer.AsCStr();

				return *this;
			}
		}
	}
}