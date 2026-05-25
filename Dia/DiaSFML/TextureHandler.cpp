#include "DiaSFML/TextureHandler.h"
#include "DiaSFML/SfmlTexture.h"

#include <DiaCore/Core/Log.h>
#include <DiaCore/Memory/Memory.h>
#include <DiaThreading/JobSystem.h>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>

// bgfx path — only included when the bgfx backend is active
#ifdef DIA_BGFX_ENABLED
#include <DiaBgfx/Resources/BgfxTextureHandle.h>
#endif

namespace Dia
{
	namespace SFML
	{
		bool TextureHandler::sBgfxActive = false;

		// Defined here so sf::Image is only pulled into DiaSFML, not into consumers
		// of TextureHandler.h that lack SFML in their include paths.
		struct PendingUpload
		{
			Dia::Core::StringCRC assetId;
			Dia::Core::Containers::String512 resolvedPath;
			sf::Image image;
			Dia::AssetRuntime::IAssetLoadCallback* callback = nullptr;
			Dia::Core::JobHandle job;
			bool success = false;
			const char* failureReason = nullptr;
		};

		TextureHandler::TextureHandler()
		{}

		TextureHandler::~TextureHandler()
		{
			Shutdown();
		}

		void TextureHandler::SetJobSystem(Dia::Core::JobSystem* jobSystem)
		{
			DIA_ASSERT(jobSystem != nullptr, "TextureHandler requires a valid JobSystem");
			mJobSystem = jobSystem;
		}

		void TextureHandler::Shutdown()
		{
			// All uploads are registered before Submit(), so swapping here catches every
			// in-flight job. Wait() on each ensures workers have exited their lambdas
			// before Job allocations and shared_ptrs are freed.
			std::vector<std::shared_ptr<PendingUpload>> pending;
			{
				std::lock_guard<std::mutex> lock(mPendingUploadsMutex);
				pending.swap(mPendingUploads);
			}
			for (auto& entry : pending)
			{
				if (entry->job.IsValid())
				{
					if (mJobSystem)
						mJobSystem->Wait(entry->job);
					entry->job = Dia::Core::JobHandle();
				}
			}

			UnloadAll();
		}

		Dia::Graphics::ITexture* TextureHandler::LookupTexture(const Dia::Core::StringCRC& assetId) const
		{
			std::shared_lock<std::shared_mutex> lock(mMutex);
			auto it = mAssetIdToTexture.find(assetId.Value());
			if (it != mAssetIdToTexture.end())
				return it->second;
			return nullptr;
		}

		unsigned int TextureHandler::GetLoadedCount() const
		{
			std::shared_lock<std::shared_mutex> lock(mMutex);
			return static_cast<unsigned int>(mAssetIdToTexture.size());
		}

		void TextureHandler::Load(const Dia::Core::StringCRC& assetId,
		                          const Dia::Core::Containers::String512& resolvedPath,
		                          Dia::AssetRuntime::IAssetLoadCallback* callback)
		{
			{
				std::shared_lock<std::shared_mutex> lock(mMutex);
				if (mAssetIdToTexture.find(assetId.Value()) != mAssetIdToTexture.end())
				{
					// Already loaded — fire callback synchronously
					lock.unlock();
					callback->OnLoadComplete(assetId);
					return;
				}
			}

			// Cache miss: submit async job for disk I/O.
			DIA_ASSERT(mJobSystem != nullptr, "TextureHandler::Load called before SetJobSystem");

			auto upload = std::make_shared<PendingUpload>();
			upload->assetId = assetId;
			upload->resolvedPath = resolvedPath;
			upload->callback = callback;
			upload->success = false;
			upload->failureReason = nullptr;

			upload->job = mJobSystem->Submit([upload]()
			{
				upload->success = upload->image.loadFromFile(upload->resolvedPath.AsCStr());
				if (!upload->success)
					upload->failureReason = "failed to load texture file";
			});

			{
				std::lock_guard<std::mutex> lock(mPendingUploadsMutex);
				mPendingUploads.push_back(upload);
			}
		}

		void TextureHandler::Tick()
		{
			std::vector<std::shared_ptr<PendingUpload>> toProcess;
			{
				std::lock_guard<std::mutex> lock(mPendingUploadsMutex);
				toProcess.swap(mPendingUploads);
			}

			for (auto& entry : toProcess)
			{
				if (entry->job.IsValid())
				{
					if (mJobSystem)
						mJobSystem->Wait(entry->job);
					entry->job = Dia::Core::JobHandle();
				}

				if (!entry->success)
				{
					entry->callback->OnLoadFailed(entry->assetId, entry->failureReason);
					continue;
				}

				{
					std::unique_lock<std::shared_mutex> lock(mMutex);

					// Check if this asset was already registered by a concurrent job
					auto it = mAssetIdToTexture.find(entry->assetId.Value());
					if (it == mAssetIdToTexture.end())
					{
#ifdef DIA_BGFX_ENABLED
						if (sBgfxActive)
						{
							Dia::Bgfx::BgfxTextureHandle* bgfxTex =
								DIA_NEW(Dia::Bgfx::BgfxTextureHandle(entry->assetId));
							const sf::Vector2u sz = entry->image.getSize();
							if (!bgfxTex->UploadFromMemory(
									entry->image.getPixelsPtr(), sz.x, sz.y))
							{
								DIA_DELETE(bgfxTex);
								lock.unlock();
								entry->callback->OnLoadFailed(entry->assetId, "bgfx texture upload failed");
								continue;
							}
							mAssetIdToTexture[entry->assetId.Value()] = bgfxTex;
						}
						else
#endif
						{
							SfmlTexture* sfTex = DIA_NEW(SfmlTexture(entry->assetId));
							if (!sfTex->UploadFromImage(entry->image))
							{
								DIA_DELETE(sfTex);
								lock.unlock();
								entry->callback->OnLoadFailed(entry->assetId, "failed to upload texture to GPU");
								continue;
							}
							mAssetIdToTexture[entry->assetId.Value()] = sfTex;
						}
					}
				}

				entry->callback->OnLoadComplete(entry->assetId);
			}
		}

		void TextureHandler::Unload(const Dia::Core::StringCRC& assetId)
		{
			SfmlTexture* toDelete = nullptr;
			{
				std::unique_lock<std::shared_mutex> lock(mMutex);
				auto it = mAssetIdToTexture.find(assetId.Value());
				if (it == mAssetIdToTexture.end())
					return;

				toDelete = it->second;
				mAssetIdToTexture.erase(it);
			}

			if (toDelete)
			{
				std::lock_guard<std::mutex> delLock(mPendingDeletionsMutex);
				mPendingDeletions.push_back(toDelete);
			}
		}

		void TextureHandler::ProcessGpuDeletions()
		{
			std::vector<SfmlTexture*> toDelete;
			{
				std::lock_guard<std::mutex> lock(mPendingDeletionsMutex);
				toDelete.swap(mPendingDeletions);
			}
			for (SfmlTexture* tex : toDelete)
			{
				DIA_DELETE(tex);
			}
		}

		void TextureHandler::UnloadAll()
		{
			{
				std::unique_lock<std::shared_mutex> lock(mMutex);
				for (auto& entry : mAssetIdToTexture)
				{
					DIA_DELETE(entry.second);
				}
				mAssetIdToTexture.clear();
			}

			std::vector<SfmlTexture*> toDelete;
			{
				std::lock_guard<std::mutex> lock(mPendingDeletionsMutex);
				toDelete.swap(mPendingDeletions);
			}
			for (SfmlTexture* tex : toDelete)
			{
				DIA_DELETE(tex);
			}
		}
	}
}
