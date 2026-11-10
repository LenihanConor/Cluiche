#pragma once

// Test helper (Testing/ convention, no GoogleTest): registers a CellLayerViewerDomain's sim-hosted commands on a
// FakeMirrorRegistrar (standing in for SimQueryHostModule) and runs them by name. Exec wraps the raw handler result the way the
// former DiaAPI command path did ({success:true,data} / {success:false,error}), so older assertions keep their shape; the wire
// itself returns the raw handler result. One ViewerCommands is "current" at a time (the static Exec uses it).

#ifdef DIA_DEBUG

#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>

namespace Dia::CellLayers::Testing
{
	class ViewerCommands
	{
	public:
		explicit ViewerCommands(CellLayerViewerDomain& domain) : mDomain(domain)
		{
			mRegistered = mDomain.RegisterCommands(mRegistrar);
			Current() = this;
		}

		~ViewerCommands()
		{
			mDomain.UnregisterCommands(mRegistrar);
			if (Current() == this) { Current() = nullptr; }
		}

		ViewerCommands(const ViewerCommands&) = delete;
		ViewerCommands& operator=(const ViewerCommands&) = delete;

		void Unregister() { mDomain.UnregisterCommands(mRegistrar); }	// idempotent; the destructor does it too

		bool Registered() const { return mRegistered; }
		FakeMirrorRegistrar& Registrar() { return mRegistrar; }

		// Raw handler result (what the wire carries). Null value when the name is not registered.
		Json::Value Raw(const char* name, const Json::Value& params) const { return mRegistrar.Execute(name, params); }

		static Json::Value Exec(const char* name, const Json::Value& params)
		{
			Json::Value response(Json::objectValue);
			if (Current() == nullptr || !Current()->mRegistrar.Has(name))
			{
				response["success"] = false;
				response["error"] = "command not found";
				return response;
			}
			const Json::Value data = Current()->mRegistrar.Execute(name, params);
			if (data.isObject() && data.isMember("error"))
			{
				response["success"] = false;
				response["error"] = data["error"];
			}
			else
			{
				response["success"] = true;
				response["data"] = data;
			}
			return response;
		}

	private:
		static ViewerCommands*& Current() { static ViewerCommands* sCurrent = nullptr; return sCurrent; }

		CellLayerViewerDomain&	mDomain;
		FakeMirrorRegistrar		mRegistrar;
		bool					mRegistered = false;
	};
}

#endif // DIA_DEBUG
