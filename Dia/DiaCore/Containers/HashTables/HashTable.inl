#include "limits.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//------------------------------------------------------------------------------------
			//	HashTable - Implementation
			//------------------------------------------------------------------------------------

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor > 
			HashTable<Key, Payload, HashFunctor>::HashTable()
			{
				RemoveAll();
			}
			
			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor > 
			HashTable<Key, Payload, HashFunctor>::HashTable(unsigned int sizePayload, unsigned int sizeTable)
				: mHasFunctorData(sizeTable)
				, mTable(sizeTable)
				, mPayloadNodes(sizePayload)
			{
				RemoveAll();
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor > 
			HashTable<Key, Payload, HashFunctor>::HashTable(const HashTable<Key, Payload, HashFunctor >& copy)
				: mHasFunctorData(copy.mTable.Size())
				, mTable(copy.mTable)
				, mPayloadNodes(copy.mPayloadNodes)
			{}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor > 
			HashTable<Key, Payload, HashFunctor >::~HashTable()
			{
				mHasFunctorData.DePopulate();
				RemoveAll();
			}
			
			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor > 
			void HashTable<Key, Payload, HashFunctor >::SetSize(unsigned int sizePayload, unsigned int sizeTable)
			{
				DIA_ASSERT(mTable.Size() == 0,  "Only set the size once");
				DIA_ASSERT(mPayloadNodes.Size() == 0,  "Only set the size once");
					
				mHasFunctorData.Populate(sizeTable);
				mTable.Reserve(sizeTable);
				mPayloadNodes.Reserve(sizePayload);
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor > 
			HashTable<Key, Payload, HashFunctor >& HashTable<Key, Payload, HashFunctor >::operator=(const HashTable<Key, Payload, HashFunctor >& rhs)
			{
				mHasFunctorData.Populate(rhs.TableSize());
				
				mTable.Reserve(rhs.mTable.Size());
				mPayloadNodes.Reserve(rhs.mPayloadNodes.Capacity());

				mTable = rhs.mTable;
				mPayloadNodes = rhs.mPayloadNodes;

				return *this;
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor > 
			unsigned int HashTable<Key, Payload, HashFunctor >::Capacity() const
			{
				return mPayloadNodes.Capacity();
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor > 
			unsigned int HashTable<Key, Payload, HashFunctor >::Size() const
			{
				return mPayloadNodes.Size();
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor > 
			unsigned int HashTable<Key, Payload, HashFunctor >::TableSize() const
			{
				return mTable.Size();
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor > 
			unsigned int HashTable<Key, Payload, HashFunctor >::DeepestTable() const
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
			template< class Key, class Payload, class HashFunctor > 
			unsigned int HashTable<Key, Payload, HashFunctor >::AssignedTableEntries() const
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
			template< class Key, class Payload, class HashFunctor > 
			unsigned int HashTable<Key, Payload, HashFunctor >::UnAssignedTableEntries() const
			{
				unsigned int numberAssignedTableEntries = AssignedTableEntries();
				unsigned int result = mTable.Size() - numberAssignedTableEntries;
				return result;
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor > 
			bool HashTable<Key, Payload, HashFunctor >::IsEmpty() const
			{
				return mPayloadNodes.IsEmpty();
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor > 
			bool HashTable<Key, Payload, HashFunctor >::IsFull() const
			{
				return mPayloadNodes.IsFull();
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor > 
			bool HashTable<Key, Payload, HashFunctor >::IsSizeSet() const
			{
				return (mPayloadNodes.Capacity() != 0);
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor > typename
			void HashTable<Key, Payload, HashFunctor >::Add (const Key& key, const Payload& value)
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
			template< class Key, class Payload, class HashFunctor>
			void HashTable<Key, Payload, HashFunctor >::Remove ( const Key& key )
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
			template< class Key, class Payload, class HashFunctor>
			void HashTable<Key, Payload, HashFunctor >::RemoveByIndex( const unsigned int index )
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
			template< class Key, class Payload, class HashFunctor>
			void HashTable<Key, Payload, HashFunctor >::RemoveByPayload ( const Payload& value )
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
			template< class Key, class Payload, class HashFunctor > template<class Comparisson> 
			void HashTable<Key, Payload, HashFunctor >::RemoveByPayload( const Payload& payloadValue, const Comparisson& functor )
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
			template< class Key, class Payload, class HashFunctor > 
			void HashTable<Key, Payload, HashFunctor >::RemoveAll()
			{
				for(unsigned int i = 0; i < mTable.Size(); i++)
				{
					mTable[i] = NULL;
				}

				mPayloadNodes.RemoveAll();
			}


			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor > 
			bool HashTable<Key, Payload, HashFunctor >::ContainsKey ( const Key& key ) const
			{
				return (TryGetItemConst(key) != NULL);
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor > 
			bool HashTable<Key, Payload, HashFunctor >::ContainsPayload ( const Payload& value ) const
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
			template< class Key, class Payload, class HashFunctor > template<class Equal> 
			bool HashTable<Key, Payload, HashFunctor >::ContainsPayload	( const Payload& value, const Equal& functor ) const
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
			template< class Key, class Payload, class HashFunctor > 
			Payload& HashTable<Key, Payload, HashFunctor >::GetItemByIndex ( const unsigned int index )
			{
				return mPayloadNodes[index].GetPayload();
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor > 
			const Payload& HashTable<Key, Payload, HashFunctor >::GetItemByIndexConst ( const unsigned int index ) const
			{
				return mPayloadNodes[index].GetPayloadConst();
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor > 
			Payload& HashTable<Key, Payload, HashFunctor >::GetItem(const Key& key)
			{
				Payload* payload = TryGetItem(key);

				DIA_ASSERT(payload, "Could not find");

				return *payload;
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor>
			const Payload& HashTable<Key, Payload, HashFunctor >::GetItemConst(const Key& key) const
			{
				const Payload* payload = TryGetItemConst(key);
				 
 				DIA_ASSERT(payload, "Could not find");
 
 				return *payload;
			}

			//----------------------------------------------------------
			template< class Key, class Payload, class HashFunctor>
			Payload* HashTable<Key, Payload, HashFunctor >::TryGetItem( const Key& key )
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
			template< class Key, class Payload, class HashFunctor>
			const Payload* HashTable<Key, Payload, HashFunctor >::TryGetItemConst ( const Key& key )const
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