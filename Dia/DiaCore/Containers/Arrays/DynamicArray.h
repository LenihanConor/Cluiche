#pragma once

#include <string.h>

#include "DiaCore/Core/Assert.h"
#include "DiaCore/Containers/Arrays/ArrayIterator.h"
#include "DiaCore/Containers/Arrays/ReverseArrayIterator.h"

//------------------------------------------------------------------------------------
//			
//			|---------------------------------------------------+--------------------
//			|			Array		|		DynamicArray		| Dynamic Allocation
//			|-----------------------+---------------------------+--------------------
//			|	DynamicArray		|		DynamicDynamicArray	| Static Allocation
//			|-----------------------+---------------------------+--------------------
//			|		Fixed Size		|		Variable Size		|
//
//------------------------------------------------------------------------------------

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//------------------------------------------------------------------------------------
			//	Interface
			//------------------------------------------------------------------------------------
			template <class T>
			class DynamicArray
			{
			public:
				typedef	T			ValueType;
				typedef	T*			Pointer;
				typedef	T&			Reference;
				typedef	const T*	ConstPointer;
				typedef	const T&	ConstReference;

				typedef	ArrayIterator<T>				Iterator;
				typedef	ArrayConstIterator<T>			ConstIterator;
				typedef	ReverseArrayIterator<T>			ReverseIterator;
				typedef	ReverseArrayConstIterator<T>	ConstReverseIterator;

				// these are all use memcpy instead of a copy construct 
				DynamicArray ();
				~DynamicArray ();
				explicit DynamicArray ( unsigned int capacity );		
				explicit DynamicArray ( ConstPointer pData, unsigned int numberElements );			
				explicit DynamicArray ( ConstReference data, unsigned int numberElements ); 						
				explicit DynamicArray ( const DynamicArray<T>& rhs );		
				explicit DynamicArray ( const DynamicArray<T>& rhs, unsigned int startIndex, unsigned int numberElements );  
				explicit DynamicArray ( unsigned int capacity, ConstIterator& iter ); 
				explicit DynamicArray ( unsigned int capacity, ConstReverseIterator& iter ); 
				template<class Evaluator> explicit DynamicArray (unsigned int capacity, ConstIterator& iter, const Evaluator& filter );

				// these are all use memcpy instead of a copy construct
				DynamicArray<T>&								Assign ( ConstPointer pData, unsigned int numberElements);
				DynamicArray<T>&								Assign ( ConstReference data, unsigned int numberElements);
				DynamicArray<T>&								Assign ( const DynamicArray<T>& rhs );
				DynamicArray<T>&								Assign ( const DynamicArray<T>& rhs, unsigned int startIndex, unsigned int numberElements = _size );
				DynamicArray<T>&								Assign ( ConstIterator& iter );
				DynamicArray<T>&								Assign ( ConstReverseIterator& iter );
				template<class Evaluator> DynamicArray<T>&		Assign ( ConstIterator& iter, const Evaluator& filter );

				/*DynamicArray<T, size>&								AssignWithCopyConstructors (ConstPointer pData, unsigned int numberElements);
				DynamicArray<T, size>&									AssignWithCopyConstructors (ConstReference data, unsigned int numberElements);
				template<unsigned int _size>DynamicArray<T, size>&		AssignWithCopyConstructors ( const DynamicArray<T,_size>& rhs );
				template<unsigned int _size>DynamicArray<T, size>&		AssignWithCopyConstructors ( const DynamicArray<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements = _size );
				DynamicArray<T, size>&									AssignWithCopyConstructors ( ConstIterator& iter );
				DynamicArray<T, size>&									AssignWithCopyConstructors ( ConstReverseIterator& iter );
				template<class Evaluator> DynamicArray<T, size>&		AssignWithCopyConstructors ( ConstIterator& iter, const Evaluator& filter );*/

				DynamicArray<T>&						operator=		(const DynamicArray<T>& other);
				bool									operator==		(const DynamicArray<T>& other) const; 
				bool									operator!=		(const DynamicArray<T>& other) const;

				void									Reserve			( unsigned int capacity );
				bool									IsEmpty			() const;
				bool									IsFull			() const;
				unsigned int							Capacity		() const;
				unsigned int							Size			() const;		

				Reference								operator[]		(int index);										
				ConstReference							operator[]		(int index) const;	
				Reference								operator[]		(unsigned int index);										
				ConstReference							operator[]		(unsigned int index) const;									

				Reference								At				(unsigned int index);											
				ConstReference							At				(unsigned int index) const;
				Reference								Front			();	
				ConstReference							Front			() const;		
				Reference								Back			();
				ConstReference							Back			() const;

				Iterator								IteratorAt				(unsigned int index);											
				ConstIterator							IteratorAtConst			(unsigned int index) const;
				ReverseIterator							ReverseIteratorAt		(unsigned int index);											
				ConstReverseIterator					ReverseIteratorAtConst	(unsigned int index) const;

				Iterator								Begin					();														
				ConstIterator							BeginConst				() const;												
				ReverseIterator							End						();														
				ConstReverseIterator					EndConst				() const;

				int										FrequencyOfElement		(ConstReference value)const;
				void									UniqueElements			(DynamicArray<T>& unique)const;
				void									FrequencyUniqueElements	(DynamicArray<T>& unique, DynamicArray<int>& uniqueFrequency)const;

				bool									IsSorted		()const;
				template<class Equality> bool			IsSorted		(const Equality& functor)const;

				void									Sort			();
				template<class Equality> void			Sort			(const Equality& functor);

				void									Swap			(DynamicArray<T>& other);
			
				void									AddDefault				();	
				void									Add						(ConstReference value);

				void									FillDefault				();
				void									Fill					(ConstReference value);

				void									AddAt					(ConstReference value, unsigned int index);
				void									Remove					();
				void									RemoveAt				(unsigned int index);
				void									RemoveFirst				(ConstReference value);
				void									RemoveLast				(ConstReference value);		
				void									RemoveAll				();
				void									RemoveAll				(ConstReference value);
				template<class Evaluate> void			RemoveAll				(const Evaluate& functor);

				// Search through the array from the top and return an index
				int										FindIndex			(ConstReference value) const;
				template<class Comparisson> int			FindIndex			(ConstReference value, const Comparisson& functor) const;
				int										FindBetweenIndex	(ConstReference value, int startIndex, int endIndex)const;
				template<class Comparisson> int			FindBetweenIndex	(ConstReference value, const Comparisson& functor, int startIndex, int endIndex)const;

				// Search through the array from the bottom and return an index
				int										FindLastIndex		(ConstReference value) const;
				template<class Comparisson> int			FindLastIndex		(ConstReference value, const Comparisson& functor) const;
				int										FindLastBetweenIndex(ConstReference value, int startIndex, int endIndex)const;
				template<class Comparisson> int			FindLastBetweenIndex(ConstReference value, const Comparisson& functor, int startIndex, int endIndex)const;

				// Search through the array if it is sorted
				int										FindSortedIndex		(ConstReference value) const;
				template<class Equality> int			FindSortedIndex		(ConstReference value, const Equality& functor) const;
				int										FindLastSortedIndex	(ConstReference value) const;
				template<class Equality> int			FindLastSortedIndex	(ConstReference value, const Equality& functor) const;

				// Evaluates all the variables and returns the highest evaluation
				template<class Evaluate> int			HighestEvalutionIndex	(const Evaluate& functor) const;

			protected:
				void									PrivateSort		(DynamicArray<T>& array, unsigned int bottom, unsigned int top)const;
				template<class Equality> void			PrivateSort		(const Equality& functor, DynamicArray<T>& array, unsigned int bottom, unsigned int top)const;
				unsigned int							PrivatePartition(DynamicArray<T>& array, unsigned int bottom, unsigned int top)const;
				template<class Equality> unsigned int	PrivatePartition(const Equality& functor, DynamicArray<T>& array, unsigned int bottom, unsigned int top)const;

				int										PrivateFindSortedIndex		(ConstReference value) const;
				template<class Equality> int			PrivateFindSortedIndex		(ConstReference value, const Equality& functor) const;

				unsigned int							mSize;
				unsigned int							mCapacity;
				T*										mData;
			};
		}
	}
}

#include "DiaCore/Containers/Arrays/DynamicArray.inl"
