#pragma once

// Test helper (Testing/ convention, no GoogleTest): an IMirrorQueryRegistrar that records registrations in memory and can run
// the registered handlers, standing in for SimQueryHostModule (which lives in Cluiche, not Dia).

#ifdef DIA_DEBUG

#include <DiaCellLayers/MirrorQueries.h>

#include <functional>
#include <string>
#include <vector>

namespace Dia::CellLayers::Testing
{
	class FakeMirrorRegistrar : public IMirrorQueryRegistrar
	{
	public:
		bool Register(const char* name, MirrorQueryHandler handler) override { return Add(name, std::move(handler), false); }
		bool RegisterCommand(const char* name, MirrorQueryHandler handler) override { return Add(name, std::move(handler), true); }

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
		bool Add(const char* name, MirrorQueryHandler handler, bool isCommand)
		{
			if (Has(name)) { return false; }	// like SimQueryHost: the first handler stays
			mEntries.push_back({ name, std::move(handler), isCommand });
			mRegisterCount++;
			return true;
		}

		struct Entry { std::string name; MirrorQueryHandler handler; bool isCommand; };
		std::vector<Entry> mEntries;
		size_t mRegisterCount = 0;
	};
}

#endif // DIA_DEBUG
