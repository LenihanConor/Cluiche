// ChannelNameAnnounce: POD truncation and the Main-side ChannelNameTable.
#include <gtest/gtest.h>
#include <DiaObservation/Profile/ChannelNameAnnounce.h>
#include <cstring>
#include <string>

using namespace Dia::Observation::Profile;

TEST(ChannelNameAnnounce, ShortNameIsCopiedAndTerminated)
{
	const ChannelNameAnnounce a = MakeChannelNameAnnounce(0x1234u, "DeferredStepSynthetic");
	EXPECT_EQ(0x1234u, a.crc);
	EXPECT_STREQ("DeferredStepSynthetic", a.name);
}

TEST(ChannelNameAnnounce, NameOf63CharsFitsExactly)
{
	const std::string n(63, 'a');
	const ChannelNameAnnounce a = MakeChannelNameAnnounce(1u, n.c_str());
	EXPECT_EQ(63u, std::strlen(a.name));
	EXPECT_EQ(n, std::string(a.name));
}

TEST(ChannelNameAnnounce, LongerNameIsTruncatedAndNulTerminated)
{
	const std::string n(200, 'x');
	const ChannelNameAnnounce a = MakeChannelNameAnnounce(1u, n.c_str());
	EXPECT_EQ('\0', a.name[ChannelNameAnnounce::kMaxName - 1]);
	EXPECT_EQ(63u, std::strlen(a.name));
	EXPECT_EQ(std::string(63, 'x'), std::string(a.name));
}

TEST(ChannelNameAnnounce, NullNameGivesEmptyName)
{
	const ChannelNameAnnounce a = MakeChannelNameAnnounce(1u, nullptr);
	EXPECT_EQ('\0', a.name[0]);
}

TEST(ChannelNameTable, ApplyThenFindAndLastWriteWins)
{
	ChannelNameTable t;
	EXPECT_EQ(nullptr, t.Find(7u));
	t.Apply(MakeChannelNameAnnounce(7u, "first"));
	t.Apply(MakeChannelNameAnnounce(7u, "second"));
	ASSERT_NE(nullptr, t.Find(7u));
	EXPECT_STREQ("second", t.Find(7u));
	EXPECT_EQ(1u, t.Count());
}

TEST(ChannelNameTable, EmptyNameIgnoredAndUnterminatedBufferIsBounded)
{
	ChannelNameTable t;
	t.Apply(MakeChannelNameAnnounce(1u, ""));
	EXPECT_EQ(0u, t.Count());

	ChannelNameAnnounce raw;
	std::memset(&raw, 'z', sizeof(raw));   // no terminator anywhere
	raw.crc = 9u;
	t.Apply(raw);
	ASSERT_NE(nullptr, t.Find(9u));
	EXPECT_EQ(static_cast<size_t>(ChannelNameAnnounce::kMaxName), std::strlen(t.Find(9u)));
}

TEST(ChannelNameTable, IsBounded)
{
	ChannelNameTable t;
	for (uint32_t i = 1; i <= ChannelNameTable::kMaxEntries + 10; ++i)
		t.Apply(MakeChannelNameAnnounce(i, "n"));
	EXPECT_EQ(ChannelNameTable::kMaxEntries, t.Count());
}
