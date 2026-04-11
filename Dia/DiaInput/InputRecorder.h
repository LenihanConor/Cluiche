////////////////////////////////////////////////////////////////////////////////
// Filename: InputRecorder.h - Record and playback input for testing/replays
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include "DiaInput/Event.h"
#include "DiaInput/EventData.h"
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
		/// recorder.RecordEvents(events, TimeAbsolute::Now());
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
		/// recorder.UpdatePlayback(TimeAbsolute::Now(), events);
		/// // Process events as normal
		/// @endcode
		////////////////////////////////////////////////////////////////////////////////
		class InputRecorder
		{
		public:
			/// @brief Frame of recorded input
			struct RecordedFrame
			{
				TimeAbsolute timestamp;
				EventDataT<64> events;

				RecordedFrame() : timestamp(TimeAbsolute::Zero()) {}
				RecordedFrame(const TimeAbsolute& time, const EventData& evts)
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
				, mPlaybackStartTime(TimeAbsolute::Zero())
				, mRecordingStartTime(TimeAbsolute::Zero())
				, mPlaybackFrameIndex(0)
			{}

			/// @brief Start recording input
			void StartRecording()
			{
				mRecordedFrames.RemoveAll();
				mIsRecording = true;
				mRecordingStartTime = TimeAbsolute::Now();
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
			void RecordEvents(const EventData& events, const TimeAbsolute& timestamp)
			{
				if (!mIsRecording || events.Size() == 0)
				{
					return;
				}

				// Store relative time from recording start
				TimeAbsolute relativeTime = timestamp - mRecordingStartTime;
				RecordedFrame frame(relativeTime, events);
				mRecordedFrames.Add(frame);
			}

			/// @brief Save recorded input to file
			///
			/// @param path File path to save to
			/// @return true if successful, false on error
			bool SaveToFile(const char* path) const
			{
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
					double timeSeconds = frame.timestamp.GetTimeInSeconds();
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
					TimeAbsolute timestamp(timeSeconds);

					// Read event count
					unsigned int eventCount = 0;
					fread(&eventCount, sizeof(unsigned int), 1, file);

					// Read events
					EventDataT<64> events;
					for (unsigned int j = 0; j < eventCount; j++)
					{
						Event event;
						fread(&event, sizeof(Event), 1, file);
						events.Add(event);
					}

					mRecordedFrames.Add(RecordedFrame(timestamp, events));
				}

				fclose(file);
				return true;
			}

			/// @brief Start playback of recorded input
			void StartPlayback()
			{
				mIsPlayingBack = true;
				mPlaybackStartTime = TimeAbsolute::Now();
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
			void UpdatePlayback(const TimeAbsolute& currentTime, EventData& outEvents)
			{
				if (!mIsPlayingBack || mPlaybackFrameIndex >= mRecordedFrames.Size())
				{
					return;
				}

				// Calculate playback time relative to start
				TimeAbsolute playbackTime = currentTime - mPlaybackStartTime;

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

				return mRecordedFrames[mRecordedFrames.Size() - 1].timestamp.GetTimeInSeconds();
			}

			/// @brief Get number of recorded frames
			unsigned int GetFrameCount() const
			{
				return mRecordedFrames.Size();
			}

		private:
			bool mIsRecording;
			bool mIsPlayingBack;
			TimeAbsolute mRecordingStartTime;
			TimeAbsolute mPlaybackStartTime;
			unsigned int mPlaybackFrameIndex;
			Core::Containers::DynamicArrayC<RecordedFrame, 1024> mRecordedFrames;
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
				: IInputSource(Priority::Normal)
				, mRecorder(recorder)
			{}

			virtual void Poll(EventData& outStream) override
			{
				if (mRecorder)
				{
					mRecorder->UpdatePlayback(TimeAbsolute::Now(), outStream);
				}
			}

		private:
			InputRecorder* mRecorder;
		};
	}
}
