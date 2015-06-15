#ifndef HASH_TABLE
#define HASH_TABLE

#include "DiaCore/Core/Assert.h"
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
			//	HashTable - Interface
			//------------------------------------------------------------------------------------
			template< class Key, class Payload, class HashFunctor> 
			class HashTable
			{
			public:
				typedef HashTableNode<Key, Payload> PayloadTableNode;

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

			private:
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