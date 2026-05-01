#include "DiaEditor/MVC/EditorModel.h"

#include <DiaCore/Core/Assert.h>
#include <DiaCore/Json/external/json/json.h>

#include <fstream>
#include <string.h>

namespace Dia
{
	namespace Editor
	{
		const Dia::Core::StringCRC EditorModel::kUniqueId("EditorModel");

		EditorModel::EditorModel()
			: mHasOpenProject(false)
			, mIsDirty(false)
			, mCloseRequested(false)
			, mManifestCount(0)
		{
			mProjectPath[0] = '\0';
			for (unsigned int i = 0; i < kMaxManifests; ++i)
				mManifestPaths[i][0] = '\0';
		}

		void EditorModel::LoadProject(const char* path)
		{
			DIA_ASSERT(path != nullptr, "EditorModel: project path must not be null");

			strncpy_s(mProjectPath, kMaxPathLength, path, _TRUNCATE);

			std::ifstream file(path);
			if (!file.is_open())
			{
				DIA_ASSERT(false, "EditorModel: failed to open project file");
				return;
			}

			Json::Value root;
			Json::CharReaderBuilder builder;
			std::string errors;
			if (!Json::parseFromStream(builder, file, &root, &errors))
			{
				DIA_ASSERT(false, "EditorModel: failed to parse project JSON");
				return;
			}

			mManifestCount = 0;
			const Json::Value& manifests = root["manifests"];
			for (unsigned int i = 0; i < manifests.size() && i < kMaxManifests; ++i)
			{
				std::string mpath = manifests[i].asString();
				strncpy_s(mManifestPaths[mManifestCount], kMaxPathLength, mpath.c_str(), _TRUNCATE);
				++mManifestCount;
			}

			mHasOpenProject = true;
			mIsDirty = false;

			NotifyObservers(ObserverMessage::kProjectChanged);
		}

		void EditorModel::MarkDirty()
		{
			if (!mIsDirty)
			{
				mIsDirty = true;
				NotifyObservers(ObserverMessage::kDirtyStateChanged);
			}
		}

		void EditorModel::ClearDirty()
		{
			if (mIsDirty)
			{
				mIsDirty = false;
				NotifyObservers(ObserverMessage::kDirtyStateChanged);
			}
		}

		void EditorModel::RequestClose()
		{
			mCloseRequested = true;
			NotifyObservers(ObserverMessage::kCloseRequested);
		}

		void EditorModel::Reset()
		{
			mHasOpenProject = false;
			mIsDirty = false;
			mCloseRequested = false;
			mManifestCount = 0;
			mProjectPath[0] = '\0';
		}

		bool EditorModel::HasOpenProject() const { return mHasOpenProject; }
		bool EditorModel::IsDirty() const { return mIsDirty; }
		bool EditorModel::IsCloseRequested() const { return mCloseRequested; }
		const char* EditorModel::GetProjectPath() const { return mProjectPath; }
		unsigned int EditorModel::GetManifestCount() const { return mManifestCount; }

		const char* EditorModel::GetManifestPath(unsigned int index) const
		{
			DIA_ASSERT(index < mManifestCount, "EditorModel: manifest index out of range");
			return mManifestPaths[index];
		}
	}
}
