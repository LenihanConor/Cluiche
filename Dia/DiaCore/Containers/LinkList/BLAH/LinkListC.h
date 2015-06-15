#ifndef LINK_LIST_C
#define LINK_LIST_C

#include "DiaCore/Containers/Arrays/ArrayC.h"

namespace Dia
{
	namespace Core
	{
		template <class Payload, unsigned int size>
		class LinkListC
		{
		public:
			typedef	Payload			ValueType;
			typedef	Payload*		Pointer;
			typedef	Payload&		Reference;
			typedef	const Payload*	ConstPointer;
			typedef	const Payload&	ConstReference;
			
			typedef	ArrayIterator<Payload>				Iterator;
			typedef	ArrayConstIterator<Payload>			ConstIterator;
			typedef	ReverseArrayIterator<Payload>		ReverseIterator;
			typedef	ReverseArrayConstIterator<Payload>	ConstReverseIterator;

			LinkListC();
			LinkListC(const LinkListC<Payload, size>& rhs);

	/*		
			
			// ---------- adding links ----------------
			void			Add					(const Payload& nodeToAdd);
			void			AddNode				(const LinkListNode<Payload>& node);
			void			AddNodes			(const LinkListNode<Payload>& nodes);
			void			AddDefault			();
			void			Insert				(const unsigned int index, const Payload& nodeToAdd);
			void			InsertNode			(const unsigned int index, LinkListNode<Payload>& node);
			void			InsertNodes			(const unsigned int index, LinkListNode<Payload>& nodes);
			void			InsertDefault		(const unsigned int index);
			
			void			Remove				(const unsigned int index);
			void			RemoveHead			();
			void			RemoveTail			();
			void			RemoveAll			();

			LinkListC<Payload>&						operator=		(const LinkListC<Payload>& other);
			bool									operator==		(const LinkListC<Payload>& other) const; 
			bool									operator!=		(const LinkListC<Payload>& other) const;

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
			
			int										FrequencyOfElement		(ConstReference value)const;
			void									UniqueElements			(Array<T>& unique, unsigned int& number)const;
			void									FrequencyUniqueElements	(Array<T>& unique, Array<int>& uniqueFrequency, unsigned int& number)const;

			bool									IsSorted		()const;
			template<class Equality> bool			IsSorted		(const Equality& functor)const;

			void									Sort			();
			template<class Equality> void			Sort			(const Equality& functor);

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
*/

		private:
// 			void									PrivateSort		(Array<T>& array, unsigned int bottom, unsigned int top)const;
// 			template<class Equality> void			PrivateSort		(const Equality& functor, Array<T>& array, unsigned int bottom, unsigned int top)const;
// 			unsigned int							PrivatePartition(Array<T>& array, unsigned int bottom, unsigned int top)const;
// 			template<class Equality> unsigned int	PrivatePartition(const Equality& functor, Array<T>& array, unsigned int bottom, unsigned int top)const;
// 
// 			int										PrivateFindSortedIndex		(ConstReference value) const;
// 			template<class Equality> int			PrivateFindSortedIndex		(ConstReference value, const Equality& functor) const;

			unsigned int mCurrentNumberNodes;
			LinkListNode<Payload> mRootNode;
			Dia::Core::Containers::ArrayC<LinkListNode<Payload>, size> mFreeNodeList;
		};
	}
}

#include "DiaCore/Containers/LinkList/LinkListC.inl"

#endif