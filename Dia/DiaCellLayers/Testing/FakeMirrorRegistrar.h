#pragma once

// Test helper (Testing/ convention, no GoogleTest): an IMirrorQueryRegistrar that records registrations in memory and can run
// the registered handlers, standing in for SimQueryHostModule (which lives in Cluiche, not Dia).

#ifdef DIA_DEBUG

#include <DiaCellLayers/MirrorQueries.h>
#include <DiaDebugServer/SimHelpInfo.h>

#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace Dia::CellLayers::Testing
{
	class FakeMirrorRegistrar : public IMirrorQueryRegistrar
	{
	public:
		bool Register(const char* name, const Dia::DebugServer::SimHelpInfo& help, MirrorQueryHandler handler) override { return Add(name, help, std::move(handler), false); }
		bool RegisterCommand(const char* name, const Dia::DebugServer::SimHelpInfo& help, MirrorQueryHandler handler) override { return Add(name, help, std::move(handler), true); }

		void Unregister(const char* name) override
		{
			for (size_t i = 0; i < mEntries.size(); ++i)
			{
				if (mEntries[i].name == name) { mEntries.erase(mEntries.begin() + i); return; }
			}
		}

		bool IsCommand(const char* name) const
		{
			for (const Entry& e : mEntries) { if (e.name == name) { return e.isCommand; } }
			return false;
		}

		bool Has(const char* name) const
		{
			for (const Entry& e : mEntries) { if (e.name == name) { return true; } }
			return false;
		}

		// Stored help for a registered name (what the host would keep); null when not registered.
		const Dia::DebugServer::SimHelpStored* Help(const char* name) const
		{
			for (const Entry& e : mEntries) { if (e.name == name) { return &e.help; } }
			return nullptr;
		}

		Json::Value Execute(const char* name, const Json::Value& args) const
		{
			for (const Entry& e : mEntries) { if (e.name == name) { return e.handler(args); } }
			return Json::Value(Json::nullValue);
		}

		size_t RegisterCount() const { return mRegisterCount; }
		size_t Count() const { return mEntries.size(); }
		std::vector<std::string> Names() const
		{
			std::vector<std::string> out;
			for (const Entry& e : mEntries) { out.push_back(e.name); }
			return out;
		}

	private:
		bool Add(const char* name, const Dia::DebugServer::SimHelpInfo& help, MirrorQueryHandler handler, bool isCommand)
		{
			if (Has(name)) { return false; }	// like SimQueryHost: the first handler stays
			Dia::DebugServer::SimHelpStored stored;
			std::memset(&stored, 0, sizeof(stored));
			if (Dia::DebugServer::CopyHelp(help, stored) != Dia::DebugServer::SimHelpResult::Ok) { return false; }	// like the host: invalid help rejects
			mEntries.push_back({ name, std::move(handler), isCommand, stored });
			mRegisterCount++;
			return true;
		}

		struct Entry { std::string name; MirrorQueryHandler handler; bool isCommand; Dia::DebugServer::SimHelpStored help; };
		std::vector<Entry> mEntries;
		size_t mRegisterCount = 0;
	};
}

#endif // DIA_DEBUG
