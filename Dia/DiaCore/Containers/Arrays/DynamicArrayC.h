#pragma once

#include <string.h>

#include "DiaCore/Core/Assert.h"
#include "DiaCore/Containers/Arrays/ArrayIterator.h"
#include "DiaCore/Containers/Arrays/ReverseArrayIterator.h"
#include "DiaCore/Type/TypeDeclarationMacros.h"
//------------------------------------------------------------------------------------
//			
//			|---------------------------------------------------+--------------------
//			|			Array		|		DynamicArray		| Dynamic Allocation
//			|-----------------------+---------------------------+--------------------
//			|			ArrayC		|		DynamicArrayC		| Static Allocation
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
			template <class T, unsigned int capacity>
			class DynamicArrayC
			{
			public:
				DIA_TYPE_DECLARATION;

				typedef	T			ValueType;
				typedef	T*			Pointer;
				typedef	T&			Reference;
				typedef	const T*	ConstPointer;
				typedef	const T&	ConstReference;

				typedef	ArrayIterator<T>				Iterator;
				typedef	ArrayConstIterator<T>			ConstIterator;
				typedef	ReverseArrayIterator<T>			ReverseIterator;
				typedef	ReverseArrayConstIterator<T>	ConstReverseIterator;
	  
				DynamicArrayC 	();															
				explicit DynamicArrayC  (ConstPointer pData, unsigned int numberElements);			
				explicit DynamicArrayC  (ConstReference data, unsigned int numberElements); 						
				template<unsigned int _size> explicit DynamicArrayC (const DynamicArrayC<T,_size>& rhs);						
				template<unsigned int _size> explicit DynamicArrayC (const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements = _size );  
				explicit DynamicArrayC ( ConstIterator& iter ); 
				explicit DynamicArrayC ( ConstReverseIterator& iter ); 
				template<class Evaluator> explicit DynamicArrayC ( ConstIterator& iter, const Evaluator& filter);

				DynamicArrayC<T, capacity>&								Assign (ConstPointer pData, unsigned int numberElements);
				DynamicArrayC<T, capacity>&								Assign (ConstReference data, unsigned int numberElements);
				template<unsigned int _size>DynamicArrayC<T, capacity>&	Assign ( const DynamicArrayC<T,_size>& rhs );
				template<unsigned int _size>DynamicArrayC<T, capacity>&	Assign ( const DynamicArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements = _size );
				DynamicArrayC<T, capacity>&								Assign ( ConstIterator& iter );
				DynamicArrayC<T, capacity>&								Assign ( ConstReverseIterator& iter );
				template<class Evaluator> DynamicArrayC<T, capacity>&	Assign ( ConstIterator& iter, const Evaluator& filter );

				template<unsigned int _size> DynamicArrayC<T, capacity>& 
														operator=		(const DynamicArrayC<T,_size>& other);
				bool									operator==		(const DynamicArrayC<T, capacity>& other) const; 
				bool									operator!=		(const DynamicArrayC<T, capacity>& other) const;
		 		
				bool									IsEmpty					() const;
				bool									IsFull					() const;
				unsigned int							Capacity				() const;
				unsigned int							Size					() const;

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
			
				Iterator								Begin			();														
				ConstIterator							BeginConst		() const;												
				ReverseIterator							End				();														
				ConstReverseIterator					EndConst		() const;												
				
				int										FrequencyOfElement		( ConstReference value )const;
				void									UniqueElements			(DynamicArrayC<T, capacity>& unique)const;
				void									FrequencyUniqueElements	(DynamicArrayC<T, capacity>& unique, DynamicArrayC<int, capacity>& uniqueFrequency)const;
		
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

				bool									IsSorted		()const;
				template<class Equality> bool			IsSorted		(const Equality& functor)const;

				void									Sort			();
				template<class Equality> void			Sort			(const Equality& functor);

				void									Swap			(DynamicArrayC<T, capacity>& other);

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
				void									PrivateSort			(DynamicArrayC<T, capacity>& array, unsigned int bottom, unsigned int top)const;
				template<class Equality> void			PrivateSort			(const Equality& functor, DynamicArrayC<T, capacity>& array, unsigned int bottom, unsigned int top)const;
				unsigned int							PrivatePartition	(DynamicArrayC<T, capacity>& array, unsigned int bottom, unsigned int top)const;
				template<class Equality> unsigned int	PrivatePartition	(const Equality& functor, DynamicArrayC<T, capacity>& array, unsigned int bottom, unsigned int top)const;
				
				int										PrivateFindSortedIndex		(ConstReference value) const;
				template<class Equality> int			PrivateFindSortedIndex		(ConstReference value, const Equality& functor) const;
				

				unsigned int							mSize;
				T										mData [capacity];
			};
		}
		}
}

#include "DiaCore/Containers/Arrays/DynamicArrayC.inl"