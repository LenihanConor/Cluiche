#ifndef HASH_TABLE_C
#define HASH_TABLE_C

#include "DiaCore/Core/Assert.h"
#include "DiaCore/Containers/Arrays/ArrayC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

#include "DiaCore/Containers/HashTables/HashTableNode.h"
#include "DiaCore/Containers/HashTables/HashTableHashFunctionData.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//------------------------------------------------------------------------------------
			//	HashTableC - Interface
			//------------------------------------------------------------------------------------
			template< class Key, class Payload, class HashFunctor, unsigned int sizePayload, unsigned int sizeTable = sizePayload> 
			class HashTableC
			{
			public:
				typedef HashTableNode<Key, Payload> PayloadTableNode;

				HashTableC ();
				HashTableC (const HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>& copy);
				~HashTableC ();
				
				HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>&		
									operator=				(const HashTableC<Key, Payload, HashFunctor, sizePayload, sizeTable>& rhs);

				//--------- size operations ---------------------
				unsigned int		Capacity				() const;
				unsigned int		Size					() const;
				unsigned int		TableSize				() const;
				unsigned int		DeepestTable			() const;
				unsigned int		AssignedTableEntries	() const;
				unsigned int		UnAssignedTableEntries	() const;
				bool				IsEmpty					() const;
				bool				IsFull					() const;
				
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
				ArrayC<PayloadTableNode*, sizeTable> 			mTable;
				DynamicArrayC<PayloadTableNode, sizePayload> 	mPayloadNodes; 
			};
		}
	}
}

#include "DiaCore/Containers/HashTables/HashTableC.inl"

#endif