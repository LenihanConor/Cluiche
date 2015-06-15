#include "DiaCore/Type/BasicTypeDefines.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//------------------------------------------------
			template <class Payload> 
			LinkListNode<Payload>::LinkListNode()
				: mNext(NULL)
			{}

			//------------------------------------------------
			template <class Payload> 
			LinkListNode<Payload>::LinkListNode(const Payload& payload)
				: mNext(NULL)
				, mPayload(payload)
			{}

			//------------------------------------------------
			template <class Payload> 
			LinkListNode<Payload>::LinkListNode(const Payload& payload, LinkListNode<Payload>* nextNode)
				: mNext(nextNode)
				, mPayload(payload)
			{

			}
			//------------------------------------------------
			template <class Payload> 
			void LinkListNode<Payload>::SetPayload(const Payload& payload)
			{
				mPayload = payload;
			}

			//------------------------------------------------
			template <class Payload> 
			void LinkListNode<Payload>::SetNext(LinkListNode<Payload>* next)
			{
				mNext = next;
			}

			//------------------------------------------------
			template <class Payload> 
			void LinkListNode<Payload>::Add(LinkListNode<Payload>* nextNode)
			{
				if (mNext == NULL)
				{
					mNext = nextNode;
				}
				else
				{
					mNext->Add(nextNode);
				}
			}

			//------------------------------------------------
			template <class Payload> 
			void LinkListNode<Payload>::Insert(LinkListNode<Payload>* nextNode)
			{
				if (mNext == NULL)
				{
					mNext = nextNode;
				}
				else
				{
					LinkListNode<Payload>* previousNextNode = mNext;
					LinkListNode<Payload>* lastNewNode = nextNode->FindLast();

					mNext = nextNode;
					lastNewNode->mNext = previousNextNode;
				}
			}

			//------------------------------------------------
			template <class Payload> 
			LinkListNode<Payload>* LinkListNode<Payload>::RemoveNext()
			{
				DIA_ASSERT(mNext, "Cannot remove link if there is none");
				
				LinkListNode<Payload>* result = mNext->GetNext();

				mNext = mNext->GetNext();

				return result;
			}

			//------------------------------------------------
			template <class Payload> 
			Payload& LinkListNode<Payload>::GetPayload()
			{
				return mPayload;
			}

			//------------------------------------------------
			template <class Payload> 
			const Payload& LinkListNode<Payload>::GetPayloadConst()const
			{
				return mPayload;
			}

			//------------------------------------------------
			template <class Payload> 
			LinkListNode<Payload>* LinkListNode<Payload>::GetNext()
			{
				return mNext;
			}

			//------------------------------------------------
			template <class Payload> 
			const LinkListNode<Payload>* LinkListNode<Payload>::GetNextConst()const 
			{
				return mNext;
			}

			//------------------------------------------------
			template <class Payload> 
			LinkListNode<Payload>* LinkListNode<Payload>::FindPreviousTo(const LinkListNode<Payload>* previousNode)
			{
				if (mNext == previousNode)
				{
					return this;
				}

				if (mNext == NULL)
				{
					return NULL;
				}

				return mNext->FindPreviousTo(previousNode);
			}

			//------------------------------------------------
			template <class Payload> 
			const LinkListNode<Payload>* LinkListNode<Payload>::FindPreviousToConst(const LinkListNode<Payload>* previousNode)const
			{
				if (mNext == previousNode)
				{
					return this;
				}

				if (mNext == NULL)
				{
					return NULL;
				}

				return mNext->FindPreviousToConst(previousNode);
			}

			//------------------------------------------------
			template <class Payload> 
			LinkListNode<Payload>* LinkListNode<Payload>::FindPreviousToLast()
			{
				if (mNext->GetNext() == NULL)
				{
					return this;
				}

				return mNext->FindPreviousToLast();
			}

			//------------------------------------------------
			template <class Payload> 
			const LinkListNode<Payload>* LinkListNode<Payload>::FindPreviousToLastConst()const
			{
				if (mNext->GetNextConst() == NULL)
				{
					return this;
				}

				return mNext->FindPreviousToLastConst();
			}

			//------------------------------------------------
			template <class Payload> 
			LinkListNode<Payload>* LinkListNode<Payload>::FindLast()
			{
				if (GetNext() == NULL)
				{
					return this;
				}

				return GetNext()->FindLast();
			}

			//------------------------------------------------
			template <class Payload> 
			const LinkListNode<Payload>* LinkListNode<Payload>::FindLastConst()const
			{
				if (GetNextConst() == NULL)
				{
					return this;
				}

				return GetNextConst()->FindLastConst();
			}

			//------------------------------------------------
			template <class Payload> 
			LinkListNode<Payload>* LinkListNode<Payload>::Find(const Payload& other)
			{
				if (GetPayload() == other)
				{
					return this;
				}

				if (GetNext() == NULL)
				{
					return NULL;
				}

				return GetNext()->Find(other);
			}
			
			//------------------------------------------------	
			template <class Payload> 
			const LinkListNode<Payload>* LinkListNode<Payload>::FindConst(const Payload& other)const
			{
				if (GetPayloadConst() == other)
				{
					return this;
				}

				if (GetNextConst() == NULL)
				{
					return NULL;
				}

				return GetNextConst()->FindConst(other);
			}

			//------------------------------------------------	
			template <class Payload> template<class Comparisson> 
			LinkListNode<Payload>* LinkListNode<Payload>::Find(const Payload& other, const Comparisson& functor)
			{
				if (functor.Equal(GetPayload(), other))
				{
					return this;
				}

				if (GetNext() == NULL)
				{
					return NULL;
				}

				return GetNext()->Find(other, functor);
			}

			//------------------------------------------------	
			template <class Payload> template<class Comparisson> 
			const LinkListNode<Payload>* LinkListNode<Payload>::FindConst(const Payload& other, const Comparisson& functor)const
			{
				if (functor.Equal(GetPayloadConst(), other))
				{
					return this;
				}

				if (GetNextConst() == NULL)
				{
					return NULL;
				}

				return GetNext()->FindConst(other, functor);
			}

			//------------------------------------------------	
			template <class Payload> template<class Value, class Comparisson> 
			LinkListNode<Payload>* LinkListNode<Payload>::Find(const Value& other, const Comparisson& functor)
			{
				if (functor.Equal(GetPayload(), other))
				{
					return this;
				}

				if (GetNext() == NULL)
				{
					return NULL;
				}

				return GetNext()->Find(other, functor);
			}

			//------------------------------------------------	
			template <class Payload> template<class Value, class Comparisson> 
			const LinkListNode<Payload>* LinkListNode<Payload>::FindConst(const Value& other, const Comparisson& functor)const
			{
				if (functor.Equal(GetPayloadConst(), other))
				{
					return this;
				}

				if (GetNextConst() == NULL)
				{
					return NULL;
				}

				return GetNext()->FindConst(other, functor);
			}

			//------------------------------------------------	
			template <class Payload>
			bool LinkListNode<Payload>::Contains(const LinkListNode<Payload>* node)const
			{
				if (this == node)
				{
					return true;
				}

				if (mNext == NULL)
				{
					return false;
				}

				return mNext->Contains(node);
			}

			//------------------------------------------------
			template <class Payload> 
			bool LinkListNode<Payload>::IsLast()const
			{
				return (GetNextConst() == NULL);
			}
			
			//------------------------------------------------
			template <class Payload> 
			bool LinkListNode<Payload>::IsSorted()const
			{
				if (GetNextConst() == NULL)
				{
					return true;
				}

				if (GetPayloadConst() > mNext->GetPayloadConst())
				{
					return false;
				}

				return mNext->IsSorted();
			}

			//------------------------------------------------
			template <class Payload> template<class Equality>
			bool LinkListNode<Payload>::IsSorted(const Equality& functor)const
			{
				if (GetNextConst() == NULL)
				{
					return true;
				}

				if (functor.GreaterThen(GetPayloadConst(), mNext->GetPayloadConst()))
				{
					return false;
				}

				return mNext->IsSorted(functor);
			}

			//------------------------------------------------
			template <class Payload> 
			bool  LinkListNode<Payload>::IsCircular(unsigned int expectedDepth, unsigned int currentDepth)const
			{
				currentDepth++;

				if (mNext == NULL)
				{
					return (expectedDepth == currentDepth);
				}

				return mNext->IsCircular(expectedDepth, currentDepth);
			}

			//------------------------------------------------
			template <class Payload> 
			unsigned int LinkListNode<Payload>::Depth(unsigned int currentDepth)const
			{
				currentDepth++;

				if (mNext == NULL)
				{
					return currentDepth;
				}
				
				return mNext->Depth(currentDepth);
			}

			//------------------------------------------------
			template <class Payload> 
			unsigned int LinkListNode<Payload>::Depth(const LinkListNode<Payload>* node, unsigned int currentDepth)const
			{		
				if (this == node)
				{
					return currentDepth;
				}

				DIA_ASSERT(mNext,"Does not contain node");

				currentDepth++;

				return mNext->Depth(node, currentDepth);
			}
		}
	}
}