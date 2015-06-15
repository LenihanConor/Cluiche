namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//------------------------------------------------------------------------------------
			//	ArrayIterator - Implementation
			//------------------------------------------------------------------------------------

			//-----------------------------------------------------------------------------
			template <class T> inline	
			ArrayIterator<T>::ArrayIterator(T* start, const T* begin, const T* end)
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
			const T* ArrayIterator<T>::Begin()const
			{
				return mBegin;
			}
			
			//-----------------------------------------------------------------------------
			template <class T> inline	
			const T* ArrayIterator<T>::End()const 
			{
				return mEnd;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			void ArrayIterator<T>::Next()
			{
				mIter++;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			void ArrayIterator<T>::Previous()
			{
				--mIter;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ArrayIterator<T>::IsDone() const
			{
				return (mIter < mBegin || mIter > mEnd);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			T* ArrayIterator<T>::Current() 
			{
				DIA_ASSERT(mIter >= mBegin && mIter <= mEnd, "Current iterator out of bounds");
					
				return mIter;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			const T* ArrayIterator<T>::Current() const
			{
				DIA_ASSERT(mIter >= mBegin && mIter <= mEnd, "Current iterator out of bounds");

				return mIter;
			}
			
			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ArrayIterator<T>::operator==(const ArrayIterator<T>& other) const
			{
				return (mIter == other.mIter);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ArrayIterator<T>::operator!=(const ArrayIterator<T>& other) const
			{
				return (mIter != other.mIter);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ArrayIterator<T>::operator< (const ArrayIterator<T>& other) const
			{
				DIA_ASSERT(other.Current() >= mBegin && other.Current() <= mEnd, "other must between begin and end");

				return (mIter < other.Current());
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ArrayIterator<T>::operator<=(const ArrayIterator<T>& other) const
			{
				DIA_ASSERT(other.Current() >= mBegin && other.Current() <= mEnd, "other must between begin and end");

				return (mIter <= other.Current());
			}
				
			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ArrayIterator<T>::operator>(const ArrayIterator<T>& other) const
			{
				DIA_ASSERT(other.Current() >= mBegin && other.Current() <= mEnd, "other must between begin and end");

				return (mIter > other.Current());
			}
			
			
			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ArrayIterator<T>::operator>=(const ArrayIterator<T>& other) const
			{
				DIA_ASSERT(other.Current() >= mBegin && other.Current() <= mEnd, "other must between begin and end");

				return (mIter >= other.Current());
			}


			
			
			//------------------------------------------------------------------------------------
			//	ArrayConstIterator - Implementation
			//------------------------------------------------------------------------------------


			//-----------------------------------------------------------------------------
			template <class T> inline	
			ArrayConstIterator<T>::ArrayConstIterator(const T* start, const T* begin, const T* end)
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
			ArrayConstIterator<T>::ArrayConstIterator(ArrayIterator<T>& rhs)
				: mBegin(rhs.Begin())
				, mEnd(rhs.End())
				, mIter(rhs.Current())
			{
				DIA_ASSERT(mBegin, "Empty array");
				DIA_ASSERT(mEnd, "Empty array");
				DIA_ASSERT(mIter, "Empty array");

				DIA_ASSERT(mIter >= mBegin && mIter <= mEnd, "Start must between begin and end");
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			const T* ArrayConstIterator<T>::Begin()const
			{
				return mBegin;
			}
			
			//-----------------------------------------------------------------------------
			template <class T> inline	
			const T* ArrayConstIterator<T>::End()const 
			{
				return mEnd;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			void ArrayConstIterator<T>::Next()
			{
				mIter++;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			void ArrayConstIterator<T>::Previous()
			{
				--mIter;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ArrayConstIterator<T>::IsDone() const
			{
				return (mIter < mBegin || mIter > mEnd);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			const T* ArrayConstIterator<T>::Current() const
			{
				DIA_ASSERT(mIter >= mBegin && mIter <= mEnd, "Current iterator out of bounds");

				return mIter;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ArrayConstIterator<T>::operator==(const ArrayConstIterator<T>& other) const
			{
				return (mIter == other.mIter);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ArrayConstIterator<T>::operator!=(const ArrayConstIterator<T>& other) const
			{
				return (mIter != other.mIter);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ArrayConstIterator<T>::operator<(const ArrayConstIterator<T>& other) const
			{
				DIA_ASSERT(other.Current() >= mBegin && other.Current() <= mEnd, "other must between begin and end");

				return (mIter < other.Current());
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ArrayConstIterator<T>::operator<=(const ArrayConstIterator<T>& other) const
			{
				DIA_ASSERT(other.Current() >= mBegin && other.Current() <= mEnd, "other must between begin and end");

				return (mIter <= other.Current());
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ArrayConstIterator<T>::operator>(const ArrayConstIterator<T>& other) const
			{
				DIA_ASSERT(other.Current() >= mBegin && other.Current() <= mEnd, "other must between begin and end");

				return (mIter > other.Current());
			}
			
			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ArrayConstIterator<T>::operator>=(const ArrayConstIterator<T>& other) const
			{
				DIA_ASSERT(other.Current() >= mBegin && other.Current() <= mEnd, "other must between begin and end");

				return (mIter >= other.Current());
			}
		}
	}
}