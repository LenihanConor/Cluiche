namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//------------------------------------------------------------------------------------
			//	CircularBufferIterator - Implementation
			//------------------------------------------------------------------------------------

			//-----------------------------------------------------------------------------
			template <class T> inline	
			CircularBufferIterator<T>::CircularBufferIterator(T* start, const T* begin, const T* end)
				: mBegin(begin)
				, mEnd(end)
				, mIter(start)
			{
				DIA_ASSERT(begin, "Empty array");
				DIA_ASSERT(end, "Empty array");
				DIA_ASSERT(start, "Empty array");

				DIA_ASSERT(start >= begin && start <= end, "Start must between begin and end");
			}
		
			//-----------------------------------------------------------------------------
			template <class T> inline	
			const T* CircularBufferIterator<T>::Begin()const
			{
				return mBegin;
			}
			
			//-----------------------------------------------------------------------------
			template <class T> inline	
			const T* CircularBufferIterator<T>::End()const 
			{
				return mEnd;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			void CircularBufferIterator<T>::Next()
			{
				mIter++;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			void CircularBufferIterator<T>::Previous()
			{
				--mIter;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool CircularBufferIterator<T>::IsDone() const
			{
				return (mIter < mBegin || mIter > mEnd);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			T* CircularBufferIterator<T>::Current() 
			{
				DIA_ASSERT(mIter >= mBegin && mIter <= mEnd, "Current iterator out of bounds");
					
				return mIter;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			const T* CircularBufferIterator<T>::Current() const
			{
				DIA_ASSERT(mIter >= mBegin && mIter <= mEnd, "Current iterator out of bounds");

				return mIter;
			}
			
			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool CircularBufferIterator<T>::operator==(const CircularBufferIterator<T>& other) const
			{
				return (mIter == other.mIter);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool CircularBufferIterator<T>::operator!=(const CircularBufferIterator<T>& other) const
			{
				return (mIter != other.mIter);
			}
			
			
			//------------------------------------------------------------------------------------
			//	CircularBufferConstIterator - Implementation
			//------------------------------------------------------------------------------------


			//-----------------------------------------------------------------------------
			template <class T> inline	
			CircularBufferConstIterator<T>::CircularBufferConstIterator(const T* start, const T* begin, const T* end)
				: mBegin(begin)
				, mEnd(end)
				, mIter(start)
			{
				DIA_ASSERT(begin, "Empty array");
				DIA_ASSERT(end, "Empty array");
				DIA_ASSERT(start, "Empty array");

				DIA_ASSERT(begin <= end, "Start must be less then or equal to beginning");
				DIA_ASSERT(start >= begin && start <= end, "Start must between begin and end");
			}
			
			//-----------------------------------------------------------------------------
			template <class T> inline	
			const T* CircularBufferConstIterator<T>::Begin()const
			{
				return mBegin;
			}
			
			//-----------------------------------------------------------------------------
			template <class T> inline	
			const T* CircularBufferConstIterator<T>::End()const 
			{
				return mEnd;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			void CircularBufferConstIterator<T>::Next()
			{
				if (mIter == mEnd)
				{
					mIter = mBegin;
				}
				else
				{
					mIter++;
				}
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			void CircularBufferConstIterator<T>::Previous()
			{
				if (mIter == mBegin)
				{
					mIter = mEnd;
				}
				else
				{
					--mIter;
				}
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			const T* CircularBufferConstIterator<T>::Current() const
			{
				DIA_ASSERT(mIter >= mBegin && mIter <= mEnd, "Current iterator out of bounds");

				return mIter;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool CircularBufferConstIterator<T>::operator==(const CircularBufferConstIterator<T>& other) const
			{
				return (mIter == other.mIter);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool CircularBufferConstIterator<T>::operator!=(const CircularBufferConstIterator<T>& other) const
			{
				return (mIter != other.mIter);
			}
		}
	}
}