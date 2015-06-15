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
			class CircularBufferIterator
			{
			public:

				CircularBufferIterator(T* start, const T* begin, const T* end);
		
				const T*	Begin()const;
				const T*	End()const;
		
				void		Next();
				void		Previous();
				
				bool		IsDone() const;

				T*			Current();
				const T*	Current() const;

				bool		operator==		(const CircularBufferIterator<T>& other) const; 
				bool		operator!=		(const CircularBufferIterator<T>& other) const;
				
			private:
				const T*	mBegin;
				const T*	mEnd;
				T*			mIter;
			};

			//--------------------------------------------------------------------------
			template <class T>
			class CircularBufferConstIterator
			{
			public:
				CircularBufferConstIterator(const T* start, const T* begin, const T* end);

				const T*	Begin()const;
				const T*	End()const;
		
				void		Next();
				void		Previous();

				const T*	Current() const;
					
				bool		operator==		(const CircularBufferConstIterator<T>& other) const; 
				bool		operator!=		(const CircularBufferConstIterator<T>& other) const;

			private:
				const T*	mBegin;
				const T*	mEnd;
				const T*	mIter;
			};
		}
	}
} 

#include "DiaCore/Containers/Misc/CircularBufferIterator.inl"

