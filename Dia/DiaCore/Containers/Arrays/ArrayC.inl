#include "DiaCore/Type/TypeDefinitionMacros.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			static char* ArrayC_NameBuilder(const Dia::Core::Types::TypeParameterInput& input, unsigned int size, std::string& out)
			{
				const Dia::Core::Types::TypeDefinition::VariableLinkListNode* variable = input.GetVariables().HeadConst();
				
				DIA_ASSERT(variable, "A variable type must be defined");
				const Dia::Core::Types::TypeVariable& typeVariable = *variable->GetPayloadConst();

				std::snprintf(const_cast<char*>(out.c_str()), out.capacity(), "ArrayC<%s, %u>", typeVariable.GetTypeAsString(), size);

				return const_cast<char*>(out.c_str());
			}

			template <class T, unsigned int size> Dia::Core::Types::TypeDefinition* ArrayC<T, size>::sType;
			template <class T, unsigned int size> Dia::Core::Types::TypeDefinition& ArrayC<T, size>::GetType() { if (sType == nullptr) { const Dia::Core::Types::TypeParameterInput& input = ArrayC<T, size>::TypeCreationalInput(); std::string name; name.reserve(128);  sType = DIA_NEW(Dia::Core::Types::TypeDefinition(ArrayC_NameBuilder(input, size, name), sizeof(ArrayC<T, size>), __is_polymorphic(ArrayC<T, size>), input)); } return *sType; }\
			template <class T, unsigned int size> Dia::Core::Types::TypeInstance ArrayC<T, size>::CreateTypeInstance() { return (Dia::Core::Types::TypeInstance(ArrayC<T, size>::GetType(), this)); }\
			template <class T, unsigned int size> Dia::Core::Types::TypeInstance ArrayC<T, size>::CreateTypeInstanceConst()const { return (Dia::Core::Types::TypeInstance(ArrayC<T, size>::GetType(), this)); }\
			template <class T, unsigned int size> Dia::Core::Types::TypeParameterInput& ArrayC<T, size>::TypeCreationalInput()\
			{ \
				typedef ArrayC<T, size> MyType; \
				static MyType foo; \
				static Dia::Core::Types::TypeParameterInput typeInput; \
				Dia::Core::Types::TypeVariable* lastVariable = NULL; \
				DIA_TYPE_ADD_VARIABLE_ARRAY("mData", mData, size)
				return typeInput; \
			}\

			//------------------------------------------------------------------------------------
			//	Implementation
			//------------------------------------------------------------------------------------
			template <class T, unsigned int size> 
			ArrayC<T, size>::ArrayC()
			{
				memset(mData, 0, sizeof(T)*size);
			}
		
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size>
			ArrayC<T, size>::ArrayC (ConstPointer pData, unsigned int numberElements )
			{
				DIA_ASSERT(pData != NULL, "Invalid Data");
				DIA_ASSERT(numberElements <= Size(), "Will Outbound array");

				memset(mData, 0, sizeof(T)*size);
				memcpy(mData, pData, sizeof(T)*numberElements);
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> 
			ArrayC<T, size>::ArrayC (ConstReference data, unsigned int numberElements)
			{
				DIA_ASSERT(numberElements <= Size(), "Will Outbound array");

				memset(mData, 0, sizeof(T)*size);
				memcpy(mData, &data, sizeof(T)*numberElements);
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size>
			template<unsigned int _size> ArrayC<T, size>::ArrayC(const ArrayC<T,_size>& rhs)					
			{
				memset(mData, 0, sizeof(T)*size);

				unsigned int x = _size;
				if( size <= _size )
				{
					x = size;
				}

				memcpy(mData, &rhs.At(0), sizeof(T)*x);
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size>
			template<unsigned int _size> ArrayC<T, size>::ArrayC(const ArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)
			{
				DIA_ASSERT(numberElements >= 1, "You must want to get more then one element"); 
				DIA_ASSERT(numberElements <= Size(), "Will Outbound array"); 
				DIA_ASSERT(startIndex + numberElements <= rhs.Size(), "Will Outbound array"); 
				DIA_ASSERT(startIndex < rhs.Size(), "Start index must be greater then end index"); 
				
				memset(mData, 0, sizeof(T)*size);
				memcpy(mData, &rhs.At(startIndex), sizeof(T)*numberElements);
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size>
			ArrayC<T, size>::ArrayC ( ConstIterator& iter )
			{
				memset(mData, 0, sizeof(T)*size);

				int j = 0;
				for (; !iter.IsDone(); iter.Next()) 
				{
					At(j) = *iter.Current();
					j++;
				}
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size>
			ArrayC<T, size>::ArrayC ( ConstReverseIterator& iter )
			{
				memset(mData, 0, sizeof(T)*size);

				int j = 0;
				for (; !iter.IsDone(); iter.Next()) 
				{
					At(j) = *iter.Current();
					j++;
				}
			}
				
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> 
			template<class Evaluator> ArrayC<T, size>::ArrayC ( ConstIterator& iter, const Evaluator& filter)
			{
				memset(mData, 0, sizeof(T)*size);

				int j = 0;
				for (; !iter.IsDone(); iter.Next()) 
				{
					const T* temp = iter.Current();
					if (filter.Evaluate(*temp ))
					{
						At(j) = *temp;
						j++;
					}
				}
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size>
			ArrayC<T, size>& ArrayC<T, size>::Assign ( ConstPointer pData, unsigned int numberElements )
			{
				DIA_ASSERT(pData != NULL, "Invalid Data");
				DIA_ASSERT(numberElements <= Size(), "Will Outbound array");

				memset(mData, 0, sizeof(T)*size);
				memcpy(mData, pData, sizeof(T)*numberElements);

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size>
			ArrayC<T, size>& ArrayC<T, size>::Assign ( ConstReference data, unsigned int numberElements )
			{
				DIA_ASSERT(numberElements <= Size(), "Will Outbound array");

				memset(mData, 0, sizeof(T)*size);
				memcpy(mData, &data, sizeof(T)*numberElements);

				return *this;
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size>
			template<unsigned int _size> ArrayC<T, size>& ArrayC<T, size>::Assign ( const ArrayC<T,_size>& rhs )
			{
				memset(mData, 0, sizeof(T)*size);

				unsigned int x = _size;
				if( size <= _size )
				{
					x = size;
				}

				memcpy(mData, &rhs.At(0), sizeof(T)*x);

				return *this;
			}
				
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size>
			template<unsigned int _size> ArrayC<T, size>& ArrayC<T, size>::Assign ( const ArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements )
			{
				DIA_ASSERT(numberElements >= 1, "You must want to get more then one element"); 
				DIA_ASSERT(numberElements <= Size(), "Will Outbound array"); 
				DIA_ASSERT(startIndex + numberElements <= rhs.Size(), "Will Outbound array"); 
				DIA_ASSERT(startIndex < rhs.Size(), "Start index must be greater then end index"); 

				memset(mData, 0, sizeof(T)*size);
				memcpy(mData, &rhs.At(startIndex), sizeof(T)*numberElements);

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size>
			ArrayC<T, size>& ArrayC<T, size>::Assign ( ConstIterator& iter )
			{
				memset(mData, 0, sizeof(T)*size);

				int j = 0;
				for (; !iter.IsDone(); iter.Next()) 
				{
					At(j) = *iter.Current();
					j++;
				}

				return *this;
			}
				
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size>
			ArrayC<T, size>& ArrayC<T, size>::Assign ( ConstReverseIterator& iter )
			{
				memset(mData, 0, sizeof(T)*size);

				int j = 0;
				for (; !iter.IsDone(); iter.Next()) 
				{
					At(j) = *iter.Current();
					j++;
				}

				return *this;
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size>
			template<class Evaluator> ArrayC<T, size>& ArrayC<T, size>::Assign ( ConstIterator& iter, const Evaluator& filter )
			{
				memset(mData, 0, sizeof(T)*size);

				int j = 0;
				for (; !iter.IsDone(); iter.Next()) 
				{
					const T* temp = iter.Current();
					if (filter.Evaluate(*temp ))
					{
						At(j) = *temp;
						j++;
					}
				}

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size>
			template<unsigned int _size> ArrayC<T, size>& ArrayC<T, size>::operator=(const ArrayC<T,_size>& rhs)
			{
				memset(mData, 0, sizeof(T)*size);

				unsigned int x = _size;
				if( size <= _size )
				{
					x = size;
				}

				memcpy(mData, &rhs.At(0), sizeof(T)*x);

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size>
			bool ArrayC<T, size>::operator==( const ArrayC<T, size>& rhs) const
			{
				for(unsigned int i = 0; i < static_cast<int>(Size()); i++)
				{
					if (rhs[i] != At(i))
					{
						return false;
					}
				}
				return true;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size>
			bool ArrayC<T, size>::operator!=( const ArrayC<T, size>& rhs ) const
			{
				return !(*this == rhs);
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			unsigned int ArrayC<T, size>::Size() const
			{
				return size;
			}

			//-----------------------------------------------------------------------------
 			template <class T, unsigned int size> inline
			typename ArrayC<T, size>::Reference ArrayC<T, size>::operator[](unsigned int index)
			{
				DIA_ASSERT(index < size, "Index out of range");
				return (mData[index]);
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			typename ArrayC<T, size>::ConstReference ArrayC<T, size>::operator[](unsigned int index) const
			{
				DIA_ASSERT(index < size, "Index out of range");
				return (mData[index]);
			}
			
			//-----------------------------------------------------------------------------
 			template <class T, unsigned int size> inline
			typename ArrayC<T, size>::Reference ArrayC<T, size>::operator[](int index)
			{
				DIA_ASSERT(index >= 0 && index < size, "Index out of range");
				return (mData[index]);
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			typename ArrayC<T, size>::ConstReference ArrayC<T, size>::operator[](int index) const
			{
				DIA_ASSERT(index >= 0 && index < size, "Index out of range");
				return (mData[index]);
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline	
			typename ArrayC<T, size>::Reference ArrayC<T, size>::At(unsigned int index)
			{
				DIA_ASSERT(index < size, "Index out of range");
				return (mData[index]);
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			typename ArrayC<T, size>::ConstReference ArrayC<T, size>::At(unsigned int index) const
			{
				DIA_ASSERT(index < size, "Index out of range");
				return (mData[index]);
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			typename ArrayC<T, size>::Reference ArrayC<T, size>::Front()
			{
				return *mData;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			typename ArrayC<T, size>::ConstReference ArrayC<T, size>::Front() const
			{
				return *mData;
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			typename ArrayC<T, size>::Reference ArrayC<T, size>::Back()
			{
				return *(mData + size - 1);
			}

			//-----------------------------------------------------------------------------	
			template <class T, unsigned int size> inline
			typename ArrayC<T, size>::ConstReference ArrayC<T, size>::Back() const
			{
				return *(mData + size - 1);
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			typename ArrayC<T, size>::Iterator ArrayC<T, size>::IteratorAt(unsigned int index)
			{
				return Iterator(&At(index), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			typename ArrayC<T, size>::ConstIterator ArrayC<T, size>::IteratorAtConst(unsigned int index) const
			{
				return ConstIterator(&At(index), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			typename ArrayC<T, size>::ReverseIterator ArrayC<T, size>::ReverseIteratorAt (unsigned int index)
			{
				return ReverseIterator(&At(index), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			typename ArrayC<T, size>::ConstReverseIterator ArrayC<T, size>::ReverseIteratorAtConst (unsigned int index) const
			{
				return ConstReverseIterator(&At(index), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			typename ArrayC<T, size>::Iterator ArrayC<T, size>::Begin()
			{
				return Iterator(&Front(), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			typename ArrayC<T, size>::ConstIterator ArrayC<T, size>::BeginConst()const
			{
				return ConstIterator(&Front(), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			typename ArrayC<T, size>::ReverseIterator ArrayC<T, size>::End()
			{
				return ReverseIterator(&Back(), &Front(), &Back());
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			typename ArrayC<T, size>::ConstReverseIterator ArrayC<T, size>::EndConst()const
			{
				return ConstReverseIterator(&Back(), &Front(), &Back());
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			int	ArrayC<T, size>::FrequencyOfElement( ConstReference value )const
			{
				int frequency = 0;
				for (unsigned int i = 0; i < Size(); i++)
				{
					if (value == At(i))
					{
						frequency++;
					}
				}

				return frequency;
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			void ArrayC<T, size>::UniqueElements(ArrayC<T, size>& unique, unsigned int& number)const
			{
				number = 0;
				for (unsigned int i = 0; i < Size(); i++)
				{
					T possibleUnique = At(i);
					bool foundInList = false;
					for (unsigned int j = 0; j < number; j++)
					{
						if (possibleUnique == unique[j])
						{
							foundInList = true; 
							break;
						}
					}

					if (!foundInList)
					{
						unique[number] = possibleUnique;
						number++;
					}
				}
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			void ArrayC<T, size>::FrequencyUniqueElements(ArrayC<T, size>& unique, ArrayC<int, size>& uniqueFrequency, unsigned int& number)const
			{
				number = 0;
				for (unsigned int i = 0; i < Size(); i++)
				{
					T possibleUnique = At(i);
					bool foundInList = false;
					for (unsigned int j = 0; j < number; j++)
					{
						if (possibleUnique == unique[j])
						{
							foundInList = true; 
							break;
						}
					}

					if (!foundInList)
					{
						unique[number] = possibleUnique;
						uniqueFrequency[number] = FrequencyOfElement( possibleUnique );
						number++;
					}
				}
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			void ArrayC<T, size>::Swap(ArrayC<T,size>& other)
			{
				ArrayC<T, size> t = other;
				other.Assign(*this);
				Assign(t);
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			bool ArrayC<T, size>::IsSorted()const
			{
				for(unsigned int i = 0; i < Size() - 1; i++)
				{
					if (At(i) > At(i+1))
					{
						return false;
					}
				}
				return true;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> template<class Equality> inline
			bool ArrayC<T, size>::IsSorted(const Equality& functor)const
			{
				for(unsigned int i = 0; i < Size() - 1; i++)
				{
					if (functor.GreaterThen(At(i), At(i+1)))
					{
						return false;
					}
				}
				return true;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			void ArrayC<T, size>::Sort()
			{
				PrivateSort( *this, 0, size - 1 );
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> template<class Equality> inline
			void ArrayC<T, size>::Sort(const Equality& functor)
			{
				PrivateSort( functor, *this, 0, size - 1 );
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			int	ArrayC<T, size>::FindIndex(ConstReference value) const
			{
				return FindBetweenIndex(value, 0, Size() - 1);
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> template<class Comparisson> inline
			int	ArrayC<T, size>::FindIndex(ConstReference value, const Comparisson& functor) const
			{
				return FindBetweenIndex(value, functor, 0, Size() - 1);
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			int	ArrayC<T, size>::FindBetweenIndex(ConstReference value, int startIndex, int endIndex)const
			{
				DIA_ASSERT(startIndex >= 0, "Start index must greater the zero");
				DIA_ASSERT(startIndex <= endIndex, "Start index must be smaller then end index");
				DIA_ASSERT(endIndex > 0, "End index out of range");
				DIA_ASSERT(static_cast<unsigned int>(endIndex) < Size(), "End index out of range");

				int index = -1;
				for (int i = startIndex; i <= endIndex; i++)
				{
					if (At(i) == value) 
					{
						index = i;
						break;
					}
				}
				return index;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> template<class Comparisson> inline
			int	ArrayC<T, size>::FindBetweenIndex(ConstReference value, const Comparisson& functor, int startIndex, int endIndex)const
			{
				DIA_ASSERT(startIndex >= 0, "Start index must greater the zero");
				DIA_ASSERT(startIndex <= endIndex, "Start index must be smaller then end index");
				DIA_ASSERT(endIndex > 0, "End index out of range");
				DIA_ASSERT(static_cast<unsigned int>(endIndex) < Size(), "End index out of range");

				int index = -1;
				for (int i = startIndex; i <= endIndex; i++)
				{
					if (functor.Equal(At(i), value)) 
					{
						index = i;
						break;
					}
				}
				return index;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			int	ArrayC<T, size>::FindLastIndex(ConstReference value) const
			{
				return FindLastBetweenIndex(value, 0, Size() - 1);
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> template<class Comparisson> inline
			int	ArrayC<T, size>::FindLastIndex(ConstReference value, const Comparisson& functor) const
			{
				return FindLastBetweenIndex(value, functor, 0, Size() - 1);
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			int	ArrayC<T, size>::FindLastBetweenIndex(ConstReference value, int startIndex, int endIndex)const
			{
				DIA_ASSERT(startIndex >= 0, "Start index must greater the zero");
				DIA_ASSERT(startIndex <= endIndex, "Start index must be smaller then end index");
				DIA_ASSERT(endIndex > 0, "End index out of range");
				DIA_ASSERT(static_cast<unsigned int>(endIndex) < Size(), "End index out of range");

				int index = -1;
				for (int i = endIndex; i >= startIndex; i--)
				{
					if (At(i) == value) 
					{
						index = i;
						break;
					}
				}
				return index;
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> template<class Comparisson> inline
			int	ArrayC<T, size>::FindLastBetweenIndex(ConstReference value, const Comparisson& functor, int startIndex, int endIndex)const
			{
				DIA_ASSERT(startIndex >= 0, "Start index must greater the zero");
				DIA_ASSERT(startIndex <= endIndex, "Start index must be smaller then end index");
				DIA_ASSERT(endIndex > 0, "End index out of range");
				DIA_ASSERT(static_cast<unsigned int>(endIndex) < Size(), "End index out of range");

				int index = -1;
				for (int i = endIndex; i >= startIndex; i--)
				{
					if (functor.Equal(At(i), value)) 
					{
						index = i;
						break;
					}
				}
				return index;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			int	ArrayC<T, size>::FindSortedIndex(ConstReference value) const
			{
				int sortIndex = PrivateFindSortedIndex(value);

				while (sortIndex > 0)
				{
					if (At(sortIndex - 1) == value)
					{
						sortIndex--;
					}
					else
					{
						break;
					}
				}

				return sortIndex;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> template<class Equality> inline
			int	ArrayC<T, size>::FindSortedIndex(ConstReference value, const Equality& functor) const
			{
				int sortIndex = PrivateFindSortedIndex(value, functor);

				while (sortIndex > 0)
				{
					if (At(sortIndex - 1) == value)
					{
						sortIndex--;
					}
					else
					{
						break;
					}
				}		

				return sortIndex;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			int	ArrayC<T, size>::FindLastSortedIndex(ConstReference value) const
			{
				int sortIndex = PrivateFindSortedIndex(value);

				while (sortIndex < static_cast<int>(Size()))
				{
					if (At(sortIndex + 1) == value)
					{
						sortIndex++;
					}
					else
					{
						break;
					}
				}

				return sortIndex;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> template<class Equality> inline
			int	ArrayC<T, size>::FindLastSortedIndex(ConstReference value, const Equality& functor) const
			{
				int sortIndex = PrivateFindSortedIndex(value, functor);

				while (sortIndex < static_cast<int>(Size()))
				{
					if (At(sortIndex + 1) == value)
					{
						sortIndex++;
					}
					else
					{
						break;
					}
				}
				return sortIndex;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> template<class Evaluate>inline	
			int ArrayC<T, size>::HighestEvalutionIndex(const Evaluate& functor) const
			{
				DIA_ASSERT(Size() > 0, "Must have at least one entry to evaluate");

				int index = 0;
				float currentHighest = functor.Evaluate(At(0));

				for (unsigned int i = 1; i < Size(); i++)
				{
					float temp = functor.Evaluate(At(i));
					if (temp > currentHighest)
					{
						currentHighest = temp;
						index = static_cast<int>(i);
					}
				}

				return index;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			void ArrayC<T, size>::PrivateSort(ArrayC<T, size>& array, unsigned int bottom, unsigned int top)const
			{
				unsigned int middle;
				if (bottom < top)
				{
					middle = PrivatePartition(array, bottom, top);
					PrivateSort(array, bottom, middle);   // sort top partition
					PrivateSort(array, middle+1, top);    // sort bottom partition
				}
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> template<class Equality> inline
			void ArrayC<T, size>::PrivateSort(const Equality& functor, ArrayC<T, size>& array, unsigned int bottom, unsigned int top)const
			{
				unsigned int middle;
				if (bottom < top)
				{
					middle = PrivatePartition(functor, array, bottom, top);
					PrivateSort(functor, array, bottom, middle);   // sort top partition
					PrivateSort(functor, array, middle+1, top);    // sort bottom partition
				}
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			unsigned int  ArrayC<T, size>::PrivatePartition(ArrayC<T, size>& array, unsigned int bottom, unsigned int top)const
			{
				T temp;
				T x = array[bottom];
				unsigned int i = bottom - 1;
				unsigned int j = top + 1;
				
				do
				{
					do      
					{	
						j--;
					}while ( x < array[j] );
			
					do  
					{
						i++;
					} while (x > array[i] );

					if (i < j)
					{ 
						temp = array[i];    // switch elements at positions i and j
						array[i] = array[j];
						array[j] = temp;
					}
				}while (i < j);     
				
				return j;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> template<class Equality> inline
			unsigned int  ArrayC<T, size>::PrivatePartition(const Equality& functor, ArrayC<T, size>& array, unsigned int bottom, unsigned int top)const
			{
				T temp;
				T x = array[bottom];
				unsigned int i = bottom - 1;
				unsigned int j = top + 1;
				
				do
				{
					do      
					{	
						j--;
					}while (functor.LessThen( x, array[j] ));
			
					do  
					{
						i++;
					} while (functor.GreaterThen( x, array[i] ));

					if (i < j)
					{ 
						temp = array[i];    // switch elements at positions i and j
						array[i] = array[j];
						array[j] = temp;
					}
				}while (i < j);     
				
				return j;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			int	ArrayC<T, size>::PrivateFindSortedIndex(ConstReference value) const
			{
				int low = 0, high = size-1;   
				while (low < high && (At(low) <= At(high))) 
				{
					int mid = (low+high)/2;  
					if (value == At(mid)) 
					{
						return mid;   
					}
					if (value < At(mid)) 
					{
						high = mid-1;  
					}
					else   
					{
						low  = mid+1;  
					}
				} 
				return -1; 
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> template<class Equality> inline
			int	ArrayC<T, size>::PrivateFindSortedIndex(ConstReference value, const Equality& functor) const
			{
				int low = 0, high = size-1;   
				while (low < high && (functor.LessThen(At(low), At(high)) || functor.Equal(At(low), At(high)))) 
				{
					int mid = (low+high)/2;  
					if (functor.Equal(value, At(mid))) 
					{
						return mid;   
					}
					if (functor.LessThen(value, At(mid))) 
					{
						high = mid-1;  
					}
					else   
					{
						low  = mid+1;  
					}
				} 
				return -1; 
			}
		}
	}
}