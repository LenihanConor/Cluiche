#ifndef LINK_LIST_NODE
#define LINK_LIST_NODE

namespace Dia
{
	namespace Core
	{
		template <class Payload>
		class LinkListNode
		{
		public:
			void Add(LinkListNode<Payload>* nextNode);
			bool Remove();
			
		private:
			Payload					mPayload;
			LinkListNode<Payload>*	mNext;
		};
	}
}

#include "DiaCore/Containers/LinkList/LinkListNode.inl"

#endif