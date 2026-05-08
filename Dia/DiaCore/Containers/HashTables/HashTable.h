#ifndef HASH_TABLE
#define HASH_TABLE

#include "DiaCore/Core/Assert.h"
#include "DiaCore/Core/Log.h"
#include "DiaCore/Containers/Arrays/Array.h"
#include "DiaCore/Containers/Arrays/DynamicArray.h"

#include "DiaCore/Containers/HashTables/HashTableNode.h"
#include "DiaCore/Containers/HashTables/HashTableHashFunctionData.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//------------------------------------------------------------------------------------
			//	Default Hash Functor - Uses Key's Value() method or operator==
			//------------------------------------------------------------------------------------
			template<class Key>
			struct DefaultHashFunctor
			{
				unsigned int GetHashIndex(const Key& key, const HashTableHashFunctionData* data) const
				{
					// Use key's Value() method (for CRC-based keys) modulo table size
					return key.Value() % data->GetTableSize();
				}

				bool Equals(const Key& a, const Key& b) const
				{
					return a == b;
				}
			};

			//------------------------------------------------------------------------------------
			//	HashTable - Interface
			//------------------------------------------------------------------------------------
			template< class Key, class Payload, class HashFunctor = DefaultHashFunctor<Key>>
			class HashTable
			{
			public:
				typedef HashTableNode<Key, Payload> PayloadTableNode;

				// Forward declare iterator
				class Iterator;
				class ConstIterator;

				HashTable ();
				HashTable ( unsigned int sizePayload, unsigned int sizeTable);
				HashTable ( const HashTable<Key, Payload, HashFunctor>& copy );
				~HashTable ();
				
				HashTable<Key, Payload, HashFunctor>&		
									operator=				(const HashTable<Key, Payload, HashFunctor>& rhs);
				
				void SetSize(unsigned int sizePayload, unsigned int sizeTable);

				//--------- size operations ---------------------
				unsigned int		Capacity				() const;
				unsigned int		Size					() const;
				unsigned int		TableSize				() const;
				unsigned int		DeepestTable			() const;
				unsigned int		AssignedTableEntries	() const;
				unsigned int		UnAssignedTableEntries	() const;
				bool				IsEmpty					() const;
				bool				IsFull					() const;
				bool				IsSizeSet				() const;

				//------- insert, remove, has operations --------
				void										Add						( const Key& key, const Payload& value );

				void										Remove					( const Key& key );
				void										RemoveByIndex			( const unsigned int index );
				void										RemoveByPayload			( const Payload& key );
				template<class Comparisson> void			RemoveByPayload			( const Payload& key, const Comparisson& functor );
				void										RemoveAll				();

				bool				ContainsKey				( const Key& key ) const;
				bool				ContainsPayload			( const Payload& value ) const;
				template<class Equal> bool	ContainsPayload	( const Payload& value, const Equal& functor ) const;

				//------------ Item Indexing --------------------
				Payload&			GetItemByIndex			( const unsigned int payloadNodesIndex );
				const Payload&		GetItemByIndexConst		( const unsigned int payloadNodesIndex ) const;

				//------------- Key Indexing -------------------
				Payload&			GetItem					( const Key& key);
				const Payload&		GetItemConst			( const Key& key) const;
				Payload* 			TryGetItem				( const Key& key );
				const Payload* 		TryGetItemConst			( const Key& key )const;

				//------------- STL-style Interface -------------------
				// Convenience aliases for STL compatibility
				void				Insert					( const Key& key, const Payload& value ) { Add(key, value); }
				bool				Contains				( const Key& key ) const { return ContainsKey(key); }
				void				Clear					() { RemoveAll(); }
				void				Erase					( const Key& key ) { Remove(key); }
				Payload&			operator[]				( const Key& key ) { return GetItem(key); }
				const Payload&		operator[]				( const Key& key ) const { return GetItemConst(key); }

				//------------- Iterator Support -------------------
				Iterator			Begin();
				Iterator			End();
				ConstIterator		Begin() const;
				ConstIterator		End() const;

				//------------- Iterator Classes -------------------
				class Iterator
				{
				public:
					Iterator(HashTable* table, unsigned int index)
						: mTable(table), mIndex(index)
					{}

					bool operator!=(const Iterator& other) const
					{
						return mIndex != other.mIndex || mTable != other.mTable;
					}

					bool operator==(const Iterator& other) const
					{
						return mIndex == other.mIndex && mTable == other.mTable;
					}

					Iterator& operator++()
					{
						++mIndex;
						return *this;
					}

					Iterator operator++(int)
					{
						Iterator temp = *this;
						++mIndex;
						return temp;
					}

					// Access key
					const Key& Key() const
					{
						DIA_ASSERT(mTable != nullptr, "Invalid iterator");
						DIA_ASSERT(mIndex < mTable->Size(), "Iterator out of bounds");
						return mTable->mPayloadNodes[mIndex].GetKeyConst();
					}

					// Access value
					Payload& Value()
					{
						DIA_ASSERT(mTable != nullptr, "Invalid iterator");
						DIA_ASSERT(mIndex < mTable->Size(), "Iterator out of bounds");
						return mTable->mPayloadNodes[mIndex].GetPayload();
					}

					const Payload& Value() const
					{
						DIA_ASSERT(mTable != nullptr, "Invalid iterator");
						DIA_ASSERT(mIndex < mTable->Size(), "Iterator out of bounds");
						return mTable->mPayloadNodes[mIndex].GetPayloadConst();
					}

				private:
					HashTable* mTable;
					unsigned int mIndex;
				};

				class ConstIterator
				{
				public:
					ConstIterator(const HashTable* table, unsigned int index)
						: mTable(table), mIndex(index)
					{}

					bool operator!=(const ConstIterator& other) const
					{
						return mIndex != other.mIndex || mTable != other.mTable;
					}

					bool operator==(const ConstIterator& other) const
					{
						return mIndex == other.mIndex && mTable == other.mTable;
					}

					ConstIterator& operator++()
					{
						++mIndex;
						return *this;
					}

					ConstIterator operator++(int)
					{
						ConstIterator temp = *this;
						++mIndex;
						return temp;
					}

					// Access key
					const Key& Key() const
					{
						DIA_ASSERT(mTable != nullptr, "Invalid iterator");
						DIA_ASSERT(mIndex < mTable->Size(), "Iterator out of bounds");
						return mTable->mPayloadNodes[mIndex].GetKeyConst();
					}

					// Access value
					const Payload& Value() const
					{
						DIA_ASSERT(mTable != nullptr, "Invalid iterator");
						DIA_ASSERT(mIndex < mTable->Size(), "Iterator out of bounds");
						return mTable->mPayloadNodes[mIndex].GetPayloadConst();
					}

				private:
					const HashTable* mTable;
					unsigned int mIndex;
				};

			private:
				void				RebuildTable			();

				HashTableHashFunctionData						mHasFunctorData;
				HashFunctor										mHashFunctor;
				Array<PayloadTableNode*> 						mTable;
				DynamicArray<PayloadTableNode>					mPayloadNodes;
			};
		}
	}
}

#include "DiaCore/Containers/HashTables/HashTable.inl"

#endif