#pragma once

#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//--------------------------------------------------------------------------
			template <class T>
			class ReverseArrayIterator
			{
			public:
				ReverseArrayIterator(T* start, const T* begin, const T* end);
		
				const T*	Begin()const;
				const T*	End()const;
		
				void		Next();
				void		Previous();
				
				bool		IsDone() const;
				T*			Current();
				const T*	Current() const;

				bool		operator==		(const ReverseArrayIterator<T>& other) const; 
				bool		operator!=		(const ReverseArrayIterator<T>& other) const;
				
				bool		operator<		(const ReverseArrayIterator<T>& other) const; 
				bool		operator<=		(const ReverseArrayIterator<T>& other) const;
				
				bool		operator>		(const ReverseArrayIterator<T>& other) const; 
				bool		operator>=		(const ReverseArrayIterator<T>& other) const;

			private:
				const T*	mBegin;
				const T*	mEnd;
				T*			mIter;
			};

			//--------------------------------------------------------------------------
			template <class T>
			class ReverseArrayConstIterator
			{
			public:
				ReverseArrayConstIterator(const T* start, const T* begin, const T* end);
				ReverseArrayConstIterator(ReverseArrayIterator<T>& rhs);

				const T*	Begin()const;
				const T*	End()const;
		
				void		Next();
				void		Previous();
				
				bool		IsDone() const;
				const T*	Current() const;
					
				bool		operator==		(const ReverseArrayConstIterator<T>& other) const; 
				bool		operator!=		(const ReverseArrayConstIterator<T>& other) const;

				bool		operator<		(const ReverseArrayConstIterator<T>& other) const; 
				bool		operator<=		(const ReverseArrayConstIterator<T>& other) const;
				
				bool		operator>		(const ReverseArrayConstIterator<T>& other) const; 
				bool		operator>=		(const ReverseArrayConstIterator<T>& other) const;

			private:
				const T*	mBegin;
				const T*	mEnd;
				const T*	mIter;
			};
		}
	}
} 

#include "DiaCore/Containers/Arrays/ReverseArrayIterator.inl"