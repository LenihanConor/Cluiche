////////////////////////////////////////////////////////////////////////////////
// Tests for CEF page ID management logic
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

TEST(CEFPageManagement, PageIdIncrement)
{
	int nextPageId = 0;
	EXPECT_EQ(0, nextPageId++);
	EXPECT_EQ(1, nextPageId++);
	EXPECT_EQ(2, nextPageId++);
	EXPECT_EQ(3, nextPageId);
}

TEST(CEFPageManagement, DynamicArrayC_MaxPages)
{
	Dia::Core::Containers::DynamicArrayC<int, 16> pages;
	for (int i = 0; i < 16; ++i)
		pages.Add(i);

	EXPECT_EQ(16u, pages.Size());
}

TEST(CEFPageManagement, DynamicArrayC_RemoveAt)
{
	Dia::Core::Containers::DynamicArrayC<int, 16> pages;
	pages.Add(10);
	pages.Add(20);
	pages.Add(30);

	pages.RemoveAt(1);

	EXPECT_EQ(2u, pages.Size());
	EXPECT_EQ(10, pages[0]);
	EXPECT_EQ(30, pages[1]);
}
