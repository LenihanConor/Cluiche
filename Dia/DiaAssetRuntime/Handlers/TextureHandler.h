#pragma once

#include <DiaAsset/IAssetTypeHandler.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaGraphics/Assets/ITexture.h>
#include <DiaThreading/JobSystem.h>

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <unordered_map>

namespace Dia
{
	namespace Bgfx { class BgfxTextureHandle; }
}

namespace Dia
{
	namespace AssetRuntime
	{
		struct PendingUpload;

		// Async texture loader. Worker thread decodes image bytes; Tick() (render thread)
		// uploads the decoded pixels to bgfx and fires callbacks.
		//
		// Thread contract:
		//   Load()       — any thread
		//   Unload()     — any thread (destruction deferred to render thread)
		//   Tick()       — render thread only
		//   Shutdown()   — render thread only
		//   LookupTexture() — any thread
		//
		// Callbacks (OnLoadComplete/OnLoadFailed) fire on the render thread during Tick().
		// Callbacks must NOT re-enter Load/Unload for the same assetId.
		class TextureHandler : public Dia::AssetRuntime::IAssetTypeHandler
		{
		public:
			TextureHandler();
			~TextureHandler();

			void SetJobSystem(Dia::Core::JobSystem* jobSystem);

			Dia::Graphics::ITexture* LookupTexture(const Dia::Core::StringCRC& assetId) const;

			unsigned int GetLoadedCount() const;

			virtual void Load(const Dia::Core::StringCRC& assetId,
			                  const Dia::Core::Containers::String512& resolvedPath,
			                  Dia::AssetRuntime::IAssetLoadCallback* callback) override;

			virtual void Unload(const Dia::Core::StringCRC& assetId) override;

			// Drains decoded images -> bgfx GPU upload + processes deferred deletions.
			// Must be called on the render thread each frame.
			void Tick();

			// Drain all pending work and destroy remaining textures.
			// Must be called on the render thread before bgfx shutdown.
			void Shutdown();

		private:
			void UnloadAll();
			void ProcessDeferredDeletions();

			mutable std::shared_mutex mMutex;
			std::unordered_map<unsigned int, Dia::Bgfx::BgfxTextureHandle*> mAssetIdToTexture;

			std::mutex mPendingUploadsMutex;
			std::vector<std::shared_ptr<PendingUpload>> mPendingUploads;

			std::mutex mDeferredDeleteMutex;
			std::vector<Dia::Bgfx::BgfxTextureHandle*> mDeferredDeletes;

			Dia::Core::JobSystem* mJobSystem = nullptr;
		};
	}
}
