#ifndef DIA_STRING_READER__
#define DIA_STRING_READER__

#include "DiaCore/Type/BasicTypeDefines.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			class StringReader
			{
			public:
				StringReader();
				StringReader( StringReader& rhs );
				StringReader( const char* buffer );

				void			AssignBuffer		( const char* buffer );
				void			ClearBuffer			();

				const char*		AsCStr				() const;
				
				unsigned int	Length				() const;
				unsigned int	CurrentLength		() const;
				
				bool			IsInBounds			()const;
 				char			GetCurrent			()const;
				const char*		GetCurrentPtr		()const;
 				void			Advance				(unsigned int step = 1);
				void			Rewind				(unsigned int step = 1);
				void			JumpToStart			();
				void			JumpToEnd			();
				
			private:
				const char* mCurrentBufferPtr;
				const char* mStartBufferPtr;
				const char* mEndBufferPtr;
			};
		}
	}
} 
#endif 

