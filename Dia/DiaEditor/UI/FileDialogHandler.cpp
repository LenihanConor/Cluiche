#include "DiaEditor/UI/FileDialogHandler.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>

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

			char* headless = nullptr;
			size_t len = 0;
			_dupenv_s(&headless, &len, "DIA_HEADLESS");
			bool isHeadless = headless && headless[0] != '\0';
			free(headless);
			if (isHeadless)
			{
				result["success"] = false;
				return result;
			}

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

		Json::Value FileDialogHandler::HandleFolderDialog(const Json::Value& data)
		{
			Json::Value result;

			char* headless = nullptr;
			size_t len = 0;
			_dupenv_s(&headless, &len, "DIA_HEADLESS");
			bool isHeadless = headless && headless[0] != '\0';
			free(headless);
			if (isHeadless)
			{
				result["success"] = false;
				return result;
			}

			std::string title      = data.get("title",       "Select Folder").asString();
			std::string initialDir = data.get("initial_dir", "").asString();

			// SHBrowseForFolderA requires COM to be initialised; call CoInitialize defensively.
			CoInitialize(nullptr);

			char displayName[MAX_PATH] = {};
			BROWSEINFOA bi = {};
			bi.hwndOwner      = GetActiveWindow();
			bi.pszDisplayName = displayName;
			bi.lpszTitle      = title.c_str();
			bi.ulFlags        = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

			// Set initial selection via callback when initial_dir is provided.
			struct CallbackData { const char* path; };
			CallbackData cbData{ initialDir.c_str() };

			if (!initialDir.empty())
			{
				bi.lpfn = [](HWND hwnd, UINT msg, LPARAM, LPARAM lp) -> int
				{
					if (msg == BFFM_INITIALIZED)
					{
						const CallbackData* cd = reinterpret_cast<const CallbackData*>(lp);
						SendMessageA(hwnd, BFFM_SETSELECTION, TRUE,
						             reinterpret_cast<LPARAM>(cd->path));
					}
					return 0;
				};
				bi.lParam = reinterpret_cast<LPARAM>(&cbData);
			}

			LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
			if (pidl)
			{
				char folderPath[MAX_PATH] = {};
				if (SHGetPathFromIDListA(pidl, folderPath))
				{
					result["success"] = true;
					result["path"]    = folderPath;
				}
				else
				{
					result["success"] = false;
				}
				CoTaskMemFree(pidl);
			}
			else
			{
				result["success"] = false;
			}

			CoUninitialize();
			return result;
		}
	}
}
