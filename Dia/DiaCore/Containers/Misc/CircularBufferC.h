#pragma once

#include "DiaCore/Core/Assert.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaCore/Containers/Misc/CircularBufferIterator.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//------------------------------------------------------------------------------------
			//	Interface
			//------------------------------------------------------------------------------------
			template <class T, unsigned int size>
			class CircularBufferC
			{
			public:
				typedef	T			ValueType;
				typedef	T*			Pointer;
				typedef	T&			Reference;
				typedef	const T*	ConstPointer;
				typedef	const T&	ConstReference;

				typedef	CircularBufferIterator<T>				Iterator;
				typedef	CircularBufferConstIterator<T>			ConstIterator;

				CircularBufferC();

				template<unsigned int _size> CircularBufferC<T, size>&
														operator=		(const CircularBufferC<T, _size>& other);
				bool									operator==		(const CircularBufferC<T, size>& other) const;
				bool									operator!=		(const CircularBufferC<T, size>& other) const;
		 		
				bool									IsEmpty			() const;
				unsigned int							Capacity		() const;
				unsigned int							Size			() const;		
				
				Reference								Front			();	
				ConstReference							Front			() const;		
				Reference								Back			();
				ConstReference							Back			() const;										
					
				Iterator								Begin();
				ConstIterator							BeginConst() const;
				Iterator								End();
				ConstIterator							EndConst() const;

				void 									PushNext(ConstReference value);
				
				void									RemoveAll();
				
			protected:
				unsigned int CalculateNextIndex(unsigned int index)const;
				unsigned int CalculatePreviousIndex(unsigned int index)const;
				
				unsigned int mNextIndex;			// Index into the circular buffer
				DynamicArrayC<T, size> mData;		// Buffered data
			};
		}
	}
}

#include "DiaCore/Containers/Misc/CircularBufferC.inl"
