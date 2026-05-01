////////////////////////////////////////////////////////////////////////////////
// Tests for CEFUtils URL parsing and path traversal detection
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaUICEF/CEFUtils.h>

using namespace Dia::UICEF::Utils;

TEST(CEFURLParsing, BasicPath_ExtractsCorrectly)
{
	EXPECT_EQ("app/index.html", ExtractPathFromDiaURL("dia://app/index.html"));
}

TEST(CEFURLParsing, NestedPath_ExtractsCorrectly)
{
	EXPECT_EQ("assets/ui/styles/main.css", ExtractPathFromDiaURL("dia://assets/ui/styles/main.css"));
}

TEST(CEFURLParsing, RootFile_ExtractsCorrectly)
{
	EXPECT_EQ("index.html", ExtractPathFromDiaURL("dia://index.html"));
}

TEST(CEFURLParsing, NonDiaURL_ReturnsAsIs)
{
	EXPECT_EQ("http://example.com", ExtractPathFromDiaURL("http://example.com"));
}

TEST(CEFURLParsing, EmptyAfterScheme_ReturnsEmpty)
{
	EXPECT_EQ("", ExtractPathFromDiaURL("dia://"));
}

TEST(CEFPathTraversal, DoubleDot_Rejected)
{
	EXPECT_TRUE(IsPathTraversal("../etc/passwd"));
	EXPECT_TRUE(IsPathTraversal("app/../../secret"));
	EXPECT_TRUE(IsPathTraversal(".."));
}

TEST(CEFPathTraversal, NormalPath_Accepted)
{
	EXPECT_FALSE(IsPathTraversal("app/index.html"));
	EXPECT_FALSE(IsPathTraversal("assets/logo.png"));
	EXPECT_FALSE(IsPathTraversal("styles/main.css"));
}

TEST(CEFPathTraversal, SingleDot_Accepted)
{
	EXPECT_FALSE(IsPathTraversal("./app/index.html"));
}
