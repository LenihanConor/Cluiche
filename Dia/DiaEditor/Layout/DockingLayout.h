#pragma once

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace Editor
	{
		class DockingLayout
		{
		public:
			struct PanelInfo
			{
				char name[64];
				char uiPath[256];
				bool visible;

				PanelInfo() : visible(true)
				{
					name[0] = '\0';
					uiPath[0] = '\0';
				}
			};

			DockingLayout();

			void RegisterPanel(const char* name, const char* uiPath);
			bool IsPanelRegistered(const char* name) const;

			unsigned int GetPanelCount() const;
			const PanelInfo& GetPanel(unsigned int index) const;

			void Serialize(Json::Value& out) const;
			void Deserialize(const Json::Value& in);

			void ValidateLayout(Json::Value& layout) const;

			bool SaveToDisk(const char* path) const;
			bool LoadFromDisk(const char* path);

		private:
			static const unsigned int kMaxPanels = 32;
			Dia::Core::Containers::DynamicArrayC<PanelInfo, kMaxPanels> mPanels;
		};
	}
}
