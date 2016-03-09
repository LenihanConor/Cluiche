#include <stdio.h>
#include <stdarg.h>

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//------------------------------------------------------------------------------------
			//	Implementation
			//------------------------------------------------------------------------------------
			template <unsigned int size> inline
			void String<size>::DeserializeIntenal(Dia::Core::Types::TypeInstance& instance, const Dia::Core::Types::TypeVariable& currentTypeVariable, const Json::Value& jsonData, Dia::Core::Types::TypeJsonSerializerExternalDeserializeInterface& parent)
			{
				std::string str = jsonData.asString();

				DIA_ASSERT(str.length() <= Capacity(), "String being read in will not fit in size of string being allocated");

				char* pointerToDst = reinterpret_cast<char*>(instance.Pointee());
				Dia::Core::StringCopy(pointerToDst, &str[0], str.length());
			}

			//------------------------------------------------------------------------------------
			template <unsigned int size> inline
			void String<size>::SerializeInternal(const Dia::Core::Types::TypeInstance& instance, const Dia::Core::Types::TypeVariable& currentTypeVariable, Json::Value& jsonData, Dia::Core::Types::TypeJsonSerializerExternalSerializeInterface& parent)
			{
				unsigned int size = currentTypeVariable.GetNumberOfElements();
				for (unsigned int i = 0; i < size; i++)
				{
					std::string str;

					// Set the meta data
					const char* name = currentTypeVariable.GetName();
					const char* className = currentTypeVariable.GetClassDefinition()->GetName();
		
					// Pull out the string
					DIA_ASSERT(currentTypeVariable.IsClassType(), "The string serializer did not find the expected class definition");

					const Dia::Core::Types::TypeDefinition::VariableLinkListNode* variable = currentTypeVariable.GetClassDefinition()->GetVariables().HeadConst();

					DIA_ASSERT(variable, "A variable type must be defined");

					const Dia::Core::Types::TypeVariable& typeVariable = *variable->GetPayloadConst();

					for (unsigned int j = 0; j < Capacity(); j++)
					{
						char temp = typeVariable.GetArithmeticValue<char>(instance, j);
						str.push_back(temp);

						if (temp == '\0')
						{
							break;
						}
					}

					str.shrink_to_fit();

					//Commit to the jsondata function
					if (currentTypeVariable.GetNumberOfElements() > 1)
						jsonData[name][i] = str;
					else
						jsonData[name] = str;
				}
			}
			
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
				return Append(s, strlen(s));
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
				String<size> result(pRawString);
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
				return mData[Length()];
			}
			
			//-----------------------------------------------------------------------------
			template <unsigned int size> inline
			const char& String<size>::Back() const
			{
				return mData[Length()];
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
				return strlen(&mData[0]);
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
		}
	}
}