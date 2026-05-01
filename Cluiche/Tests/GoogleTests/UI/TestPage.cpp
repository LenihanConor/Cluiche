#include <gtest/gtest.h>
#include <DiaUI/Page.h>

using namespace Dia::UI;

static void DummyCallback(const BoundMethodArgs&) {}

// ==============================================================================
// Page Tests
// ==============================================================================

TEST(Page, DefaultConstruct_BoundMethodListEmpty)
{
    Page page;
    EXPECT_EQ(page.GetBoundMenthods().Size(), 0u);
}

TEST(Page, BindMethod_AddsToList)
{
    Page page;
    BoundMethod::MethodPtr ptr = &DummyCallback;
    BoundMethod method = BoundMethod::CreateBoundMethod("onLoad", ptr);
    page.BindMethod(method);
    EXPECT_EQ(page.GetBoundMenthods().Size(), 1u);
}

TEST(Page, BindMethod_MultipleAdds_SizeCorrect)
{
    Page page;
    BoundMethod::MethodPtr ptr = &DummyCallback;
    page.BindMethod(BoundMethod::CreateBoundMethod("a", ptr));
    page.BindMethod(BoundMethod::CreateBoundMethod("b", ptr));
    page.BindMethod(BoundMethod::CreateBoundMethod("c", ptr));
    EXPECT_EQ(page.GetBoundMenthods().Size(), 3u);
}

TEST(Page, BindMethod_NameIsPreserved)
{
    Page page;
    BoundMethod::MethodPtr ptr = &DummyCallback;
    page.BindMethod(BoundMethod::CreateBoundMethod("myEvent", ptr));
    EXPECT_STREQ(page.GetBoundMenthods()[0].GetName().AsCStr(), "myEvent");
}
