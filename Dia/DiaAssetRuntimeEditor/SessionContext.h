#pragma once

namespace Dia
{
	namespace AssetRuntime
	{
		namespace Editor
		{
			class SessionContext
			{
			public:
				SessionContext();

				void Load(const char* outputDir);
				void Save(const char* outputDir) const;

				float GetPollInterval() const { return mPollInterval; }
				void SetPollInterval(float interval);

				unsigned int GetMaxLogEntries() const { return mMaxLogEntries; }
				void SetMaxLogEntries(unsigned int max);

				const char* GetStateFilter() const { return mStateFilter; }
				void SetStateFilter(const char* filter);

				const char* GetIdSearchText() const { return mIdSearchText; }
				void SetIdSearchText(const char* text);

			private:
				float mPollInterval;
				unsigned int mMaxLogEntries;
				char mStateFilter[32];
				char mIdSearchText[128];
			};
		}
	}
}
