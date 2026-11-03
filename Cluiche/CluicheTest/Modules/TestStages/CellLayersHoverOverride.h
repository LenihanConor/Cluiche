#pragma once
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaMaths/Vector/Vector2D.h>
#ifdef DIA_DEBUG
#include <DiaCellLayers/MirrorQueries.h>
#endif

#include <string>

namespace CluicheTest {

// Test-only hover injection for CellLayersViewerTestStageModule (application code, no Dia change).
// The viewer's hover reads IDebugDraw::GetMousePixel(), which the game fills from the real input stream, so an E2E cannot move
// it. The stage hands the viewer a HoverOverrideDraw instead of the real draw: it forwards everything and reports the injected
// pixel while the override is active. The state is written by sim-hosted commands and read by the stage's Sim update (same PU, no lock).

struct HoverOverride
{
	bool active = false;
	Dia::Maths::Vector2D pixel;
};

// {x, y} (numbers) activates the override at that screen pixel; {clear: true} deactivates it; {clear: false, x, y} is an override.
// On failure returns false with outError set and leaves `out` untouched.
inline bool ParseHoverParams(const Json::Value& params, HoverOverride& out, std::string& outError)
{
	if (!params.isObject())
	{
		outError = "params must be an object";
		return false;
	}
	const bool hasXY = params.isMember("x") || params.isMember("y");
	if (params.isMember("clear"))
	{
		if (!params["clear"].isBool())
		{
			outError = "non-bool param 'clear'";
			return false;
		}
		if (params["clear"].asBool())
		{
			if (hasXY)
			{
				outError = "'clear' conflicts with x/y";
				return false;
			}
			out.active = false;
			return true;
		}
	}
	if (!params["x"].isNumeric()) { outError = "missing or non-numeric 'x'"; return false; }
	if (!params["y"].isNumeric()) { outError = "missing or non-numeric 'y'"; return false; }
	out.active = true;
	out.pixel = Dia::Maths::Vector2D(params["x"].asFloat(), params["y"].asFloat());
	return true;
}

// Forwards every draw call to `inner`; GetMousePixel() reports the override while it is active, otherwise the real mouse.
class HoverOverrideDraw : public Dia::Core::IDebugDraw
{
public:
	HoverOverrideDraw(Dia::Core::IDebugDraw& inner, const HoverOverride& hover) : mInner(inner), mHover(hover) {}

	using Dia::Core::IDebugDraw::RequestDraw;
	using Dia::Core::IDebugDraw::RequestDrawRect;

	void RequestDraw(const Dia::Maths::Vector2D& p, float r, Dia::Core::RGBA outline, Dia::Core::RGBA fill) override { mInner.RequestDraw(p, r, outline, fill); }
	void RequestDraw(const Dia::Maths::Vector2D& a, const Dia::Maths::Vector2D& b, Dia::Core::RGBA c) override { mInner.RequestDraw(a, b, c); }
	void RequestDrawPoint(const Dia::Maths::Vector2D& p, Dia::Core::RGBA c) override { mInner.RequestDrawPoint(p, c); }
	void RequestDrawRect(const Dia::Maths::Vector2D& mn, const Dia::Maths::Vector2D& mx, Dia::Core::RGBA outline, Dia::Core::RGBA fill) override { mInner.RequestDrawRect(mn, mx, outline, fill); }
	void RequestDrawArc(const Dia::Maths::Vector2D& p, float r, float s, float e, Dia::Core::RGBA c) override { mInner.RequestDrawArc(p, r, s, e, c); }
	void RequestDrawRay(const Dia::Maths::Vector2D& o, const Dia::Maths::Vector2D& d, float l, Dia::Core::RGBA c) override { mInner.RequestDrawRay(o, d, l, c); }
	void RequestDraw(const Dia::Maths::Vector2D& p1, const Dia::Maths::Vector2D& p2, const Dia::Maths::Vector2D& p3, Dia::Core::RGBA outline, Dia::Core::RGBA fill) override { mInner.RequestDraw(p1, p2, p3, outline, fill); }
	void RequestDrawText(const Dia::Maths::Vector2D& p, const char* t, float size, Dia::Core::RGBA c) override { mInner.RequestDrawText(p, t, size, c); }
	void RequestDrawLine3D(const Dia::Maths::Vector3D& a, const Dia::Maths::Vector3D& b, Dia::Core::RGBA c) override { mInner.RequestDrawLine3D(a, b, c); }
	void RequestDrawRay3D(const Dia::Maths::Vector3D& o, const Dia::Maths::Vector3D& d, float l, Dia::Core::RGBA c) override { mInner.RequestDrawRay3D(o, d, l, c); }
	void RequestDrawBox3D(const Dia::Maths::Vector3D& mn, const Dia::Maths::Vector3D& mx, Dia::Core::RGBA c) override { mInner.RequestDrawBox3D(mn, mx, c); }
	void RequestDrawSphere3D(const Dia::Maths::Vector3D& p, float r, Dia::Core::RGBA c) override { mInner.RequestDrawSphere3D(p, r, c); }
	void RequestDrawArrow3D(const Dia::Maths::Vector3D& o, const Dia::Maths::Vector3D& d, float l, float h, Dia::Core::RGBA c) override { mInner.RequestDrawArrow3D(o, d, l, h, c); }
	void RequestDrawCellField(const Dia::Core::CellFieldDrawRequest& r) override { mInner.RequestDrawCellField(r); }
	uint32_t DroppedCount() const override { return mInner.DroppedCount(); }
	const Dia::Maths::Vector2D& GetMousePixel() const override { return mHover.active ? mHover.pixel : mInner.GetMousePixel(); }

private:
	Dia::Core::IDebugDraw& mInner;
	const HoverOverride& mHover;
};

#ifdef DIA_DEBUG
// Registers cluichetest.celllayers.hover (command) on the sim-hosted registrar. Runs on SimPU; `hover` must outlive the registration.
inline void UnregisterHoverCommand(Dia::CellLayers::IMirrorQueryRegistrar& registrar)
{
	registrar.Unregister("cluichetest.celllayers.hover");
}

inline bool RegisterHoverCommand(Dia::CellLayers::IMirrorQueryRegistrar& registrar, HoverOverride& hover)
{
	static const Dia::API::ValueParam kParams[] = {
		{ "x", "float", false, "Screen pixel X (top-left origin) to hover; required unless clear is true" },
		{ "y", "float", false, "Screen pixel Y (top-left origin) to hover; required unless clear is true" },
		{ "clear", "bool", false, "true: stop overriding and use the real mouse again" } };
	Dia::DebugServer::SimHelpInfo help;
	help.description = "Test only: override the viewer's hover cursor with a screen pixel, or clear the override. Returns {ok, active}.";
	help.category = "test"; help.owner = "CluicheTest";
	help.params = kParams; help.paramCount = 3;
	return registrar.RegisterCommand("cluichetest.celllayers.hover", help, [&hover](const Json::Value& params) -> Json::Value
	{
		Json::Value out(Json::objectValue);
		std::string err;
		if (!ParseHoverParams(params, hover, err))
		{
			out["error"] = err;
			return out;
		}
		out["ok"] = true;
		out["active"] = hover.active;
		return out;
	});
}
#endif // DIA_DEBUG

} // namespace CluicheTest
