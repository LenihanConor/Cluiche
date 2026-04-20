#include "DiaEditor/Layout/DockingLayout.h"

#include <DiaCore/Core/Assert.h>

#include <fstream>
#include <string.h>

namespace Dia
{
	namespace Editor
	{
		DockingLayout::DockingLayout()
		{
		}

		void DockingLayout::RegisterPanel(const char* name, const char* uiPath)
		{
			DIA_ASSERT(name != nullptr, "DockingLayout: panel name must not be null");
			DIA_ASSERT(uiPath != nullptr, "DockingLayout: panel uiPath must not be null");
			DIA_ASSERT(!mPanels.IsFull(), "DockingLayout: max panel capacity reached");

			PanelInfo info;
			strncpy_s(info.name, sizeof(info.name), name, _TRUNCATE);
			strncpy_s(info.uiPath, sizeof(info.uiPath), uiPath, _TRUNCATE);
			info.visible = true;
			mPanels.Add(info);
		}

		bool DockingLayout::IsPanelRegistered(const char* name) const
		{
			for (unsigned int i = 0; i < mPanels.Size(); ++i)
			{
				if (strcmp(mPanels[i].name, name) == 0)
					return true;
			}
			return false;
		}

		unsigned int DockingLayout::GetPanelCount() const
		{
			return mPanels.Size();
		}

		const DockingLayout::PanelInfo& DockingLayout::GetPanel(unsigned int index) const
		{
			DIA_ASSERT(index < mPanels.Size(), "DockingLayout: index out of range");
			return mPanels[index];
		}

		void DockingLayout::Serialize(Json::Value& out) const
		{
			out["panels"] = Json::arrayValue;
			for (unsigned int i = 0; i < mPanels.Size(); ++i)
			{
				Json::Value panel;
				panel["name"] = mPanels[i].name;
				panel["uiPath"] = mPanels[i].uiPath;
				panel["visible"] = mPanels[i].visible;
				out["panels"].append(panel);
			}
		}

		void DockingLayout::Deserialize(const Json::Value& in)
		{
			mPanels.RemoveAll();
			const Json::Value& panels = in["panels"];
			for (unsigned int i = 0; i < panels.size() && i < kMaxPanels; ++i)
			{
				PanelInfo info;
				std::string name = panels[i]["name"].asString();
				std::string uiPath = panels[i]["uiPath"].asString();
				strncpy_s(info.name, sizeof(info.name), name.c_str(), _TRUNCATE);
				strncpy_s(info.uiPath, sizeof(info.uiPath), uiPath.c_str(), _TRUNCATE);
				info.visible = panels[i].get("visible", true).asBool();
				mPanels.Add(info);
			}
		}

		void DockingLayout::ValidateLayout(Json::Value& layout) const
		{
			if (!layout.isObject() || !layout.isMember("panels"))
				return;

			Json::Value& panels = layout["panels"];
			if (!panels.isArray())
				return;

			Json::Value validPanels = Json::arrayValue;
			for (unsigned int i = 0; i < panels.size(); ++i)
			{
				std::string name = panels[i].get("name", "").asString();
				if (IsPanelRegistered(name.c_str()))
					validPanels.append(panels[i]);
			}
			layout["panels"] = validPanels;
		}

		bool DockingLayout::SaveToDisk(const char* path) const
		{
			Json::Value root;
			Serialize(root);

			Json::StreamWriterBuilder writer;
			std::ofstream file(path);
			if (!file.is_open())
				return false;

			file << Json::writeString(writer, root);
			return true;
		}

		bool DockingLayout::LoadFromDisk(const char* path)
		{
			std::ifstream file(path);
			if (!file.is_open())
				return false;

			Json::Value root;
			Json::CharReaderBuilder builder;
			std::string errors;
			if (!Json::parseFromStream(builder, file, &root, &errors))
				return false;

			Deserialize(root);
			return true;
		}
	}
}
