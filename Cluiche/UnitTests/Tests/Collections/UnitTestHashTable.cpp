
#include "UnitTests/Tests/Collections/UnitTestHashTable.h"

#include <DiaCore/Containers/HashTables/HashTableC.h>
#include <DiaCore/Containers/HashTables/HashTable.h>

#include "UnitTests/Infrastructure/UnitTestMacros.h"

namespace UnitTests
{	
	UnitTestHashTableC::UnitTestHashTableC(const Dia::Core::Containers::String32& name)
		: UnitTestCoreContainers(name)
	{}

	UnitTestHashTableC::UnitTestHashTableC(void)
		: UnitTestCoreContainers()
	{}

	void UnitTestHashTableC::DoTest()
	{
		UNIT_TEST_BLOCK_START();
			
			class HashFunctorTest1
			{
			public:
				typedef unsigned int Key;
				typedef Dia::Core::Containers::HashTableHashFunctionData TableData;

				unsigned int GetHashIndex(Key key, const TableData* tableData)const
				{
					if (key >= tableData->GetTableSize())
					{
						key = 0;
					}
					return key; 
				}
			};

			Dia::Core::Containers::HashTableC<unsigned int, char, HashFunctorTest1, 10> hashMap;

			UNIT_TEST_POSITIVE(hashMap.IsEmpty(), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap.IsFull(), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.Size() == 0, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.Capacity() == 10, "HashTableC()");
			
			hashMap.Add(0, 'a');
			hashMap.Add(1, 'b');
			hashMap.Add(2, 'c');
			hashMap.Add(4, 'e');
			hashMap.Add(3, 'd');
			hashMap.Add(5, 'f');
			hashMap.Add(20,'g');		

			UNIT_TEST_POSITIVE(!hashMap.IsEmpty(), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap.IsFull(), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.Size() == 7, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.DeepestTable() == 2, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.AssignedTableEntries() == 6, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.UnAssignedTableEntries() == 4, "HashTableC()");
			
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 0 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 1 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 2 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 3 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 4 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 5 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 20 ), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap.ContainsKey( 6 ), "HashTableC()");

			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'a' ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'b' ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'c' ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'd' ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'e' ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'f' ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'g' ), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap.ContainsPayload( 'h' ), "HashTableC()");
			
			class Equal
			{
			public:
				bool Equals(const char& a, const char& b)const
				{
					return (a == b);
				};
			};

			Equal compare;
			
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'a', compare), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'b', compare ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'c', compare ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'd', compare ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'e', compare ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'f', compare ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'g', compare ), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap.ContainsPayload( 'h', compare ), "HashTableC()");

			Dia::Core::Containers::HashTableC<unsigned int, char, HashFunctorTest1, 10> hashMap2(hashMap);

			UNIT_TEST_POSITIVE(!hashMap2.IsEmpty(), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap2.IsFull(), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.Size() == 7, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.DeepestTable() == 2, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.AssignedTableEntries() == 6, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.UnAssignedTableEntries() == 4, "HashTableC()");

			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 0 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 1 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 2 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 3 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 4 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 5 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 20 ), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap2.ContainsKey( 6 ), "HashTableC()");

			Dia::Core::Containers::HashTableC<unsigned int, char, HashFunctorTest1, 10> hashMap3;
			hashMap3 = hashMap;

			UNIT_TEST_POSITIVE(!hashMap3.IsEmpty(), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap3.IsFull(), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.Size() == 7, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.DeepestTable() == 2, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.AssignedTableEntries() == 6, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.UnAssignedTableEntries() == 4, "HashTableC()");

			UNIT_TEST_POSITIVE(hashMap3.ContainsKey( 0 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.ContainsKey( 1 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.ContainsKey( 2 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.ContainsKey( 3 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.ContainsKey( 4 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.ContainsKey( 5 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.ContainsKey( 20 ), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap3.ContainsKey( 6 ), "HashTableC()");

			UNIT_TEST_POSITIVE(hashMap3.GetItem( 0 ) == 'a', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItem( 1 ) == 'b', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItem( 2 ) == 'c', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItem( 3 ) == 'd', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItem( 4 ) == 'e', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItem( 5 ) == 'f', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItem( 20 ) == 'g', "HashTableC()");
			
			UNIT_TEST_POSITIVE(hashMap3.GetItemConst( 0 ) == 'a', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemConst( 1 ) == 'b', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemConst( 2 ) == 'c', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemConst( 3 ) == 'd', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemConst( 4 ) == 'e', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemConst( 5 ) == 'f', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemConst( 20 ) == 'g', "HashTableC()");

			UNIT_TEST_POSITIVE(hashMap3.TryGetItem( 0 ) != NULL, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.TryGetItem( 11 ) == NULL, "HashTableC()");

			UNIT_TEST_POSITIVE(hashMap3.TryGetItemConst( 0 ) != NULL, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.TryGetItemConst( 11 ) == NULL, "HashTableC()");
			
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 0 ) == 'a', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 1 ) == 'b', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 2 ) == 'c', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 3 ) == 'e', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 4 ) == 'd', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 5 ) == 'f', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 6 ) == 'g', "HashTableC()");
	
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndexConst( 0 ) == 'a', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndexConst( 1 ) == 'b', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndexConst( 2 ) == 'c', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndexConst( 3 ) == 'e', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndexConst( 4 ) == 'd', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndexConst( 5 ) == 'f', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndexConst( 6 ) == 'g', "HashTableC()");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			hashMap.Add(0, 'h');
			UNIT_TEST_ASSERT_EXPECTED_END();	


		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			class HashFunctorTest1
			{
			public:
				typedef unsigned int Key;
				typedef Dia::Core::Containers::HashTableHashFunctionData TableData;

				unsigned int GetHashIndex(Key key, const TableData* tableData)const
				{
					if (key >= tableData->GetTableSize())
					{
						key = 0;
					}
					return key; 
				}
			};

			Dia::Core::Containers::HashTableC<unsigned int, char, HashFunctorTest1, 10> hashMap;

			UNIT_TEST_POSITIVE(hashMap.IsEmpty(), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap.IsFull(), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.Size() == 0, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.Capacity() == 10, "HashTableC()");

			hashMap.Add(0, 'a');
			hashMap.Add(1, 'b');
			hashMap.Add(2, 'c');
			hashMap.Add(4, 'e');
			hashMap.Add(3, 'd');
			hashMap.Add(5, 'f');
			hashMap.Add(20,'g');		

			UNIT_TEST_POSITIVE(!hashMap.IsEmpty(), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap.IsFull(), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.Size() == 7, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.DeepestTable() == 2, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.AssignedTableEntries() == 6, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.UnAssignedTableEntries() == 4, "HashTableC()");

			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 0 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 1 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 2 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 3 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 4 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 5 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 20 ), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap.ContainsKey( 6 ), "HashTableC()");
			
			Dia::Core::Containers::HashTableC<unsigned int, char, HashFunctorTest1, 10> hashMap2(hashMap);
			Dia::Core::Containers::HashTableC<unsigned int, char, HashFunctorTest1, 10> hashMap3(hashMap);
			Dia::Core::Containers::HashTableC<unsigned int, char, HashFunctorTest1, 10> hashMap4(hashMap);

			hashMap.RemoveAll();

			UNIT_TEST_POSITIVE(hashMap.IsEmpty(), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap.IsFull(), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.Size() == 0, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.DeepestTable() == 0, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.AssignedTableEntries() == 0, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap.UnAssignedTableEntries() == 10, "HashTableC()");
			
			hashMap2.Remove(5);

			UNIT_TEST_POSITIVE(!hashMap2.IsEmpty(), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap2.IsFull(), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.Size() == 6, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.DeepestTable() == 2, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.AssignedTableEntries() == 5, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.UnAssignedTableEntries() == 5, "HashTableC()");

			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 0 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 1 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 2 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 3 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 4 ), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap2.ContainsKey( 5 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 20 ), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap2.ContainsKey( 6 ), "HashTableC()");
			
			hashMap2.Remove(20);

			UNIT_TEST_POSITIVE(!hashMap2.IsEmpty(), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap2.IsFull(), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.Size() == 5, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.DeepestTable() == 1, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.AssignedTableEntries() == 5, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.UnAssignedTableEntries() == 5, "HashTableC()");

			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 0 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 1 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 2 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 3 ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 4 ), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap2.ContainsKey( 5 ), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap2.ContainsKey( 20 ), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap2.ContainsKey( 6 ), "HashTableC()");
			
			hashMap3.RemoveByIndex(0);
			
			UNIT_TEST_POSITIVE(!hashMap3.IsEmpty(), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap3.IsFull(), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.Size() == 6, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.DeepestTable() == 1, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.AssignedTableEntries() == 5, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.UnAssignedTableEntries() == 5, "HashTableC()");

			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 0 ) == 'b', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 1 ) == 'c', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 2 ) == 'e', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 3 ) == 'd', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 4 ) == 'f', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 5 ) == 'g', "HashTableC()");

			hashMap3.RemoveByIndex(0);

			UNIT_TEST_POSITIVE(!hashMap3.IsEmpty(), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap3.IsFull(), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.Size() == 5, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.DeepestTable() == 1, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.AssignedTableEntries() == 4, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.UnAssignedTableEntries() == 6, "HashTableC()");

			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 0 ) == 'c', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 1 ) == 'e', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 2 ) == 'd', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 3 ) == 'f', "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 4 ) == 'g', "HashTableC()");

			UNIT_TEST_ASSERT_EXPECTED_START();
			hashMap3.RemoveByIndex(15);
			UNIT_TEST_ASSERT_EXPECTED_END();	

			hashMap4.RemoveByPayload('a');

			UNIT_TEST_POSITIVE(!hashMap4.IsEmpty(), "HashTableC()");
			UNIT_TEST_POSITIVE(!hashMap4.IsFull(), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap4.Size() == 6, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap4.DeepestTable() == 1, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap4.AssignedTableEntries() == 5, "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap4.UnAssignedTableEntries() == 5, "HashTableC()");

			UNIT_TEST_POSITIVE(!hashMap4.ContainsPayload('a'), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap4.ContainsPayload( 'b' ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap4.ContainsPayload( 'c' ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap4.ContainsPayload( 'd' ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap4.ContainsPayload( 'e' ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap4.ContainsPayload( 'f' ), "HashTableC()");
			UNIT_TEST_POSITIVE(hashMap4.ContainsPayload( 'g' ), "HashTableC()");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			hashMap4.RemoveByPayload('h');
			UNIT_TEST_ASSERT_EXPECTED_END();	

		UNIT_TEST_BLOCK_END();

		mState = kFinished;
	}





	UnitTestHashTable::UnitTestHashTable(const Dia::Core::Containers::String32& name)
		: UnitTestCoreContainers(name)
	{}

	UnitTestHashTable::UnitTestHashTable(void)
		: UnitTestCoreContainers()
	{}

	void UnitTestHashTable::DoTest()
	{
		UNIT_TEST_BLOCK_START();
			
			class HashFunctorTest1
			{
			public:
				typedef unsigned int Key;
				typedef Dia::Core::Containers::HashTableHashFunctionData TableData;

				unsigned int GetHashIndex(Key key, const TableData* tableData)const
				{
					if (key >= tableData->GetTableSize())
					{
						key = 0;
					}
					return key; 
				}

				unsigned int mNumberElements;
			};

			Dia::Core::Containers::HashTable<unsigned int, char, HashFunctorTest1> hashMapEmpty;
			
			UNIT_TEST_POSITIVE(hashMapEmpty.IsEmpty(), "HashTable()");
			UNIT_TEST_POSITIVE(hashMapEmpty.IsFull(), "HashTable()");
			UNIT_TEST_POSITIVE(hashMapEmpty.Size() == 0, "HashTable()");
			UNIT_TEST_POSITIVE(hashMapEmpty.Capacity() == 0, "HashTable()");
			UNIT_TEST_POSITIVE(!hashMapEmpty.IsSizeSet(), "HashTable()");

			Dia::Core::Containers::HashTable<unsigned int, char, HashFunctorTest1> hashMap(10, 15);

			UNIT_TEST_POSITIVE(hashMap.IsEmpty(), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap.IsFull(), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.Size() == 0, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.Capacity() == 10, "HashTable()");
			
			hashMap.Add(0, 'a');
			hashMap.Add(1, 'b');
			hashMap.Add(2, 'c');
			hashMap.Add(4, 'e');
			hashMap.Add(3, 'd');
			hashMap.Add(5, 'f');
			hashMap.Add(20,'g');		

			UNIT_TEST_POSITIVE(!hashMap.IsEmpty(), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap.IsFull(), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.Size() == 7, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.DeepestTable() == 2, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.AssignedTableEntries() == 6, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.UnAssignedTableEntries() == 9, "HashTable()");
			
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 0 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 1 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 2 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 3 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 4 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 5 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 20 ), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap.ContainsKey( 6 ), "HashTable()");

			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'a' ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'b' ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'c' ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'd' ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'e' ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'f' ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'g' ), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap.ContainsPayload( 'h' ), "HashTable()");
			
			class Equal
			{
			public:
				bool Equals(const char& a, const char& b)const
				{
					return (a == b);
				};
			};

			Equal compare;
			
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'a', compare), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'b', compare ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'c', compare ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'd', compare ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'e', compare ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'f', compare ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsPayload( 'g', compare ), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap.ContainsPayload( 'h', compare ), "HashTable()");

			Dia::Core::Containers::HashTable<unsigned int, char, HashFunctorTest1> hashMap2(hashMap);

			UNIT_TEST_POSITIVE(!hashMap2.IsEmpty(), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap2.IsFull(), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.Size() == 7, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.DeepestTable() == 2, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.AssignedTableEntries() == 6, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.UnAssignedTableEntries() == 9, "HashTable()");

			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 0 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 1 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 2 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 3 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 4 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 5 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 20 ), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap2.ContainsKey( 6 ), "HashTable()");

			Dia::Core::Containers::HashTable<unsigned int, char, HashFunctorTest1> hashMap3;
			hashMap3 = hashMap;

			UNIT_TEST_POSITIVE(!hashMap3.IsEmpty(), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap3.IsFull(), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.Size() == 7, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.DeepestTable() == 2, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.AssignedTableEntries() == 6, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.UnAssignedTableEntries() == 9, "HashTable()");

			UNIT_TEST_POSITIVE(hashMap3.ContainsKey( 0 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.ContainsKey( 1 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.ContainsKey( 2 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.ContainsKey( 3 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.ContainsKey( 4 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.ContainsKey( 5 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.ContainsKey( 20 ), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap3.ContainsKey( 6 ), "HashTable()");

			UNIT_TEST_POSITIVE(hashMap3.GetItem( 0 ) == 'a', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItem( 1 ) == 'b', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItem( 2 ) == 'c', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItem( 3 ) == 'd', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItem( 4 ) == 'e', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItem( 5 ) == 'f', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItem( 20 ) == 'g', "HashTable()");
			
			UNIT_TEST_POSITIVE(hashMap3.GetItemConst( 0 ) == 'a', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemConst( 1 ) == 'b', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemConst( 2 ) == 'c', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemConst( 3 ) == 'd', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemConst( 4 ) == 'e', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemConst( 5 ) == 'f', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemConst( 20 ) == 'g', "HashTable()");

			UNIT_TEST_POSITIVE(hashMap3.TryGetItem( 0 ) != NULL, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.TryGetItem( 11 ) == NULL, "HashTable()");

			UNIT_TEST_POSITIVE(hashMap3.TryGetItemConst( 0 ) != NULL, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.TryGetItemConst( 11 ) == NULL, "HashTable()");
			
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 0 ) == 'a', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 1 ) == 'b', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 2 ) == 'c', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 3 ) == 'e', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 4 ) == 'd', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 5 ) == 'f', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 6 ) == 'g', "HashTable()");
	
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndexConst( 0 ) == 'a', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndexConst( 1 ) == 'b', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndexConst( 2 ) == 'c', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndexConst( 3 ) == 'e', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndexConst( 4 ) == 'd', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndexConst( 5 ) == 'f', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndexConst( 6 ) == 'g', "HashTable()");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			hashMap.Add(0, 'h');
			UNIT_TEST_ASSERT_EXPECTED_END();	


		UNIT_TEST_BLOCK_END();
		
		UNIT_TEST_BLOCK_START();

			class HashFunctorTest1
			{
			public:
				typedef unsigned int Key;
				typedef Dia::Core::Containers::HashTableHashFunctionData TableData;

				unsigned int GetHashIndex(Key key, const TableData* tableData)const
				{
					if (key >= tableData->GetTableSize())
					{
						key = 0;
					}
					return key; 
				}
			};

			Dia::Core::Containers::HashTable<unsigned int, char, HashFunctorTest1> hashMap;
			hashMap.SetSize(10, 15);

			UNIT_TEST_POSITIVE(hashMap.IsEmpty(), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap.IsFull(), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.Size() == 0, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.Capacity() == 10, "HashTable()");

			hashMap.Add(0, 'a');
			hashMap.Add(1, 'b');
			hashMap.Add(2, 'c');
			hashMap.Add(4, 'e');
			hashMap.Add(3, 'd');
			hashMap.Add(5, 'f');
			hashMap.Add(20,'g');		

			UNIT_TEST_POSITIVE(!hashMap.IsEmpty(), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap.IsFull(), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.Size() == 7, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.DeepestTable() == 2, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.AssignedTableEntries() == 6, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.UnAssignedTableEntries() == 9, "HashTable()");

			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 0 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 1 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 2 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 3 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 4 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 5 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.ContainsKey( 20 ), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap.ContainsKey( 6 ), "HashTable()");
			
			Dia::Core::Containers::HashTable<unsigned int, char, HashFunctorTest1> hashMap2(hashMap);
			Dia::Core::Containers::HashTable<unsigned int, char, HashFunctorTest1> hashMap3(hashMap);
			Dia::Core::Containers::HashTable<unsigned int, char, HashFunctorTest1> hashMap4(hashMap);

			hashMap.RemoveAll();

			UNIT_TEST_POSITIVE(hashMap.IsEmpty(), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap.IsFull(), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.Size() == 0, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.DeepestTable() == 0, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.AssignedTableEntries() == 0, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap.UnAssignedTableEntries() == 15, "HashTable()");
			
			hashMap2.Remove(5);

			UNIT_TEST_POSITIVE(!hashMap2.IsEmpty(), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap2.IsFull(), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.Size() == 6, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.DeepestTable() == 2, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.AssignedTableEntries() == 5, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.UnAssignedTableEntries() == 10, "HashTable()");

			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 0 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 1 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 2 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 3 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 4 ), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap2.ContainsKey( 5 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 20 ), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap2.ContainsKey( 6 ), "HashTable()");
			
			hashMap2.Remove(20);

			UNIT_TEST_POSITIVE(!hashMap2.IsEmpty(), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap2.IsFull(), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.Size() == 5, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.DeepestTable() == 1, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.AssignedTableEntries() == 5, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.UnAssignedTableEntries() == 10, "HashTable()");

			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 0 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 1 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 2 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 3 ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap2.ContainsKey( 4 ), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap2.ContainsKey( 5 ), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap2.ContainsKey( 20 ), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap2.ContainsKey( 6 ), "HashTable()");
			
			hashMap3.RemoveByIndex(0);
			
			UNIT_TEST_POSITIVE(!hashMap3.IsEmpty(), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap3.IsFull(), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.Size() == 6, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.DeepestTable() == 1, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.AssignedTableEntries() == 5, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.UnAssignedTableEntries() == 10, "HashTable()");

			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 0 ) == 'b', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 1 ) == 'c', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 2 ) == 'e', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 3 ) == 'd', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 4 ) == 'f', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 5 ) == 'g', "HashTable()");

			hashMap3.RemoveByIndex(0);

			UNIT_TEST_POSITIVE(!hashMap3.IsEmpty(), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap3.IsFull(), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.Size() == 5, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.DeepestTable() == 1, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.AssignedTableEntries() == 4, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.UnAssignedTableEntries() == 11, "HashTable()");

			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 0 ) == 'c', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 1 ) == 'e', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 2 ) == 'd', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 3 ) == 'f', "HashTable()");
			UNIT_TEST_POSITIVE(hashMap3.GetItemByIndex( 4 ) == 'g', "HashTable()");

			UNIT_TEST_ASSERT_EXPECTED_START();
			hashMap3.RemoveByIndex(15);
			UNIT_TEST_ASSERT_EXPECTED_END();	

			hashMap4.RemoveByPayload('a');

			UNIT_TEST_POSITIVE(!hashMap4.IsEmpty(), "HashTable()");
			UNIT_TEST_POSITIVE(!hashMap4.IsFull(), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap4.Size() == 6, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap4.DeepestTable() == 1, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap4.AssignedTableEntries() == 5, "HashTable()");
			UNIT_TEST_POSITIVE(hashMap4.UnAssignedTableEntries() == 10, "HashTable()");

			UNIT_TEST_POSITIVE(!hashMap4.ContainsPayload('a'), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap4.ContainsPayload( 'b' ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap4.ContainsPayload( 'c' ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap4.ContainsPayload( 'd' ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap4.ContainsPayload( 'e' ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap4.ContainsPayload( 'f' ), "HashTable()");
			UNIT_TEST_POSITIVE(hashMap4.ContainsPayload( 'g' ), "HashTable()");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			hashMap4.RemoveByPayload('h');
			UNIT_TEST_ASSERT_EXPECTED_END();	

		UNIT_TEST_BLOCK_END();

		mState = kFinished;
	}
}