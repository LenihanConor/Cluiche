#ifndef HASH_TABLE_NODE
#define HASH_TABLE_NODE

#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//------------------------------------------------------------------------------------
			//	HashTableNode - Interface
			//------------------------------------------------------------------------------------
			template< class Key, class Payload >
			class HashTableNode
			{
			public:
				HashTableNode();

				void Create(const Payload& payload, const Key& key);
				void Attach(HashTableNode<Key, Payload>* nextNode);
				bool Detach(const Key& key);

				void Depth(unsigned int& currentDepth)const;

				Payload* TryFindPayload(const Key& key);
				const Payload* TryFindPayloadConst(const Key& key)const;

				Payload& GetPayload();
				const Payload& GetPayloadConst()const;

				const Key& GetKeyConst()const;

			private:
				Payload							mPayload;
				Key								mKey;
				HashTableNode<Key, Payload>*	mNext;
			};
		}
	}
}

#include "DiaCore/Containers/HashTables/HashTableNode.inl"

#endif