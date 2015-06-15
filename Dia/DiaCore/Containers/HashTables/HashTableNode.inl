#include "limits.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//------------------------------------------------------------------------------------
			//	HashTableNode - Implementation
			//------------------------------------------------------------------------------------
			template< class Key, class Payload >
			HashTableNode<Key, Payload>::HashTableNode()
				: mKey()
				, mNext(NULL)
			{}

			//------------------------------------------------------------------------------------
			template< class Key, class Payload >
			void HashTableNode<Key, Payload>::Create(const Payload& payload, const Key& key)
			{
				mPayload = payload;
				mKey = key;
				mNext = NULL;
			}

			//------------------------------------------------------------------------------------
			template< class Key, class Payload >
			void HashTableNode<Key, Payload>::Attach(HashTableNode<Key, Payload>* nextNode)
			{	
				DIA_ASSERT(mKey != nextNode->mKey, "Inserting key in twice");

				if (mNext == NULL)
				{
					mNext = nextNode;
				}
				else
				{
					mNext->Attach(nextNode);
				}
			}

			//------------------------------------------------------------------------------------
			template< class Key, class Payload >
			bool HashTableNode<Key, Payload>::Detach(const Key& key)
			{	
				if (mNext != NULL)
				{
					if (key == mNext->mKey)
					{
						this->mNext = mNext->mNext;
						
						return true;
					}
					else
					{
						return mNext->Detach(key);
					}
				}
				return false;
			}

			//------------------------------------------------------------------------------------
			template< class Key, class Payload >
			void HashTableNode<Key, Payload>::Depth(unsigned int& currentDepth)const
			{
				currentDepth++;
				if(mNext)
				{
					mNext->Depth(currentDepth);
				}
			}

			//------------------------------------------------------------------------------------
			template< class Key, class Payload >
			Payload* HashTableNode<Key, Payload>::TryFindPayload(const Key& key)
			{
				if (key == mKey)
				{
					return &mPayload;
				}

				if (mNext)
				{
					return mNext->TryFindPayload(key);
				}

				return NULL;
			}

			//------------------------------------------------------------------------------------
			template< class Key, class Payload >
			const Payload* HashTableNode<Key, Payload>::TryFindPayloadConst(const Key& key)const
			{
				if (key == mKey)
				{
					return &mPayload;
				}

				if (mNext)
				{
					return mNext->TryFindPayloadConst(key);
				}

				return NULL;
			}

			//------------------------------------------------------------------------------------
			template< class Key, class Payload >
			Payload& HashTableNode<Key, Payload>::GetPayload()
			{
				return mPayload;
			}

			//------------------------------------------------------------------------------------
			template< class Key, class Payload >
			const Payload& HashTableNode<Key, Payload>::GetPayloadConst()const
			{
				return mPayload;
			}

			//------------------------------------------------------------------------------------
			template< class Key, class Payload > typename
			const Key& HashTableNode<Key, Payload>::GetKeyConst()const
			{
				return mKey;
			}		
		}
	}
}