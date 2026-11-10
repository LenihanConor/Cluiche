#include <gtest/gtest.h>

#include <Modules/AppIdentity.h>

using Cluiche::AppFlow::AppIdentityFromExePath;

TEST(AppIdentity, WindowsPathCluicheTest)
{
    auto id = AppIdentityFromExePath("C:\\x\\bin\\CluicheTest\\Debug\\x64\\CluicheTest.exe");
    EXPECT_EQ(id.name, "CluicheTest");
    EXPECT_EQ(id.lower, "cluichetest");
    EXPECT_EQ(id.observationAssetRelPath, "../../../../Assets/CluicheTest/cluichetest.diaobservation");
    EXPECT_EQ(id.diagameFileName, "cluichetest.diagame");
}

TEST(AppIdentity, PosixPathCoW)
{
    auto id = AppIdentityFromExePath("/a/b/CoW.exe");
    EXPECT_EQ(id.name, "CoW");
    EXPECT_EQ(id.lower, "cow");
    EXPECT_EQ(id.observationAssetRelPath, "../../../../Assets/CoW/cow.diaobservation");
    EXPECT_EQ(id.diagameFileName, "cow.diagame");
}

TEST(AppIdentity, NoExtension)
{
    auto id = AppIdentityFromExePath("C:\\bin\\CoW");
    EXPECT_EQ(id.name, "CoW");
    EXPECT_EQ(id.diagameFileName, "cow.diagame");
}

TEST(AppIdentity, BareName)
{
    EXPECT_EQ(AppIdentityFromExePath("CoW.exe").name, "CoW");
}

TEST(AppIdentity, NullFallsBackToCluicheTest)
{
    auto id = AppIdentityFromExePath(nullptr);
    EXPECT_EQ(id.name, "CluicheTest");
    EXPECT_EQ(id.diagameFileName, "cluichetest.diagame");
}

TEST(AppIdentity, EmptyFallsBackToCluicheTest)
{
    auto id = AppIdentityFromExePath("");
    EXPECT_EQ(id.name, "CluicheTest");
    EXPECT_EQ(id.observationAssetRelPath, "../../../../Assets/CluicheTest/cluichetest.diaobservation");
}
