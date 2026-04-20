#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			String<size>::String()
				: mData()
			{
				At(0) = NULL;

				DIA_ASSERT(IsNullTerminating(), "Is not null terminating");
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> template<unsigned int _size> 
			String<size>::String (const String<_size>& rhs)
				: mData(&rhs[0], Size())
			{
				mData.At(size - 1) = NULL;
				DIA_ASSERT(IsNullTerminating(), "Is not null terminating");
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> template<unsigned int _size> 
			String<size>::String (const String<_size>& rhs, unsigned int startIndex, unsigned int numberElements)
				: mData(rhs.mData, startIndex, numberElements)
			{
				unsigned int n = size-1;
				if ( numberElements < (size-1) )
				{
					n = numberElements;
				}

				At(n) = NULL;

				DIA_ASSERT(IsNullTerminating(), "Is not null terminating");
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> 
			String<size>::String (ConstIterator& iter)
				: mData()
			{
				int j = 0;
				for (; !iter.IsDone(); iter.Next()) 
				{
					At(j) = *iter.Current();
					j++;
				}
				
				if ((j + 1) >= size)
				{
					At(size - 1) = NULL;
				}
				else
				{
					At(j+1) = NULL;
				}
				DIA_ASSERT(IsNullTerminating(), "Is not null terminating");
			}
			
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			String<size>::String ( ConstReverseIterator& iter )
				: mData()
			{
				int j = 0;

				bool foundFirstNotNull = false;
				for (; !iter.IsDone(); iter.Next()) 
				{
					if (foundFirstNotNull || *iter.Current() != NULL)
					{
						foundFirstNotNull = true;
						At(j) = *iter.Current();
						j++;
					}
				}

				if ((j + 1) >= size)
				{
					mData.At(size - 1) = NULL;
				}
				else
				{
					mData.At(j+1) = NULL;
				}
		
				DIA_ASSERT(IsNullTerminating(), "Is not null terminating");
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> template<class Evaluator> inline
			String<size>::String ( ConstIterator& iter, const Evaluator& filter )
				: mData()
			{
				int j = 0;
				for (; !iter.IsDone(); iter.Next()) 
				{
					const char* temp = iter.Current();
					if (filter.Evaluate(*temp ))
					{
						At(j) = *temp;
						j++;
					}
				}
				
				if ((j + 1) >= size)
				{
					mData.At(size - 1) = NULL;
				}
				else
				{
					mData.At(j+1) = NULL;
				}
				DIA_ASSERT(IsNullTerminating(), "Is not null terminating");
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			String<size>::String (const char* pRawString, ...)
				: mData()
			{
				DIA_ASSERT(pRawString != NULL, "Invalid Data");

				va_list args;
				va_start(args, pRawString);
				Format(pRawString, args);
				va_end(args);
			}
			
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			String<size>::String(const char* pRawString, va_list argList)
			{
				Format(pRawString, argList);			
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> template<unsigned int _size> inline
			String<size>& String<size>::Append( const String<_size>& str )
			{
				return Append(&str[0], str.Length());
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> template<unsigned int _size> inline
			String<size>& String<size>::Append( const String<_size>& str, unsigned int pos, unsigned int n )
			{
				return Append(&str[pos], n);
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			String<size>& String<size>::Append( const char* s, unsigned int n )
			{
				DIA_ASSERT(Length() + n < size, "Overbouding the string array");
				strncat_s(&mData[0], mData.Size(), s, n);
				DIA_ASSERT(IsNullTerminating(), "Is not null terminating");
				return *this;
			}
			
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			String<size>& String<size>::Append( const char* s )
			{
				return Append(s, static_cast<unsigned int>(strlen(s)));
			}
			
			//-----------------------------------------------------------------------------
			template <unsigned int size> template<unsigned int _size> inline
			String<size>& String<size>::AppendAsMuchAsCan( const String<_size>& str )
			{
				return AppendAsMuchAsCan(&str[0], str.Length());
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> template<unsigned int _size> inline
			String<size>& String<size>::AppendAsMuchAsCan( const String<_size>& str, unsigned int pos, unsigned int n )
			{
				return AppendAsMuchAsCan(&str[pos], n);
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			String<size>& String<size>::AppendAsMuchAsCan( const char* s, unsigned int n )
			{
				unsigned int maxAppend = size - Length() - 1;
				
				if ( n > maxAppend)
				{
					n = maxAppend;
				}

				strncat_s(&mData[0], mData.Size(), s, n);
				DIA_ASSERT(IsNullTerminating(), "Is not null terminating");
				return *this;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			String<size>& String<size>::AppendAsMuchAsCan( const char* s )
			{
				return AppendAsMuchAsCan(s, strlen(s));
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size>
			String<size>& String<size>::operator =(const char* pRawString)
			{
				memset(mData, 0, sizeof(char)*size);
				
				unsigned int x = strlen(pRawString);
				if( size <= x )
				{
					x = size;
				}

				memcpy(mData, pRawString, sizeof(char)*x);
				
				DIA_ASSERT(IsNullTerminating(), "is not null terminating");

				return *this;
			}
			
			template <unsigned int size>
			template<unsigned int _size> String<size>& String<size>::operator =(const String<_size>& rhs)
			{
				DIA_ASSERT(rhs.IsNullTerminating(), "rhs is not null terminating");

				memset(reinterpret_cast<void*>(&mData), 0, sizeof(char)*size);

				unsigned int x = _size;
				if( size <= _size )
				{
					x = size;
				}

				memcpy(reinterpret_cast<void*>(&mData), &rhs.At(0), sizeof(char)*x);

				DIA_ASSERT(IsNullTerminating(), "is not null terminating");

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			char& String<size>::operator[](int index)
			{
				return mData[index];
			}
			
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			const char& String<size>::operator[](int index) const
			{
				return mData[index];
			}
			
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			char& String<size>::operator[](unsigned int index)
			{
				return mData[index];
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			const char& String<size>::operator[](unsigned int index) const
			{
				return mData[index];
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			String<size> String<size>::operator	+ (const char* pRawString)const
			{
				String<size> result(*this);
				result.Append(pRawString);
				return result;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> template<unsigned int _size> inline
			String<size> String<size>::operator	+ (const String<_size>& str)const
			{
				String<size> result(*this);
				result.Append(str);
				return result;
			}
				
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			const String<size>& String<size>::operator +=(const char* pRawString)
			{
				return Append(pRawString);	
			}
				
			//-----------------------------------------------------------------------------
			template <unsigned int size> template<unsigned int _size> inline
			const String<size>& String<size>::operator +=(const String<_size>& str)
			{
				return Append(str);	
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			bool String<size>::operator	==(const char* pRawString)const
			{
				DIA_ASSERT(pRawString, "String Cannot be NULL");

				return (strcmp(&mData[0], pRawString) == 0);
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			bool String<size>::operator	!=	(const char* pRawString)const
			{
				return !(*this == pRawString);
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			bool String<size>::operator	<	(const char* pRawString)const
			{
				DIA_ASSERT(pRawString, "String Cannot be NULL");
				return (strcmp(&mData[0], pRawString) < 0);
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			bool String<size>::operator	>	(const char* pRawString)const
			{
				DIA_ASSERT(pRawString, "String Cannot be NULL");
				return (strcmp(&mData[0], pRawString) > 0);
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			bool String<size>::operator	<=	(const char* pRawString)const
			{
				DIA_ASSERT(pRawString, "String Cannot be NULL");
				return (strcmp(&mData[0], pRawString) <= 0);
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			bool String<size>::operator	>=	(const char* pRawString)const
			{
				DIA_ASSERT(pRawString, "String Cannot be NULL");
				return (strcmp(&mData[0], pRawString) >= 0);
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> template<unsigned int _size> inline
			bool String<size>::operator	== (const String<_size>& str)const
			{
				return (*this == str.AsCStr());
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> template<unsigned int _size> inline
			bool String<size>::operator	!= (const String<_size>& str)const
			{
				return !(*this == str.AsCStr());
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> template<unsigned int _size> inline
			bool String<size>::operator	< (const String<_size>& str)const
			{
				return (*this < str.AsCStr());
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> template<unsigned int _size> inline
			bool String<size>::operator	> (const String<_size>& str)const
			{
				return (*this > str.AsCStr());
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> template<unsigned int _size> inline
			bool String<size>::operator	<= (const String<_size>& str)const
			{
				return (*this <= str.AsCStr());
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> template<unsigned int _size> inline
			bool String<size>::operator	>= (const String<_size>& str)const
			{
				return (*this >= str.AsCStr());
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			char& String<size>::At(unsigned int index)
			{
				return mData.At(index);
			}
			
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			const char& String<size>::At(unsigned int index) const
			{
				return mData.At(index);
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			char& String<size>::Front()
			{
				return mData.Front();
			}
			
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			const char& String<size>::Front() const
			{
				return mData.Front();
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			char& String<size>::Back()
			{
				unsigned int len = Length();
				return mData[len > 0 ? len - 1 : 0];
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			const char& String<size>::Back() const
			{
				unsigned int len = Length();
				return mData[len > 0 ? len - 1 : 0];
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			typename String<size>::Iterator String<size>::IteratorAt(unsigned int index)
			{
				DIA_ASSERT(index <= Length(), "Iterator can not be outside string length");
				
				return mData.IteratorAt(index);
			}
				
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			typename String<size>::ConstIterator String<size>::IteratorAtConst(unsigned int index) const
			{
				DIA_ASSERT(index <= Length(), "Iterator can not be outside string length");

				return mData.IteratorAtConst(index);
			}
			
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			typename String<size>::ReverseIterator String<size>::ReverseIteratorAt(unsigned int index)
			{
				DIA_ASSERT(index <= Length(), "Iterator can not be outside string length");

				return mData.ReverseIteratorAt(index);
			}
			
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			typename String<size>::ConstReverseIterator String<size>::ReverseIteratorAtConst	(unsigned int index) const
			{
				DIA_ASSERT(index <= Length(), "Iterator can not be outside string length");

				return mData.ReverseIteratorAtConst(index);
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			typename String<size>::Iterator String<size>::Begin()
			{
				return mData.Back();
			}
			
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			typename String<size>::ConstIterator String<size>::BeginConst() const
			{
				return mData.BeginConst();
			}
			
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			typename String<size>::ReverseIterator String<size>::End()
			{
				return mData[Length()];
			}
			
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			typename String<size>::ConstReverseIterator String<size>::EndConst() const
			{
				return mData[Length()];
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			unsigned int String<size>::Size	() const
			{
				return size;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			unsigned int String<size>::Length() const
			{
				return static_cast<unsigned int>(strlen(&mData[0]));
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			bool String<size>::IsNullTerminating()const
			{
				unsigned int strLength = Length();
				return At(strLength) == NULL;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			char* String<size>::AsCStr()
			{
				return (&At(0));
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			const char* String<size>::AsCStr()const
			{
				return (&At(0));
			}
			
			//-----------------------------------------------------------------------------
			/*template <unsigned int size> inline
			char String<size>::AsChar()const
			{
				return &mData[0];
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			short String<size>::AsShort()const
			{
				return static_cast<short>(strtol(mData, NULL, 0));
			}
			
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			unsigned short String<size>::AsUShort()const
			{
				return static_cast<unsigned short>(strtol(mData, NULL, 0));
			}
			s64						As64Int			()const;
			u64						AsU64Int		()const;
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			int String<size>::AsInt()const
			{
				return static_cast<int>(strtol(mData, NULL, 0));
			}
			
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			unsigned int String<size>::AsUInt()const
			{
				return static_cast<unsigned int>(strtol(mData, NULL, 0));
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			float String<size>::AsFloat()const
			{
				return static_cast<float>(strtod(mData, NULL));
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			String<size>&  String<size>::FromInt(int val)
			{
				return Format("%d", val);
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			String<size>&  String<size>::FromUInt(unsigned int val)
			{
				Format("%d", val)
				return *this
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			String<size>&  String<size>::FromFloat(float val)
			{
				Format("%0.5f", val)
				return *this
			}*/

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			String<size>& String<size>::Format(const char* pFormatString, ...)
			{
				DIA_ASSERT(pFormatString != NULL, "Invalid Data");

				va_list args;
				va_start(args, pFormatString);
				Format(pFormatString, args);
				va_end(args);
				return *this;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			String<size>& String<size>::Format(const char* pFormatString, va_list argList)
			{
				DIA_ASSERT(pFormatString != NULL, "Invalid Data");
				DIA_ASSERT(strlen(&mData[0]) < Size(), "String too large" );

				vsnprintf_s(&mData[0], Size(), Size()-1, pFormatString, argList); 
				
				DIA_ASSERT(IsNullTerminating(), "Is not null terminating");

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			String<size>& String<size>::Trim(const unsigned int index, const unsigned int charCount)
			{	
				DIA_ASSERT(charCount <= size, "trim index out of bounds");
				DIA_ASSERT(charCount <= Length(), "trim index past string length");

				String<size> temp;
				unsigned int nullIndex = 0;
				unsigned int currentIndex = index;
				for (unsigned int i=0; i < charCount; i++)
				{
					temp[i] = this->mData[currentIndex++];
					nullIndex++;
				}
				
				*this = temp;
				mData[nullIndex] = NULL;

				DIA_ASSERT(IsNullTerminating(), "Is not null terminating");

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			String<size>& String<size>::Trim(const unsigned int index)
			{
				return Trim(index, (Length() - index));
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			String<size>& String<size>::ToUpperCase()
			{
				for (unsigned int i = 0; i < static_cast<int>(Length()); i++)
				{
					if (mData[i] >= 'a' && mData[i] <= 'z')
					{
						mData[i] -= 32;
					}
				}
				
				DIA_ASSERT(IsNullTerminating(), "Is not null terminating");

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			String<size>& String<size>::ToLowerCase()
			{
				for (unsigned int i = 0; i < static_cast<int>(Length()); i++)
				{
					if (mData[i] >= 'A' && mData[i] <= 'Z')
					{
						mData[i] += 32;
					}
				}
				
				DIA_ASSERT(IsNullTerminating(), "Is not null terminating");

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			void String<size>::ToUpperCase(String<size>& result)const
			{
				result = *this;
				result.ToUpperCase();
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			void String<size>::ToLowerCase(String<size>& result)const
			{
				result = *this;
				result.ToLowerCase();
			}
			
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			String<size> String<size>::LeftSubString(const unsigned int charCount)const
			{
				DIA_ASSERT(charCount <= size, "Index out of range for static string allocation");
				DIA_ASSERT(charCount <= Length(), "Index out of range for string");
			
				String<size> temp( *this );

				unsigned int strSize = Length();

				int i = strSize;
				if( charCount <= strSize )
				{
					i = charCount;
				}

				if( charCount <= strSize )
				{
					i = charCount;
				}
				else
				{
					i = strSize;
				}

				if( i <= 0 )
				{
					i = 0;
				}

				temp.mData[i] = NULL;
				return temp;
			}
				
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			String<size> String<size>::RightSubString(const unsigned int charCount)const
			{
				DIA_ASSERT( charCount <= size, "n out of bounds" );
				DIA_ASSERT( charCount <= Length(), "n past strings length");

				String<size> temp( *this );
				temp.Trim(charCount);
				return temp;
			}
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			String<size> String<size>::SubString(const unsigned int beginIndex, const unsigned int charCount)const
			{
				DIA_ASSERT(beginIndex < size, "Index out of bounds");
				DIA_ASSERT(charCount < size, "length of mid string past bounds");
				DIA_ASSERT(charCount <= Length(), "length of mid string past bounds");
				DIA_ASSERT(beginIndex + charCount <= Length(), "bad indexes into this string");

				String<size> temp( *this );
				temp.Trim(beginIndex, charCount);
				return temp;
			}
			
			//-----------------------------------------------------------------------------
			template <unsigned int size>
			template<unsigned int size1, unsigned int size2> inline
			void String<size>::Split(const unsigned int charCount, String<size1>& result1, String<size2>& result2)const
			{
				DIA_ASSERT(charCount < size, "Index out of bounds");

				result1 = this->SubString(0, charCount);
				result2 = this->SubString(charCount, Length()-charCount);
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size>
			template<typename ArrayType> inline
			void String<size>::Split(char delimiter, ArrayType& outTokens) const
			{
				outTokens.RemoveAll();

				if (Length() == 0)
				{
					return;
				}

				unsigned int start = 0;
				unsigned int i = 0;

				for (i = 0; i < Length(); i++)
				{
					if (mData[i] == delimiter)
					{
						// Found delimiter - extract token
						if (i > start)
						{
							String<size> token = SubString(start, i - start);
							// Convert to target type - assume ArrayType element has constructor from const char*
							typename ArrayType::ValueType targetToken(token.AsCStr());
							outTokens.Add(targetToken);
						}
						start = i + 1;
					}
				}

				// Add last token if any
				if (start < Length())
				{
					String<size> token = SubString(start, Length() - start);
					typename ArrayType::ValueType targetToken(token.AsCStr());
					outTokens.Add(targetToken);
				}
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			bool String<size>::CompareNoCase(const String<size>& str)const
			{
				return (_strnicmp (&mData[0], &str.mData[0], Length()) == 0); 
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			bool String<size>::CompareNoCase(const char* pRawString)const
			{
				return (_strnicmp (&mData[0], pRawString, Length()) == 0); 
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			int String<size>::Find( char c, unsigned int startPos ) const
			{
				DIA_ASSERT(startPos < Length(), "Startpos outbounds string");

				int length = static_cast<int>(Length());
				for(int i = startPos; i < length; i++)
				{
					if (mData[i] == c)
					{
						return i;
					}
				}

				return -1;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			int String<size>::FindLast	( char c, unsigned int startPos ) const
			{
				if (startPos == 0xffffffff)
				{
					startPos = Length() - 1;
				}
				
				DIA_ASSERT(startPos < Length(), "end pos outbounds string");

				for(int i = startPos; i >= 0; i--)
				{
					if (mData[i] == c)
					{
						return i;
					}
				}

				return -1;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size>
			String<size>& String<size>::Append( char c )
			{
				char temp[2] = {c, '\0'};
				return Append(temp);
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size>
			void String<size>::Clear()
			{
				mData[0] = '\0';
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size>
			bool String<size>::IsEmpty() const
			{
				return Length() == 0;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size>
			String<size>& String<size>::Insert( unsigned int pos, const char* s )
			{
				if (!s || pos > Length()) return *this;

				unsigned int sLen = static_cast<unsigned int>(strlen(s));
				unsigned int currentLen = Length();
				unsigned int newLen = currentLen + sLen;

				if (newLen >= size) return *this;  // Would overflow

				// Shift existing characters right
				for (int i = currentLen; i >= static_cast<int>(pos); i--)
				{
					mData[i + sLen] = mData[i];
				}

				// Insert new characters
				for (unsigned int i = 0; i < sLen; i++)
				{
					mData[pos + i] = s[i];
				}

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size>
			String<size>& String<size>::Remove( unsigned int pos, unsigned int count )
			{
				unsigned int currentLen = Length();
				if (pos >= currentLen) return *this;

				if (pos + count > currentLen)
				{
					count = currentLen - pos;
				}

				// Shift characters left
				for (unsigned int i = pos; i < currentLen - count; i++)
				{
					mData[i] = mData[i + count];
				}
				mData[currentLen - count] = '\0';

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size>
			String<size>& String<size>::Replace( const char* oldStr, const char* newStr )
			{
				if (!oldStr || !newStr) return *this;

				unsigned int oldLen = static_cast<unsigned int>(strlen(oldStr));
				unsigned int newLen = static_cast<unsigned int>(strlen(newStr));

				// Replace all occurrences
				int pos = Find(oldStr);
				while (pos != -1)
				{
					// Remove old string
					Remove(pos, oldLen);

					// Insert new string
					Insert(pos, newStr);

					// Find next occurrence, starting after the replacement
					pos = Find(oldStr, pos + newLen);
				}

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size>
			String<size>& String<size>::Trim()
			{
				TrimLeft();
				TrimRight();
				return *this;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size>
			String<size>& String<size>::TrimLeft()
			{
				unsigned int i = 0;
				while (i < Length() && isspace(static_cast<unsigned char>(mData[i])))
				{
					i++;
				}

				if (i > 0)
				{
					Remove(0, i);
				}

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size>
			String<size>& String<size>::TrimRight()
			{
				int i = Length() - 1;
				while (i >= 0 && isspace(static_cast<unsigned char>(mData[i])))
				{
					i--;
				}

				mData[i + 1] = '\0';

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size>
			bool String<size>::StartsWith( const char* prefix ) const
			{
				if (!prefix) return false;

				unsigned int prefixLen = static_cast<unsigned int>(strlen(prefix));
				if (prefixLen > Length()) return false;

				return strncmp(AsCStr(), prefix, prefixLen) == 0;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size>
			bool String<size>::EndsWith( const char* suffix ) const
			{
				if (!suffix) return false;

				unsigned int suffixLen = static_cast<unsigned int>(strlen(suffix));
				unsigned int strLen = Length();

				if (suffixLen > strLen) return false;

				return strcmp(AsCStr() + strLen - suffixLen, suffix) == 0;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size>
			bool String<size>::Contains( const char* substr ) const
			{
				return Find(substr) != -1;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size>
			String<size>& String<size>::Reverse()
			{
				unsigned int len = Length();
				for (unsigned int i = 0; i < len / 2; i++)
				{
					char temp = mData[i];
					mData[i] = mData[len - 1 - i];
					mData[len - 1 - i] = temp;
				}
				return *this;
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size>
			int String<size>::Find( const char* s, unsigned int startPos ) const
			{
				if (!s || startPos >= Length()) return -1;

				const char* result = strstr(AsCStr() + startPos, s);
				if (!result) return -1;

				return static_cast<int>(result - AsCStr());
			}

			//-----------------------------------------------------------------------------
			template <unsigned int size>
			int String<size>::FindLast( const char* s, unsigned int startPos ) const
			{
				if (!s) return -1;

				unsigned int sLen = static_cast<unsigned int>(strlen(s));
				if (sLen == 0) return -1;

				if (startPos == 0xffffffff)
				{
					startPos = Length() - 1;
				}

				if (startPos >= Length()) return -1;

				// Search backwards
				for (int i = startPos; i >= 0; i--)
				{
					if (strncmp(AsCStr() + i, s, sLen) == 0)
					{
						return i;
					}
				}

				return -1;
			}
		}
	}
}