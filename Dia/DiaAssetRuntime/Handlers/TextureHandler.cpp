#include "DiaAssetRuntime/Handlers/TextureHandler.h"

#include <DiaBgfx/Resources/BgfxTextureHandle.h>
#include <DiaCore/Core/Log.h>
#include <DiaCore/Memory/Memory.h>
#include <DiaThreading/JobSystem.h>

#include <fstream>
#include <vector>

namespace Dia
{
	namespace AssetRuntime
	{
		struct PendingUpload
		{
			Dia::Core::StringCRC assetId;
			Dia::Core::Containers::String512 resolvedPath;
			std::vector<uint8_t> fileBytes;
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

		Dia::Graphics::ITexture* TextureHandler::LookupTexture(const Dia::Core::StringCRC& assetId) const
		{
			std::shared_lock<std::shared_mutex> lock(mMutex);
			auto it = mAssetIdToTexture.find(assetId.Value());
			return (it != mAssetIdToTexture.end()) ? it->second : nullptr;
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
					lock.unlock();
					callback->OnLoadComplete(assetId);
					return;
				}
			}

			DIA_ASSERT(mJobSystem != nullptr, "TextureHandler::Load called before SetJobSystem");

			auto upload = std::make_shared<PendingUpload>();
			upload->assetId = assetId;
			upload->resolvedPath = resolvedPath;
			upload->callback = callback;

			upload->job = mJobSystem->Submit([upload]()
			{
				std::ifstream file(upload->resolvedPath.AsCStr(), std::ios::binary | std::ios::ate);
				if (!file.is_open())
				{
					upload->success = false;
					upload->failureReason = "failed to open texture file";
					return;
				}
				std::streamsize size = file.tellg();
				file.seekg(0, std::ios::beg);
				upload->fileBytes.resize(static_cast<size_t>(size));
				if (!file.read(reinterpret_cast<char*>(upload->fileBytes.data()), size))
				{
					upload->success = false;
					upload->failureReason = "failed to read texture file";
					return;
				}
				upload->success = true;
			});

			{
				std::lock_guard<std::mutex> lock(mPendingUploadsMutex);
				mPendingUploads.push_back(upload);
			}
		}

		void TextureHandler::Tick()
		{
			ProcessDeferredDeletions();

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

				bool uploaded = false;
				const char* failReason = nullptr;
				{
					std::unique_lock<std::shared_mutex> lock(mMutex);

					auto it = mAssetIdToTexture.find(entry->assetId.Value());
					if (it != mAssetIdToTexture.end())
					{
						uploaded = true;
					}
					else
					{
						Dia::Bgfx::BgfxTextureHandle* tex =
							DIA_NEW(Dia::Bgfx::BgfxTextureHandle(entry->assetId));

						if (!tex->UploadFromEncodedMemory(
								entry->fileBytes.data(),
								static_cast<unsigned int>(entry->fileBytes.size()),
								&failReason))
						{
							DIA_DELETE(tex);
						}
						else
						{
							mAssetIdToTexture[entry->assetId.Value()] = tex;
							uploaded = true;
						}
					}
				}

				if (uploaded)
					entry->callback->OnLoadComplete(entry->assetId);
				else
					entry->callback->OnLoadFailed(entry->assetId, failReason ? failReason : "texture upload failed");
			}
		}

		void TextureHandler::Unload(const Dia::Core::StringCRC& assetId)
		{
			Dia::Bgfx::BgfxTextureHandle* toDelete = nullptr;
			{
				std::unique_lock<std::shared_mutex> lock(mMutex);
				auto it = mAssetIdToTexture.find(assetId.Value());
				if (it == mAssetIdToTexture.end())
					return;
				toDelete = it->second;
				mAssetIdToTexture.erase(it);
			}

			std::lock_guard<std::mutex> lock(mDeferredDeleteMutex);
			mDeferredDeletes.push_back(toDelete);
		}

		void TextureHandler::ProcessDeferredDeletions()
		{
			std::vector<Dia::Bgfx::BgfxTextureHandle*> toDelete;
			{
				std::lock_guard<std::mutex> lock(mDeferredDeleteMutex);
				toDelete.swap(mDeferredDeletes);
			}
			for (auto* tex : toDelete)
			{
				delete tex;
			}
		}

		void TextureHandler::Shutdown()
		{
			std::vector<std::shared_ptr<PendingUpload>> pending;
			{
				std::lock_guard<std::mutex> lock(mPendingUploadsMutex);
				pending.swap(mPendingUploads);
			}
			for (auto& entry : pending)
			{
				if (entry->job.IsValid() && mJobSystem)
				{
					mJobSystem->Wait(entry->job);
					entry->job = Dia::Core::JobHandle();
				}
			}
			ProcessDeferredDeletions();
			UnloadAll();
		}

		void TextureHandler::UnloadAll()
		{
			std::unique_lock<std::shared_mutex> lock(mMutex);
			for (auto& entry : mAssetIdToTexture)
			{
				delete entry.second;
				entry.second = nullptr;
			}
			mAssetIdToTexture.clear();
		}
	}
}
