#include "DiaEditor/UI/FileDialogHandler.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>

#include <string>

namespace Dia
{
	namespace Editor
	{
		static void BuildFilterString(const Json::Value& filters, std::string& outFilter)
		{
			if (!filters.isArray() || filters.empty())
			{
				static const char kDefaultFilter[] = "All Files (*.*)\0*.*\0";
				outFilter.assign(kDefaultFilter, sizeof(kDefaultFilter) - 1);
				return;
			}

			outFilter.clear();
			for (unsigned int i = 0; i < filters.size(); ++i)
			{
				const Json::Value& f = filters[i];
				std::string name = f.get("name", "").asString();
				std::string ext = f.get("ext", "*.*").asString();
				outFilter.append(name);
				outFilter.push_back('\0');
				outFilter.append(ext);
				outFilter.push_back('\0');
			}
			outFilter.push_back('\0');
		}

		static Json::Value RunDialog(const Json::Value& data, bool isSave)
		{
			Json::Value result;

			std::string filterStr;
			BuildFilterString(data.get("filters", Json::Value(Json::arrayValue)), filterStr);

			std::string defaultExt = data.get("default_ext", "").asString();
			std::string title = data.get("title", "").asString();
			std::string initialDir = data.get("initial_dir", "").asString();

			char filePath[MAX_PATH] = {};

			OPENFILENAMEA ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = GetActiveWindow();
			ofn.lpstrFilter = filterStr.c_str();
			ofn.lpstrFile = filePath;
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

			if (!isSave)
				ofn.Flags |= OFN_FILEMUSTEXIST;
			else
				ofn.Flags |= OFN_OVERWRITEPROMPT;

			if (!defaultExt.empty())
				ofn.lpstrDefExt = defaultExt.c_str();

			if (!title.empty())
				ofn.lpstrTitle = title.c_str();

			if (!initialDir.empty())
				ofn.lpstrInitialDir = initialDir.c_str();

			BOOL ok = isSave ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn);

			if (ok)
			{
				result["success"] = true;
				result["path"] = filePath;
			}
			else
			{
				result["success"] = false;
			}

			return result;
		}

		Json::Value FileDialogHandler::HandleOpenFileDialog(const Json::Value& data)
		{
			return RunDialog(data, false);
		}

		Json::Value FileDialogHandler::HandleSaveFileDialog(const Json::Value& data)
		{
			return RunDialog(data, true);
		}
	}
}
