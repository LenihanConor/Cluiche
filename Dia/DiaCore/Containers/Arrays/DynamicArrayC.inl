#include "DiaCore/Memory/Memory.h"

#include "DiaCore/Type/TypeDefinitionMacros.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			static char* DynamicArrayC_NameBuilder(const Dia::Core::Types::TypeParameterInput& input, unsigned int size, std::string& out)
			{
				const Dia::Core::Types::TypeDefinition::VariableLinkListNode* variable = input.GetVariables().HeadConst();

				DIA_ASSERT(variable, "A variable type must be defined");
				const Dia::Core::Types::TypeVariable& typeVariable = *variable->GetPayloadConst();

				std::snprintf(const_cast<char*>(out.c_str()), out.capacity(), "DynamicArrayC<%s, %u>", typeVariable.GetTypeAsString(), size);

				return const_cast<char*>(out.c_str());
			}

			template <class T, unsigned int size> Dia::Core::Types::TypeDefinition* DynamicArrayC<T, size>::sType;
			template <class T, unsigned int size> Dia::Core::Types::TypeDefinition& DynamicArrayC<T, size>::GetType() { if (sType == nullptr) { const Dia::Core::Types::TypeParameterInput& input = DynamicArrayC<T, size>::TypeCreationalInput(); std::string name; name.reserve(128);  sType = DIA_NEW(Dia::Core::Types::TypeDefinition(DynamicArrayC_NameBuilder(input, size, name), sizeof(DynamicArrayC<T, size>), __is_polymorphic(DynamicArrayC<T, size>), input)); } return *sType; }\
			template <class T, unsigned int size> Dia::Core::Types::TypeInstance DynamicArrayC<T, size>::CreateTypeInstance() { return (Dia::Core::Types::TypeInstance(DynamicArrayC<T, size>::GetType(), this)); }\
			template <class T, unsigned int size> Dia::Core::Types::TypeInstance DynamicArrayC<T, size>::CreateTypeInstanceConst()const { return (Dia::Core::Types::TypeInstance(DynamicArrayC<T, size>::GetType(), this)); }\
			template <class T, unsigned int size> Dia::Core::Types::TypeParameterInput& DynamicArrayC<T, size>::TypeCreationalInput()\
			{ \
				typedef DynamicArrayC<T, size> MyType; \
				static MyType foo; \
				static Dia::Core::Types::TypeParameterInput typeInput; \
				Dia::Core::Types::TypeVariable* lastVariable = NULL; \
				DIA_TYPE_ADD_VARIABLE("mSize", mSize)
				DIA_TYPE_ADD_VARIABLE_ARRAY("mData", mData, size)
				return typeInput; \
			}\
			//------------------------------------------------------------------------------------
			//	Implementation
			//------------------------------------------------------------------------------------	
			template <class T, unsigned int capacity>
			DynamicArrayC<T, capacity>::DynamicArrayC()
				: mSize(0)
			{}
		
			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity>
			DynamicArrayC<T, capacity>::DynamicArrayC (ConstPointer pData, unsigned int numberElements )
			{
				DIA_ASSERT(pData != NULL, "Invalid Data");
				DIA_ASSERT(numberElements <= Capacity(), "Will Outbound array");

				MemoryCopy(mData, pData, sizeof(T)*numberElements);
				mSize = numberElements;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity> 
			DynamicArrayC<T, capacity>::DynamicArrayC (ConstReference data, unsigned int numberElements)
			{
				DIA_ASSERT(numberElements <= Capacity(), "Will Outbound array");

				MemoryCopy(mData, &data, sizeof(T)*numberElements);
				mSize = numberElements;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity>
			template<unsigned int _size> DynamicArrayC<T, capacity>::DynamicArrayC(const DynamicArrayC<T,_size>& rhs)					
			{
				DIA_ASSERT(Capacity() > rhs.Size(), "rhs larger the lhs");

				MemoryCopy(mData, &rhs.At(0), sizeof(T)*rhs.Size());
				mSize = rhs.Size();
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity>
			template<unsigned int _size> DynamicArrayC<T, capacity>::DynamicArrayC(const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements)
			{
				DIA_ASSERT(numberElements >= 1, "You must want to get more then one element"); 
				DIA_ASSERT(numberElements <= Capacity(), "Will Outbound array"); 
				DIA_ASSERT(startIndex < Capacity(), "Start index must be greater then end index"); 
			
				MemoryCopy(mData, &rhs.At(startIndex), sizeof(T)*numberElements);
				mSize = numberElements;
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity>
			DynamicArrayC<T, capacity>::DynamicArrayC ( ConstIterator& iter )
			{
				mSize = 0;

				for (; !iter.IsDone(); iter.Next()) 
				{
					Add( *iter.Current() );
				}
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity>
			DynamicArrayC<T, capacity>::DynamicArrayC ( ConstReverseIterator& iter )
			{
				mSize = 0;
				for (; !iter.IsDone(); iter.Next()) 
				{
					Add( *iter.Current() );
				}
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity> template<class Evaluator>
			DynamicArrayC<T, capacity>::DynamicArrayC ( ConstIterator& iter, const Evaluator& filter )
			{
				mSize = 0;
				for (; !iter.IsDone(); iter.Next()) 
				{
					const T* temp = iter.Current();
					if (filter.Evaluate(*temp ))
					{
						Add( *temp );
					}
				}
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity>
			DynamicArrayC<T, capacity>& DynamicArrayC<T, capacity>::Assign ( ConstPointer pData, unsigned int numberElements )
			{
				DIA_ASSERT(pData != NULL, "Invalid Data");
				DIA_ASSERT(numberElements <= Capacity(), "Will Outbound array");

				MemoryCopy(mData, pData, sizeof(T)*numberElements);
				mSize = numberElements;

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity>
			DynamicArrayC<T, capacity>& DynamicArrayC<T, capacity>::Assign ( ConstReference data, unsigned int numberElements )
			{
				DIA_ASSERT(numberElements <= Capacity(), "Will Outbound array");

				MemoryCopy(mData, &data, sizeof(T)*x);
				mSize = numberElements;

				return *this;
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity>
			template<unsigned int _size> DynamicArrayC<T, capacity>& DynamicArrayC<T, capacity>::Assign ( const DynamicArrayC<T,_size>& rhs )
			{
				unsigned int x = rhs.Size();
				if( Capacity() <= rhs.Size() )
				{
					x = Capacity();
				}

				MemoryCopy(mData, &rhs.At(0), sizeof(T)*x);
				mSize = x;

				return *this;
			}
				
			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity>
			template<unsigned int _size> DynamicArrayC<T, capacity>& DynamicArrayC<T, capacity>::Assign ( const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements )
			{
				DIA_ASSERT(numberElements >= 1, "You must want to get more then one element"); 
				DIA_ASSERT(numberElements <= Capacity(), "Will Outbound array"); 
				DIA_ASSERT(startIndex < Capacity(), "Start index must be greater then end index"); 
				
				MemoryCopy(mData, &rhs.At(startIndex), sizeof(T)*numberElements);
				mSize = numberElements;

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity>
			DynamicArrayC<T, capacity>& DynamicArrayC<T, capacity>::Assign ( ConstIterator& iter )
			{
				mSize = 0;

				for (; !iter.IsDone(); iter.Next()) 
				{
					Add( *iter.Current() );
				}

				return *this;
			}
				
			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity>
			DynamicArrayC<T, capacity>& DynamicArrayC<T, capacity>::Assign ( ConstReverseIterator& iter )
			{
				mSize = 0;

				for (; !iter.IsDone(); iter.Next()) 
				{
					Add( *iter.Current() );
				}

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity> template<class Evaluator>
			DynamicArrayC<T, capacity>& DynamicArrayC<T, capacity>::Assign ( ConstIterator& iter, const Evaluator& filter )
			{
				mSize = 0;

				for (; !iter.IsDone(); iter.Next()) 
				{
					const T* temp = iter.Current();
					if (filter.Evaluate(*temp ))
					{
						Add( *temp );
					}
				}

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity>
			template<unsigned int _size> DynamicArrayC<T, capacity>& DynamicArrayC<T, capacity>::operator=(const DynamicArrayC<T,_size>& rhs)
			{
				unsigned int x = rhs.Size();
				if( Capacity() <= rhs.Size() )
				{
					x = Capacity();
				}

				mSize = x;
				MemoryCopy(mData, &rhs.Front(), sizeof(T)*x);

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity>
			bool DynamicArrayC<T, capacity>::operator==( const DynamicArrayC<T, capacity>& rhs) const
			{
				for(unsigned int i = 0; i < Size(); i++)
				{
					if (rhs[i] != mData[i])
					{
						return false;
					}
				}
				return true;
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity>
			bool DynamicArrayC<T, capacity>::operator!=( const DynamicArrayC<T, capacity>& rhs ) const
			{
				return !(*this == rhs);
			}

			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			bool DynamicArrayC<T, capacity>::IsEmpty() const
			{
				return (Size() == 0);
			}

			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			bool DynamicArrayC<T, capacity>::IsFull() const
			{
				return (Size() == Capacity());
			}

			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			unsigned int DynamicArrayC<T, capacity>::Capacity() const
			{
				return capacity;
			}

			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			unsigned int DynamicArrayC<T, capacity>::Size() const
			{
				DIA_ASSERT(mSize >= 0 && mSize <= capacity, "Size out of bounds");

				return mSize;
			}

			//-----------------------------------------------------------------------------
 			template <typename T, unsigned int capacity> inline
			typename DynamicArrayC<T, capacity>::Reference DynamicArrayC<T, capacity>::operator[](unsigned int index)
			{
				DIA_ASSERT(index < Size(), "Index outside current capacity");
				return (mData[index]);
			}

			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			typename DynamicArrayC<T, capacity>::ConstReference DynamicArrayC<T, capacity>::operator[](unsigned int index) const
			{
				DIA_ASSERT(index < Size(), "Index outside current capacity");
				return (mData[index]);
			}
			
			//-----------------------------------------------------------------------------
 			template <typename T, unsigned int capacity> inline
			typename DynamicArrayC<T, capacity>::Reference DynamicArrayC<T, capacity>::operator[](int index)
			{
				DIA_ASSERT(index >= 0 && index < static_cast<int>(Size()), "Index outside current capacity");
				return (mData[index]);
			}

			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			typename DynamicArrayC<T, capacity>::ConstReference DynamicArrayC<T, capacity>::operator[](int index) const
			{
				DIA_ASSERT(index >= 0 && index < static_cast<int>(Size()), "Index outside current capacity");
				return (mData[index]);
			}
			
			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline	
			typename DynamicArrayC<T, capacity>::Reference DynamicArrayC<T, capacity>::At(unsigned int index)
			{
				DIA_ASSERT(index < Size(), "Index out of array capacity");
				return (mData[index]);
			}

			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			typename DynamicArrayC<T, capacity>::ConstReference DynamicArrayC<T, capacity>::At(unsigned int index) const
			{
				DIA_ASSERT(index < Size(), "Index out of array capacity");
				return (mData[index]);
			}

			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			typename DynamicArrayC<T, capacity>::Reference DynamicArrayC<T, capacity>::Front()
			{
				return *mData;
			}

			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			typename DynamicArrayC<T, capacity>::ConstReference DynamicArrayC<T, capacity>::Front() const
			{
				return *mData;
			}
			
			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			typename DynamicArrayC<T, capacity>::Reference DynamicArrayC<T, capacity>::Back()
			{
				if (Size() < 1)
				{
					return *mData;
				}

				return *(mData + Size() - 1);
			}

			//-----------------------------------------------------------------------------	
			template <typename T, unsigned int capacity> inline
			typename DynamicArrayC<T, capacity>::ConstReference DynamicArrayC<T, capacity>::Back() const
			{
				if (Size() < 1)
				{
					return *mData;
				}
				return *(mData + Size() - 1);
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity> inline
			typename DynamicArrayC<T, capacity>::Iterator DynamicArrayC<T, capacity>::IteratorAt(unsigned int index)
			{
				return Iterator(&At(index), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity> inline
			typename DynamicArrayC<T, capacity>::ConstIterator DynamicArrayC<T, capacity>::IteratorAtConst(unsigned int index) const
			{
				return ConstIterator(&At(index), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity> inline
			typename DynamicArrayC<T, capacity>::ReverseIterator DynamicArrayC<T, capacity>::ReverseIteratorAt (unsigned int index)
			{
				return ReverseIterator(&At(index), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity> inline
			typename DynamicArrayC<T, capacity>::ConstReverseIterator DynamicArrayC<T, capacity>::ReverseIteratorAtConst (unsigned int index) const
			{
				return ConstReverseIterator(&At(index), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity> inline
			typename DynamicArrayC<T, capacity>::Iterator DynamicArrayC<T, capacity>::Begin()
			{
				return Iterator(&Front(), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity> inline
			typename DynamicArrayC<T, capacity>::ConstIterator DynamicArrayC<T, capacity>::BeginConst()const
			{
				return ConstIterator(&Front(), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity> inline
			typename DynamicArrayC<T, capacity>::ReverseIterator DynamicArrayC<T, capacity>::End()
			{
				return ReverseIterator(&Back(), &Front(), &Back());
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity> inline
			typename DynamicArrayC<T, capacity>::ConstReverseIterator DynamicArrayC<T, capacity>::EndConst()const
			{
				return ConstReverseIterator(&Back(), &Front(), &Back());
			}	
			
			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			int	DynamicArrayC<T, capacity>::FrequencyOfElement( ConstReference value )const
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
			template <typename T, unsigned int capacity> inline
			void DynamicArrayC<T, capacity>::UniqueElements(DynamicArrayC<T, capacity>& unique)const
			{
				for (unsigned int i = 0; i < Size(); i++)
				{
					T possibleUnique = At(i);
					bool foundInList = false;
					for (unsigned int j = 0; j < unique.Size(); j++)
					{
						if (possibleUnique == unique[j])
						{
							foundInList = true; 
							break;
						}
					}

					if (!foundInList)
					{
						unique.Add( possibleUnique );
					}
				}
			}

			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			void DynamicArrayC<T, capacity>::FrequencyUniqueElements(DynamicArrayC<T, capacity>& unique, DynamicArrayC<int, capacity>& uniqueFrequency)const
			{
				for (unsigned int i = 0; i < Size(); i++)
				{
					T possibleUnique = At(i);
					bool foundInList = false;
					for (unsigned int j = 0; j < unique.Size(); j++)
					{
						if (possibleUnique == unique[j])
						{
							foundInList = true; 
							break;
						}
					}

					if (!foundInList)
					{
						unique.Add( possibleUnique );
						uniqueFrequency.Add( FrequencyOfElement( possibleUnique ) );
					}
				}
			}

			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			void DynamicArrayC<T, capacity>::AddDefault()
			{
				DIA_ASSERT (!IsFull(), "Array is full");
				T temp = T();
				mData[mSize] = temp;
				mSize++;
			}

			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			void DynamicArrayC<T, capacity>::Add(ConstReference value)
			{
				DIA_ASSERT (!IsFull(), "Array is full");
				mData[mSize] = value;
				mSize++;
			}

			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			void DynamicArrayC<T, capacity>::FillDefault()
			{
				DIA_ASSERT (!IsFull(), "Array is full");

				for (unsigned int i = 0; i < Capacity(); i++)
				{
					AddDefault();
				}

				DIA_ASSERT (IsFull(), "Array is full");
			}

			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			void DynamicArrayC<T, capacity>::Fill(ConstReference value)
			{
				DIA_ASSERT (!IsFull(), "Array is full");

				for (unsigned int i = 0; i < Capacity(); i++)
				{
					Add(value);
				}

				DIA_ASSERT (IsFull(), "Array is full");
			}

			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			void DynamicArrayC<T, capacity>::AddAt(ConstReference value, unsigned int index)
			{
				DIA_ASSERT (!IsFull(), "Array is full");	
				DIA_ASSERT (!IsEmpty(), "Array is empty");	
				DIA_ASSERT (index < Size(), "Index is outside of current capacity");
		
				mSize++;
				unsigned int i = Size() - 1;
				
				while (i > index)
				{
					mData[i] = At(i-1);
					i--;
				}
						
				mData[index] = value;	
			}

			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			void DynamicArrayC<T, capacity>::Remove()
			{
				At(mSize - 1).~T();
				mSize--;
			}

			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			void DynamicArrayC<T, capacity>::RemoveAt(unsigned int index)
			{
				DIA_ASSERT (!IsEmpty(), "Array is empty");	
				DIA_ASSERT (index < Size(), "Index is outside of current capacity");

				unsigned int i = index;
				while (i < (mSize - 1))
				{
					At(i) = At(i + 1);
					i++;
				}
						
				At(mSize - 1).~T();
				mSize--;
			}

			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			void DynamicArrayC<T, capacity>::RemoveFirst(ConstReference value)
			{
				unsigned int index = FindIndex(value);
				if (index != -1)
				{
					RemoveAt(index);
				}
			}
				
			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			void DynamicArrayC<T, capacity>::RemoveLast(ConstReference value)
			{
				unsigned int index = FindLastIndex(value);
				if (index != -1)
				{
					RemoveAt(index);
				}
			}
				
			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			void DynamicArrayC<T, capacity>::RemoveAll(ConstReference value)
			{
				for (int i = static_cast<int>(mSize - 1); i >= 0; i--)
				{
					if(At(i) == value)
					{
						RemoveAt(i);
					}
				}
			}
			
			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			void DynamicArrayC<T, capacity>::RemoveAll()
			{
				for (int i = static_cast<int>(mSize - 1); i >= 0; i--)
				{
					At(i).~T();
				}
				mSize = 0;
			}
			
			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> template<class Evaluate> inline
			void DynamicArrayC<T, capacity>::RemoveAll(const Evaluate& functor)
			{
				for (int i = static_cast<int>(mSize - 1); i >= 0; i--)
				{
					if(functor.Evaluate(At(i)))
					{
						RemoveAt(i);
					}
				}
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity> inline
			bool DynamicArrayC<T, capacity>::IsSorted()const
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
			template <class T, unsigned int capacity> template<class Equality> inline
			bool DynamicArrayC<T, capacity>::IsSorted(const Equality& functor)const
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
			template <class T, unsigned int capacity> inline
			void DynamicArrayC<T, capacity>::Sort()
			{
				PrivateSort( *this, 0, capacity - 1 );
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity> template<class Equality> inline
			void DynamicArrayC<T, capacity>::Sort(const Equality& functor)
			{
				PrivateSort( functor, *this, 0, capacity - 1 );
			}

			//-----------------------------------------------------------------------------
			template <typename T, unsigned int capacity> inline
			void DynamicArrayC<T, capacity>::Swap(DynamicArrayC<T,capacity>& other)
			{
				DynamicArrayC<T, capacity> t = other;
				other.Assign(*this);
				Assign(t);
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity> inline
			int	DynamicArrayC<T, capacity>::FindIndex(ConstReference value) const
			{
				if (Size() == 0)
					return -1;

				return FindBetweenIndex(value, 0, Size() - 1);
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity> template<class Comparisson> inline
			int	DynamicArrayC<T, capacity>::FindIndex(ConstReference value, const Comparisson& functor) const
			{
				if (Size() == 0)
					return -1;

				return FindBetweenIndex(value, functor, 0, Size() - 1);
			}



			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity> inline
			int	DynamicArrayC<T, capacity>::FindBetweenIndex(ConstReference value, int startIndex, int endIndex)const
			{
				DIA_ASSERT(startIndex >= 0, "Start index must greater the zero");
				DIA_ASSERT(startIndex <= endIndex, "Start index must be smaller then end index");
				DIA_ASSERT(endIndex >= 0, "End index out of range");
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
			template <class T, unsigned int capacity> template<class Comparisson> inline
			int	DynamicArrayC<T, capacity>::FindBetweenIndex(ConstReference value, const Comparisson& functor, int startIndex, int endIndex)const
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
			template <class T, unsigned int capacity> inline
			int	DynamicArrayC<T, capacity>::FindLastIndex(ConstReference value) const
			{
				return FindLastBetweenIndex(value, 0, Size() - 1);
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity> template<class Comparisson> inline
			int	DynamicArrayC<T, capacity>::FindLastIndex(ConstReference value, const Comparisson& functor) const
			{
				return FindLastBetweenIndex(value, functor, 0, Size() - 1);
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int capacity> inline
			int	DynamicArrayC<T, capacity>::FindLastBetweenIndex(ConstReference value, int startIndex, int endIndex)const
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
			template <class T, unsigned int capacity> template<class Comparisson> inline
			int	DynamicArrayC<T, capacity>::FindLastBetweenIndex(ConstReference value, const Comparisson& functor, int startIndex, int endIndex)const
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
			template <class T, unsigned int capacity> inline
			int	DynamicArrayC<T, capacity>::FindSortedIndex(ConstReference value) const
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
			template <class T, unsigned int capacity> template<class Equality> inline
			int	DynamicArrayC<T, capacity>::FindSortedIndex(ConstReference value, const Equality& functor) const
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
			template <class T, unsigned int capacity> inline
			int	DynamicArrayC<T, capacity>::FindLastSortedIndex(ConstReference value) const
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
			template <class T, unsigned int capacity> template<class Equality> inline
			int	DynamicArrayC<T, capacity>::FindLastSortedIndex(ConstReference value, const Equality& functor) const
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
			template <class T, unsigned int capacity> template<class Evaluate> inline	
			int DynamicArrayC<T, capacity>::HighestEvalutionIndex(const Evaluate& functor) const
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
			template <class T, unsigned int capacity> inline
			void DynamicArrayC<T, capacity>::PrivateSort(DynamicArrayC<T, capacity>& array, unsigned int bottom, unsigned int top)const
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
			template <class T, unsigned int capacity> template<class Equality> inline
			void DynamicArrayC<T, capacity>::PrivateSort(const Equality& functor, DynamicArrayC<T, capacity>& array, unsigned int bottom, unsigned int top)const
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
			template <class T, unsigned int capacity> inline
			unsigned int  DynamicArrayC<T, capacity>::PrivatePartition(DynamicArrayC<T, capacity>& array, unsigned int bottom, unsigned int top)const
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
			template <class T, unsigned int capacity> template<class Equality> inline
			unsigned int  DynamicArrayC<T, capacity>::PrivatePartition(const Equality& functor, DynamicArrayC<T, capacity>& array, unsigned int bottom, unsigned int top)const
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
			template <class T, unsigned int capacity> inline
			int	DynamicArrayC<T, capacity>::PrivateFindSortedIndex(ConstReference value) const
			{
				int low = 0, high = capacity-1;   
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
			template <class T, unsigned int capacity> template<class Equality> inline
			int	DynamicArrayC<T, capacity>::PrivateFindSortedIndex(ConstReference value, const Equality& functor) const
			{
				int low = 0, high = capacity-1;   
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