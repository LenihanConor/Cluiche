#pragma once

// IViewValueSource: optional hook through which a history/replay system substitutes the values the cell-layer viewer
// shows (e.g. DiaCellHistory showing a past sample). Installed on the CellLayerRegistry (the shared service); at most
// one source may be installed. Generic: no game types (CL-005), no singletons (CL-006).
//
// LayerUploader asks the installed source first. While IsLive() is true, or with no source installed, the layer's own
// read delegate is used and GetValues is never called. While not live, GetValues is the only value path: returning
// false means "no data in this view" (the viewer shows an empty map), never a fallback to live values.
// ViewEpoch() changes whenever the shown view changes without the layer's version() changing; the uploader keys its
// cached reads on (version(), ViewEpoch()).
// GetPresentation (optional, default false) lets a source supply a ramp id, value range and unit prefix that replace
// the schema's colouring and appear in the legend.

#include <DiaCore/CRC/StringCRC.h>

#include <stdint.h>

namespace Dia::CellLayers
{
	struct CellLayerDesc;

	struct ViewPresentation
	{
		Dia::Core::StringCRC	rampId;				// a ramp known to the ValueSchemaService (unknown: schema colouring is kept)
		float					rangeMin = 0.0f;
		float					rangeMax = 1.0f;
		char					unitPrefix[8] = {};	// e.g. a delta sign; empty for none
	};

	class IViewValueSource
	{
	public:
		virtual ~IViewValueSource() {}

		// Fills out[0..n) with the layer's values for the shown view; false = no data for this layer in this view.
		virtual bool		GetValues(const CellLayerDesc& layer, float* out, uint32_t n) = 0;
		virtual bool		IsLive() const = 0;
		virtual uint64_t	ViewEpoch() const = 0;
		virtual bool		GetPresentation(const CellLayerDesc& layer, ViewPresentation& out) { (void)layer; (void)out; return false; }
	};
}
