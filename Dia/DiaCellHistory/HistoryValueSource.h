#pragma once

// HistoryValueSource: the IViewValueSource that makes the cell-layer viewer show a recorded sample. Reads the
// recorder's dequantised per-cell ring values for HistoryViewState::Sample(). Read-only over the recorder and view
// state (both borrowed; they and the registry must outlive this object). Same-PU contract as the recorder. No singletons.
//
// GetValues is pure: false for a layer that is not tracked, for evicted data (viewed sample before the per-cell window
// start), when n != recorder.CellCount(), and while live (the uploader uses the layer's own read when live, so GetValues
// is not meaningful then). False is "no data", never a fallback to live values.
// IsLive() follows the view state; ViewEpoch() is the view state's epoch.
// Tick() must be called once per frame by the owner (the module): it calls HistoryViewState::Update() so the epoch
// bumps when the per-cell window passes the viewed sample.
// Install() puts this on the registry (false when the registry rejects it, e.g. another source is installed; the
// registry logs the Error). Uninstall() and the destructor remove it only if it is the installed source.

#include <DiaCellLayers/IViewValueSource.h>

#include <cstdint>

namespace Dia::CellLayers { class CellLayerRegistry; }

namespace Dia::CellHistory
{
	class CellHistoryRecorder;
	class HistoryViewState;

	class HistoryValueSource : public Dia::CellLayers::IViewValueSource
	{
	public:
		HistoryValueSource(Dia::CellLayers::CellLayerRegistry& registry, const CellHistoryRecorder& recorder, HistoryViewState& view);
		~HistoryValueSource() override;
		HistoryValueSource(const HistoryValueSource&) = delete;
		HistoryValueSource& operator=(const HistoryValueSource&) = delete;

		bool	Install();
		void	Uninstall();
		bool	IsInstalled() const { return mInstalled; }

		// Once per frame. Returns true if the eviction epoch bump happened on this call.
		bool	Tick();

		bool		GetValues(const Dia::CellLayers::CellLayerDesc& layer, float* out, uint32_t n) override;
		bool		IsLive() const override;
		uint64_t	ViewEpoch() const override;

	private:
		Dia::CellLayers::CellLayerRegistry&	mRegistry;
		const CellHistoryRecorder&			mRecorder;
		HistoryViewState&					mView;
		bool								mInstalled = false;
	};
}
