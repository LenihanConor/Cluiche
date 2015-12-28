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
			template <class T, unsigned int size>
			class ArrayC
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

				// these are all use memcpy instead of a copy construct 
				ArrayC 	();															
				explicit ArrayC  (ConstPointer pData, unsigned int numberElements);			
				explicit ArrayC  (ConstReference data, unsigned int numberElements); 						
				template<unsigned int _size> explicit ArrayC (const ArrayC<T,_size>& rhs);						
				template<unsigned int _size> explicit ArrayC (const ArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements = _size );  
				explicit ArrayC ( ConstIterator& iter ); 
				explicit ArrayC ( ConstReverseIterator& iter ); 
				template<class Evaluator> explicit ArrayC ( ConstIterator& iter, const Evaluator& filter);

				// these are all use memcpy instead of a copy construct
				ArrayC<T, size>&								Assign ( ConstPointer pData, unsigned int numberElements);
				ArrayC<T, size>&								Assign ( ConstReference data, unsigned int numberElements);
				template<unsigned int _size>ArrayC<T, size>&	Assign ( const ArrayC<T,_size>& rhs );
				template<unsigned int _size>ArrayC<T, size>&	Assign ( const ArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements = _size );
				ArrayC<T, size>&								Assign ( ConstIterator& iter );
				ArrayC<T, size>&								Assign ( ConstReverseIterator& iter );
				template<class Evaluator> ArrayC<T, size>&		Assign ( ConstIterator& iter, const Evaluator& filter );
				
				/*ArrayC<T, size>&								AssignWithCopyConstructors (ConstPointer pData, unsigned int numberElements);
				ArrayC<T, size>&								AssignWithCopyConstructors (ConstReference data, unsigned int numberElements);
				template<unsigned int _size>ArrayC<T, size>&	AssignWithCopyConstructors ( const ArrayC<T,_size>& rhs );
				template<unsigned int _size>ArrayC<T, size>&	AssignWithCopyConstructors ( const ArrayC<T,_size>& rhs, unsigned int startIndex, unsigned int numberElements = _size );
				ArrayC<T, size>&								AssignWithCopyConstructors ( ConstIterator& iter );
				ArrayC<T, size>&								AssignWithCopyConstructors ( ConstReverseIterator& iter );
				template<class Evaluator> ArrayC<T, size>&		AssignWithCopyConstructors ( ConstIterator& iter, const Evaluator& filter );*/

				template<unsigned int _size> ArrayC<T, size>& 
														operator=		(const ArrayC<T,_size>& other);
				bool									operator==		(const ArrayC<T, size>& other) const; 
				bool									operator!=		(const ArrayC<T, size>& other) const;
		 		
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
				void									UniqueElements			(ArrayC<T, size>& unique, unsigned int& number)const;
				void									FrequencyUniqueElements	(ArrayC<T, size>& unique, ArrayC<int, size>& uniqueFrequency, unsigned int& number)const;
				
				bool									IsSorted		()const;
				template<class Equality> bool			IsSorted		(const Equality& functor)const;

				void									Sort			();
				template<class Equality> void			Sort			(const Equality& functor);

				void									Swap			(ArrayC<T, size>& other);

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
				void									PrivateSort		(ArrayC<T, size>& array, unsigned int bottom, unsigned int top)const;
				template<class Equality> void			PrivateSort		(const Equality& functor, ArrayC<T, size>& array, unsigned int bottom, unsigned int top)const;
				unsigned int							PrivatePartition(ArrayC<T, size>& array, unsigned int bottom, unsigned int top)const;
				template<class Equality> unsigned int	PrivatePartition(const Equality& functor, ArrayC<T, size>& array, unsigned int bottom, unsigned int top)const;
				
				int										PrivateFindSortedIndex		(ConstReference value) const;
				template<class Equality> int			PrivateFindSortedIndex		(ConstReference value, const Equality& functor) const;
				
				T										mData [size];
			};
		}
	}
}

#include "DiaCore/Containers/Arrays/ArrayC.inl"
