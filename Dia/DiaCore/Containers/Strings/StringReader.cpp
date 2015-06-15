#include "DiaCore/Containers/Strings/StringReader.h"

#include "DiaCore/Strings/stringutils.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			StringReader::StringReader()
				: mCurrentBufferPtr(NULL)
				, mStartBufferPtr(NULL)
				, mEndBufferPtr(NULL)
			{}

			StringReader::StringReader(StringReader& rhs)
				: mCurrentBufferPtr(rhs.mCurrentBufferPtr)
				, mStartBufferPtr(rhs.mStartBufferPtr)
				, mEndBufferPtr(rhs.mEndBufferPtr)
			{}

			StringReader::StringReader( const char* buffer )
				: mCurrentBufferPtr(buffer)
				, mStartBufferPtr(buffer)
				, mEndBufferPtr(buffer + (sizeof(char) * StringLength(mStartBufferPtr)))
			{
				DIA_ASSERT(StringLength(mStartBufferPtr) > 0, "Buffer needs to larger then  zero");
			}

			void StringReader::AssignBuffer( const char* buffer )
			{
				unsigned int strLength = StringLength(mStartBufferPtr);
				DIA_ASSERT(mStartBufferPtr == NULL, "Buffer already been allocated");
				DIA_ASSERT(strLength > 0, "Buffer needs to larger then  zero");

				mCurrentBufferPtr = buffer;
				mStartBufferPtr = buffer;
				mEndBufferPtr = buffer + (sizeof(char) * strLength);
			}		

			const char* StringReader::AsCStr() const
			{
				DIA_ASSERT(mStartBufferPtr != NULL, "Not been assigned buffer");
				return mStartBufferPtr;
			}

			unsigned int StringReader::Length() const
			{
				DIA_ASSERT(mStartBufferPtr != NULL, "Not been assigned buffer");
				return (mEndBufferPtr - mStartBufferPtr);
			}

			unsigned int StringReader::CurrentLength() const
			{
				DIA_ASSERT(mStartBufferPtr != NULL, "Not been assigned buffer");
				return StringLength(mStartBufferPtr);
			}
			
			bool StringReader::IsInBounds()const
			{	
				return (mCurrentBufferPtr >= mStartBufferPtr) && (mCurrentBufferPtr <= mEndBufferPtr);
			}

 			char StringReader::GetCurrent()const
 			{
				DIA_ASSERT(mStartBufferPtr != NULL, "Not been assigned buffer");
				DIA_ASSERT(IsInBounds(), "Out of bounds");

				return *mCurrentBufferPtr;
 			}

			const char*	StringReader::GetCurrentPtr ()const
			{
				DIA_ASSERT(mStartBufferPtr != NULL, "Not been assigned buffer");
				DIA_ASSERT(IsInBounds(), "Out of bounds");

				return mCurrentBufferPtr;
			}

			void StringReader::Advance(unsigned int step)
			{
				for (unsigned int i = 0; i < step; i++)
				{
					mCurrentBufferPtr++;

					DIA_ASSERT(IsInBounds(), "Out of bounds");
				}
			}

			void StringReader::Rewind(unsigned int step)
			{
				for (unsigned int i = 0; i < step; i++)
				{
					mCurrentBufferPtr--;

					DIA_ASSERT(IsInBounds(), "Out of bounds");
				}
			}

			void StringReader::JumpToStart()
			{
				mCurrentBufferPtr = mStartBufferPtr;
			}

			void StringReader::JumpToEnd()
			{
				mCurrentBufferPtr = mEndBufferPtr;
			}
		}
	}
}