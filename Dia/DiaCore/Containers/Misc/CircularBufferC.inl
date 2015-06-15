namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size>
			CircularBufferC<T, size>::CircularBufferC()
				: mNextIndex(0)
				, mData()
			{}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size>
			template<unsigned int _size> CircularBufferC<T, size>& CircularBufferC<T, size>::operator=(const CircularBufferC<T, _size>& other)
			{
				mNextIndex = other.mNextIndex;
				mData = other.mData;
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size>
			bool CircularBufferC<T, size>::operator==(const CircularBufferC<T, size>& other) const
			{
				return (mNextIndex == other.mNextIndex && mData == other.mData);
			}
			
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size>
			bool CircularBufferC<T, size>::operator!=(const CircularBufferC<T, size>& other) const
			{
				return !(*this == rhs);
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size>
			bool CircularBufferC<T, size>::IsEmpty() const
			{
				return mData.IsEmpty();
			}
							
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			unsigned int CircularBufferC<T, size>::Size() const
			{
				return mData.Size();
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			typename CircularBufferC<T, size>::Reference CircularBufferC<T, size>::Front()
			{
				return mData[CalculatePreviousIndex(mNextIndex)];
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			typename CircularBufferC<T, size>::ConstReference CircularBufferC<T, size>::Front() const
			{
				return mData[CalculatePreviousIndex(mNextIndex)];
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			typename CircularBufferC<T, size>::Reference CircularBufferC<T, size>::Back()
			{		
				int result = 0;
				if (mData.IsFull()) // ie we are wrapping around
				{
					result = mNextIndex;
				}

				return mData[result];
			}

			//-----------------------------------------------------------------------------	
			template <class T, unsigned int size> inline
			typename CircularBufferC<T, size>::ConstReference CircularBufferC<T, size>::Back() const
			{
				int result = 0;
				if (mData.IsFull()) // ie we are wrapping around
				{
					result = mNextIndex;
				}

				return mData[result];
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			typename CircularBufferC<T, size>::Iterator CircularBufferC<T, size>::Begin()
			{
				return Iterator(&Front(), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			typename CircularBufferC<T, size>::ConstIterator CircularBufferC<T, size>::BeginConst()const
			{
				return ConstIterator(&Front(), &Front(), &Back());
			}

			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			typename CircularBufferC<T, size>::Iterator CircularBufferC<T, size>::End()
			{
				return Iterator(&Back(), &Front(), &Back());
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			typename CircularBufferC<T, size>::ConstIterator CircularBufferC<T, size>::EndConst()const
			{
				return ConstIterator(&Back(), &Front(), &Back());
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			void CircularBufferC<T, size>::PushNext(ConstReference value)
			{
				if (!mData.IsFull())
				{
					mData.AddDefault();
				}

				mData[mNextIndex] = value;
				
				mNextIndex = CalculateNextIndex(mNextIndex);
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline			
			void CircularBufferC<T, size>::RemoveAll()
			{
				mData.RemoveAll();
			}
			
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			unsigned int CircularBufferC<T, size>::CalculateNextIndex(unsigned int index)const
			{
				DIA_ASSERT(index < size, "mNextIndex is out of scope");
			
				unsigned int result = index;
				
				result++;
			
				if (result == size)
				{
					result = 0;
				}
				
				return result;
			}
						
			//-----------------------------------------------------------------------------
			template <class T, unsigned int size> inline
			unsigned int CircularBufferC<T, size>::CalculatePreviousIndex(unsigned int index)const
			{
				DIA_ASSERT(index < size, "mNextIndex is out of scope");
				
				unsigned int result = index;
				
				if (result == 0)
				{
					if (mData.IsFull())
					{
						result = size - 1;
					}
					else
					{
						result = 0;
					}
				}
				else
				{
					--result;
				}
				
				return result;
			}
		}
	}
}