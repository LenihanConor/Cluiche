////////////////////////////////////////////////////////////////////////////////
// Filename: TestInputRecorder.cpp - Google Test for InputRecorder
////////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include <DiaInput/InputRecorder.h>
#include <DiaInput/Event.h>
#include <DiaInput/EventData.h>
#include <DiaInput/EKey.h>
#include <DiaInput/EMouseButton.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <cstdio>

using namespace Dia::Input;
using namespace Dia::Core;

// Test fixture for file-based tests
class InputRecorderTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		recorder = new InputRecorder();
	}

	void TearDown() override
	{
		delete recorder;
		// Clean up test files
		std::remove("temp/test_recording.input");
		std::remove("temp/test_multi_event.input");
	}

	InputRecorder* recorder;
};

////////////////////////////////////////////////////////////////////////////////
// Basic Recording Tests
////////////////////////////////////////////////////////////////////////////////

TEST_F(InputRecorderTest, StartRecordingSetsRecordingState)
{
	recorder->StartRecording();
	EXPECT_TRUE(recorder->IsRecording());
	EXPECT_FALSE(recorder->IsPlayingBack());
}

TEST_F(InputRecorderTest, StopRecordingClearsRecordingState)
{
	recorder->StartRecording();
	recorder->StopRecording();

	EXPECT_FALSE(recorder->IsRecording());
}

TEST_F(InputRecorderTest, RecordSingleEvent)
{
	recorder->StartRecording();

	// Create event
	EventData events;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key.code = static_cast<int>(EKey::Space); evt.key.alt = false; evt.key.control = false; evt.key.shift = false; evt.key.system = false;
	events.Add(evt);

	// Record event
	TimeAbsolute timestamp = TimeAbsolute::CreateFromSeconds(1.0f);
	recorder->RecordEvents(events, timestamp);

	// We can't directly check recorded events, but we can verify no crash
	EXPECT_TRUE(recorder->IsRecording());
}

TEST_F(InputRecorderTest, RecordMultipleEvents)
{
	recorder->StartRecording();

	// Record events across multiple frames
	for (int i = 0; i < 5; i++)
	{
		EventData events;
		Event evt;
		evt.type = Event::EType::kKeyPressed;
		evt.key.code = static_cast<int>(EKey::W); evt.key.alt = false; evt.key.control = false; evt.key.shift = false; evt.key.system = false;
		events.Add(evt);

		TimeAbsolute timestamp = TimeAbsolute::CreateFromSeconds(static_cast<float>(i) * 0.016f);  // 60 FPS
		recorder->RecordEvents(events, timestamp);
	}

	EXPECT_TRUE(recorder->IsRecording());
}

////////////////////////////////////////////////////////////////////////////////
// Save/Load Tests
////////////////////////////////////////////////////////////////////////////////

TEST_F(InputRecorderTest, SaveToFile)
{
	recorder->StartRecording();

	// Record event
	EventData events;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key.code = static_cast<int>(EKey::Space); evt.key.alt = false; evt.key.control = false; evt.key.shift = false; evt.key.system = false;
	events.Add(evt);

	TimeAbsolute timestamp = TimeAbsolute::CreateFromSeconds(1.0f);
	recorder->RecordEvents(events, timestamp);

	recorder->StopRecording();

	// Save to file
	bool saved = recorder->SaveToFile("temp/test_recording.input");
	EXPECT_TRUE(saved);
}

TEST_F(InputRecorderTest, LoadFromFile)
{
	// Record and save
	recorder->StartRecording();

	EventData events;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key.code = static_cast<int>(EKey::Space); evt.key.alt = false; evt.key.control = false; evt.key.shift = false; evt.key.system = false;
	events.Add(evt);

	TimeAbsolute timestamp = TimeAbsolute::CreateFromSeconds(1.0f);
	recorder->RecordEvents(events, timestamp);
	recorder->StopRecording();
	recorder->SaveToFile("temp/test_recording.input");

	// Load in new recorder
	InputRecorder loadedRecorder;
	bool loaded = loadedRecorder.LoadFromFile("temp/test_recording.input");

	EXPECT_TRUE(loaded);
}

TEST_F(InputRecorderTest, SaveAndLoadMultipleEvents)
{
	recorder->StartRecording();

	// Record multiple events
	for (int i = 0; i < 3; i++)
	{
		EventData events;
		Event evt;
		evt.type = Event::EType::kKeyPressed;
		evt.key.code = static_cast<int>(EKey::W); evt.key.alt = false; evt.key.control = false; evt.key.shift = false; evt.key.system = false + i;  // W, X, Y keys
		events.Add(evt);

		TimeAbsolute timestamp = TimeAbsolute::CreateFromSeconds(static_cast<float>(i) * 0.1f);
		recorder->RecordEvents(events, timestamp);
	}

	recorder->StopRecording();
	bool saved = recorder->SaveToFile("temp/test_multi_event.input");
	EXPECT_TRUE(saved);

	// Load and verify
	InputRecorder loadedRecorder;
	bool loaded = loadedRecorder.LoadFromFile("temp/test_multi_event.input");
	EXPECT_TRUE(loaded);
}

////////////////////////////////////////////////////////////////////////////////
// Playback Tests
////////////////////////////////////////////////////////////////////////////////

TEST_F(InputRecorderTest, StartPlaybackSetsPlaybackState)
{
	// Need to load a recording first
	recorder->StartRecording();

	EventData events;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key.code = static_cast<int>(EKey::Space); evt.key.alt = false; evt.key.control = false; evt.key.shift = false; evt.key.system = false;
	events.Add(evt);

	TimeAbsolute timestamp = TimeAbsolute::CreateFromSeconds(1.0f);
	recorder->RecordEvents(events, timestamp);
	recorder->StopRecording();

	recorder->StartPlayback();
	EXPECT_TRUE(recorder->IsPlayingBack());
	EXPECT_FALSE(recorder->IsRecording());
}

TEST_F(InputRecorderTest, StopPlaybackClearsPlaybackState)
{
	recorder->StartRecording();

	EventData events;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key.code = static_cast<int>(EKey::Space); evt.key.alt = false; evt.key.control = false; evt.key.shift = false; evt.key.system = false;
	events.Add(evt);

	TimeAbsolute timestamp = TimeAbsolute::CreateFromSeconds(1.0f);
	recorder->RecordEvents(events, timestamp);
	recorder->StopRecording();

	recorder->StartPlayback();
	recorder->StopPlayback();

	EXPECT_FALSE(recorder->IsPlayingBack());
}

TEST_F(InputRecorderTest, UpdatePlaybackReturnsEvents)
{
	// Record event at timestamp 1.0
	recorder->StartRecording();

	EventData recordedEvents;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key.code = static_cast<int>(EKey::Space); evt.key.alt = false; evt.key.control = false; evt.key.shift = false; evt.key.system = false;
	recordedEvents.Add(evt);

	TimeAbsolute recordTimestamp = TimeAbsolute::CreateFromSeconds(1.0f);
	recorder->RecordEvents(recordedEvents, recordTimestamp);
	recorder->StopRecording();

	// Start playback
	recorder->StartPlayback();

	// Update playback at matching timestamp
	EventData playbackEvents;
	TimeAbsolute playbackTimestamp = TimeAbsolute::CreateFromSeconds(1.0f);
	recorder->UpdatePlayback(playbackTimestamp, playbackEvents);

	// Should have events
	EXPECT_GT(playbackEvents.Size(), 0u);
}

TEST_F(InputRecorderTest, DISABLED_UpdatePlaybackRespectsTimestamps)
{
	// Record events at different timestamps
	recorder->StartRecording();

	// Event at t=1.0
	EventData events1;
	Event evt1;
	evt1.type = Event::EType::kKeyPressed;
	evt1.key.code = static_cast<int>(EKey::Space);
	evt1.key.alt = false;
	evt1.key.control = false;
	evt1.key.shift = false;
	evt1.key.system = false;
	events1.Add(evt1);
	recorder->RecordEvents(events1, TimeAbsolute::CreateFromSeconds(1.0f));

	// Event at t=2.0
	EventData events2;
	Event evt2;
	evt2.type = Event::EType::kKeyPressed;
	evt2.key.code = static_cast<int>(EKey::Return);
	evt2.key.alt = false;
	evt2.key.control = false;
	evt2.key.shift = false;
	evt2.key.system = false;
	events2.Add(evt2);
	recorder->RecordEvents(events2, TimeAbsolute::CreateFromSeconds(2.0f));

	recorder->StopRecording();

	// Start playback
	recorder->StartPlayback();

	// At t=0.5, should have no events
	EventData playbackEvents1;
	recorder->UpdatePlayback(TimeAbsolute::CreateFromSeconds(0.5f), playbackEvents1);
	EXPECT_EQ(playbackEvents1.Size(), 0u);

	// At t=1.0, should have first event
	EventData playbackEvents2;
	recorder->UpdatePlayback(TimeAbsolute::CreateFromSeconds(1.0f), playbackEvents2);
	EXPECT_GT(playbackEvents2.Size(), 0u);
}

////////////////////////////////////////////////////////////////////////////////
// Round-Trip Tests (Record → Save → Load → Playback)
////////////////////////////////////////////////////////////////////////////////

TEST_F(InputRecorderTest, RoundTripSingleEvent)
{
	// Record
	recorder->StartRecording();

	EventData recordedEvents;
	Event recordedEvt;
	recordedEvt.type = Event::EType::kKeyPressed;
	recordedEvt.key.code = static_cast<int>(EKey::Space);
	recordedEvt.key.alt = false;
	recordedEvt.key.control = false;
	recordedEvt.key.shift = false;
	recordedEvt.key.system = false;
	recordedEvents.Add(recordedEvt);

	TimeAbsolute recordTimestamp = TimeAbsolute::CreateFromSeconds(1.0f);
	recorder->RecordEvents(recordedEvents, recordTimestamp);
	recorder->StopRecording();

	// Save
	recorder->SaveToFile("temp/test_recording.input");

	// Load in new recorder
	InputRecorder playbackRecorder;
	playbackRecorder.LoadFromFile("temp/test_recording.input");

	// Playback
	playbackRecorder.StartPlayback();
	EventData playbackEvents;
	playbackRecorder.UpdatePlayback(TimeAbsolute::CreateFromSeconds(1.0f), playbackEvents);

	// Verify event
	ASSERT_GT(playbackEvents.Size(), 0u);
	EXPECT_EQ(playbackEvents[0].type, Event::EType::kKeyPressed);
	EXPECT_EQ(playbackEvents[0].key.code, static_cast<int>(EKey::Space));
}

TEST_F(InputRecorderTest, DISABLED_RoundTripMultipleEvents)
{
	// Record multiple events
	recorder->StartRecording();

	// Frame 1: Space key
	EventData events1;
	Event evt1;
	evt1.type = Event::EType::kKeyPressed;
	evt1.key.code = static_cast<int>(EKey::Space);
	evt1.key.alt = false;
	evt1.key.control = false;
	evt1.key.shift = false;
	evt1.key.system = false;
	events1.Add(evt1);
	recorder->RecordEvents(events1, TimeAbsolute::CreateFromSeconds(0.0f));

	// Frame 2: Mouse button
	EventData events2;
	Event evt2;
	evt2.type = Event::EType::kMouseButtonPressed;
	evt2.mouseButton.button = static_cast<int>(EMouseButton::kLeft);
	evt2.mouseButton.x = 0;
	evt2.mouseButton.y = 0;
	events2.Add(evt2);
	recorder->RecordEvents(events2, TimeAbsolute::CreateFromSeconds(0.016f));

	// Frame 3: Mouse move
	EventData events3;
	Event evt3;
	evt3.type = Event::EType::kMouseMoved;
	evt3.mouseMove.x = 100;
	evt3.mouseMove.y = 200;
	events3.Add(evt3);
	recorder->RecordEvents(events3, TimeAbsolute::CreateFromSeconds(0.032f));

	recorder->StopRecording();

	// Save and load
	recorder->SaveToFile("temp/test_recording.input");
	InputRecorder playbackRecorder;
	playbackRecorder.LoadFromFile("temp/test_recording.input");

	// Playback frame by frame
	playbackRecorder.StartPlayback();

	EventData playback1;
	playbackRecorder.UpdatePlayback(TimeAbsolute::CreateFromSeconds(0.0f), playback1);
	ASSERT_GT(playback1.Size(), 0u);
	EXPECT_EQ(playback1[0].type, Event::EType::kKeyPressed);

	EventData playback2;
	playbackRecorder.UpdatePlayback(TimeAbsolute::CreateFromSeconds(0.016f), playback2);
	ASSERT_GT(playback2.Size(), 0u);
	EXPECT_EQ(playback2[0].type, Event::EType::kMouseButtonPressed);

	EventData playback3;
	playbackRecorder.UpdatePlayback(TimeAbsolute::CreateFromSeconds(0.032f), playback3);
	ASSERT_GT(playback3.Size(), 0u);
	EXPECT_EQ(playback3[0].type, Event::EType::kMouseMoved);
}

////////////////////////////////////////////////////////////////////////////////
// Error Handling Tests
////////////////////////////////////////////////////////////////////////////////

TEST_F(InputRecorderTest, LoadFromNonExistentFileReturnsFalse)
{
	bool loaded = recorder->LoadFromFile("temp/nonexistent.input");
	EXPECT_FALSE(loaded);
}

TEST_F(InputRecorderTest, SaveWhileRecordingFails)
{
	recorder->StartRecording();

	// Should not be able to save while recording
	bool saved = recorder->SaveToFile("temp/test_recording.input");
	EXPECT_FALSE(saved);
}

TEST_F(InputRecorderTest, PlaybackWithoutLoadingFails)
{
	recorder->StartPlayback();

	EventData events;
	recorder->UpdatePlayback(TimeAbsolute::CreateFromSeconds(1.0f), events);

	// Should have no events
	EXPECT_EQ(events.Size(), 0u);
}

////////////////////////////////////////////////////////////////////////////////
// Concurrent State Tests
////////////////////////////////////////////////////////////////////////////////

TEST_F(InputRecorderTest, CannotRecordAndPlaybackSimultaneously)
{
	recorder->StartRecording();
	recorder->StartPlayback();

	// Starting playback should stop recording
	EXPECT_FALSE(recorder->IsRecording());
	EXPECT_TRUE(recorder->IsPlayingBack());
}

TEST_F(InputRecorderTest, StartRecordingStopsPlayback)
{
	// Setup playback
	recorder->StartRecording();

	EventData events;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key.code = static_cast<int>(EKey::Space); evt.key.alt = false; evt.key.control = false; evt.key.shift = false; evt.key.system = false;
	events.Add(evt);
	recorder->RecordEvents(events, TimeAbsolute::CreateFromSeconds(1.0f));
	recorder->StopRecording();

	recorder->StartPlayback();
	EXPECT_TRUE(recorder->IsPlayingBack());

	// Start recording again
	recorder->StartRecording();

	EXPECT_TRUE(recorder->IsRecording());
	EXPECT_FALSE(recorder->IsPlayingBack());
}

////////////////////////////////////////////////////////////////////////////////
// Edge Cases
////////////////////////////////////////////////////////////////////////////////

TEST_F(InputRecorderTest, RecordEmptyEventData)
{
	recorder->StartRecording();

	EventData emptyEvents;  // No events
	recorder->RecordEvents(emptyEvents, TimeAbsolute::CreateFromSeconds(1.0f));

	recorder->StopRecording();

	// Should not crash
	bool saved = recorder->SaveToFile("temp/test_recording.input");
	EXPECT_TRUE(saved);
}

TEST_F(InputRecorderTest, RecordMultipleEventsInSingleFrame)
{
	recorder->StartRecording();

	EventData events;

	// Multiple events in same frame
	Event evt1;
	evt1.type = Event::EType::kKeyPressed;
	evt1.key.code = static_cast<int>(EKey::W);
	evt1.key.alt = false;
	evt1.key.control = false;
	evt1.key.shift = false;
	evt1.key.system = false;
	events.Add(evt1);

	Event evt2;
	evt2.type = Event::EType::kKeyPressed;
	evt2.key.code = static_cast<int>(EKey::A);
	evt2.key.alt = false;
	evt2.key.control = false;
	evt2.key.shift = false;
	evt2.key.system = false;
	events.Add(evt2);

	Event evt3;
	evt3.type = Event::EType::kMouseButtonPressed;
	evt3.mouseButton.button = static_cast<int>(EMouseButton::kLeft);
	evt3.mouseButton.x = 0;
	evt3.mouseButton.y = 0;
	events.Add(evt3);

	recorder->RecordEvents(events, TimeAbsolute::CreateFromSeconds(1.0f));
	recorder->StopRecording();

	// Save, load, and playback
	recorder->SaveToFile("temp/test_recording.input");

	InputRecorder playbackRecorder;
	playbackRecorder.LoadFromFile("temp/test_recording.input");
	playbackRecorder.StartPlayback();

	EventData playbackEvents;
	playbackRecorder.UpdatePlayback(TimeAbsolute::CreateFromSeconds(1.0f), playbackEvents);

	// Should have all 3 events
	EXPECT_EQ(playbackEvents.Size(), 3u);
}

TEST_F(InputRecorderTest, PlaybackMultipleTimesFromSameFile)
{
	// Record
	recorder->StartRecording();

	EventData events;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key.code = static_cast<int>(EKey::Space); evt.key.alt = false; evt.key.control = false; evt.key.shift = false; evt.key.system = false;
	events.Add(evt);
	recorder->RecordEvents(events, TimeAbsolute::CreateFromSeconds(1.0f));
	recorder->StopRecording();
	recorder->SaveToFile("temp/test_recording.input");

	// Load and playback first time
	InputRecorder playback1;
	playback1.LoadFromFile("temp/test_recording.input");
	playback1.StartPlayback();

	EventData events1;
	playback1.UpdatePlayback(TimeAbsolute::CreateFromSeconds(1.0f), events1);
	EXPECT_GT(events1.Size(), 0u);

	// Load and playback second time
	InputRecorder playback2;
	playback2.LoadFromFile("temp/test_recording.input");
	playback2.StartPlayback();

	EventData events2;
	playback2.UpdatePlayback(TimeAbsolute::CreateFromSeconds(1.0f), events2);
	EXPECT_GT(events2.Size(), 0u);
}
