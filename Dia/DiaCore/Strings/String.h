#ifndef STRING
#define STRING

#include <string.h>

#include "DiaCore/Containers/Arrays/ArrayC.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			template <unsigned int size>
			class String
			{
			public:
				typedef	ArrayIterator<char>					Iterator;
				typedef	ArrayConstIterator<char>			ConstIterator;
				typedef	ReverseArrayIterator<char>			ReverseIterator;
				typedef	ReverseArrayConstIterator<char>		ConstReverseIterator;

				String();																															
				template<unsigned int _size> explicit String (const String<_size>& rhs);						
				template<unsigned int _size> explicit String (const String<_size>& rhs, unsigned int startIndex, unsigned int numberElements );  
				explicit String ( ConstIterator& iter ); 
				explicit String ( ConstReverseIterator& iter ); 
				template<class Evaluator> explicit String ( ConstIterator& iter, const Evaluator& filter);

				explicit String(const char* pRawString, ...);
				explicit String(const char* pRawString, va_list argList);

				template<unsigned int _size> String<size>&			Append					( const String<_size>& str );
				template<unsigned int _size> String<size>&			Append					( const String<_size>& str, unsigned int pos, unsigned int n );
				template<unsigned int _size> String<size>&			AppendAsMuchAsCan		( const String<_size>& str );
				template<unsigned int _size> String<size>&			AppendAsMuchAsCan		( const String<_size>& str, unsigned int pos, unsigned int n );
		
				String<size>&			Append					( const char* s, unsigned int n );
				String<size>&			Append					( const char* s );
				String<size>&			AppendAsMuchAsCan		( const char* s, unsigned int n );
 				String<size>&			AppendAsMuchAsCan		( const char* s );
	 		
				String<size>&									operator	=	(const char* pRawString);
				template<unsigned int _size> String<size>&		operator	=	(const String<_size>& other);

				String<size>								operator	+	(const char* pRawString)const;
				template<unsigned int _size> String<size>	operator	+	(const String<_size>& str)const;
				
				const String<size>&									operator	+=	(const char* pRawString);
				template<unsigned int _size> const String<size>&	operator	+=	(const String<_size>& str);
				
				bool					operator	==	(const char* pRawString)const;
				bool					operator	!=	(const char* pRawString)const;
				
				template<unsigned int _size> bool	operator	==	(const String<_size>& str)const;
				template<unsigned int _size> bool	operator	!=	(const String<_size>& str)const;

				char&									operator[]		(int index);										
				const char&								operator[]		(int index) const;	
				char&									operator[]		(unsigned int index);										
				const char&								operator[]		(unsigned int index) const;									

				char&									At				(unsigned int index);											
				const char&								At				(unsigned int index) const;
				char&									Front			();	
				const char&								Front			() const;		
				char&									Back			();
				const char&								Back			() const;

				Iterator								IteratorAt				(unsigned int index);											
				ConstIterator							IteratorAtConst			(unsigned int index) const;
				ReverseIterator							ReverseIteratorAt		(unsigned int index);											
				ConstReverseIterator					ReverseIteratorAtConst	(unsigned int index) const;

				Iterator								Begin					();														
				ConstIterator							BeginConst				() const;												
				ReverseIterator							End						();														
				ConstReverseIterator					EndConst				() const;

				static unsigned int		Capacity		()  { return size; }
				unsigned int			Size			() const;
				unsigned int			Length			() const;
				bool					IsNullTerminating()const;

				char*					AsCStr			();
				const char*				AsCStr			()const;
				
				/*char					AsChar			()const;
				short					AsShort			()const;
				unsigned short			AsUShort		()const;
				int						AsInt			()const;
				unsigned int			AsUInt			()const;
				float					AsFloat			()const;
				s64						As64Int			()const;
				u64						AsU64Int		()const;
				
				String<size>&			FromChar		(char val);
				String<size>&			FromShort		(short val);
				String<size>&			FromUShort		(unsigned short val);
				String<size>&			FromInt			(int val);
				String<size>&			FromUInt		(unsigned int val);
				String<size>&			FromFloat		(float val);
				String<size>&			FromInt64		(s64 val);
				String<size>&			FromUInt64		(u64 val);*/

				String<size>&			Format			(const char* format, ...);
				String<size>&			Format			(const char* format, va_list argList);
				String<size>&			Trim			(const unsigned int index, const unsigned int charCount);
				String<size>&			Trim			(const unsigned int index);

				String<size>&			ToUpperCase		();
				String<size>&			ToLowerCase		();

				void					ToUpperCase		(String<size>& result)const;
				void					ToLowerCase		(String<size>& result)const;
				
				String<size>			SubString		(const unsigned int beginIndex, const unsigned int charCount)const;
				String<size>			LeftSubString	(const unsigned int charCount)const;
				String<size>			RightSubString	(const unsigned int charCount)const;		
				
				template<unsigned int size1, unsigned int size2> 
				void					Split			(const unsigned int charCount, String<size1>& result1, String<size2>& result2)const;

				bool					CompareNoCase	(const String<size>& str) const;
				bool					CompareNoCase	(const char* pRawString)const;

				int						Find			( char s, unsigned int startPos = 0 ) const;
				int						FindLast		( char s, unsigned int startPos = 0xffffffff ) const;

			protected:
				ArrayC<char, size> mData;
			};
		}
	}
}

#include "DiaCore/Strings/String.inl"

#endif