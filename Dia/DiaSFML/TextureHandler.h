#pragma once

#include <DiaAsset/IAssetTypeHandler.h>
#include <DiaCore/CRC/StringCRC.h>

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Dia { namespace Core { class JobSystem; struct JobHandle; } }

namespace sf
{
	class Texture;
}

namespace Dia
{
	namespace SFML
	{
		// Forward-declared; defined in TextureHandler.cpp (contains sf::Image)
		struct PendingUpload;

		class TextureHandler : public Dia::AssetRuntime::IAssetTypeHandler
		{
		public:
			TextureHandler();
			~TextureHandler();

			void SetJobSystem(Dia::Core::JobSystem* jobSystem);

			unsigned int GetTextureId(const Dia::Core::StringCRC& assetId) const;
			const sf::Texture* GetTexture(unsigned int textureId) const;
			unsigned int GetLoadedCount() const;

			virtual void Load(const Dia::Core::StringCRC& assetId,
			                  const Dia::Core::Containers::String512& resolvedPath,
			                  Dia::AssetRuntime::IAssetLoadCallback* callback) override;

			virtual void Unload(const Dia::Core::StringCRC& assetId) override;

			// Pumps decoded image -> GPU upload queue. Must be called on the
			// thread owning the GL context (RenderPU). Safe to call every frame.
			void Tick();

			// Drains the deferred-deletion queue, destroying sf::Texture objects.
			// MUST be called on the thread owning the GL context. Designed to be
			// invoked once per frame from RenderModule::DoUpdate (post-present)
			// and one final time from RenderModule::DoStop while the context is
			// still active.
			//
			// Why deferred: Unload() can be called from any thread (e.g. MainPU
			// during AssetServiceModule::DoStop while the RenderPU still owns
			// the GL context). Deleting an sf::Texture from a thread that does
			// not own the context blocks indefinitely on driver synchronisation
			// — observed as a hang during shutdown teardown. By queueing in
			// Unload and draining here, we guarantee GPU deletes happen on the
			// only thread allowed to issue GL calls.
			void ProcessGpuDeletions();

			// Drains in-flight async uploads, the deferred-deletion queue, and
			// any remaining textures. Must be called on the thread owning the
			// GL context, before that context is destroyed. ~TextureHandler
			// also calls this, but value members destruct after their owner's
			// destructor body — owners holding the GL context (e.g.
			// RenderWindow) must invoke explicitly while the context is still
			// alive. Idempotent.
			void Shutdown();

		private:
			void UnloadAll();

			mutable std::shared_mutex mMutex;
			std::unordered_map<unsigned int, unsigned int> mAssetToTextureId;
			std::unordered_map<std::string, unsigned int> mPathToId;
			std::unordered_map<unsigned int, sf::Texture*> mIdToTexture;
			unsigned int mNextId;

			std::mutex mPendingUploadsMutex;
			std::vector<std::shared_ptr<PendingUpload>> mPendingUploads;

			// Deferred GPU-side deletion. Unload() (any thread) appends here.
			// ProcessGpuDeletions() (GL-context thread only) drains and deletes.
			std::mutex mPendingDeletionsMutex;
			std::vector<sf::Texture*> mPendingDeletions;

			Dia::Core::JobSystem* mJobSystem = nullptr;
		};
	}
}
