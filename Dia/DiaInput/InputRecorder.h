////////////////////////////////////////////////////////////////////////////////
// Filename: InputRecorder.h - Record and playback input for testing/replays
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Time/TimeRelative.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include "DiaInput/Event.h"
#include "DiaInput/EventData.h"
#include "DiaInput/IInputSource.h"
#include <cstdio>

namespace Dia
{
	namespace Input
	{
		/// @brief Input recording and playback system
		///
		/// InputRecorder captures input events with timestamps for later playback.
		/// Useful for automated testing, bug reproduction, and replay systems.
		///
		/// **Recording Usage:**
		/// @code
		/// InputRecorder recorder;
		/// recorder.StartRecording();
		///
		/// // Each frame:
		/// EventData events;
		/// inputSourceManager.Update(events);
		/// recorder.RecordEvents(events, TimeAbsolute::GetSystemTime());
		///
		/// // Save recording
		/// recorder.SaveToFile("gameplay.input");
		/// @endcode
		///
		/// **Playback Usage:**
		/// @code
		/// InputRecorder recorder;
		/// recorder.LoadFromFile("gameplay.input");
		/// recorder.StartPlayback();
		///
		/// // Each frame:
		/// EventData events;
		/// recorder.UpdatePlayback(TimeAbsolute::GetSystemTime(), events);
		/// // Process events as normal
		/// @endcode
		////////////////////////////////////////////////////////////////////////////////
		class InputRecorder
		{
		public:
			/// @brief Frame of recorded input
			struct RecordedFrame
			{
				Dia::Core::TimeAbsolute timestamp;
				// Use smaller capacity (8 events) to avoid stack overflow when copying frames
				// Most frames have very few events (1-3 typically), so 8 is sufficient
				// If a frame needs more, EventDataT will assert in debug builds
				EventDataT<8> events;

				RecordedFrame() : timestamp(Dia::Core::TimeAbsolute::Zero()) {}
				RecordedFrame(const Dia::Core::TimeAbsolute& time, const EventData& evts)
					: timestamp(time)
				{
					for (unsigned int i = 0; i < evts.Size(); i++)
					{
						events.Add(evts[i]);
					}
				}
			};

			InputRecorder()
				: mIsRecording(false)
				, mIsPlayingBack(false)
				, mPlaybackStartTime(Dia::Core::TimeAbsolute::Zero())
				, mRecordingStartTime(Dia::Core::TimeAbsolute::Zero())
				, mPlaybackFrameIndex(0)
			{}

			/// @brief Start recording input
			void StartRecording()
			{
				// Stop playback if active (mutual exclusion)
				if (mIsPlayingBack)
				{
					StopPlayback();
				}

				mRecordedFrames.RemoveAll();
				mIsRecording = true;
				mRecordingStartTime = Dia::Core::TimeAbsolute::GetSystemTime();
			}

			/// @brief Stop recording input
			void StopRecording()
			{
				mIsRecording = false;
			}

			/// @brief Check if currently recording
			bool IsRecording() const { return mIsRecording; }

			/// @brief Record events for current frame
			///
			/// @param events EventData containing input events
			/// @param timestamp Current timestamp
			void RecordEvents(const EventData& events, const Dia::Core::TimeAbsolute& timestamp)
			{
				if (!mIsRecording || events.Size() == 0)
				{
					return;
				}

				// Store relative time from recording start
				Dia::Core::TimeRelative relativeTime = timestamp - mRecordingStartTime;
				RecordedFrame frame(Dia::Core::TimeAbsolute::Zero() + relativeTime, events);
				mRecordedFrames.Add(frame);
			}

			/// @brief Save recorded input to file
			///
			/// @param path File path to save to
			/// @return true if successful, false on error
			bool SaveToFile(const char* path) const
			{
				// Cannot save while recording
				if (mIsRecording)
				{
					return false;
				}

				FILE* file = nullptr;
				fopen_s(&file, path, "wb");
				if (!file)
				{
					return false;
				}

				// Write header: magic number and frame count
				const unsigned int kMagicNumber = 0x494E5055;  // "INPU"
				fwrite(&kMagicNumber, sizeof(unsigned int), 1, file);

				unsigned int frameCount = mRecordedFrames.Size();
				fwrite(&frameCount, sizeof(unsigned int), 1, file);

				// Write each frame
				for (unsigned int i = 0; i < frameCount; i++)
				{
					const RecordedFrame& frame = mRecordedFrames[i];

					// Write timestamp
					double timeSeconds = frame.timestamp.AsFloatInSeconds();
					fwrite(&timeSeconds, sizeof(double), 1, file);

					// Write event count
					unsigned int eventCount = frame.events.Size();
					fwrite(&eventCount, sizeof(unsigned int), 1, file);

					// Write events
					for (unsigned int j = 0; j < eventCount; j++)
					{
						fwrite(&frame.events[j], sizeof(Event), 1, file);
					}
				}

				fclose(file);
				return true;
			}

			/// @brief Load recorded input from file
			///
			/// @param path File path to load from
			/// @return true if successful, false on error
			bool LoadFromFile(const char* path)
			{
				FILE* file = nullptr;
				fopen_s(&file, path, "rb");
				if (!file)
				{
					return false;
				}

				// Read header
				unsigned int magicNumber = 0;
				fread(&magicNumber, sizeof(unsigned int), 1, file);

				const unsigned int kExpectedMagic = 0x494E5055;  // "INPU"
				if (magicNumber != kExpectedMagic)
				{
					fclose(file);
					return false;
				}

				unsigned int frameCount = 0;
				fread(&frameCount, sizeof(unsigned int), 1, file);

				// Read frames
				mRecordedFrames.RemoveAll();
				for (unsigned int i = 0; i < frameCount; i++)
				{
					// Read timestamp
					double timeSeconds = 0.0;
					fread(&timeSeconds, sizeof(double), 1, file);
					Dia::Core::TimeAbsolute timestamp = Dia::Core::TimeAbsolute::CreateFromSeconds(static_cast<float>(timeSeconds));

					// Read event count
					unsigned int eventCount = 0;
					fread(&eventCount, sizeof(unsigned int), 1, file);

					// Create frame and read events directly into it
					// Note: We use AddDefault() to avoid stack overflow from creating
					// large EventDataT<64> temporaries (16KB each!)
					mRecordedFrames.AddDefault();
					RecordedFrame& frame = mRecordedFrames[mRecordedFrames.Size() - 1];
					frame.timestamp = timestamp;

					for (unsigned int j = 0; j < eventCount; j++)
					{
						Event event;
						fread(&event, sizeof(Event), 1, file);
						frame.events.Add(event);
					}
				}

				fclose(file);
				return true;
			}

			/// @brief Start playback of recorded input
			void StartPlayback()
			{
				// Stop recording if active (mutual exclusion)
				if (mIsRecording)
				{
					StopRecording();
				}

				mIsPlayingBack = true;
				mPlaybackStartTime = Dia::Core::TimeAbsolute::GetSystemTime();
				mPlaybackFrameIndex = 0;
			}

			/// @brief Stop playback
			void StopPlayback()
			{
				mIsPlayingBack = false;
				mPlaybackFrameIndex = 0;
			}

			/// @brief Check if currently playing back
			bool IsPlayingBack() const { return mIsPlayingBack; }

			/// @brief Update playback and emit events for current time
			///
			/// @param currentTime Current timestamp
			/// @param outEvents Event buffer to populate with playback events
			///
			/// Call this each frame during playback. Events matching the current
			/// playback time will be added to outEvents.
			void UpdatePlayback(const Dia::Core::TimeAbsolute& currentTime, EventData& outEvents)
			{
				if (!mIsPlayingBack || mPlaybackFrameIndex >= mRecordedFrames.Size())
				{
					return;
				}

				// Calculate playback time relative to start
				Dia::Core::TimeRelative playbackTimeRel = currentTime - mPlaybackStartTime;
				Dia::Core::TimeAbsolute playbackTime = Dia::Core::TimeAbsolute::Zero() + playbackTimeRel;

				// Emit all events up to current playback time
				while (mPlaybackFrameIndex < mRecordedFrames.Size())
				{
					const RecordedFrame& frame = mRecordedFrames[mPlaybackFrameIndex];

					if (frame.timestamp <= playbackTime)
					{
						// Emit this frame's events
						for (unsigned int i = 0; i < frame.events.Size(); i++)
						{
							outEvents.Add(frame.events[i]);
						}
						mPlaybackFrameIndex++;
					}
					else
					{
						// This frame is in the future, stop for now
						break;
					}
				}

				// Stop playback if we've reached the end
				if (mPlaybackFrameIndex >= mRecordedFrames.Size())
				{
					mIsPlayingBack = false;
				}
			}

			/// @brief Get total duration of recording
			///
			/// @return Duration in seconds, or 0.0 if no recording
			double GetRecordingDuration() const
			{
				if (mRecordedFrames.Size() == 0)
				{
					return 0.0;
				}

				return mRecordedFrames[mRecordedFrames.Size() - 1].timestamp.AsFloatInSeconds();
			}

			/// @brief Get number of recorded frames
			unsigned int GetFrameCount() const
			{
				return mRecordedFrames.Size();
			}

		private:
			bool mIsRecording;
			bool mIsPlayingBack;
			Dia::Core::TimeAbsolute mRecordingStartTime;
			Dia::Core::TimeAbsolute mPlaybackStartTime;
			unsigned int mPlaybackFrameIndex;
			Dia::Core::Containers::DynamicArrayC<RecordedFrame, 1024> mRecordedFrames;
		};

		/// @brief IInputSource implementation for playback
		///
		/// PlaybackInputSource wraps InputRecorder as an IInputSource,
		/// allowing recorded input to be treated like any other input source.
		///
		/// **Usage:**
		/// @code
		/// InputRecorder recorder;
		/// recorder.LoadFromFile("gameplay.input");
		/// recorder.StartPlayback();
		///
		/// PlaybackInputSource playbackSource(&recorder);
		/// inputSourceManager.AddInputSource(&playbackSource);
		///
		/// // Each frame: playback is polled automatically
		/// @endcode
		////////////////////////////////////////////////////////////////////////////////
		class PlaybackInputSource : public IInputSource
		{
		public:
			PlaybackInputSource(InputRecorder* recorder)
				: IInputSource(IInputSource::Priority::Normal)
				, mRecorder(recorder)
			{}

			virtual void Poll(EventData& outStream) override
			{
				if (mRecorder)
				{
					mRecorder->UpdatePlayback(Dia::Core::TimeAbsolute::GetSystemTime(), outStream);
				}
			}

		private:
			InputRecorder* mRecorder;
		};
	}
}
