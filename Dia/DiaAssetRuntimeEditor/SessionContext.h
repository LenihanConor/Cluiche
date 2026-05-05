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

			private:
				float mPollInterval;
				unsigned int mMaxLogEntries;
			};
		}
	}
}
