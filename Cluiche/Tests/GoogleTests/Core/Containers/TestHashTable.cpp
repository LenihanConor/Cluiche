#include <gtest/gtest.h>
#include <DiaCore/Containers/HashTables/HashTableC.h>
#include <DiaCore/Containers/HashTables/HashTable.h>

using namespace Dia::Core::Containers;

class HashFunctorTest
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

class EqualCompare
{
public:
	bool Equals(const char& a, const char& b)const
	{
		return (a == b);
	};
};

TEST(HashTableC, AddAndQuery_WorksCorrectly)
{
	HashTableC<unsigned int, char, HashFunctorTest, 10> hashMap;

	EXPECT_TRUE(hashMap.IsEmpty());
	EXPECT_FALSE(hashMap.IsFull());
	EXPECT_EQ(hashMap.Size(), 0);
	EXPECT_EQ(hashMap.Capacity(), 10);

	hashMap.Add(0, 'a');
	hashMap.Add(1, 'b');
	hashMap.Add(2, 'c');
	hashMap.Add(4, 'e');
	hashMap.Add(3, 'd');
	hashMap.Add(5, 'f');
	hashMap.Add(20, 'g');

	EXPECT_FALSE(hashMap.IsEmpty());
	EXPECT_FALSE(hashMap.IsFull());
	EXPECT_EQ(hashMap.Size(), 7);
	EXPECT_EQ(hashMap.DeepestTable(), 2);
	EXPECT_EQ(hashMap.AssignedTableEntries(), 6);
	EXPECT_EQ(hashMap.UnAssignedTableEntries(), 4);

	EXPECT_TRUE(hashMap.ContainsKey(0));
	EXPECT_TRUE(hashMap.ContainsKey(1));
	EXPECT_TRUE(hashMap.ContainsKey(2));
	EXPECT_TRUE(hashMap.ContainsKey(3));
	EXPECT_TRUE(hashMap.ContainsKey(4));
	EXPECT_TRUE(hashMap.ContainsKey(5));
	EXPECT_TRUE(hashMap.ContainsKey(20));
	EXPECT_FALSE(hashMap.ContainsKey(6));

	EXPECT_TRUE(hashMap.ContainsPayload('a'));
	EXPECT_TRUE(hashMap.ContainsPayload('b'));
	EXPECT_TRUE(hashMap.ContainsPayload('c'));
	EXPECT_TRUE(hashMap.ContainsPayload('d'));
	EXPECT_TRUE(hashMap.ContainsPayload('e'));
	EXPECT_TRUE(hashMap.ContainsPayload('f'));
	EXPECT_TRUE(hashMap.ContainsPayload('g'));
	EXPECT_FALSE(hashMap.ContainsPayload('h'));

	EqualCompare compare;

	EXPECT_TRUE(hashMap.ContainsPayload('a', compare));
	EXPECT_TRUE(hashMap.ContainsPayload('b', compare));
	EXPECT_TRUE(hashMap.ContainsPayload('c', compare));
	EXPECT_TRUE(hashMap.ContainsPayload('d', compare));
	EXPECT_TRUE(hashMap.ContainsPayload('e', compare));
	EXPECT_TRUE(hashMap.ContainsPayload('f', compare));
	EXPECT_TRUE(hashMap.ContainsPayload('g', compare));
	EXPECT_FALSE(hashMap.ContainsPayload('h', compare));

	HashTableC<unsigned int, char, HashFunctorTest, 10> hashMap2(hashMap);

	EXPECT_FALSE(hashMap2.IsEmpty());
	EXPECT_FALSE(hashMap2.IsFull());
	EXPECT_EQ(hashMap2.Size(), 7);
	EXPECT_EQ(hashMap2.DeepestTable(), 2);
	EXPECT_EQ(hashMap2.AssignedTableEntries(), 6);
	EXPECT_EQ(hashMap2.UnAssignedTableEntries(), 4);

	EXPECT_TRUE(hashMap2.ContainsKey(0));
	EXPECT_TRUE(hashMap2.ContainsKey(1));
	EXPECT_TRUE(hashMap2.ContainsKey(2));
	EXPECT_TRUE(hashMap2.ContainsKey(3));
	EXPECT_TRUE(hashMap2.ContainsKey(4));
	EXPECT_TRUE(hashMap2.ContainsKey(5));
	EXPECT_TRUE(hashMap2.ContainsKey(20));
	EXPECT_FALSE(hashMap2.ContainsKey(6));

	HashTableC<unsigned int, char, HashFunctorTest, 10> hashMap3;
	hashMap3 = hashMap;

	EXPECT_FALSE(hashMap3.IsEmpty());
	EXPECT_FALSE(hashMap3.IsFull());
	EXPECT_EQ(hashMap3.Size(), 7);
	EXPECT_EQ(hashMap3.DeepestTable(), 2);
	EXPECT_EQ(hashMap3.AssignedTableEntries(), 6);
	EXPECT_EQ(hashMap3.UnAssignedTableEntries(), 4);

	EXPECT_TRUE(hashMap3.ContainsKey(0));
	EXPECT_TRUE(hashMap3.ContainsKey(1));
	EXPECT_TRUE(hashMap3.ContainsKey(2));
	EXPECT_TRUE(hashMap3.ContainsKey(3));
	EXPECT_TRUE(hashMap3.ContainsKey(4));
	EXPECT_TRUE(hashMap3.ContainsKey(5));
	EXPECT_TRUE(hashMap3.ContainsKey(20));
	EXPECT_FALSE(hashMap3.ContainsKey(6));

	EXPECT_EQ(hashMap3.GetItem(0), 'a');
	EXPECT_EQ(hashMap3.GetItem(1), 'b');
	EXPECT_EQ(hashMap3.GetItem(2), 'c');
	EXPECT_EQ(hashMap3.GetItem(3), 'd');
	EXPECT_EQ(hashMap3.GetItem(4), 'e');
	EXPECT_EQ(hashMap3.GetItem(5), 'f');
	EXPECT_EQ(hashMap3.GetItem(20), 'g');

	EXPECT_EQ(hashMap3.GetItemConst(0), 'a');
	EXPECT_EQ(hashMap3.GetItemConst(1), 'b');
	EXPECT_EQ(hashMap3.GetItemConst(2), 'c');
	EXPECT_EQ(hashMap3.GetItemConst(3), 'd');
	EXPECT_EQ(hashMap3.GetItemConst(4), 'e');
	EXPECT_EQ(hashMap3.GetItemConst(5), 'f');
	EXPECT_EQ(hashMap3.GetItemConst(20), 'g');

	EXPECT_NE(hashMap3.TryGetItem(0), nullptr);
	EXPECT_EQ(hashMap3.TryGetItem(11), nullptr);

	EXPECT_NE(hashMap3.TryGetItemConst(0), nullptr);
	EXPECT_EQ(hashMap3.TryGetItemConst(11), nullptr);

	EXPECT_EQ(hashMap3.GetItemByIndex(0), 'a');
	EXPECT_EQ(hashMap3.GetItemByIndex(1), 'b');
	EXPECT_EQ(hashMap3.GetItemByIndex(2), 'c');
	EXPECT_EQ(hashMap3.GetItemByIndex(3), 'e');
	EXPECT_EQ(hashMap3.GetItemByIndex(4), 'd');
	EXPECT_EQ(hashMap3.GetItemByIndex(5), 'f');
	EXPECT_EQ(hashMap3.GetItemByIndex(6), 'g');

	EXPECT_EQ(hashMap3.GetItemByIndexConst(0), 'a');
	EXPECT_EQ(hashMap3.GetItemByIndexConst(1), 'b');
	EXPECT_EQ(hashMap3.GetItemByIndexConst(2), 'c');
	EXPECT_EQ(hashMap3.GetItemByIndexConst(3), 'e');
	EXPECT_EQ(hashMap3.GetItemByIndexConst(4), 'd');
	EXPECT_EQ(hashMap3.GetItemByIndexConst(5), 'f');
	EXPECT_EQ(hashMap3.GetItemByIndexConst(6), 'g');

	EXPECT_DEATH(hashMap.Add(0, 'h'), "");
}

TEST(HashTableC, RemoveOperations_WorkCorrectly)
{
	HashTableC<unsigned int, char, HashFunctorTest, 10> hashMap;

	EXPECT_TRUE(hashMap.IsEmpty());
	EXPECT_FALSE(hashMap.IsFull());
	EXPECT_EQ(hashMap.Size(), 0);
	EXPECT_EQ(hashMap.Capacity(), 10);

	hashMap.Add(0, 'a');
	hashMap.Add(1, 'b');
	hashMap.Add(2, 'c');
	hashMap.Add(4, 'e');
	hashMap.Add(3, 'd');
	hashMap.Add(5, 'f');
	hashMap.Add(20, 'g');

	EXPECT_FALSE(hashMap.IsEmpty());
	EXPECT_FALSE(hashMap.IsFull());
	EXPECT_EQ(hashMap.Size(), 7);
	EXPECT_EQ(hashMap.DeepestTable(), 2);
	EXPECT_EQ(hashMap.AssignedTableEntries(), 6);
	EXPECT_EQ(hashMap.UnAssignedTableEntries(), 4);

	EXPECT_TRUE(hashMap.ContainsKey(0));
	EXPECT_TRUE(hashMap.ContainsKey(1));
	EXPECT_TRUE(hashMap.ContainsKey(2));
	EXPECT_TRUE(hashMap.ContainsKey(3));
	EXPECT_TRUE(hashMap.ContainsKey(4));
	EXPECT_TRUE(hashMap.ContainsKey(5));
	EXPECT_TRUE(hashMap.ContainsKey(20));
	EXPECT_FALSE(hashMap.ContainsKey(6));

	HashTableC<unsigned int, char, HashFunctorTest, 10> hashMap2(hashMap);
	HashTableC<unsigned int, char, HashFunctorTest, 10> hashMap3(hashMap);
	HashTableC<unsigned int, char, HashFunctorTest, 10> hashMap4(hashMap);

	hashMap.RemoveAll();

	EXPECT_TRUE(hashMap.IsEmpty());
	EXPECT_FALSE(hashMap.IsFull());
	EXPECT_EQ(hashMap.Size(), 0);
	EXPECT_EQ(hashMap.DeepestTable(), 0);
	EXPECT_EQ(hashMap.AssignedTableEntries(), 0);
	EXPECT_EQ(hashMap.UnAssignedTableEntries(), 10);

	hashMap2.Remove(5);

	EXPECT_FALSE(hashMap2.IsEmpty());
	EXPECT_FALSE(hashMap2.IsFull());
	EXPECT_EQ(hashMap2.Size(), 6);
	EXPECT_EQ(hashMap2.DeepestTable(), 2);
	EXPECT_EQ(hashMap2.AssignedTableEntries(), 5);
	EXPECT_EQ(hashMap2.UnAssignedTableEntries(), 5);

	EXPECT_TRUE(hashMap2.ContainsKey(0));
	EXPECT_TRUE(hashMap2.ContainsKey(1));
	EXPECT_TRUE(hashMap2.ContainsKey(2));
	EXPECT_TRUE(hashMap2.ContainsKey(3));
	EXPECT_TRUE(hashMap2.ContainsKey(4));
	EXPECT_FALSE(hashMap2.ContainsKey(5));
	EXPECT_TRUE(hashMap2.ContainsKey(20));
	EXPECT_FALSE(hashMap2.ContainsKey(6));

	hashMap2.Remove(20);

	EXPECT_FALSE(hashMap2.IsEmpty());
	EXPECT_FALSE(hashMap2.IsFull());
	EXPECT_EQ(hashMap2.Size(), 5);
	EXPECT_EQ(hashMap2.DeepestTable(), 1);
	EXPECT_EQ(hashMap2.AssignedTableEntries(), 5);
	EXPECT_EQ(hashMap2.UnAssignedTableEntries(), 5);

	EXPECT_TRUE(hashMap2.ContainsKey(0));
	EXPECT_TRUE(hashMap2.ContainsKey(1));
	EXPECT_TRUE(hashMap2.ContainsKey(2));
	EXPECT_TRUE(hashMap2.ContainsKey(3));
	EXPECT_TRUE(hashMap2.ContainsKey(4));
	EXPECT_FALSE(hashMap2.ContainsKey(5));
	EXPECT_FALSE(hashMap2.ContainsKey(20));
	EXPECT_FALSE(hashMap2.ContainsKey(6));

	hashMap3.RemoveByIndex(0);

	EXPECT_FALSE(hashMap3.IsEmpty());
	EXPECT_FALSE(hashMap3.IsFull());
	EXPECT_EQ(hashMap3.Size(), 6);
	EXPECT_EQ(hashMap3.DeepestTable(), 1);
	EXPECT_EQ(hashMap3.AssignedTableEntries(), 6);
	EXPECT_EQ(hashMap3.UnAssignedTableEntries(), 4);

	EXPECT_EQ(hashMap3.GetItemByIndex(0), 'b');
	EXPECT_EQ(hashMap3.GetItemByIndex(1), 'c');
	EXPECT_EQ(hashMap3.GetItemByIndex(2), 'e');
	EXPECT_EQ(hashMap3.GetItemByIndex(3), 'd');
	EXPECT_EQ(hashMap3.GetItemByIndex(4), 'f');
	EXPECT_EQ(hashMap3.GetItemByIndex(5), 'g');

	hashMap3.RemoveByIndex(0);

	EXPECT_FALSE(hashMap3.IsEmpty());
	EXPECT_FALSE(hashMap3.IsFull());
	EXPECT_EQ(hashMap3.Size(), 5);
	EXPECT_EQ(hashMap3.DeepestTable(), 1);
	EXPECT_EQ(hashMap3.AssignedTableEntries(), 5);
	EXPECT_EQ(hashMap3.UnAssignedTableEntries(), 5);

	EXPECT_EQ(hashMap3.GetItemByIndex(0), 'c');
	EXPECT_EQ(hashMap3.GetItemByIndex(1), 'e');
	EXPECT_EQ(hashMap3.GetItemByIndex(2), 'd');
	EXPECT_EQ(hashMap3.GetItemByIndex(3), 'f');
	EXPECT_EQ(hashMap3.GetItemByIndex(4), 'g');

	EXPECT_DEATH(hashMap3.RemoveByIndex(15), "");

	hashMap4.RemoveByPayload('a');

	EXPECT_FALSE(hashMap4.IsEmpty());
	EXPECT_FALSE(hashMap4.IsFull());
	EXPECT_EQ(hashMap4.Size(), 6);
	EXPECT_EQ(hashMap4.DeepestTable(), 1);
	EXPECT_EQ(hashMap4.AssignedTableEntries(), 6);
	EXPECT_EQ(hashMap4.UnAssignedTableEntries(), 4);

	EXPECT_FALSE(hashMap4.ContainsPayload('a'));
	EXPECT_TRUE(hashMap4.ContainsPayload('b'));
	EXPECT_TRUE(hashMap4.ContainsPayload('c'));
	EXPECT_TRUE(hashMap4.ContainsPayload('d'));
	EXPECT_TRUE(hashMap4.ContainsPayload('e'));
	EXPECT_TRUE(hashMap4.ContainsPayload('f'));
	EXPECT_TRUE(hashMap4.ContainsPayload('g'));

	EXPECT_DEATH(hashMap4.RemoveByPayload('h'), "");
}

TEST(HashTable, AddAndQuery_WorksCorrectly)
{
	HashTable<unsigned int, char, HashFunctorTest> hashMapEmpty;

	EXPECT_TRUE(hashMapEmpty.IsEmpty());
	EXPECT_TRUE(hashMapEmpty.IsFull());
	EXPECT_EQ(hashMapEmpty.Size(), 0);
	EXPECT_EQ(hashMapEmpty.Capacity(), 0);
	EXPECT_FALSE(hashMapEmpty.IsSizeSet());

	HashTable<unsigned int, char, HashFunctorTest> hashMap(10, 15);

	EXPECT_TRUE(hashMap.IsEmpty());
	EXPECT_FALSE(hashMap.IsFull());
	EXPECT_EQ(hashMap.Size(), 0);
	EXPECT_EQ(hashMap.Capacity(), 10);

	hashMap.Add(0, 'a');
	hashMap.Add(1, 'b');
	hashMap.Add(2, 'c');
	hashMap.Add(4, 'e');
	hashMap.Add(3, 'd');
	hashMap.Add(5, 'f');
	hashMap.Add(20, 'g');

	EXPECT_FALSE(hashMap.IsEmpty());
	EXPECT_FALSE(hashMap.IsFull());
	EXPECT_EQ(hashMap.Size(), 7);
	EXPECT_EQ(hashMap.DeepestTable(), 2);
	EXPECT_EQ(hashMap.AssignedTableEntries(), 6);
	EXPECT_EQ(hashMap.UnAssignedTableEntries(), 9);

	EXPECT_TRUE(hashMap.ContainsKey(0));
	EXPECT_TRUE(hashMap.ContainsKey(1));
	EXPECT_TRUE(hashMap.ContainsKey(2));
	EXPECT_TRUE(hashMap.ContainsKey(3));
	EXPECT_TRUE(hashMap.ContainsKey(4));
	EXPECT_TRUE(hashMap.ContainsKey(5));
	EXPECT_TRUE(hashMap.ContainsKey(20));
	EXPECT_FALSE(hashMap.ContainsKey(6));

	EXPECT_TRUE(hashMap.ContainsPayload('a'));
	EXPECT_TRUE(hashMap.ContainsPayload('b'));
	EXPECT_TRUE(hashMap.ContainsPayload('c'));
	EXPECT_TRUE(hashMap.ContainsPayload('d'));
	EXPECT_TRUE(hashMap.ContainsPayload('e'));
	EXPECT_TRUE(hashMap.ContainsPayload('f'));
	EXPECT_TRUE(hashMap.ContainsPayload('g'));
	EXPECT_FALSE(hashMap.ContainsPayload('h'));

	EqualCompare compare;

	EXPECT_TRUE(hashMap.ContainsPayload('a', compare));
	EXPECT_TRUE(hashMap.ContainsPayload('b', compare));
	EXPECT_TRUE(hashMap.ContainsPayload('c', compare));
	EXPECT_TRUE(hashMap.ContainsPayload('d', compare));
	EXPECT_TRUE(hashMap.ContainsPayload('e', compare));
	EXPECT_TRUE(hashMap.ContainsPayload('f', compare));
	EXPECT_TRUE(hashMap.ContainsPayload('g', compare));
	EXPECT_FALSE(hashMap.ContainsPayload('h', compare));

	HashTable<unsigned int, char, HashFunctorTest> hashMap2(hashMap);

	EXPECT_FALSE(hashMap2.IsEmpty());
	EXPECT_FALSE(hashMap2.IsFull());
	EXPECT_EQ(hashMap2.Size(), 7);
	EXPECT_EQ(hashMap2.DeepestTable(), 2);
	EXPECT_EQ(hashMap2.AssignedTableEntries(), 6);
	EXPECT_EQ(hashMap2.UnAssignedTableEntries(), 9);

	EXPECT_TRUE(hashMap2.ContainsKey(0));
	EXPECT_TRUE(hashMap2.ContainsKey(1));
	EXPECT_TRUE(hashMap2.ContainsKey(2));
	EXPECT_TRUE(hashMap2.ContainsKey(3));
	EXPECT_TRUE(hashMap2.ContainsKey(4));
	EXPECT_TRUE(hashMap2.ContainsKey(5));
	EXPECT_TRUE(hashMap2.ContainsKey(20));
	EXPECT_FALSE(hashMap2.ContainsKey(6));

	HashTable<unsigned int, char, HashFunctorTest> hashMap3;
	hashMap3 = hashMap;

	EXPECT_FALSE(hashMap3.IsEmpty());
	EXPECT_FALSE(hashMap3.IsFull());
	EXPECT_EQ(hashMap3.Size(), 7);
	EXPECT_EQ(hashMap3.DeepestTable(), 2);
	EXPECT_EQ(hashMap3.AssignedTableEntries(), 6);
	EXPECT_EQ(hashMap3.UnAssignedTableEntries(), 9);

	EXPECT_TRUE(hashMap3.ContainsKey(0));
	EXPECT_TRUE(hashMap3.ContainsKey(1));
	EXPECT_TRUE(hashMap3.ContainsKey(2));
	EXPECT_TRUE(hashMap3.ContainsKey(3));
	EXPECT_TRUE(hashMap3.ContainsKey(4));
	EXPECT_TRUE(hashMap3.ContainsKey(5));
	EXPECT_TRUE(hashMap3.ContainsKey(20));
	EXPECT_FALSE(hashMap3.ContainsKey(6));

	EXPECT_EQ(hashMap3.GetItem(0), 'a');
	EXPECT_EQ(hashMap3.GetItem(1), 'b');
	EXPECT_EQ(hashMap3.GetItem(2), 'c');
	EXPECT_EQ(hashMap3.GetItem(3), 'd');
	EXPECT_EQ(hashMap3.GetItem(4), 'e');
	EXPECT_EQ(hashMap3.GetItem(5), 'f');
	EXPECT_EQ(hashMap3.GetItem(20), 'g');

	EXPECT_EQ(hashMap3.GetItemConst(0), 'a');
	EXPECT_EQ(hashMap3.GetItemConst(1), 'b');
	EXPECT_EQ(hashMap3.GetItemConst(2), 'c');
	EXPECT_EQ(hashMap3.GetItemConst(3), 'd');
	EXPECT_EQ(hashMap3.GetItemConst(4), 'e');
	EXPECT_EQ(hashMap3.GetItemConst(5), 'f');
	EXPECT_EQ(hashMap3.GetItemConst(20), 'g');

	EXPECT_NE(hashMap3.TryGetItem(0), nullptr);
	EXPECT_EQ(hashMap3.TryGetItem(11), nullptr);

	EXPECT_NE(hashMap3.TryGetItemConst(0), nullptr);
	EXPECT_EQ(hashMap3.TryGetItemConst(11), nullptr);

	EXPECT_EQ(hashMap3.GetItemByIndex(0), 'a');
	EXPECT_EQ(hashMap3.GetItemByIndex(1), 'b');
	EXPECT_EQ(hashMap3.GetItemByIndex(2), 'c');
	EXPECT_EQ(hashMap3.GetItemByIndex(3), 'e');
	EXPECT_EQ(hashMap3.GetItemByIndex(4), 'd');
	EXPECT_EQ(hashMap3.GetItemByIndex(5), 'f');
	EXPECT_EQ(hashMap3.GetItemByIndex(6), 'g');

	EXPECT_EQ(hashMap3.GetItemByIndexConst(0), 'a');
	EXPECT_EQ(hashMap3.GetItemByIndexConst(1), 'b');
	EXPECT_EQ(hashMap3.GetItemByIndexConst(2), 'c');
	EXPECT_EQ(hashMap3.GetItemByIndexConst(3), 'e');
	EXPECT_EQ(hashMap3.GetItemByIndexConst(4), 'd');
	EXPECT_EQ(hashMap3.GetItemByIndexConst(5), 'f');
	EXPECT_EQ(hashMap3.GetItemByIndexConst(6), 'g');

	EXPECT_DEATH(hashMap.Add(0, 'h'), "");
}

TEST(HashTable, RemoveOperations_WorkCorrectly)
{
	HashTable<unsigned int, char, HashFunctorTest> hashMap;
	hashMap.SetSize(10, 15);

	EXPECT_TRUE(hashMap.IsEmpty());
	EXPECT_FALSE(hashMap.IsFull());
	EXPECT_EQ(hashMap.Size(), 0);
	EXPECT_EQ(hashMap.Capacity(), 10);

	hashMap.Add(0, 'a');
	hashMap.Add(1, 'b');
	hashMap.Add(2, 'c');
	hashMap.Add(4, 'e');
	hashMap.Add(3, 'd');
	hashMap.Add(5, 'f');
	hashMap.Add(20, 'g');

	EXPECT_FALSE(hashMap.IsEmpty());
	EXPECT_FALSE(hashMap.IsFull());
	EXPECT_EQ(hashMap.Size(), 7);
	EXPECT_EQ(hashMap.DeepestTable(), 2);
	EXPECT_EQ(hashMap.AssignedTableEntries(), 6);
	EXPECT_EQ(hashMap.UnAssignedTableEntries(), 9);

	EXPECT_TRUE(hashMap.ContainsKey(0));
	EXPECT_TRUE(hashMap.ContainsKey(1));
	EXPECT_TRUE(hashMap.ContainsKey(2));
	EXPECT_TRUE(hashMap.ContainsKey(3));
	EXPECT_TRUE(hashMap.ContainsKey(4));
	EXPECT_TRUE(hashMap.ContainsKey(5));
	EXPECT_TRUE(hashMap.ContainsKey(20));
	EXPECT_FALSE(hashMap.ContainsKey(6));

	HashTable<unsigned int, char, HashFunctorTest> hashMap2(hashMap);
	HashTable<unsigned int, char, HashFunctorTest> hashMap3(hashMap);
	HashTable<unsigned int, char, HashFunctorTest> hashMap4(hashMap);

	hashMap.RemoveAll();

	EXPECT_TRUE(hashMap.IsEmpty());
	EXPECT_FALSE(hashMap.IsFull());
	EXPECT_EQ(hashMap.Size(), 0);
	EXPECT_EQ(hashMap.DeepestTable(), 0);
	EXPECT_EQ(hashMap.AssignedTableEntries(), 0);
	EXPECT_EQ(hashMap.UnAssignedTableEntries(), 15);

	hashMap2.Remove(5);

	EXPECT_FALSE(hashMap2.IsEmpty());
	EXPECT_FALSE(hashMap2.IsFull());
	EXPECT_EQ(hashMap2.Size(), 6);
	EXPECT_EQ(hashMap2.DeepestTable(), 2);
	EXPECT_EQ(hashMap2.AssignedTableEntries(), 5);
	EXPECT_EQ(hashMap2.UnAssignedTableEntries(), 10);

	EXPECT_TRUE(hashMap2.ContainsKey(0));
	EXPECT_TRUE(hashMap2.ContainsKey(1));
	EXPECT_TRUE(hashMap2.ContainsKey(2));
	EXPECT_TRUE(hashMap2.ContainsKey(3));
	EXPECT_TRUE(hashMap2.ContainsKey(4));
	EXPECT_FALSE(hashMap2.ContainsKey(5));
	EXPECT_TRUE(hashMap2.ContainsKey(20));
	EXPECT_FALSE(hashMap2.ContainsKey(6));

	hashMap2.Remove(20);

	EXPECT_FALSE(hashMap2.IsEmpty());
	EXPECT_FALSE(hashMap2.IsFull());
	EXPECT_EQ(hashMap2.Size(), 5);
	EXPECT_EQ(hashMap2.DeepestTable(), 1);
	EXPECT_EQ(hashMap2.AssignedTableEntries(), 5);
	EXPECT_EQ(hashMap2.UnAssignedTableEntries(), 10);

	EXPECT_TRUE(hashMap2.ContainsKey(0));
	EXPECT_TRUE(hashMap2.ContainsKey(1));
	EXPECT_TRUE(hashMap2.ContainsKey(2));
	EXPECT_TRUE(hashMap2.ContainsKey(3));
	EXPECT_TRUE(hashMap2.ContainsKey(4));
	EXPECT_FALSE(hashMap2.ContainsKey(5));
	EXPECT_FALSE(hashMap2.ContainsKey(20));
	EXPECT_FALSE(hashMap2.ContainsKey(6));

	hashMap3.RemoveByIndex(0);

	EXPECT_FALSE(hashMap3.IsEmpty());
	EXPECT_FALSE(hashMap3.IsFull());
	EXPECT_EQ(hashMap3.Size(), 6);
	EXPECT_EQ(hashMap3.DeepestTable(), 1);
	EXPECT_EQ(hashMap3.AssignedTableEntries(), 6);
	EXPECT_EQ(hashMap3.UnAssignedTableEntries(), 9);

	EXPECT_EQ(hashMap3.GetItemByIndex(0), 'b');
	EXPECT_EQ(hashMap3.GetItemByIndex(1), 'c');
	EXPECT_EQ(hashMap3.GetItemByIndex(2), 'e');
	EXPECT_EQ(hashMap3.GetItemByIndex(3), 'd');
	EXPECT_EQ(hashMap3.GetItemByIndex(4), 'f');
	EXPECT_EQ(hashMap3.GetItemByIndex(5), 'g');

	hashMap3.RemoveByIndex(0);

	EXPECT_FALSE(hashMap3.IsEmpty());
	EXPECT_FALSE(hashMap3.IsFull());
	EXPECT_EQ(hashMap3.Size(), 5);
	EXPECT_EQ(hashMap3.DeepestTable(), 1);
	EXPECT_EQ(hashMap3.AssignedTableEntries(), 5);
	EXPECT_EQ(hashMap3.UnAssignedTableEntries(), 10);

	EXPECT_EQ(hashMap3.GetItemByIndex(0), 'c');
	EXPECT_EQ(hashMap3.GetItemByIndex(1), 'e');
	EXPECT_EQ(hashMap3.GetItemByIndex(2), 'd');
	EXPECT_EQ(hashMap3.GetItemByIndex(3), 'f');
	EXPECT_EQ(hashMap3.GetItemByIndex(4), 'g');

	EXPECT_DEATH(hashMap3.RemoveByIndex(15), "");

	hashMap4.RemoveByPayload('a');

	EXPECT_FALSE(hashMap4.IsEmpty());
	EXPECT_FALSE(hashMap4.IsFull());
	EXPECT_EQ(hashMap4.Size(), 6);
	EXPECT_EQ(hashMap4.DeepestTable(), 1);
	EXPECT_EQ(hashMap4.AssignedTableEntries(), 6);
	EXPECT_EQ(hashMap4.UnAssignedTableEntries(), 9);

	EXPECT_FALSE(hashMap4.ContainsPayload('a'));
	EXPECT_TRUE(hashMap4.ContainsPayload('b'));
	EXPECT_TRUE(hashMap4.ContainsPayload('c'));
	EXPECT_TRUE(hashMap4.ContainsPayload('d'));
	EXPECT_TRUE(hashMap4.ContainsPayload('e'));
	EXPECT_TRUE(hashMap4.ContainsPayload('f'));
	EXPECT_TRUE(hashMap4.ContainsPayload('g'));

	EXPECT_DEATH(hashMap4.RemoveByPayload('h'), "");
}
