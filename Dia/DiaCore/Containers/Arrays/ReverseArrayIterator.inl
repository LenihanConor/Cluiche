namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//------------------------------------------------------------------------------------
			//	ReverseArrayIterator - Implementation
			//------------------------------------------------------------------------------------


			//-----------------------------------------------------------------------------
			template <class T> inline	
			ReverseArrayIterator<T>::ReverseArrayIterator(T* start, const T* begin, const T* end)
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
			const T* ReverseArrayIterator<T>::Begin()const
			{
				return mEnd;
			}
			
			//-----------------------------------------------------------------------------
			template <class T> inline	
			const T* ReverseArrayIterator<T>::End()const 
			{
				return mBegin;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			void ReverseArrayIterator<T>::Next()
			{	
				--mIter;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			void ReverseArrayIterator<T>::Previous()
			{
				mIter++;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ReverseArrayIterator<T>::IsDone() const
			{
				return (mIter < mStart || mIter > mEnd);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			T* ReverseArrayIterator<T>::Current() 
			{
				DIA_ASSERT(mIter >= mBegin && mIter <= mEnd, "Current iterator out of bounds");

				return mIter;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			const T* ReverseArrayIterator<T>::Current() const
			{
				DIA_ASSERT(mIter >= mBegin && mIter <= mEnd, "Current iterator out of bounds");

				return mIter;
			}
			
			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ReverseArrayIterator<T>::operator==(const ReverseArrayIterator<T>& other) const
			{
				return (mIter == other.mIter);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ReverseArrayIterator<T>::operator!=(const ReverseArrayIterator<T>& other) const
			{
				return (mIter != other.mIter);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ReverseArrayIterator<T>::operator< (const ReverseArrayIterator<T>& other) const
			{
				DIA_ASSERT(other.Current() >= mBegin && other.Current() <= mEnd, "other must between begin and end");

				return (mIter > other.Current());
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ReverseArrayIterator<T>::operator<=(const ReverseArrayIterator<T>& other) const
			{
				DIA_ASSERT(other.Current() >= mBegin && other.Current() <= mEnd, "other must between begin and end");

				return (mIter >= other.Current());
			}
				
			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ReverseArrayIterator<T>::operator>(const ReverseArrayIterator<T>& other) const
			{
				DIA_ASSERT(other.Current() >= mBegin && other.Current() <= mEnd, "other must between begin and end");

				return (mIter < other.Current());
			}
			
			
			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ReverseArrayIterator<T>::operator>=(const ReverseArrayIterator<T>& other) const
			{
				DIA_ASSERT(other.Current() >= mBegin && other.Current() <= mEnd, "other must between begin and end");	
				
				return (mIter <= other.Current());
			}



			//------------------------------------------------------------------------------------
			//	ReverseArrayConstIterator - Implementation
			//------------------------------------------------------------------------------------


			//-----------------------------------------------------------------------------
			template <class T> inline	
			ReverseArrayConstIterator<T>::ReverseArrayConstIterator(const T* start, const T* begin, const T* end)
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
			ReverseArrayConstIterator<T>::ReverseArrayConstIterator(ReverseArrayIterator<T>& rhs)
				: mBegin(rhs.End())
				, mEnd(rhs.Begin())
				, mIter(rhs.Current())
			{
				DIA_ASSERT(mBegin, "Empty array");
				DIA_ASSERT(mEnd, "Empty array");
				DIA_ASSERT(mIter, "Empty array");

				DIA_ASSERT(mIter >= mBegin && mIter <= mEnd, "Start must between begin and end");
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			const T* ReverseArrayConstIterator<T>::Begin()const
			{
				return mEnd;
			}
			
			//-----------------------------------------------------------------------------
			template <class T> inline	
			const T* ReverseArrayConstIterator<T>::End()const 
			{
				return mBegin;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			void ReverseArrayConstIterator<T>::Next()
			{
				--mIter;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			void ReverseArrayConstIterator<T>::Previous()
			{
				mIter++;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ReverseArrayConstIterator<T>::IsDone() const
			{
				return (mIter < mBegin || mIter > mEnd);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			const T* ReverseArrayConstIterator<T>::Current() const
			{
				DIA_ASSERT(mIter >= mBegin && mIter <= mEnd, "Current iterator out of bounds");

				return mIter;
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ReverseArrayConstIterator<T>::operator==(const ReverseArrayConstIterator<T>& other) const
			{
				return (mIter == other.mIter);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ReverseArrayConstIterator<T>::operator!=(const ReverseArrayConstIterator<T>& other) const
			{
				return (mIter != other.mIter);
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ReverseArrayConstIterator<T>::operator<(const ReverseArrayConstIterator<T>& other) const
			{
				DIA_ASSERT(other.Current() >= mBegin && other.Current() <= mEnd, "other must between begin and end");

				return (mIter > other.Current());
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ReverseArrayConstIterator<T>::operator<=(const ReverseArrayConstIterator<T>& other) const
			{
				DIA_ASSERT(other.Current() >= mBegin && other.Current() <= mEnd, "other must between begin and end");

				return (mIter >= other.Current());
			}

			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ReverseArrayConstIterator<T>::operator>(const ReverseArrayConstIterator<T>& other) const
			{
				DIA_ASSERT(other.Current() >= mBegin && other.Current() <= mEnd, "other must between begin and end");

				return (mIter < other.Current());
			}
			
			//-----------------------------------------------------------------------------
			template <class T> inline	
			bool ReverseArrayConstIterator<T>::operator>=(const ReverseArrayConstIterator<T>& other) const
			{
				DIA_ASSERT(other.Current() >= mBegin && other.Current() <= mEnd, "other must between begin and end");

				return (mIter <= other.Current());
			}
		}
	}
}