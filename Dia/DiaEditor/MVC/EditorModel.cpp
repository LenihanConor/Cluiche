#include "DiaEditor/MVC/EditorModel.h"

#include <DiaCore/Core/Assert.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaGame/DiaGameManifestLoader.h>
#include <DiaGame/DiaGameManifest.h>
#include <DiaObservation/Log/DiaLog.h>

#include <fstream>
#include <string.h>

namespace Dia
{
	namespace Editor
	{
		const Dia::Core::StringCRC EditorModel::kUniqueId("EditorModel");

		namespace
		{
			// Copy src into dst (fixed buffer size N), ensuring null termination.
			template<unsigned int N>
			void SafeCopy(char (&dst)[N], const char* src)
			{
				if (src == nullptr)
					dst[0] = '\0';
				else
					strncpy_s(dst, N, src, _TRUNCATE);
			}

			// Extract the directory component from a file path into dirOut.
			// "C:/foo/bar/baz.diagame" -> "C:/foo/bar"
			void ExtractDirectory(const char* path, char* dirOut, unsigned int dirOutSize)
			{
				if (path == nullptr || dirOutSize == 0)
				{
					if (dirOut && dirOutSize > 0) dirOut[0] = '\0';
					return;
				}
				strncpy_s(dirOut, dirOutSize, path, _TRUNCATE);
				// Walk back to last slash
				char* last = nullptr;
				for (char* p = dirOut; *p; ++p)
					if (*p == '/' || *p == '\\') last = p;
				if (last)
					*last = '\0';
				else
					dirOut[0] = '\0';
			}

			// Join dir + "/" + rel into outBuf, unless rel is already absolute.
			void JoinPath(const char* dir, const char* rel, char* outBuf, unsigned int outSize)
			{
				if (rel == nullptr || rel[0] == '\0')
				{
					outBuf[0] = '\0';
					return;
				}
				// Absolute path check (e.g. "C:\" or "/")
				bool isAbsolute = (rel[1] == ':') || (rel[0] == '/') || (rel[0] == '\\');
				if (isAbsolute || dir == nullptr || dir[0] == '\0')
				{
					strncpy_s(outBuf, outSize, rel, _TRUNCATE);
					return;
				}
				// Build "dir/rel"
				unsigned int needed = static_cast<unsigned int>(strlen(dir)) + 1 + static_cast<unsigned int>(strlen(rel)) + 1;
				if (needed > outSize)
				{
					outBuf[0] = '\0';
					return;
				}
				strncpy_s(outBuf, outSize, dir, _TRUNCATE);
				strncat_s(outBuf, outSize, "/", _TRUNCATE);
				strncat_s(outBuf, outSize, rel, _TRUNCATE);
			}
		}

		EditorModel::EditorModel()
			: mHasOpenProject(false)
			, mIsDirty(false)
			, mCloseRequested(false)
			, mManifestCount(0)
			, mCallbackCount(0)
			, mRecentCount(0)
		{
			mProjectPath[0] = '\0';
			for (unsigned int i = 0; i < kMaxManifests; ++i)
				mManifestPaths[i][0] = '\0';
			for (unsigned int i = 0; i < kMaxCallbacks; ++i)
			{
				mCallbacks[i].callback = nullptr;
				mCallbacks[i].userData = nullptr;
			}
			for (unsigned int i = 0; i < kMaxRecent; ++i)
				mRecentPaths[i][0] = '\0';
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
			mDiagameContext = ProjectContext{};
			mCallbackCount = 0;
			mRecentCount = 0;
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

		// --- IEditorContext implementation ---

		bool EditorModel::LoadDiagameProject(const char* diagamePath)
		{
			DIA_LOG_INFO("Editor", "EditorModel: LoadDiagameProject called with path='%s'", diagamePath ? diagamePath : "<null>");
			if (diagamePath == nullptr || diagamePath[0] == '\0')
			{
				DIA_LOG_WARNING("Editor", "EditorModel: LoadDiagameProject called with null/empty path");
				return false;
			}

			Dia::Game::DiaGameManifest manifest;
			bool loaded = Dia::Game::DiaGameManifestLoader::LoadGameFile(diagamePath, manifest);
			if (!loaded)
			{
				DIA_LOG_WARNING("Editor", "EditorModel: failed to load .diagame '%s'", diagamePath);
				return false;
			}

			// Resolve directory of the .diagame file so relative import paths can be joined.
			char dir[ProjectContext::kMaxPath];
			ExtractDirectory(diagamePath, dir, sizeof(dir));

			mDiagameContext = ProjectContext{};
			SafeCopy(mDiagameContext.diagamePath, diagamePath);

			// First manifest-type import becomes applicationManifestPath.
			for (unsigned int i = 0; i < manifest.imports.Size(); ++i)
			{
				if (manifest.imports[i].type == Dia::Application::TypedImport::ImportType::kManifest)
				{
					JoinPath(dir, manifest.imports[i].path.AsCStr(),
						mDiagameContext.applicationManifestPath,
						ProjectContext::kMaxPath);
					break;
				}
			}

			if (manifest.config.assetCatalogue.Length() > 0)
			{
				JoinPath(dir, manifest.config.assetCatalogue.AsCStr(),
					mDiagameContext.assetCataloguePath,
					ProjectContext::kMaxPath);
				DIA_LOG_INFO("Editor", "EditorModel: assetCataloguePath resolved to '%s'", mDiagameContext.assetCataloguePath);
			}
			else
			{
				DIA_LOG_WARNING("Editor", "EditorModel: .diagame '%s' has no asset_catalogue field — catalogue will not load", diagamePath);
			}

			if (manifest.config.assetRoot.Length() > 0)
			{
				JoinPath(dir, manifest.config.assetRoot.AsCStr(),
					mDiagameContext.assetRoot,
					ProjectContext::kMaxPath);
			}

			DIA_LOG_INFO("Editor", "EditorModel: firing project callbacks for '%s'", diagamePath);
			FireProjectCallbacks();
			DIA_LOG_INFO("Editor", "EditorModel: LoadDiagameProject complete");
			return true;
		}

		void EditorModel::ClearDiagameProject()
		{
			mDiagameContext = ProjectContext{};
			FireProjectCallbacks();
		}

		const ProjectContext& EditorModel::GetDiagameProject() const
		{
			return mDiagameContext;
		}

		void EditorModel::OnDiagameProjectChanged(ProjectChangedCallback callback, void* userData)
		{
			if (callback == nullptr)
				return;
			DIA_ASSERT(mCallbackCount < kMaxCallbacks, "EditorModel: max OnDiagameProjectChanged callbacks reached");
			if (mCallbackCount >= kMaxCallbacks)
				return;
			mCallbacks[mCallbackCount].callback = callback;
			mCallbacks[mCallbackCount].userData = userData;
			++mCallbackCount;
			if (mDiagameContext.IsValid())
				callback(mDiagameContext, userData);
		}

		unsigned int EditorModel::GetRecentProjectCount() const { return mRecentCount; }

		const char* EditorModel::GetRecentProject(unsigned int index) const
		{
			if (index >= mRecentCount) return nullptr;
			return mRecentPaths[index];
		}

		void EditorModel::SetRecentProjects(const char* const* paths, unsigned int count)
		{
			mRecentCount = (count < kMaxRecent) ? count : kMaxRecent;
			for (unsigned int i = 0; i < mRecentCount; ++i)
				SafeCopy(mRecentPaths[i], paths[i] ? paths[i] : "");
		}

		void EditorModel::FireProjectCallbacks()
		{
			for (unsigned int i = 0; i < mCallbackCount; ++i)
				mCallbacks[i].callback(mDiagameContext, mCallbacks[i].userData);
		}
	}
}
