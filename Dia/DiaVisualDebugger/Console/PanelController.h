////////////////////////////////////////////////////////////////////////////////
// Filename: PanelController.h
// Description: Deferred-request logic behind the debug panel's console history,
//              completion and palette. JS callbacks only RECORD requests
//              (Request*); Process() performs them later, from the module
//              update, and pushes results through an IPanelPusher. This keeps
//              CallJSFunction out of running JS callbacks (re-entering the JS
//              engine crashed the app). Pure logic, no UI or stream types.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
//               docs/specs/applications/dia/systems/diadebugdomain/panel-palette.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Console/ConsoleHistory.h>
#include <DiaVisualDebugger/Console/PaletteIndex.h>
#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			// Receives the JS function name and JSON payload. Returns false when the page cannot receive.
			class IPanelPusher
			{
			public:
				virtual ~IPanelPusher() {}
				virtual bool Push(const char* jsFunction, const char* json) = 0;
			};

			class PanelController
			{
			public:
				static const unsigned kMaxHistory = 8;
				static const unsigned kMaxRejected = 4;
				static const unsigned kMaxTextLength = 63;

				PanelController();
				~PanelController();
				PanelController(const PanelController&) = delete;
				PanelController& operator=(const PanelController&) = delete;

				// Callback-side API: only records a request. Never calls a pusher.
				void RequestComplete(const char* token);
				void RequestHistory(bool previous);
				void RequestPaletteOpen();
				void RequestPaletteSearch(const char* query, int mask);
				void RequestRejectedLine(unsigned seq);

				// Records an accepted console line in the history.
				void AddToHistory(const char* line);

				// Update-side API: carries out pending work in a fixed order (rejected lines, history,
				// completion, palette open, palette search). domains may be null.
				void Process(IPanelPusher& pusher, const DiaDebugDomainRegistry* domains);

				// Clears pending requests and drops the palette index.
				void Reset();

			private:
				void PushPaletteResults(IPanelPusher& pusher, const char* query, int mask);

				bool mCompletePending;
				char mCompleteToken[kMaxTextLength + 1];

				unsigned char mHistoryQueue[kMaxHistory];	// 1 = previous, 0 = next
				unsigned mHistoryCount;

				bool mPaletteOpenPending;
				bool mPaletteSearchPending;
				char mPaletteQuery[kMaxTextLength + 1];
				int mPaletteMask;

				unsigned mRejectedSeq[kMaxRejected];
				unsigned mRejectedCount;

				ConsoleHistory mHistory;
				PaletteIndex* mPalette;
			};
		}
	}
}

#endif // DIA_DEBUG
