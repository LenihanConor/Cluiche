#include "DiaEditor/Project/GameScopeDiscovery.h"

#include "DiaEditor/EditorManifestLoader.h"
#include "DiaEditor/Project/GamePath.h"
#include <DiaGame/DiaGameManifestLoader.h>
#include <DiaGame/DiaGameManifest.h>
#include <DiaObservation/Log/DiaLog.h>

#include <filesystem>
#include <set>
#include <system_error>

namespace Dia
{
	namespace Editor
	{
		namespace GameScopeDiscovery
		{
			namespace
			{
				struct Collector
				{
					TypeList* types;

					static void OnPlugin(const EditorManifestLoader::PluginEntry& entry, void* userData)
					{
						Collector* self = static_cast<Collector*>(userData);
						const Dia::Core::StringCRC id(entry.typeId);
						if (!EditorPluginRegistry::Instance().IsPluginRegistered(id))
						{
							DIA_LOG_WARNING("Editor", "GameScopeDiscovery: game-scoped plugin type '%s' is not registered; skipped", entry.typeId);
							return;
						}
						for (unsigned int i = 0; i < self->types->Size(); ++i)
							if ((*self->types)[i] == id)
								return;
						if (self->types->IsFull())
						{
							DIA_LOG_WARNING("Editor", "GameScopeDiscovery: game scope list full; '%s' skipped", entry.typeId);
							return;
						}
						self->types->Add(id);
					}
				};
			}

			Dia::Core::StringCRC CollectGameScope(const char* diagamePath, const Dia::Game::DiaGameManifest& manifest, TypeList& outTypes)
			{
				char gameKey[1024];
				NormalizeGamePath(diagamePath, gameKey, sizeof(gameKey));

				Collector collector{ &outTypes };
				const std::filesystem::path dir = std::filesystem::path(diagamePath).parent_path();
				for (unsigned int i = 0; i < manifest.editorManifests.Size(); ++i)
				{
					const char* rel = manifest.editorManifests[i].AsCStr();
					std::string resolved;
					if (rel != nullptr && rel[0] != '\0')
					{
						std::filesystem::path p(rel);
						resolved = (p.is_absolute() || p.has_root_directory() || dir.empty()) ? p.generic_string() : (dir / p).generic_string();
					}
					if (resolved.empty() || !EditorManifestLoader::Load(resolved.c_str(), &Collector::OnPlugin, &collector))
						DIA_LOG_WARNING("Editor", "GameScopeDiscovery: could not load editor manifest '%s' listed by '%s'", rel ? rel : "", diagamePath);
				}
				return Dia::Core::StringCRC(gameKey);
			}

			bool TagGameFromDiagame(const char* diagamePath)
			{
				if (diagamePath == nullptr || diagamePath[0] == '\0')
					return false;

				Dia::Game::DiaGameManifest manifest;
				if (!Dia::Game::DiaGameManifestLoader::LoadGameFile(diagamePath, manifest))
				{
					DIA_LOG_WARNING("Editor", "GameScopeDiscovery: could not load .diagame '%s'; skipped", diagamePath);
					return false;
				}

				TypeList types;
				const Dia::Core::StringCRC gameId = CollectGameScope(diagamePath, manifest, types);
				for (unsigned int i = 0; i < types.Size(); ++i)
					EditorPluginRegistry::Instance().TagGameScoped(types[i], gameId);
				DIA_LOG_INFO("Editor", "GameScopeDiscovery: tagged %u type(s) for '%s'", types.Size(), diagamePath);
				return true;
			}

			void ScanForDiagames(const char* root, unsigned int maxDepth, std::vector<std::string>& outPaths)
			{
				if (root == nullptr || root[0] == '\0')
					return;

				std::error_code ec;
				if (!std::filesystem::is_directory(root, ec))
				{
					DIA_LOG_INFO("Editor", "GameScopeDiscovery: game root '%s' is not a directory; skipped", root);
					return;
				}

				std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec);
				const std::filesystem::recursive_directory_iterator end;
				while (!ec && it != end)
				{
					if (it->is_regular_file(ec) && it->path().extension() == ".diagame")
						outPaths.push_back(it->path().generic_string());
					if (static_cast<unsigned int>(it.depth()) >= maxDepth)
						it.disable_recursion_pending();
					it.increment(ec);
				}
				if (ec)
					DIA_LOG_WARNING("Editor", "GameScopeDiscovery: scan of '%s' stopped early: %s", root, ec.message().c_str());
			}

			unsigned int DiscoverAndTag(const std::vector<std::string>& roots, unsigned int maxDepth,
			                            const std::vector<std::string>& gamePaths)
			{
				std::vector<std::string> all;
				for (const std::string& r : roots)
					ScanForDiagames(r.c_str(), maxDepth, all);
				all.insert(all.end(), gamePaths.begin(), gamePaths.end());

				std::set<std::string> seen;
				unsigned int tagged = 0;
				for (const std::string& p : all)
				{
					char key[1024];
					NormalizeGamePath(p.c_str(), key, sizeof(key));
					if (key[0] == '\0' || !seen.insert(key).second)
						continue;
					if (TagGameFromDiagame(p.c_str()))
						++tagged;
				}
				return tagged;
			}
		}
	}
}
