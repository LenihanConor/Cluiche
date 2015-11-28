#include "DiaCore/Type/BasicTypeDefines.h"

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
			Array<T>::Array()
				: mSize(0)
				, mData(NULL)
			{}

			//------------------------------------------------------------------------------------
			template <class T>
			Array<T>::~Array()
			{
				DIA_DELETE_ARRAY(mData);
				mSize = 0;
			}
			
			//------------------------------------------------------------------------------------
			template <class T>
			Array<T>::Array (unsigned int size)
				: mSize(size)
				, mData(NULL)
			{
				mData = DIA_NEW_ARRAY(Size(), T);
				MemorySet(mData, 0, sizeof(T)*size);
			}

			//------------------------------------------------------------------------------------
			template <class T>
			Array<T>::Array (ConstPointer pData, unsigned int numberElements)
				: mSize(numberElements)
				, mData(NULL)
			{
				DIA_ASSERT(pData != NULL, "Invalid Data");

				mData = DIA_NEW_ARRAY(Size(), T);
				MemoryCopy(mData, pData, sizeof(T)*Size());
			}

			//-----------------------------------------------------------------------------
			template <class T> 
			Array<T>::Array (ConstReference data, unsigned int numberElements)
				: mSize(numberElements)
				, mData(NULL)
			{
				mData = DIA_NEW_ARRAY(Size(), T);
				MemoryCopy(mData, &data, sizeof(T)*Size());
			}

			//-----------------------------------------------------------------------------
			template <class T>
			Array<T>::Array(const Array<T>& rhs)	
				: mSize(rhs.Size())
				, mData(NULL)
			{
				mData = DIA_NEW_ARRAY(Size(), T);
				MemoryCopy(mData, &rhs.At(0), sizeof(T)*Size());
			}

			//-----------------------------------------------------------------------------
			template <class T>
			Array<T>::Array(const Array<T>& rhs, unsigned int startIndex, unsigned int numberElements)
				: mSize(numberElements)
				, mData(NULL)
			{
				DIA_ASSERT(numberElements <= rhs.Size(), "Will outbound the array being copied from");
				DIA_ASSERT(numberElements >= 1, "You must want to get more then one element"); 
				DIA_ASSERT(startIndex < rhs.Size(), "Start index must be greater then end index"); 

				mData = DIA_NEW_ARRAY(Size(), T);
				MemoryCopy(mData, &rhs.At(startIndex), sizeof(T)*Size());
			}

			//-----------------------------------------------------------------------------
			template <class T>
			Array<T>::Array ( unsigned int size, ConstIterator& iter )
				: mSize(size)
				, mData(NULL)
			{
				mData = DIA_NEW_ARRAY(Size(), T);
				MemorySet(mData, 0, sizeof(T)*size);

				int j = 0;
				for (; !iter.IsDone(); iter.Next()) 
				{
					At(j) = *iter.Current();
					j++;
				}
			}

			//-----------------------------------------------------------------------------
			template <class T>
			Array<T>::Array ( unsigned int size, ConstReverseIterator& iter )
				: mSize(size)
				, mData(NULL)
			{
				mData = DIA_NEW_ARRAY(Size(), T);
				MemorySet(mData, 0, sizeof(T)*size);

				int j = 0;
				for (; !iter.IsDone(); iter.Next()) 
				{
					At(j) = *iter.Current();
					j++;
				}
			}

			//-----------------------------------------------------------------------------
			template <class T> 
			template<class Evaluator> Array<T>::Array ( unsigned int size, ConstIterator& iter, const Evaluator& filter)
				: mSize(size)
				, mData(NULL)
			{
				mData = DIA_NEW_ARRAY(Size(), T);
				MemorySet(mData, 0, sizeof(T)*size);

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
			template <class T>
			Array<T>& Array<T>::Assign ( ConstPointer pData, unsigned int numberElements )
			{
				DIA_ASSERT(pData != NULL, "Invalid Data");
				DIA_ASSERT(numberElements <= Size(), "Will Outbound array");

				MemorySet(mData, 0, sizeof(T)*Size());
				MemoryCopy(mData, pData, sizeof(T)*numberElements);

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <class T>
			Array<T>& Array<T>::Assign ( ConstReference data, unsigned int numberElements )
			{
				DIA_ASSERT(numberElements <= Size(), "Will Outbound array");

				MemorySet(mData, 0, sizeof(T)* Size());
				MemoryCopy(mData, &data, sizeof(T)*numberElements);

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <class T>
			Array<T>& Array<T>::Assign ( const Array<T>& rhs )
			{
				MemorySet(mData, 0, sizeof(T)*Size());

				unsigned int x = rhs.Size();
				if( Size() <= rhs.Size() )
				{
					x = Size();
				}

				MemoryCopy(mData, rhs.mData, sizeof(T)*x);

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <class T>
			Array<T>& Array<T>::Assign ( const Array<T>& rhs, unsigned int startIndex, unsigned int numberElements )
			{
				DIA_ASSERT(numberElements >= 1, "You must want to get more then one element"); 
				DIA_ASSERT(numberElements <= Size(), "Will Outbound array"); 
				DIA_ASSERT(startIndex + numberElements <= rhs.Size(), "Will Outbound array"); 
				DIA_ASSERT(startIndex < rhs.Size(), "Start index must be greater then end index"); 

				MemorySet(mData, 0, sizeof(T)*Size());
				MemoryCopy(mData, &rhs.At(startIndex), sizeof(T)*numberElements);

				return *this;
			}
	
			//-----------------------------------------------------------------------------
			template <class T>
			Array<T>& Array<T>::Assign ( ConstIterator& iter )
			{
				MemorySet(mData, 0, sizeof(T)*Size());

				int j = 0;
				for (; !iter.IsDone(); iter.Next()) 
				{
					At(j) = *iter.Current();
					j++;
				}

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <class T>
			Array<T>& Array<T>::Assign ( ConstReverseIterator& iter )
			{
				MemorySet(mData, 0, sizeof(T)*Size());

				int j = 0;
				for (; !iter.IsDone(); iter.Next()) 
				{
					At(j) = *iter.Current();
					j++;
				}

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <class T>
			template<class Evaluator> Array<T>& Array<T>::Assign ( ConstIterator& iter, const Evaluator& filter )
			{
				MemorySet(mData, 0, sizeof(T)*Size());

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
			template <class T>
			Array<T>& Array<T>::operator=(const Array<T>& rhs)
			{
				MemorySet(mData, 0, sizeof(T)*Size());

				unsigned int x = rhs.Size();
				if( Size() <=  rhs.Size() )
				{
					x = Size();
				}
				
				MemoryCopy(mData, rhs.mData, sizeof(T)*x);

				return *this;
			}

			//-----------------------------------------------------------------------------
			template <class T>
			bool Array<T>::operator==( const Array<T>& rhs) const
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
			template <class T>
			bool Array<T>::operator!=( const Array<T>& rhs ) const
			{
				return !(*this == rhs);
			}

			//-----------------------------------------------------------------------------
 			template <class T> inline
			typename Array<T>::Reference Array<T>::operator[](unsigned int index)
			{
				DIA_ASSERT(index < Size(), "Index out of range");
				return (mData[index]);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			typename Array<T>::ConstReference Array<T>::operator[](unsigned int index) const
			{
				DIA_ASSERT(index < Size(), "Index out of range");
				return (mData[index]);
			}
			
			//-----------------------------------------------------------------------------
 			template <class T> inline
			typename Array<T>::Reference Array<T>::operator[](int index)
			{
				DIA_ASSERT(index >= 0 && index < static_cast<int>(Size()), "Index out of range");
				return (mData[index]);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			typename Array<T>::ConstReference Array<T>::operator[](int index) const
			{
				DIA_ASSERT(index >= 0 && index < Size(), "Index out of range");
				return (mData[index]);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			typename Array<T>::Iterator Array<T>::Begin()
			{
				return Iterator(&Front(), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			typename Array<T>::ConstIterator Array<T>::BeginConst()const
			{
				return ConstIterator(&Front(), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			typename Array<T>::ReverseIterator Array<T>::End()
			{
				return ReverseIterator(&Back(), &Front(), &Back());
			}
			
			//-----------------------------------------------------------------------------
			template <class T> inline
			typename Array<T>::ConstReverseIterator Array<T>::EndConst()const
			{
				return ConstReverseIterator(&Back(), &Front(), &Back());
			}
			
			//-----------------------------------------------------------------------------
			template <class T> inline
			typename Array<T>::Reference Array<T>::Front()
			{
				return *mData;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			typename Array<T>::ConstReference Array<T>::Front() const
			{
				return *mData;
			}
			
			//-----------------------------------------------------------------------------
			template <class T> inline
			typename Array<T>::Reference Array<T>::Back()
			{
				return *(mData + Size() - 1);
			}

			//-----------------------------------------------------------------------------	
			template <class T> inline
			typename Array<T>::ConstReference Array<T>::Back() const
			{
				return *(mData + Size() - 1);
			}
				
			//-----------------------------------------------------------------------------
			template <class T> inline	
			typename Array<T>::Reference Array<T>::At(unsigned int index)
			{
				DIA_ASSERT(index < Size(), "Index out of range");
				return (mData[index]);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			typename Array<T>::ConstReference Array<T>::At(unsigned int index) const
			{
				DIA_ASSERT(index < Size(), "Index out of range");
				return (mData[index]);
			}


			//-----------------------------------------------------------------------------
			template <class T> inline
			typename Array<T>::Iterator Array<T>::IteratorAt(unsigned int index)
			{
				return Iterator(&At(index), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			typename Array<T>::ConstIterator Array<T>::IteratorAtConst(unsigned int index) const
			{
				return ConstIterator(&At(index), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			typename Array<T>::ReverseIterator Array<T>::ReverseIteratorAt (unsigned int index)
			{
				return ReverseIterator(&At(index), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			typename Array<T>::ConstReverseIterator Array<T>::ReverseIteratorAtConst (unsigned int index) const
			{
				return ConstReverseIterator(&At(index), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			void Array<T>::Reserve(unsigned int size)
			{
				mSize = size;

				if (mData == NULL)
				{
					mData = DIA_NEW_ARRAY(Size(), T);
					MemorySet(mData, 0, sizeof(T)*Size());

				}
				else
				{
					T* temp = DIA_NEW_ARRAY(Size(), T);
					MemorySet(temp, 0, sizeof(T)*Size());
					MemoryCopy(temp, mData, Size());
					DIA_DELETE_ARRAY(mData);
					mData = temp;
				}		
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			unsigned int Array<T>::Size() const
			{
				return mSize;
			}
			
			//-----------------------------------------------------------------------------
			template <class T> inline
			int	Array<T>::FrequencyOfElement( ConstReference value )const
			{
				int frequency = 0;
				for (unsigned int i = 0; i < static_cast<int>(Size()); i++)
				{
					if (value == mData[i])
					{
						frequency++;
					}
				}

				return frequency;
			}
			
			//-----------------------------------------------------------------------------
			template <class T> inline
			void Array<T>::UniqueElements(Array<T>& unique, unsigned int& number)const
			{
				number = 0;
				for (unsigned int i = 0; i < Size(); i++)
				{
					T possibleUnique = mData[i];
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
			template <class T> inline
			void Array<T>::FrequencyUniqueElements(Array<T>& unique, Array<int>& uniqueFrequency, unsigned int& number)const
			{
				number = 0;
				for (unsigned int i = 0; i < Size(); i++)
				{
					T possibleUnique = mData[i];
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
			template <class T> inline
			bool Array<T>::IsSorted()const
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
			bool Array<T>::IsSorted(const Equality& functor)const
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
			void Array<T>::Sort()
			{
				PrivateSort( *this, 0, Size() - 1 );
			}

			//-----------------------------------------------------------------------------
			template <class T> template<class Equality> inline
			void Array<T>::Sort(const Equality& functor)
			{
				PrivateSort( functor, *this, 0, Size() - 1 );
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			int	Array<T>::FindIndex(ConstReference value) const
			{
				return FindBetweenIndex(value, 0, Size());
			}
			
			//-----------------------------------------------------------------------------
			template <class T> template<class Comparisson> inline
			int	Array<T>::FindIndex(ConstReference value, const Comparisson& functor) const
			{
				return FindBetweenIndex(value, functor, 0, Size());
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			int	Array<T>::FindBetweenIndex(ConstReference value, int startIndex, int endIndex)const
			{
				DIA_ASSERT(startIndex >= 0, "Start index must greater the zero");
				DIA_ASSERT(startIndex <= endIndex, "Start index must be smaller then end index");
				DIA_ASSERT(endIndex > 0, "End index out of range");
				DIA_ASSERT(static_cast<unsigned int>(endIndex) <= Size(), "End index out of range");

				int index = -1;
				for (int i = startIndex; i < endIndex; i++)
				{
					if (mData[i] == value) 
					{
						index = i;
						break;
					}
				}
				return index;
			}

			//-----------------------------------------------------------------------------
			template <class T> template<class Comparisson> inline
			int	Array<T>::FindBetweenIndex(ConstReference value, const Comparisson& functor, int startIndex, int endIndex)const
			{
				DIA_ASSERT(startIndex >= 0, "Start index must greater the zero");
				DIA_ASSERT(startIndex <= endIndex, "Start index must be smaller then end index");
				DIA_ASSERT(endIndex > 0, "End index out of range");
				DIA_ASSERT(static_cast<unsigned int>(endIndex) <= Size(), "End index out of range");

				int index = -1;
				for (int i = startIndex; i < endIndex; i++)
				{
					if (functor.Equal(mData[i], value)) 
					{
						index = i;
						break;
					}
				}
				return index;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			int	Array<T>::FindLastIndex(ConstReference value) const
			{
				return FindLastBetweenIndex(value, 0, Size() - 1);
			}

			//-----------------------------------------------------------------------------
			template <class T> template<class Comparisson> inline
			int	Array<T>::FindLastIndex(ConstReference value, const Comparisson& functor) const
			{
				return FindLastBetweenIndex(value, functor, 0, Size() - 1);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline
			int	Array<T>::FindLastBetweenIndex(ConstReference value, int startIndex, int endIndex)const
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
			int	Array<T>::FindLastBetweenIndex(ConstReference value, const Comparisson& functor, int startIndex, int endIndex)const
			{
				DIA_ASSERT(startIndex >= 0, "Start index must greater the zero");
				DIA_ASSERT(startIndex <= endIndex, "Start index must be smaller then end index");
				DIA_ASSERT(endIndex > 0, "End index out of range");
				DIA_ASSERT(static_cast<unsigned int>(endIndex) <= Size(), "End index out of range");

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
			int	Array<T>::FindSortedIndex(ConstReference value) const
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
			int	Array<T>::FindSortedIndex(ConstReference value, const Equality& functor) const
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
			int	Array<T>::FindLastSortedIndex(ConstReference value) const
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
			int	Array<T>::FindLastSortedIndex(ConstReference value, const Equality& functor) const
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
			int Array<T>::HighestEvalutionIndex(const Evaluate& functor) const
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
			void Array<T>::Swap(Array<T>& other)
			{
				Array<T> t(other);
				other.Reserve(this->Size());
				other.Assign(*this);
				Assign(t);
			}
			
			//-----------------------------------------------------------------------------
			template <class T> inline
			void Array<T>::PrivateSort(Array<T>& array, unsigned int bottom, unsigned int top)const
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
			void Array<T>::PrivateSort(const Equality& functor, Array<T>& array, unsigned int bottom, unsigned int top)const
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
			unsigned int  Array<T>::PrivatePartition(Array<T>& array, unsigned int bottom, unsigned int top)const
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
			unsigned int  Array<T>::PrivatePartition(const Equality& functor, Array<T>& array, unsigned int bottom, unsigned int top)const
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
			int	Array<T>::PrivateFindSortedIndex(ConstReference value) const
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
			int	Array<T>::PrivateFindSortedIndex(ConstReference value, const Equality& functor) const
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