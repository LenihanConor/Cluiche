#include <gtest/gtest.h>

#include <DiaSaveGame/SaveRegistry.h>
#include <DiaSaveGame/ISaveable.h>
#include <DiaSaveGame/SaveContext.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::SaveGame;
using Dia::Core::StringCRC;

// Minimal ISaveable that does nothing — only identity matters for registry tests
struct NullSaveable : ISaveable
{
    void     Serialize   (SaveContext&) const override {}
    void     Deserialize (LoadContext&) override       {}
    uint32_t GetVersion  () const override { return 1; }
};

// ---------------------------------------------------------------------------
// RegisterThree_EnumerateInOrder
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_SaveRegistry, RegisterThree_EnumerateInOrder)
{
    NullSaveable a, b, c;
    SaveRegistry reg;

    reg.Register(StringCRC("A"), &a);
    reg.Register(StringCRC("B"), &b);
    reg.Register(StringCRC("C"), &c);

    ASSERT_EQ(3u, reg.GetParticipantCount());
    EXPECT_EQ(StringCRC("A"), reg.GetIdAt(0));
    EXPECT_EQ(StringCRC("B"), reg.GetIdAt(1));
    EXPECT_EQ(StringCRC("C"), reg.GetIdAt(2));

    EXPECT_EQ(&a, reg.GetParticipantAt(0));
    EXPECT_EQ(&b, reg.GetParticipantAt(1));
    EXPECT_EQ(&c, reg.GetParticipantAt(2));
}

// ---------------------------------------------------------------------------
// UnregisterMiddle_OrderPreserved
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_SaveRegistry, UnregisterMiddle_OrderPreserved)
{
    NullSaveable a, b, c;
    SaveRegistry reg;

    reg.Register(StringCRC("A"), &a);
    reg.Register(StringCRC("B"), &b);
    reg.Register(StringCRC("C"), &c);

    reg.Unregister(StringCRC("B"));

    ASSERT_EQ(2u, reg.GetParticipantCount());
    EXPECT_EQ(StringCRC("A"), reg.GetIdAt(0));
    EXPECT_EQ(StringCRC("C"), reg.GetIdAt(1));
}

// ---------------------------------------------------------------------------
// ReRegisterAtEnd
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_SaveRegistry, ReRegisterAtEnd)
{
    NullSaveable a, b;
    SaveRegistry reg;

    reg.Register(StringCRC("A"), &a);
    reg.Register(StringCRC("B"), &b);

    reg.Unregister(StringCRC("A"));
    reg.Register(StringCRC("A"), &a);   // A appended at the end

    ASSERT_EQ(2u, reg.GetParticipantCount());
    EXPECT_EQ(StringCRC("B"), reg.GetIdAt(0));
    EXPECT_EQ(StringCRC("A"), reg.GetIdAt(1));
}
