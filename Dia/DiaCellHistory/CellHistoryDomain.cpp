#ifdef DIA_DEBUG
#include <DiaCellHistory/CellHistoryDomain.h>

#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>

namespace Dia::CellHistory
{
	using Dia::Core::StringCRC;

	CellHistoryDomain::CellHistoryDomain(CellHistoryRecorder& recorder, HistoryViewState& view, HistoryViewCommands& viewCommands,
		CellHistoryCommands& historyCommands, BaseLayerFn baseLayer, const Dia::ValueSchema::ValueSchemaService* schema, const HistoryConfig* config)
		: mRecorder(recorder), mView(view), mViewCommands(viewCommands), mHistoryCommands(historyCommands), mBaseLayer(baseLayer)
		, mSchema(schema), mConfig(config)
	{
	}

	StringCRC CellHistoryDomain::GetDomainId() const { return StringCRC("cellhistory"); }
	const char* CellHistoryDomain::GetDisplayName() const { return "Cell History"; }
	const char* CellHistoryDomain::GetDescription() const { return "Scrub timeline, event ticks and not-live banner for the cell layers"; }
	StringCRC CellHistoryDomain::GetGroup() const { return StringCRC("Spatial"); }
	Dia::Core::RGBA CellHistoryDomain::GetAccentColour() const { return Dia::VisualDebugger::DebugGroupAccents::kSpatial; }

	void CellHistoryDomain::Update()
	{
		mView.Update();
		const StringCRC base = mBaseLayer.empty() ? StringCRC::kZero : mBaseLayer();
		mCard.Update(mRecorder, mView, base, mSchema, mConfig);
	}

	void CellHistoryDomain::GetJSONState(Json::Value& out)
	{
		const Json::Value& card = mCard.Card();
		if (!card.isObject())
		{
			out["available"] = false;
			return;
		}
		for (Json::Value::const_iterator it = card.begin(); it != card.end(); ++it)
		{
			out[it.name()] = *it;
		}
	}

	void CellHistoryDomain::OnCommand(StringCRC cmd, const Json::Value& args)
	{
		Json::Value result;
		const char* name = nullptr;
		if (cmd == StringCRC("view")) { name = "view"; result = mViewCommands.View(args); }
		else if (cmd == StringCRC("live")) { name = "live"; result = mViewCommands.Live(); }
		else if (cmd == StringCRC("step")) { name = "step"; result = mViewCommands.Step(args); }
		else if (cmd == StringCRC("event")) { name = "event"; result = mViewCommands.Event(args); }
		else if (cmd == StringCRC("delta")) { name = "delta"; result = mViewCommands.Delta(args); }
		else if (cmd == StringCRC("track"))
		{
			name = "track";
			Json::Value a(Json::objectValue);
			Json::Value layers(Json::arrayValue);
			if (args.isObject() && args["layer"].isString() && args["layer"].asString().size() > 0) { layers.append(args["layer"].asString()); }
			else
			{
				const StringCRC base = mBaseLayer.empty() ? StringCRC::kZero : mBaseLayer();
				if (base != StringCRC::kZero) { layers.append(base.AsChar()); }
			}
			a["layers"] = layers;
			result = mHistoryCommands.Track(a);
		}
		else { return; }

		mLastResult = result;
		if (result.isObject() && result.isMember("error"))
		{
			DIA_LOG_WARNING("HIS", "cell history panel command '%s' failed: %s", name, result["error"].asString().c_str());
		}
		Update();	// the next state push shows the result without waiting for the frame driver
	}
}
#endif
