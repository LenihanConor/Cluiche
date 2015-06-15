#include "DiaCore/Type/BasicTypeDefines.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//------------------------------------------------------------------------------------
			//	LinkListC
			//------------------------------------------------------------------------------------
			//------------------------------------------------------------------------------------
			template <class Payload>
			LinkListC<Payload>::LinkListC()
				: mRootNode(0)
				, mCurrentSize(0)
				, mHighWaterMark(0)
			{}

			//------------------------------------------------------------------------------------
			template <class Payload>
			LinkListC<Payload>::LinkListC(const LinkListC<Payload>& rhs)
				: mRootNode(rhs.mRootNode)
				, mCurrentSize(rhs.mCurrentSize)
				, mHighWaterMark(rhs.mHighWaterMark)
			{}

			//------------------------------------------------------------------------------------
			template <class Payload>
			bool LinkListC<Payload>::AddNodeToList(LinkListC<Payload>& list, LinkListNode<Payload>* node)
			{
				list.AddNodeToHead(node);
				return true;
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			LinkListNode<Payload>* LinkListC<Payload>::AddNodeToHead(LinkListNode<Payload>* node)
			{
				DIA_ASSERT(!Contains(node), "Node already in link list, would create cirular buffer");
				
				unsigned int nodesBeingAdded = 0;
				nodesBeingAdded = node->Depth(nodesBeingAdded);

				LinkListNode<Payload>* previousRoot = mRootNode;

				mRootNode = node;

				node->Add( previousRoot );

				mCurrentSize += nodesBeingAdded;

				TestForHighWater();

				return node;
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			LinkListNode<Payload>* LinkListC<Payload>::AddNodeToTail(LinkListNode<Payload>* node)
			{
				DIA_ASSERT(!Contains(node), "Node already in link list, would create cirular buffer");
				
				unsigned int nodesBeingAdded = 0;
				nodesBeingAdded = node->Depth(nodesBeingAdded);
			
				if (mRootNode == NULL)
				{
					mRootNode = node;
				}
				else
				{
					mRootNode->Add(node);
				}

				mCurrentSize += nodesBeingAdded;

				TestForHighWater();

				return node;
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			LinkListNode<Payload>* LinkListC<Payload>::InsertAsNextNodeTo(LinkListNode<Payload>* currentNode, LinkListNode<Payload>* node)
			{
				DIA_ASSERT(node, "node can not be NULL");
				DIA_ASSERT(mRootNode, "Must have at least one node to use insert");
				DIA_ASSERT(Contains(currentNode), "Does not contain node");
				DIA_ASSERT(!Contains(node), "Node already in link list, would create cirular buffer");		
		
				unsigned int nodesBeingAdded = 0;
				nodesBeingAdded = node->Depth(nodesBeingAdded);

				currentNode->Insert(node);
				
				mCurrentSize += nodesBeingAdded;

				TestForHighWater();

				return node;
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			LinkListNode<Payload>* LinkListC<Payload>::InsertAsNextPreviousTo(LinkListNode<Payload>* currentNode, LinkListNode<Payload>* node)
			{
				DIA_ASSERT(mRootNode, "Must have at least one node to use insert");

				return (InsertAsNextNodeTo(mRootNode->FindPreviousTo(currentNode), node));
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			LinkListNode<Payload>* LinkListC<Payload>::Remove(LinkListNode<Payload>* currentNode)
			{
				DIA_ASSERT(mRootNode, "Does not contain nodes");
				DIA_ASSERT(Contains(currentNode), "Does not contain node");
			
				if (mRootNode == currentNode)
				{
					RemoveHead();
				}
				else
				{
					RemoveNext(mRootNode->FindPreviousTo(currentNode));
				}

				return currentNode;
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			LinkListNode<Payload>* LinkListC<Payload>::RemoveNext(LinkListNode<Payload>* currentNode)
			{
				DIA_ASSERT(Contains(currentNode), "Does not contain node");
				
				mCurrentSize--;
				return currentNode->RemoveNext();
			}
			
			//------------------------------------------------------------------------------------
			template <class Payload>
			LinkListNode<Payload>* LinkListC<Payload>::RemoveHead()
			{
				DIA_ASSERT(mRootNode, "Does not contain nodes");
							
				LinkListNode<Payload>* head = mRootNode;

				mRootNode = mRootNode->GetNext();
				
				mCurrentSize--;

				return head;
			}
			
			//------------------------------------------------------------------------------------
			template <class Payload>
			LinkListNode<Payload>* LinkListC<Payload>::RemoveTail()
			{
				DIA_ASSERT(mRootNode, "Does not contain nodes");

				LinkListNode<Payload>* previousToLastNode = mRootNode->FindPreviousToLast();

				return RemoveNext(previousToLastNode);
			}
			
			//------------------------------------------------------------------------------------
			template <class Payload>
			void LinkListC<Payload>::RemoveAll()
			{
				mRootNode = NULL;
				mCurrentSize = 0;
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			LinkListC<Payload>& LinkListC<Payload>::operator=( const LinkListC<Payload>& other )
			{
				mRootNode = other.mRootNode;
				mCurrentSize = other.mCurrentSize;
				mHighWaterMark = other.mHighWaterMark;

				return *this;
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			bool LinkListC<Payload>::operator==	( const LinkListC<Payload>& other) const
			{
				return (mRootNode == other.mRootNode);
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			bool LinkListC<Payload>::operator!=	(const LinkListC<Payload>& other) const
			{
				return (mRootNode != other.mRootNode);
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			LinkListNode<Payload>* LinkListC<Payload>::Find(const Payload& other)
			{		
				if (mRootNode == NULL)
				{
					return NULL;
				}

				return mRootNode->Find(other);
			}
			
			//------------------------------------------------------------------------------------
			template <class Payload>
			const LinkListNode<Payload>* LinkListC<Payload>::FindConst(const Payload& other)const
			{		
				if (mRootNode == NULL)
				{
					return NULL;
				}

				return mRootNode->Find(other);
			}

			//------------------------------------------------------------------------------------
			template <class Payload> template<class Comparisson>
			LinkListNode<Payload>* LinkListC<Payload>::Find(const Payload& other, const Comparisson& functor)
			{		
				if (mRootNode == NULL)
				{
					return NULL;
				}

				return mRootNode->Find(other, functor);
			}

			//------------------------------------------------------------------------------------
			template <class Payload> template<class Comparisson>
			const LinkListNode<Payload>* LinkListC<Payload>::FindConst(const Payload& other, const Comparisson& functor)const
			{		
				if (mRootNode == NULL)
				{
					return NULL;
				}

				return mRootNode->Find(other, functor);
			}

			//------------------------------------------------------------------------------------
			template <class Payload> template<class Value, class Comparisson> 
			LinkListNode<Payload>* LinkListC<Payload>::Find(const Value& value, const Comparisson& functor) 
			{
				if (mRootNode == NULL)
				{
					return NULL;
				}

				return mRootNode->Find(value, functor);
			}

			//------------------------------------------------------------------------------------
			template <class Payload> template<class Value, class Comparisson> 
			const LinkListNode<Payload>*  LinkListC<Payload>::FindConst(const Value& value, const Comparisson& functor) const
			{
				if (mRootNode == NULL)
				{
					return NULL;
				}

				return mRootNode->Find(value, functor);
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			bool LinkListC<Payload>::Contains(const LinkListNode<Payload>* node)const
			{
				if (mRootNode == NULL)
				{
					return false;
				}

				return mRootNode->Contains(node);
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			bool LinkListC<Payload>::Contains(const Payload& node)const
			{
				if (mRootNode == NULL)
				{
					return false;
				}
				
				const LinkListNode<Payload>* tempNode = FindConst(node);
				bool temp = (tempNode != NULL);
				return temp;
			}

			//------------------------------------------------------------------------------------
			template <class Payload> template<class Comparisson>
			bool LinkListC<Payload>::Contains(const Payload& node, const Comparisson& functor) const
			{
				if (mRootNode == NULL)
				{
					return false;
				}
				
				const LinkListNode<Payload>* tempNode = FindConst(node, functor);
				bool temp = (tempNode != NULL);
				return temp;
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			void LinkListC<Payload>::SwapNodes(LinkListNode<Payload>* nodeA, LinkListNode<Payload>* nodeB)
			{
				DIA_ASSERT(mCurrentSize > 1, "Must have two nodes minimual");
				DIA_ASSERT(Contains(nodeA), "Does not contain node");
				DIA_ASSERT(Contains(nodeB), "Does not contain node");
				DIA_ASSERT(nodeA != nodeB, "these are teh same nodes");
				
				LinkListNode<Payload>* nodeANext = nodeA->GetNext();
				LinkListNode<Payload>* nodeBNext = nodeB->GetNext();

				LinkListNode<Payload>* nodeAPrevious = mRootNode->FindPreviousTo(nodeA);
				LinkListNode<Payload>* nodeBPrevious = mRootNode->FindPreviousTo(nodeB);

				PrivateSwap(nodeA, nodeB, nodeANext, nodeBNext, nodeAPrevious, nodeBPrevious);
			}

			//------------------------------------------------------------------------------------
 			template <class Payload>
 			void LinkListC<Payload>::MoveNodeForward(LinkListNode<Payload>* node)
			{
				DIA_ASSERT(mCurrentSize > 1, "Must have two nodes minimual");
				DIA_ASSERT(Contains(node), "Does not contain node");
				DIA_ASSERT(node->GetNext() != NULL, "End of the line");
				
				LinkListNode<Payload>* nodeA = node;
				LinkListNode<Payload>* nodeB = node->GetNext();

				LinkListNode<Payload>* nodeANext = node->GetNext();
				LinkListNode<Payload>* nodeBNext = nodeB->GetNext();

				LinkListNode<Payload>* nodeAPrevious = mRootNode->FindPreviousTo(node);
				LinkListNode<Payload>* nodeBPrevious = node;

				PrivateSwap(nodeA, nodeB, nodeANext, nodeBNext, nodeAPrevious, nodeBPrevious);
 			}
 
 			//------------------------------------------------------------------------------------
 			template <class Payload>
 			void LinkListC<Payload>::MoveNodeBackwards(LinkListNode<Payload>* node)
 			{
				DIA_ASSERT(mCurrentSize > 1, "Must have two nodes minimual");
				DIA_ASSERT(Contains(node), "Does not contain node");
				DIA_ASSERT(node != mRootNode, "End of the line");

				LinkListNode<Payload>* nodeA = node;
				LinkListNode<Payload>* nodeB =  mRootNode->FindPreviousTo(node);

				LinkListNode<Payload>* nodeANext = node->GetNext();
				LinkListNode<Payload>* nodeBNext = nodeA;

				LinkListNode<Payload>* nodeAPrevious = nodeB;
				LinkListNode<Payload>* nodeBPrevious = mRootNode->FindPreviousTo(nodeB);;

				PrivateSwap(nodeA, nodeB, nodeANext, nodeBNext, nodeAPrevious, nodeBPrevious);
 			}
		
			//------------------------------------------------------------------------------------
			template <class Payload>
			void LinkListC<Payload>::PrivateSwap(LinkListNode<Payload>* nodeA, 
								LinkListNode<Payload>* nodeB, 
								LinkListNode<Payload>* nodeANext, 
								LinkListNode<Payload>* nodeBNext, 
								LinkListNode<Payload>* nodeAPrevious, 
								LinkListNode<Payload>* nodeBPrevious)
			{
				if (nodeAPrevious != NULL)
				{
					nodeAPrevious->SetNext(nodeB);
				}
				else
				{
					mRootNode = nodeB;
				}

				if (nodeB == nodeANext)
				{
					nodeB->SetNext(nodeA);
				}
				else
				{
					nodeB->SetNext(nodeANext);
				}

				if (nodeBPrevious != NULL)
				{
					nodeBPrevious->SetNext(nodeA);
				}
				else
				{
					mRootNode = nodeB;
				}

				if (nodeA == nodeBNext)
				{
					nodeA->SetNext(nodeB);
				}
				else
				{
					nodeA->SetNext(nodeBNext);
				}
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			unsigned int LinkListC<Payload>::NodeDepth	(const LinkListNode<Payload>* node)const
			{
				DIA_ASSERT(mRootNode, "No root node");

				unsigned int currentDepth = 0;
				return mRootNode->Depth(node, currentDepth);
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			typename Payload&  LinkListC<Payload>::First()
			{
				DIA_ASSERT(mRootNode, "No Root");

				return mRootNode->GetPayload();
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			typename const Payload& LinkListC<Payload>::First() const
			{
				DIA_ASSERT(mRootNode, "No Root");

				return mRootNode->GetPayload();
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			typename Payload&  LinkListC<Payload>::Last()
			{
				DIA_ASSERT(mRootNode, "No Root");

				return mRootNode->FindLast()->GetPayload();
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			typename const Payload&  LinkListC<Payload>::Last() const
			{
				DIA_ASSERT(mRootNode, "No Root");

				return mRootNode->FindLast()->GetPayload();
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			typename LinkListNode<Payload>*	 LinkListC<Payload>::Head()
			{
				DIA_ASSERT(mRootNode, "No Root");

				return mRootNode;
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			typename const LinkListNode<Payload>* LinkListC<Payload>::HeadConst() const
			{
				DIA_ASSERT(mRootNode, "No Root");

				return mRootNode;
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			typename LinkListNode<Payload>*	LinkListC<Payload>::Tail()
			{
				DIA_ASSERT(mRootNode, "No Root");

				return mRootNode->FindLast();
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			typename const LinkListNode<Payload>* LinkListC<Payload>::TailConst() const
			{
				DIA_ASSERT(mRootNode, "No Root");

				return mRootNode->FindLast();
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			unsigned int LinkListC<Payload>::Size()const
			{
				DIA_ASSERT_SUPPORT(unsigned int depth = 0;)
				DIA_ASSERT(mRootNode ? mCurrentSize == mRootNode->Depth(depth) : mCurrentSize == 0, "Size if wrong");
				return mCurrentSize;
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			unsigned int LinkListC<Payload>::HighWaterMark()const
			{
				return mHighWaterMark;
			}

			//------------------------------------------------------------------------------------
			template <class Payload>
			bool LinkListC<Payload>::IsCircular()const
			{
				if (mRootNode == NULL)
				{
					return false;
				}

				unsigned int currentLevel = 0;
				return mRootNode->IsCircular(mCurrentSize, currentLevel);
			}

			//-----------------------------------------------------------------------------
			template <class Payload> inline
			bool LinkListC<Payload>::IsSorted()const
			{
				if (mRootNode == NULL)
				{
					return true;
				}
				
				return mRootNode->IsSorted();
			}

			//-----------------------------------------------------------------------------
			template <class Payload> template<class Equality> inline
			bool LinkListC<Payload>::IsSorted(const Equality& functor)const
			{
				if (mRootNode == NULL)
				{
					return true;
				}

				return mRootNode->IsSorted(functor);
			}

			//-----------------------------------------------------------------------------
			template <class Payload> inline
			void LinkListC<Payload>::Sort()
			{
				// current ptr is used to point the current node in the list
				//prev ptr is used to point the previous node of current node
				//next ptr is used to point the next node of current node

				LinkListNode<Payload>* prev = NULL;
				LinkListNode<Payload>* next = NULL;

				//Initially current node and previous node should be the same(first) node in the list
				LinkListNode<Payload>* current = mRootNode;
				LinkListNode<Payload>* head = mRootNode;
				prev = mRootNode;
				next = mRootNode->GetNext();
				for( unsigned int i = 0; i < mCurrentSize - 1; i++)
				{
					for(unsigned int j = 0; j < mCurrentSize - i - 1; j++)
					{
						if(current->GetPayload() > next->GetPayload())
						{
							MoveNodeForward(current);

							if(current == head)
							{
								head = next;
								prev = next;
							}
							else
							{
								prev->SetNext( next );
								prev = next;
							}
							if(next != NULL)//check whether final node is reached 
							{
								next = current->GetNext();
							}

						}
						else//just moved each node ptr by one position
						{
							prev = current;
							current = next;
							next = current->GetNext();
						}

					}
					//For next iteration make the initial settings again
					current = head;
					prev = head;
					next = current->GetNext();
				}
			}

			//-----------------------------------------------------------------------------
	 		template <class Payload> template<class Equality> inline
	 		void LinkListC<Payload>::Sort (const Equality& functor)
	 		{
				// current ptr is used to point the current node in the list
				//prev ptr is used to point the previous node of current node
				//next ptr is used to point the next node of current node

				LinkListNode<Payload>* prev = NULL;
				LinkListNode<Payload>* next = NULL;

				//Initially current node and previous node should be the same(first) node in the list
				LinkListNode<Payload>* current = mRootNode;
				LinkListNode<Payload>* head = mRootNode;
				prev = mRootNode;
				next = mRootNode->GetNext();
				for( unsigned int i = 0; i < mCurrentSize - 1; i++)
				{
					for(unsigned int j = 0; j < mCurrentSize - i - 1; j++)
					{
						if(functor.GreaterThen(current->GetPayload(), next->GetPayload()))
						{
							MoveNodeForward(current);

							if(current == head)
							{
								head = next;
								prev = next;
							}
							else
							{
								prev->SetNext( next );
								prev = next;
							}
							if(next != NULL)//check whether final node is reached 
							{
								next = current->GetNext();
							}

						}
						else//just moved each node ptr by one position
						{
							prev = current;
							current = next;
							next = current->GetNext();
						}

					}
					//For next iteration make the initial settings again
					current = head;
					prev = head;
					next = current->GetNext();
				}
 			}

			//-----------------------------------------------------------------------------
			template <class Payload> template<class Evaluate> 
			LinkListNode<Payload>*	LinkListC<Payload>::HighestEvalution(const Evaluate& functor)
			{
				DIA_ASSERT(Size() > 0, "Must have at least one entry to evaluate");

				float currentHighest = functor.Evaluate(mRootNode->GetPayload());
				
				LinkListNode<Payload>* result = mRootNode;
				LinkListNode<Payload>* currentNode = mRootNode->GetNext();

				for (unsigned int i = 1; i < Size(); i++)
				{
					if (currentNode == NULL)
					{
						break;
					}

					float temp = functor.Evaluate(currentNode->GetPayload());
					if (temp > currentHighest)
					{
						currentHighest = temp;
						result = currentNode;
					}

					currentNode = currentNode->GetNext();
				}

				return result;
			}

			//-----------------------------------------------------------------------------
			template <class Payload> template<class Evaluate> 
			const LinkListNode<Payload>* LinkListC<Payload>::HighestEvalutionConst(const Evaluate& functor)const
			{
				DIA_ASSERT(Size() > 0, "Must have at least one entry to evaluate");

				float currentHighest = functor.Evaluate(mRootNode->GetPayloadConst());

				const LinkListNode<Payload>* result = mRootNode;
				const LinkListNode<Payload>* currentNode = mRootNode->GetNextConst();

				for (unsigned int i = 1; i < Size(); i++)
				{
					if (currentNode == NULL)
					{
						break;
					}

					float temp = functor.Evaluate(currentNode->GetPayloadConst());
					if (temp > currentHighest)
					{
						currentHighest = temp;
						result = currentNode;
					}

					currentNode = currentNode->GetNextConst();
				}

				return result;
			}

			//-----------------------------------------------------------------------------
			template <class Payload> inline
			void LinkListC<Payload>::TestForHighWater()
			{
				if (mCurrentSize > mHighWaterMark)
				{
					mHighWaterMark = mCurrentSize;
				}
			}
		}
	}
}