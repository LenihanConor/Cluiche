#ifndef LINK_LIST_NODE
#define LINK_LIST_NODE

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			template <class Payload> class LinkListC;

			template <class Payload>
			class LinkListNode
			{
			public:
				LinkListNode();
				LinkListNode(const Payload& payload);
				LinkListNode(const Payload& payload, LinkListNode<Payload>* nextNode);
			
				void							SetPayload(const Payload& payload);
				void							SetNext(LinkListNode<Payload>* next);

				void							Add(LinkListNode<Payload>* nextNode);
				void							Insert(LinkListNode<Payload>* nextNode);
 				LinkListNode<Payload>*			RemoveNext();
				
				Payload&						GetPayload();
				const Payload&					GetPayloadConst()const;

				LinkListNode<Payload>*			GetNext();
				const LinkListNode<Payload>*	GetNextConst()const;
				
				LinkListNode<Payload>*			FindPreviousTo(const LinkListNode<Payload>* previousNode);
				const LinkListNode<Payload>*	FindPreviousToConst(const LinkListNode<Payload>* previousNode)const;

				LinkListNode<Payload>*			FindPreviousToLast();
				const LinkListNode<Payload>*	FindPreviousToLastConst()const;

				LinkListNode<Payload>*			FindLast() ;
				const LinkListNode<Payload>*	FindLastConst() const;
				
				LinkListNode<Payload>*			Find(const Payload& other);
				const LinkListNode<Payload>*	FindConst(const Payload& other)const;

				bool							Contains(const LinkListNode<Payload>* node)const;

				template<class Comparisson> LinkListNode<Payload>*			Find(const Payload& other, const Comparisson& functor);
				template<class Comparisson> const LinkListNode<Payload>*	FindConst(const Payload& other, const Comparisson& functor)const;
				
				template<class Value, class Comparisson> LinkListNode<Payload>*			Find(const Value& other, const Comparisson& functor);
				template<class Value, class Comparisson> const LinkListNode<Payload>*	FindConst(const Value& other, const Comparisson& functor)const;

				bool							IsLast() const;
				bool							IsSorted()const;
				template<class Equality> bool	IsSorted(const Equality& functor)const;
				bool							IsCircular(unsigned int expectedDepth, unsigned int currentDepth)const;
				
				unsigned int					Depth(unsigned int currentDepth)const;
				unsigned int					Depth(const LinkListNode<Payload>* node, unsigned int currentDepth)const;

			private:
				Payload					mPayload;
				LinkListNode<Payload>*	mNext;
			};
		}
	}
}

#include "DiaCore/Containers/LinkList/LinkListNode.inl"

#endif