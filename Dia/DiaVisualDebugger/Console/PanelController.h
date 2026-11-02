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
#include <DiaCore/Json/external/json/json.h>
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

			// Runs a named DiaDebugServer-style query. argsJson is a compact JSON object. On success writes the
			// NUL-terminated JSON result to outJson (capacity bytes) and returns true. Returns false when the
			// query is unknown, failed, or the result does not fit. Implemented by the host (Main PU, same
			// thread as PanelController::Process); DiaVisualDebugger never depends on the query provider.
			class IPanelQueryExecutor
			{
			public:
				virtual ~IPanelQueryExecutor() {}
				virtual bool ExecuteQuery(const char* queryName, const char* argsJson, char* outJson, unsigned capacity) = 0;
			};

			class PanelController
			{
			public:
				static const unsigned kMaxHistory = 8;
				static const unsigned kMaxSchemaPrefixes = 8;
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

				// valueschema.get bridge (JS app.onValueSchemaGet(prefix) -> JS valueSchemaResult(json)).
				// Duplicate pending prefixes collapse; requests beyond kMaxSchemaPrefixes are dropped.
				void RequestValueSchemaGet(const char* prefix);

				// Not owned; null (the default) means schema requests are ignored. Cleared by nothing: the host sets it once.
				void SetQueryExecutor(IPanelQueryExecutor* executor);

				// Records an accepted console line in the history.
				void AddToHistory(const char* line);

				// Update-side API: carries out pending work in a fixed order (rejected lines, history,
				// completion, palette open, palette search). domains may be null.
				void Process(IPanelPusher& pusher, const DiaDebugDomainRegistry* domains);

				// Same, but the palette's domains come from the parsed domain state snapshot root
				// (see ParseDomainStateSnapshot) instead of the registry, so nothing Sim-owned is read.
				// domainStatesRoot is null until the first snapshot has arrived.
				void ProcessWithRoot(IPanelPusher& pusher, const Json::Value* domainStatesRoot);

				// Clears pending requests and drops the palette index.
				void Reset();

			private:
				void ProcessImpl(IPanelPusher& pusher, const DiaDebugDomainRegistry* domains, const Json::Value* root);
				void ProcessSchemaRequests(IPanelPusher& pusher);
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

				char mSchemaPrefixes[kMaxSchemaPrefixes][kMaxTextLength + 1];
				unsigned mSchemaPrefixCount;
				IPanelQueryExecutor* mQueryExecutor;

				ConsoleHistory mHistory;
				PaletteIndex* mPalette;
			};
		}
	}
}

#endif // DIA_DEBUG
