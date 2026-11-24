// DeferredStepModule announces registered channel names on "$perfnames" (crc + name), with resend for late readers.
#include "DeferredStepModuleHarness.h"
#include <DiaObservation/Profile/ChannelNameAnnounce.h>

using Dia::Observation::Profile::ChannelNameAnnounce;

namespace
{
	typedef Dia::Core::Containers::DynamicArrayC<Event<ChannelNameAnnounce>, 32> NameBuf;

	// Drains the reader into a vector of announced (crc, name).
	std::vector<std::pair<uint32_t, std::string>> DrainNames(EventStreamStore<ChannelNameAnnounce>& store, int reader)
	{
		std::vector<std::pair<uint32_t, std::string>> out;
		auto buf = std::make_unique<NameBuf>();
		store.Consume(reader, *buf);
		for (unsigned i = 0; i < buf->Size(); ++i)
			out.emplace_back((*buf)[i].payload.crc, std::string((*buf)[i].payload.name));
		return out;
	}
}

TEST(DeferredStepNameAnnounce, RegisteredChannelIsAnnouncedExactlyOncePerStart)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	auto* store = h.mApp->GetPerfNamesStore();
	ASSERT_NE(nullptr, store);
	const int reader = store->RegisterReader();
	ASSERT_GE(reader, 0);
	DrainNames(*store, reader);

	const StringCRC channel("DSNA_Channel");
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(g_probe->GetScheduler().Register(channel, step));
	for (int i = 0; i < 5; ++i) g_probe->CallUpdate();

	auto names = DrainNames(*store, reader);
	ASSERT_EQ(1u, names.size());   // once, not once per update
	EXPECT_EQ(channel.Value(), names[0].first);
	EXPECT_EQ("DSNA_Channel", names[0].second);

	// A restart announces afresh.
	g_probe->GetScheduler().Unregister(channel);
	g_probe->CallStop();
	ASSERT_EQ(Dia::ApplicationFlow::StartResult::kReady, g_probe->CallStart());
	Dia::DeferredStep::Testing::RecordingStep step2;
	ASSERT_TRUE(g_probe->GetScheduler().Register(channel, step2));
	g_probe->CallUpdate();
	g_probe->CallUpdate();
	names = DrainNames(*store, reader);
	ASSERT_EQ(1u, names.size());
	EXPECT_EQ("DSNA_Channel", names[0].second);
	g_probe->GetScheduler().Unregister(channel);
}

TEST(DeferredStepNameAnnounce, ANewChannelAnnouncesTheWholeSetAndZeroIntervalResends)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	auto* store = h.mApp->GetPerfNamesStore();
	const int reader = store->RegisterReader();
	ASSERT_GE(reader, 0);
	DrainNames(*store, reader);

	Dia::DeferredStep::Testing::RecordingStep a, b;
	const StringCRC ca("DSNA_A"), cb("DSNA_B");
	ASSERT_TRUE(g_probe->GetScheduler().Register(ca, a));
	g_probe->CallUpdate();
	EXPECT_EQ(1u, DrainNames(*store, reader).size());
	ASSERT_TRUE(g_probe->GetScheduler().Register(cb, b));
	g_probe->CallUpdate();
	EXPECT_EQ(2u, DrainNames(*store, reader).size());   // set changed: both resent

	// A late reader learns names from the periodic resend (interval 0 = every update).
	g_probe->SetChannelNameResendIntervalMs(0);
	const int late = store->RegisterReader();
	ASSERT_GE(late, 0);
	g_probe->CallUpdate();
	EXPECT_EQ(2u, DrainNames(*store, late).size());
	g_probe->GetScheduler().Unregister(ca);
	g_probe->GetScheduler().Unregister(cb);
}

TEST(DeferredStepNameAnnounce, LongChannelNameIsTruncatedTo63)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	auto* store = h.mApp->GetPerfNamesStore();
	const int reader = store->RegisterReader();
	ASSERT_GE(reader, 0);
	DrainNames(*store, reader);

	const std::string longName(100, 'q');
	const StringCRC channel(longName.c_str());
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(g_probe->GetScheduler().Register(channel, step));
	g_probe->CallUpdate();
	auto names = DrainNames(*store, reader);
	ASSERT_EQ(1u, names.size());
	EXPECT_EQ(63u, names[0].second.size());
	g_probe->GetScheduler().Unregister(channel);
}

TEST(DeferredStepNameAnnounce, NoSinkMeansNothingIsSentAndTheModuleStillWorks)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->SetChannelNameSink(nullptr);
	const StringCRC channel("DSNA_NoSink");
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(g_probe->GetScheduler().Register(channel, step));
	for (int i = 0; i < 3; ++i) g_probe->CallUpdate();
	ASSERT_EQ(Dia::DeferredStep::KickResult::kLaunched, g_probe->GetScheduler().Kick(channel, 1, 2));
	g_probe->GetScheduler().Tick(2);
	EXPECT_EQ(1u, step.GetApplyCount());
	g_probe->GetScheduler().Unregister(channel);
}
