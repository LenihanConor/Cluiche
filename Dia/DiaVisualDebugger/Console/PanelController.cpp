////////////////////////////////////////////////////////////////////////////////
// Filename: PanelController.cpp
// Description: Deferred panel request handling. See PanelController.h.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#include <DiaVisualDebugger/Console/PanelController.h>

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Console/CommandCompleter.h>
#include <DiaVisualDebugger/Console/CommandLineParser.h>
#include <DiaVisualDebugger/Console/ConsoleJson.h>
#include <DiaObservation/Log/DiaLog.h>

#include <cstring>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			namespace
			{
				void CopyText(char* dst, unsigned capacity, const char* src)
				{
					if (src == nullptr)
						src = "";
					strncpy_s(dst, capacity, src, capacity - 1);
				}
			}

			PanelController::PanelController()
				: mCompletePending(false)
				, mHistoryCount(0)
				, mPaletteOpenPending(false)
				, mPaletteSearchPending(false)
				, mPaletteMask(PaletteIndex::kTypeCommands | PaletteIndex::kTypeDomains)
				, mRejectedCount(0)
				, mPalette(nullptr)
			{
				mCompleteToken[0] = '\0';
				mPaletteQuery[0] = '\0';
				std::memset(mHistoryQueue, 0, sizeof(mHistoryQueue));
				std::memset(mRejectedSeq, 0, sizeof(mRejectedSeq));
			}

			PanelController::~PanelController()
			{
				delete mPalette;
			}

			void PanelController::RequestComplete(const char* token)
			{
				mCompletePending = true;
				CopyText(mCompleteToken, sizeof(mCompleteToken), token);
			}

			void PanelController::RequestHistory(bool previous)
			{
				if (mHistoryCount < kMaxHistory)
					mHistoryQueue[mHistoryCount++] = previous ? 1 : 0;
			}

			void PanelController::RequestPaletteOpen()
			{
				mPaletteOpenPending = true;
				// Opening always shows the full list; drop any older search so it cannot overwrite it.
				mPaletteSearchPending = false;
			}

			void PanelController::RequestPaletteSearch(const char* query, int mask)
			{
				mPaletteSearchPending = true;
				mPaletteMask = mask;
				CopyText(mPaletteQuery, sizeof(mPaletteQuery), query);
			}

			void PanelController::RequestRejectedLine(unsigned seq)
			{
				if (mRejectedCount < kMaxRejected)
					mRejectedSeq[mRejectedCount++] = seq;
			}

			void PanelController::AddToHistory(const char* line)
			{
				mHistory.Add(line);
			}

			void PanelController::Process(IPanelPusher& pusher, const DiaDebugDomainRegistry* domains)
			{
				// Rejected console lines (too long): report back as a result.
				for (unsigned i = 0; i < mRejectedCount; ++i)
				{
					char json[256];
					if (ConsoleJson::BuildConsoleResult(mRejectedSeq[i], 2, static_cast<unsigned>(ParseError::TooLong),
														"line too long", json, sizeof(json)))
						pusher.Push("consoleResult", json);
				}
				mRejectedCount = 0;

				// History navigation, in the order the keys were pressed.
				for (unsigned i = 0; i < mHistoryCount; ++i)
				{
					const bool previous = (mHistoryQueue[i] != 0);
					const char* entry = previous ? mHistory.GetPrevious() : mHistory.GetNext();
					if (entry == nullptr && previous)
						continue;	// empty history: leave the input alone

					// entry == nullptr on "next" means we stepped past the newest entry: restore the draft.
					char json[1024];
					if (ConsoleJson::BuildSetInput(entry != nullptr ? entry : "", entry == nullptr, json, sizeof(json)))
						pusher.Push("consoleSetInput", json);
				}
				mHistoryCount = 0;

				if (mCompletePending)
				{
					mCompletePending = false;
					Completion items[CommandCompleter::kDefaultMaxResults];
					const unsigned count = CommandCompleter::Complete(mCompleteToken, items,
																	  CommandCompleter::kDefaultMaxResults);
					char json[1024];
					if (ConsoleJson::BuildCompletions(items, count, json, sizeof(json)))
						pusher.Push("consoleCompletions", json);
				}

				if (mPaletteOpenPending)
				{
					mPaletteOpenPending = false;
					if (mPalette == nullptr)
						mPalette = new PaletteIndex();

					if (domains != nullptr)
						mPalette->Build(*domains);
					else
						DIA_LOG_WARNING("Debug", "PanelController: palette opened before the domain registry is available");

					PushPaletteResults(pusher, "", PaletteIndex::kTypeCommands | PaletteIndex::kTypeDomains);
				}

				if (mPaletteSearchPending)
				{
					mPaletteSearchPending = false;
					if (mPalette != nullptr)	// never opened: nothing to search
						PushPaletteResults(pusher, mPaletteQuery, mPaletteMask);
				}
			}

			void PanelController::Reset()
			{
				mCompletePending = false;
				mCompleteToken[0] = '\0';
				mHistoryCount = 0;
				mPaletteOpenPending = false;
				mPaletteSearchPending = false;
				mPaletteQuery[0] = '\0';
				mPaletteMask = PaletteIndex::kTypeCommands | PaletteIndex::kTypeDomains;
				mRejectedCount = 0;
				delete mPalette;
				mPalette = nullptr;
			}

			void PanelController::PushPaletteResults(IPanelPusher& pusher, const char* query, int mask)
			{
				// 50 items x ~600 bytes worst case; heap-free static buffer, Main PU only.
				static char json[32 * 1024];
				if (ConsoleJson::BuildPaletteResults(*mPalette, query, mask, json, sizeof(json)))
					pusher.Push("paletteResults", json);
				else
					DIA_LOG_WARNING("Debug", "PanelController: palette results did not fit the JSON buffer");
			}
		}
	}
}

#endif // DIA_DEBUG
