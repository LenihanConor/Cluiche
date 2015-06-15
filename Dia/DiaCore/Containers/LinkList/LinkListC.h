#ifndef LINK_LIST_C
#define LINK_LIST_C

#include "DiaCore/Containers/LinkList/LinkListNode.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{		
			// LinkListC -> No management of nodes, will not allocate node
			template <class Payload>
			class LinkListC
			{
			public:			
				LinkListC();				
				LinkListC(const LinkListC<Payload>& rhs);
				
				static bool									AddNodeToList			(LinkListC<Payload>& list, LinkListNode<Payload>* node);

				LinkListNode<Payload>*						AddNodeToHead			(LinkListNode<Payload>* node);
				LinkListNode<Payload>*						AddNodeToTail			(LinkListNode<Payload>* node);
				LinkListNode<Payload>*						InsertAsNextNodeTo		(LinkListNode<Payload>* currentNode, LinkListNode<Payload>* node);
				LinkListNode<Payload>*						InsertAsNextPreviousTo	(LinkListNode<Payload>* currentNode, LinkListNode<Payload>* node);
				
				LinkListNode<Payload>*						Remove				(LinkListNode<Payload>* currentNode);
				LinkListNode<Payload>*						RemoveNext			(LinkListNode<Payload>* currentNode);
				LinkListNode<Payload>*						RemoveHead			();
				LinkListNode<Payload>*						RemoveTail			();
				void										RemoveAll			();
			
				LinkListC<Payload>&							operator=			(const LinkListC<Payload>& other);
				bool										operator==			(const LinkListC<Payload>& other) const; 
				bool										operator!=			(const LinkListC<Payload>& other) const;

				LinkListNode<Payload>*										Find				(const Payload& other);
				const LinkListNode<Payload>*								FindConst			(const Payload& other)const;
				
				template<class Comparisson> LinkListNode<Payload>*			Find				(const Payload& value, const Comparisson& functor) ;
				template<class Comparisson> const LinkListNode<Payload>*	FindConst			(const Payload& value, const Comparisson& functor) const;
				
				template<class Value, class Comparisson> LinkListNode<Payload>*			Find		(const Value& value, const Comparisson& functor) ;
				template<class Value, class Comparisson> const LinkListNode<Payload>*	FindConst	(const Value& value, const Comparisson& functor) const;

				bool										Contains			(const LinkListNode<Payload>* node)const;
				bool										Contains			(const Payload& node)const;
				template<class Comparisson> bool			Contains			(const Payload& node, const Comparisson& functor) const;
				
				void										SwapNodes			(LinkListNode<Payload>* nodeA, LinkListNode<Payload>* nodeB);
				void										MoveNodeForward		(LinkListNode<Payload>* node);
				void										MoveNodeBackwards	(LinkListNode<Payload>* node);

				unsigned int								NodeDepth			(const LinkListNode<Payload>* node)const;

				unsigned int								Size				()const;
				unsigned int								HighWaterMark		()const;
				bool										IsCircular			()const;

				Payload&									First				();	
				const Payload&								First				() const;		
				Payload&									Last				();
				const Payload&								Last				() const;
				
				LinkListNode<Payload>*						Head				();														
				const LinkListNode<Payload>*				HeadConst			() const;														
				LinkListNode<Payload>*						Tail				();														
				const LinkListNode<Payload>*				TailConst			() const;
		
				bool										IsSorted			()const;
				template<class Equality> bool				IsSorted			(const Equality& functor)const;

				void										Sort				();
				template<class Equality> void				Sort				(const Equality& functor);
				
				template<class Evaluate> LinkListNode<Payload>*			HighestEvalution		(const Evaluate& functor);
				template<class Evaluate> const LinkListNode<Payload>*	HighestEvalutionConst	(const Evaluate& functor)const;

			private:
				void PrivateSwap(LinkListNode<Payload>* nodeA, LinkListNode<Payload>* nodeB, LinkListNode<Payload>* nodeANext, LinkListNode<Payload>* nodeBNext, LinkListNode<Payload>* nodeAPrevious, LinkListNode<Payload>* nodeBPrevious);
				void TestForHighWater();

				LinkListNode<Payload>* mRootNode;

				unsigned int mCurrentSize;
				unsigned int mHighWaterMark;
			};
		}
	}
}

#include "DiaCore/Containers/LinkList/LinkListC.inl"

#endif