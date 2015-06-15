#include "DiaCore/Memory/Memory.h"

#include <stdio.h>
#include <stdlib.h>

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{

			//------------------------------------------------------------------------------------
			//	Implementation
			//------------------------------------------------------------------------------------	
			template <class T>
			DynamicArray<T>::DynamicArray()
				: mSize(0)
				, mCapacity(0)
				, mData(NULL)
			{}

			//------------------------------------------------------------------------------------	
			template <class T>
			DynamicArray<T>::~DynamicArray ()
			{	
				if (mData != NULL)
				{
					DIA_DELETE(mData);

					mCapacity = 0;
					mSize = 0;
				}
			}

			//------------------------------------------------------------------------------------	
			template <class T>
			DynamicArray<T>::DynamicArray ( unsigned int capacity )
				: mSize(0)
				, mCapacity(capacity)
				, mData(NULL)
			{
				mData = DIA_NEW_ARRAY(Capacity(), T);//mData = (T*) malloc(mCapacity * sizeof(T));
			}

			//-----------------------------------------------------------------------------
			template <class T>
			DynamicArray<T>::DynamicArray ( ConstPointer pData, unsigned int numberElements )
				: mSize(numberElements)
				, mCapacity(numberElements)
				, mData(NULL)
			{
				DIA_ASSERT(pData != NULL, "Invalid Data");
				
				mData = DIA_NEW_ARRAY(Capacity(), T);//mData = (T*)malloc(mCapacity * sizeof(T));//mData = DIA_NEW(Capacity(), T);
				MemoryCopy(mData, pData, sizeof(T)*numberElements);
			}

			//-----------------------------------------------------------------------------
			template <class T> 
			DynamicArray<T>::DynamicArray (ConstReference data, unsigned int numberElements)
				: mSize(numberElements)
				, mCapacity(numberElements)
				, mData(NULL)
			{
				DIA_ASSERT(numberElements <= Capacity(), "Will Outbound array");

				mData = DIA_NEW_ARRAY(Capacity(), T);//Data = (T*)malloc(mCapacity * sizeof(T));//mData = DIA_NEW(Capacity(), T);
				MemoryCopy(mData, &data, sizeof(T)*numberElements);
			}

			//-----------------------------------------------------------------------------
			template <class T>
			DynamicArray<T>::DynamicArray(const DynamicArray<T>& rhs)					
				: mSize(rhs.Size())
				, mCapacity(rhs.Capacity())
				, mData(NULL)
			{
				DIA_ASSERT(Capacity() >= rhs.Size(), "rhs larger then lhs");

				mData = DIA_NEW_ARRAY(Capacity(), T);//mData = (T*)malloc(mCapacity * sizeof(T));//mData = DIA_NEW(Capacity(), T);
				MemoryCopy(mData, &rhs.At(0), sizeof(T)*rhs.Size());
			}

			//-----------------------------------------------------------------------------
			template <class T>
			DynamicArray<T>::DynamicArray(const DynamicArray<T>& rhs, unsigned int startIndex, unsigned int numberElements)
				: mSize(numberElements)
				, mCapacity(numberElements)
				, mData(NULL)
			{
				DIA_ASSERT(numberElements >= 1, "You must want to get more then one element"); 
				DIA_ASSERT(numberElements <= Capacity(), "Will Outbound array"); 
				DIA_ASSERT(startIndex < Capacity(), "Start index must be greater then end index"); 

				mData = DIA_NEW_ARRAY(Capacity(), T);//mData = (T*)malloc(mCapacity * sizeof(T));//mData = DIA_NEW(Capacity(), T);
				MemoryCopy(mData, &rhs.At(startIndex), sizeof(T)*numberElements);
			}

			//-----------------------------------------------------------------------------
			template <class T>
			DynamicArray<T>::DynamicArray ( unsigned int capacity, ConstIterator& iter )
				: mCapacity(capacity)
				, mData(NULL)
				, mSize(0)
			{
				mData = DIA_NEW_ARRAY(Capacity(), T);//mData = (T*)malloc(mCapacity * sizeof(T));//mData = DIA_NEW(Capacity(), T);

				for (; !iter.IsDone(); iter.Next()) 
				{
					Add( *iter.Current() );
				}
			}

			//-----------------------------------------------------------------------------
			template <class T>
			DynamicArray<T>::DynamicArray ( unsigned int capacity, ConstReverseIterator& iter )
				: mCapacity(capacity)
				, mData(NULL)
				, mSize(0)
			{
				mData = DIA_NEW_ARRAY(Capacity(), T);//mData = (T*)malloc(mCapacity * sizeof(T));//mData = DIA_NEW(Capacity(), T);

				for (; !iter.IsDone(); iter.Next()) 
				{
					Add( *iter.Current() );
				}
			}

			//-----------------------------------------------------------------------------
			template <class T> template<class Evaluator>
			DynamicArray<T>::DynamicArray ( unsigned int capacity, ConstIterator& iter, const Evaluator& filter )
				: mCapacity(capacity)
				, mData(NULL)
				, mSize(0)
			{
				mData = DIA_NEW_ARRAY(Capacity(), T);//mData = (T*)malloc(mCapacity * sizeof(T));//mData = DIA_NEW(Capacity(), T);

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
			template <class T>
			DynamicArray<T>& DynamicArray<T>::Assign ( ConstPointer pData, unsigned int numberElements )
			{
				DIA_ASSERT(pData != NULL, "Invalid Data");
				DIA_ASSERT(numberElements <= Capacity(), "Will Outbound array");

				MemoryCopy(mData, pData, sizeof(T)*numberElements);
				mSize = numberElements;

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <class T>
			DynamicArray<T>& DynamicArray<T>::Assign ( ConstReference data, unsigned int numberElements )
			{
				DIA_ASSERT(numberElements <= Capacity(), "Will Outbound array");

				MemoryCopy(mData, &data, sizeof(T)*x);
				mSize = numberElements;

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <class T>
			DynamicArray<T>& DynamicArray<T>::Assign ( const DynamicArray<T>& rhs )
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
			template <class T>
			DynamicArray<T>& DynamicArray<T>::Assign ( const DynamicArray<T>& rhs, unsigned int startIndex, unsigned int numberElements )
			{
				DIA_ASSERT(numberElements >= 1, "You must want to get more then one element"); 
				DIA_ASSERT(numberElements <= Capacity(), "Will Outbound array"); 
				DIA_ASSERT(startIndex < Capacity(), "Start index must be greater then end index"); 

				MemoryCopy(mData, &rhs.At(startIndex), sizeof(T)*numberElements);
				mSize = numberElements;

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <class T>
			DynamicArray<T>& DynamicArray<T>::Assign ( ConstIterator& iter )
			{
				mSize = 0;

				for (; !iter.IsDone(); iter.Next()) 
				{
					Add( *iter.Current() );
				}

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <class T>
			DynamicArray<T>& DynamicArray<T>::Assign ( ConstReverseIterator& iter )
			{
				mSize = 0;

				for (; !iter.IsDone(); iter.Next()) 
				{
					Add( *iter.Current() );
				}

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <class T> template<class Evaluator>
			DynamicArray<T>& DynamicArray<T>::Assign ( ConstIterator& iter, const Evaluator& filter )
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
			template <class T>
			DynamicArray<T>& DynamicArray<T>::operator=(const DynamicArray<T>& rhs)
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
			template <class T>
			bool DynamicArray<T>::operator==( const DynamicArray<T>& rhs) const
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
			template <class T>
			bool DynamicArray<T>::operator!=( const DynamicArray<T>& rhs ) const
			{
				return !(*this == rhs);
			}

			//-----------------------------------------------------------------------------
			template <typename T> inline
				bool DynamicArray<T>::IsEmpty() const
			{
				return (Size() == 0);
			}

			//-----------------------------------------------------------------------------
			template <typename T> inline
				bool DynamicArray<T>::IsFull() const
			{
				return (Size() == Capacity());
			}

			//-----------------------------------------------------------------------------
			template <typename T> inline
				unsigned int DynamicArray<T>::Capacity() const
			{
				return mCapacity;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			void DynamicArray<T>::Reserve(unsigned int capcaity)
			{
				mCapacity = capcaity;

				if (mData == NULL)
				{
					mData = DIA_NEW_ARRAY(Capacity(), T);//mData = (T*) malloc(mCapacity * sizeof(T));//mData = DIA_NEW(Capacity(), T);
				}
				else
				{
					T* temp = DIA_NEW_ARRAY(Capacity(), T); //(T*)malloc(mCapacity * sizeof(T));//DIA_NEW_ARRAY(Capacity(), T);
					MemoryCopy(temp, mData, Capacity());
					DIA_DELETE(mData);
					mData = temp;
				}		
			}

			//-----------------------------------------------------------------------------
			template <typename T> inline
			unsigned int DynamicArray<T>::Size() const
			{
				DIA_ASSERT(mSize >= 0 && mSize <= Capacity(), "Size out of bounds");

				return mSize;
			}

			//-----------------------------------------------------------------------------
			template <typename T> inline
			typename DynamicArray<T>::Reference DynamicArray<T>::operator[](unsigned int index)
			{
				DIA_ASSERT(index < Size(), "Index outside current size");
				return (mData[index]);
			}

			//-----------------------------------------------------------------------------
			template <typename T> inline
			typename DynamicArray<T>::ConstReference DynamicArray<T>::operator[](unsigned int index) const
			{
				DIA_ASSERT(index < Size(), "Index outside current size");
				return (mData[index]);
			}

			//-----------------------------------------------------------------------------
			template <typename T> inline
				typename DynamicArray<T>::Reference DynamicArray<T>::operator[](int index)
			{
				DIA_ASSERT(index >= 0 && index < static_cast<int>(Size()), "Index outside current size");
				return (mData[index]);
			}

			//-----------------------------------------------------------------------------
			template <typename T> inline
				typename DynamicArray<T>::ConstReference DynamicArray<T>::operator[](int index) const
			{
				DIA_ASSERT(index >= 0 && index < static_cast<int>(Size()), "Index outside current size");
				return (mData[index]);
			}

			//-----------------------------------------------------------------------------
			template <typename T> inline	
				typename DynamicArray<T>::Reference DynamicArray<T>::At(unsigned int index)
			{
				DIA_ASSERT(index < Size(), "Index out of array size");
				return (mData[index]);
			}

			//-----------------------------------------------------------------------------
			template <typename T> inline
				typename DynamicArray<T>::ConstReference DynamicArray<T>::At(unsigned int index) const
			{
				DIA_ASSERT(index < Size(), "Index out of array size");
				return (mData[index]);
			}

			//-----------------------------------------------------------------------------
			template <typename T> inline
				typename DynamicArray<T>::Reference DynamicArray<T>::Front()
			{
				return *mData;
			}

			//-----------------------------------------------------------------------------
			template <typename T> inline
				typename DynamicArray<T>::ConstReference DynamicArray<T>::Front() const
			{
				return *mData;
			}

			//-----------------------------------------------------------------------------
			template <typename T> inline
				typename DynamicArray<T>::Reference DynamicArray<T>::Back()
			{
				if (Size() < 1)
				{
					return *mData;
				}

				return *(mData + Size() - 1);
			}

			//-----------------------------------------------------------------------------	
			template <typename T> inline
				typename DynamicArray<T>::ConstReference DynamicArray<T>::Back() const
			{
				if (Size() < 1)
				{
					return *mData;
				}
				return *(mData + Size() - 1);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
				typename DynamicArray<T>::Iterator DynamicArray<T>::IteratorAt(unsigned int index)
			{
				return Iterator(&At(index), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
				typename DynamicArray<T>::ConstIterator DynamicArray<T>::IteratorAtConst(unsigned int index) const
			{
				return ConstIterator(&At(index), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
				typename DynamicArray<T>::ReverseIterator DynamicArray<T>::ReverseIteratorAt (unsigned int index)
			{
				return ReverseIterator(&At(index), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
				typename DynamicArray<T>::ConstReverseIterator DynamicArray<T>::ReverseIteratorAtConst (unsigned int index) const
			{
				return ConstReverseIterator(&At(index), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
				typename DynamicArray<T>::Iterator DynamicArray<T>::Begin()
			{
				return Iterator(&Front(), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
				typename DynamicArray<T>::ConstIterator DynamicArray<T>::BeginConst()const
			{
				return ConstIterator(&Front(), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
				typename DynamicArray<T>::ReverseIterator DynamicArray<T>::End()
			{
				return ReverseIterator(&Back(), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
				typename DynamicArray<T>::ConstReverseIterator DynamicArray<T>::EndConst()const
			{
				return ConstReverseIterator(&Back(), &Front(), &Back());
			}	

			//-----------------------------------------------------------------------------
			template <typename T> inline
				int	DynamicArray<T>::FrequencyOfElement( ConstReference value )const
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
			template <typename T> inline
			void DynamicArray<T>::UniqueElements(DynamicArray<T>& unique)const
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
			template <typename T> inline
			void DynamicArray<T>::FrequencyUniqueElements(DynamicArray<T>& unique, DynamicArray<int>& uniqueFrequency)const
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
			template <typename T> inline
				void DynamicArray<T>::AddDefault()
			{
				DIA_ASSERT (!IsFull(), "Array is full");
				mData[mSize] = T();
				mSize++;
			}

			//-----------------------------------------------------------------------------
			template <typename T> inline
				void DynamicArray<T>::Add(ConstReference value)
			{
				DIA_ASSERT (!IsFull(), "Array is full");
				mData[mSize] = value;
				mSize++;
			}

			//-----------------------------------------------------------------------------
			template <typename T> inline
				void DynamicArray<T>::FillDefault()
			{
				DIA_ASSERT (!IsFull(), "Array is full");

				for (unsigned int i = 0; i < Capacity(); i++)
				{
					AddDefault();
				}

				DIA_ASSERT (IsFull(), "Array is full");
			}

			//-----------------------------------------------------------------------------
			template <typename T> inline
				void DynamicArray<T>::Fill(ConstReference value)
			{
				DIA_ASSERT (!IsFull(), "Array is full");

				for (unsigned int i = 0; i < Capacity(); i++)
				{
					Add(value);
				}

				DIA_ASSERT (IsFull(), "Array is full");
			}

			//-----------------------------------------------------------------------------
			template <typename T> inline
			void DynamicArray<T>::AddAt(ConstReference value, unsigned int index)
			{
				DIA_ASSERT (!IsFull(), "Array is full");	
				DIA_ASSERT (!IsEmpty(), "Array is empty");	
				DIA_ASSERT (index < Size(), "Index is outside of current size");

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
			template <typename T> inline
			void DynamicArray<T>::Remove()
			{
				At(mSize - 1).~T();
				mSize--;
			}

			//-----------------------------------------------------------------------------
			template <typename T> inline
			void DynamicArray<T>::RemoveAt(unsigned int index)
			{
				DIA_ASSERT (!IsEmpty(), "Array is empty");	
				DIA_ASSERT (index < Size(), "Index is outside of current size");

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
			template <typename T> inline
			void DynamicArray<T>::RemoveFirst(ConstReference value)
			{
				unsigned int index = FindIndex(value);
				if (index != -1)
				{
					RemoveAt(index);
				}
			}

			//-----------------------------------------------------------------------------
			template <typename T> inline
			void DynamicArray<T>::RemoveLast(ConstReference value)
			{
				unsigned int index = FindLastIndex(value);
				if (index != -1)
				{
					RemoveAt(index);
				}
			}

			//-----------------------------------------------------------------------------
			template <typename T> inline
			void DynamicArray<T>::RemoveAll(ConstReference value)
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
			template <typename T> inline
			void DynamicArray<T>::RemoveAll()
			{
				for (int i = static_cast<int>(mSize - 1); i >= 0; i--)
				{
					At(i).~T();
				}
				mSize = 0;
			}

			//-----------------------------------------------------------------------------
			template <typename T> template<class Evaluate> inline
			void DynamicArray<T>::RemoveAll(const Evaluate& functor)
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
			template <class T> inline
			bool DynamicArray<T>::IsSorted()const
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
			template <class T> template<class Equality> inline
			bool DynamicArray<T>::IsSorted(const Equality& functor)const
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
			template <class T> inline
			void DynamicArray<T>::Sort()
			{
				PrivateSort( *this, 0, Size() - 1 );
			}

			//-----------------------------------------------------------------------------
			template <class T> template<class Equality> inline
			void DynamicArray<T>::Sort(const Equality& functor)
			{
				PrivateSort( functor, *this, 0, Size() - 1 );
			}

			//-----------------------------------------------------------------------------
			template <typename T> inline
			void DynamicArray<T>::Swap(DynamicArray<T>& other)
			{
				DynamicArray<T> t(other);
				other.Reserve(this->Size());
				other.Assign(*this);
				Assign(t);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
				int	DynamicArray<T>::FindIndex(ConstReference value) const
			{
				return FindBetweenIndex(value, 0, Size() - 1);
			}

			//-----------------------------------------------------------------------------
			template <class T> template<class Comparisson> inline
				int	DynamicArray<T>::FindIndex(ConstReference value, const Comparisson& functor) const
			{
				return FindBetweenIndex(value, functor, 0, Size() - 1);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
				int	DynamicArray<T>::FindBetweenIndex(ConstReference value, int startIndex, int endIndex)const
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
			template <class T> template<class Comparisson> inline
			int	DynamicArray<T>::FindBetweenIndex(ConstReference value, const Comparisson& functor, int startIndex, int endIndex)const
			{
				DIA_ASSERT(startIndex >= 0, "Start index must greater the zero");
				DIA_ASSERT(startIndex <= endIndex, "Start index must be smaller then end index");
				DIA_ASSERT(endIndex >= 0, "End index out of range");
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
			template <class T> inline
			int	DynamicArray<T>::FindLastIndex(ConstReference value) const
			{
				return FindLastBetweenIndex(value, 0, Size() - 1);
			}

			//-----------------------------------------------------------------------------
			template <class T> template<class Comparisson> inline
			int	DynamicArray<T>::FindLastIndex(ConstReference value, const Comparisson& functor) const
			{
				return FindLastBetweenIndex(value, functor, 0, Size() - 1);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			int	DynamicArray<T>::FindLastBetweenIndex(ConstReference value, int startIndex, int endIndex)const
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
			template <class T> template<class Comparisson> inline
			int	DynamicArray<T>::FindLastBetweenIndex(ConstReference value, const Comparisson& functor, int startIndex, int endIndex)const
			{
				DIA_ASSERT(startIndex >= 0, "Start index must greater the zero");
				DIA_ASSERT(startIndex <= endIndex, "Start index must be smaller then end index");
				DIA_ASSERT(endIndex >= 0, "End index out of range");
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
			template <class T> inline
			int	DynamicArray<T>::FindSortedIndex(ConstReference value) const
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
			template <class T> template<class Equality> inline
			int	DynamicArray<T>::FindSortedIndex(ConstReference value, const Equality& functor) const
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
			template <class T> inline
				int	DynamicArray<T>::FindLastSortedIndex(ConstReference value) const
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
			template <class T> template<class Equality> inline
				int	DynamicArray<T>::FindLastSortedIndex(ConstReference value, const Equality& functor) const
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
			template <class T> template<class Evaluate> inline	
				int DynamicArray<T>::HighestEvalutionIndex(const Evaluate& functor) const
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
			template <class T> inline
				void DynamicArray<T>::PrivateSort(DynamicArray<T>& array, unsigned int bottom, unsigned int top)const
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
			template <class T> template<class Equality> inline
				void DynamicArray<T>::PrivateSort(const Equality& functor, DynamicArray<T>& array, unsigned int bottom, unsigned int top)const
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
			template <class T> inline
				unsigned int  DynamicArray<T>::PrivatePartition(DynamicArray<T>& array, unsigned int bottom, unsigned int top)const
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
			template <class T> template<class Equality> inline
				unsigned int  DynamicArray<T>::PrivatePartition(const Equality& functor, DynamicArray<T>& array, unsigned int bottom, unsigned int top)const
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
			template <class T> inline
			int	DynamicArray<T>::PrivateFindSortedIndex(ConstReference value) const
			{
				int low = 0, high = Size()-1;   
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
			template <class T> template<class Equality> inline
			int	DynamicArray<T>::PrivateFindSortedIndex(ConstReference value, const Equality& functor) const
			{
				int low = 0, high = Size()-1;   
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