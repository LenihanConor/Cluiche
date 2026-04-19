#include "limits.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//------------------------------------------------------------------------------------
			//	HashTableC - Implementation
			//------------------------------------------------------------------------------------

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable > 
			HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::HashTableC()
				 : mHasFunctorData(sizeTable)
			{
				RemoveAll();
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable > 
			HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::HashTableC(const HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>& copy)
				: mHasFunctorData(sizeTable)
				 , mTable(copy.mTable)
				, mPayloadNodes(copy.mPayloadNodes)
			{}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable > 
			HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::~HashTableC()
			{
				mHasFunctorData.DePopulate();
				RemoveAll();
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable > 
			HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>& HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::operator=(const HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>& rhs)
			{
				mHasFunctorData.Populate(rhs.TableSize());
				mTable = rhs.mTable;
				mPayloadNodes = rhs.mPayloadNodes;

				return *this;
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable > 
			unsigned int HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::Capacity() const
			{
				return mPayloadNodes.Capacity();
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable > 
			unsigned int HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::Size() const
			{
				return mPayloadNodes.Size();
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable > 
			unsigned int HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::TableSize() const
			{
				return mTable.Size();
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable > 
			unsigned int HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::DeepestTable() const
			{
				unsigned int deepest = 0;
				for (unsigned int i = 0; i < mTable.Size(); i++)
				{
					if (mTable[i])
					{
						unsigned int currentDepth = 0;
						
						mTable[i]->Depth(currentDepth);
						
						if (currentDepth > deepest)
						{
							deepest = currentDepth;
						}
					}
				}

				return deepest;
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable > 
			unsigned int HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::AssignedTableEntries() const
			{
				unsigned int numberAssignedTableEntries = 0;
				for (unsigned int i = 0; i < mTable.Size(); i++)
				{
					if (mTable[i])
					{
						numberAssignedTableEntries++;	
					}
				}

				return numberAssignedTableEntries;
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable > 
			unsigned int HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::UnAssignedTableEntries() const
			{
				unsigned int numberAssignedTableEntries = AssignedTableEntries();
				unsigned int result = mTable.Size() - numberAssignedTableEntries;
				return result;
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable > 
			bool HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::IsEmpty() const
			{
				return mPayloadNodes.IsEmpty();
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable > 
			bool HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::IsFull() const
			{
				return mPayloadNodes.IsFull();
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable > typename
			void HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::Add (const Key& key, const Payload& value)
			{
				mPayloadNodes.AddDefault();

				PayloadTableNode& newPayloadNode = mPayloadNodes.Back();

				newPayloadNode.Create(value, key);

				unsigned int keyIndex = mHashFunctor.GetHashIndex(key, &mHasFunctorData);

				PayloadTableNode* pTableNode = mTable[keyIndex];

				if (pTableNode == NULL)
				{
					mTable[keyIndex] = &newPayloadNode;
				}
				else
				{
					mTable[keyIndex]->Attach(&newPayloadNode);
				}
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable >
			void HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::Remove ( const Key& key )
			{
				DIA_ASSERT_SUPPORT(bool found = false);
				for (unsigned int i = 0; i < mPayloadNodes.Size(); i++)
				{
					if (mPayloadNodes[i].GetKeyConst() == key)
					{
						DIA_ASSERT_SUPPORT(found = true);
						RemoveByIndex(i);
						break;
					}
				}
				DIA_ASSERT(found, "Cound not find int hash table");
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable >
			void HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::RemoveByIndex( const unsigned int index )
			{
				const Key& key = mPayloadNodes[index].GetKeyConst();

				unsigned int keyIndex = mHashFunctor.GetHashIndex(key, &mHasFunctorData);

				PayloadTableNode* pTableNode = mTable[keyIndex];

				DIA_ASSERT(pTableNode, "There must be at least one node");

				// If your removing the first node
				if (pTableNode->GetKeyConst() == key)
				{
					mTable[keyIndex] = NULL;
				}
				else
				{
					bool foundDetach = pTableNode->Detach(key);
					DIA_ASSERT(foundDetach, "Cound not find int hash table");
				}

				mPayloadNodes.RemoveAt(index);
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable >
			void HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::RemoveByPayload ( const Payload& value )
			{
				DIA_ASSERT_SUPPORT(bool found = false);
				for (unsigned int i = 0; i < mPayloadNodes.Size(); i++)
				{
					if (mPayloadNodes[i].GetPayloadConst() == value)
					{
						DIA_ASSERT_SUPPORT(found = true);
						RemoveByIndex(i);
						break;
					}
				}
				DIA_ASSERT(found, "Cound not find int hash table");
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable > template<class Comparisson> 
			void HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::RemoveByPayload( const Payload& payloadValue, const Comparisson& functor )
			{
				DIA_ASSERT_SUPPORT(bool found = false);
				for (unsigned int i = 0; i < mPayloadNodes.Size(); i++)
				{
					if (functor.Equals(mPayloadNodes[i].GetPayloadConst(), value))
					{
						DIA_ASSERT_SUPPORT(found = true);
						RemoveByIndex(i);
						break;
					}
				}
				DIA_ASSERT(found, "Cound not find int hash table");
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable > 
			void HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::RemoveAll()
			{
				for(unsigned int i = 0; i < mTable.Size(); i++)
				{
					mTable[i] = NULL;
				}

				mPayloadNodes.RemoveAll();
			}


			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable > 
			bool HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::ContainsKey ( const Key& key ) const
			{
				return (TryGetItemConst(key) != NULL);
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable > 
			bool HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::ContainsPayload ( const Payload& value ) const
			{
				for (unsigned int i = 0; i < mPayloadNodes.Size(); i++)
				{
					if (mPayloadNodes[i].GetPayloadConst() == value)
					{
						return true;
					}
				}
				return false;
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable > template<class Equal> 
			bool HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::ContainsPayload	( const Payload& value, const Equal& functor ) const
			{
				for (unsigned int i = 0; i < mPayloadNodes.Size(); i++)
				{
					if (functor.Equals(mPayloadNodes[i].GetPayloadConst(), value))
					{
						return true;
					}
				}
				return false;
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable > 
			Payload& HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::GetItemByIndex ( const unsigned int index )
			{
				return mPayloadNodes[index].GetPayload();
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable > 
			const Payload& HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::GetItemByIndexConst ( const unsigned int index ) const
			{
				return mPayloadNodes[index].GetPayloadConst();
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable > 
			Payload& HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::GetItem(const Key& key)
			{
				Payload* payload = TryGetItem(key);

				DIA_ASSERT(payload, "Could not find");

				return *payload;
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable >
			const Payload& HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::GetItemConst(const Key& key) const
			{
				const Payload* payload = TryGetItemConst(key);
				 
 				DIA_ASSERT(payload, "Could not find");
 
 				return *payload;
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable >
			Payload* HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::TryGetItem( const Key& key )
			{
				unsigned int keyIndex = mHashFunctor.GetHashIndex(key, &mHasFunctorData);

				PayloadTableNode* pTableNode = mTable[keyIndex];

				if (pTableNode == NULL)
				{
					return NULL;
				}

				Payload* foundNode = pTableNode->TryFindPayload(key);

				return foundNode;
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable >
			const Payload* HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>::TryGetItemConst ( const Key& key )const
			{
 				unsigned int keyIndex = mHashFunctor.GetHashIndex(key, &mHasFunctorData);
 
 				const PayloadTableNode* pTableNode = mTable[keyIndex];
 
 				if (pTableNode == NULL)
 				{
 					return NULL;
 				}
 
 				const Payload* foundNode = pTableNode->TryFindPayloadConst(key);
 
 				return foundNode;
			}			
		}
	}
}