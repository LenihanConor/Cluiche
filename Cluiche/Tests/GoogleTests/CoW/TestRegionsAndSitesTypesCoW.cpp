// CoW-specific built-in node kind ids and info (neutral coverage lives in Draoi/TestRegionsAndSitesTypes.cpp).
#include <gtest/gtest.h>
#include <Shared/CoWNodeKinds.h>
#include <DraoiHexWorld/Sites.h>

#include <set>

using namespace CoW::WorldGeneration;
using namespace Draoi::HexWorld;
using Dia::Core::StringCRC;

TEST(SiteKinds, BuiltinIdsAndInfo)
{
    EXPECT_STREQ(BuiltinNodeKinds::Corridor().AsChar(), "CoW.Node.Corridor");
    EXPECT_STREQ(BuiltinNodeKinds::River().AsChar(), "CoW.Node.River");
    EXPECT_STREQ(BuiltinNodeKinds::Hunting().AsChar(), "CoW.Node.Hunting");
    EXPECT_STREQ(BuiltinNodeKinds::Resource().AsChar(), "CoW.Node.Resource");

    const StringCRC kinds[4] = { BuiltinNodeKinds::Corridor(), BuiltinNodeKinds::River(),
                                 BuiltinNodeKinds::Hunting(), BuiltinNodeKinds::Resource() };
    const char* names[4] = { "Corridor Node", "River Node", "Hunting Ground", "Resource Deposit" };
    std::set<uint32_t> crcs, colours;
    for (int i = 0; i < 4; ++i)
    {
        const SiteKindInfo info = BuiltinNodeKindInfo(CoWNodeKindIds(), kinds[i]);
        EXPECT_EQ(info.kindCrc, SiteKindId(kinds[i]));
        EXPECT_STREQ(info.id, kinds[i].AsChar());
        EXPECT_STREQ(info.displayName, names[i]);
        EXPECT_EQ(info.debugColour & 0xFF000000u, 0u);
        crcs.insert(info.kindCrc);
        colours.insert(info.debugColour);
    }
    EXPECT_EQ(crcs.size(), 4u);
    EXPECT_EQ(colours.size(), 4u);
}

TEST(SiteKinds, UnknownKindInfoIsEmptyAndFlagged)
{
    const SiteKindInfo info = BuiltinNodeKindInfo(CoWNodeKindIds(), StringCRC("Dragon.Hearth"));
    EXPECT_EQ(info.kindCrc, SiteKindId(StringCRC("Dragon.Hearth")));
    EXPECT_EQ(info.displayName[0], '\0');
}
