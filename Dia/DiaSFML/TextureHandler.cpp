#include "DiaSFML/TextureHandler.h"

#include <DiaCore/Core/Log.h>
#include <DiaCore/Memory/Memory.h>
#include <DiaCore/Threading/JobSystem.h>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>

namespace Dia
{
	namespace SFML
	{
		// Defined here so sf::Image is only pulled into DiaSFML, not into consumers
		// of TextureHandler.h that lack SFML in their include paths.
		struct PendingUpload
		{
			Dia::Core::StringCRC assetId;
			Dia::Core::Containers::String512 resolvedPath;
			sf::Image image;
			Dia::AssetRuntime::IAssetLoadCallback* callback = nullptr;
			Dia::Core::Job* job = nullptr;
			bool success = false;
			const char* failureReason = nullptr;
		};

		TextureHandler::TextureHandler()
			: mNextId(1)
		{}

		TextureHandler::~TextureHandler()
		{
			Shutdown();
		}

		void TextureHandler::Shutdown()
		{
			// All uploads are registered before Run(), so swapping here catches every
			// in-flight job. Wait() on each ensures workers have exited their lambdas
			// before Job allocations and shared_ptrs are freed.
			std::vector<std::shared_ptr<PendingUpload>> pending;
			{
				std::lock_guard<std::mutex> lock(mPendingUploadsMutex);
				pending.swap(mPendingUploads);
			}
			for (auto& entry : pending)
			{
				if (entry->job)
				{
					Dia::Core::JobSystem::Wait(entry->job);
					entry->job = nullptr;
				}
			}

			UnloadAll();
		}

		unsigned int TextureHandler::GetTextureId(const Dia::Core::StringCRC& assetId) const
		{
			std::shared_lock<std::shared_mutex> lock(mMutex);
			auto it = mAssetToTextureId.find(assetId.Value());
			if (it != mAssetToTextureId.end())
				return it->second;
			return 0;
		}

		const sf::Texture* TextureHandler::GetTexture(unsigned int textureId) const
		{
			std::shared_lock<std::shared_mutex> lock(mMutex);
			auto it = mIdToTexture.find(textureId);
			if (it != mIdToTexture.end())
				return it->second;
			return nullptr;
		}

		unsigned int TextureHandler::GetLoadedCount() const
		{
			std::shared_lock<std::shared_mutex> lock(mMutex);
			return static_cast<unsigned int>(mIdToTexture.size());
		}

		void TextureHandler::Load(const Dia::Core::StringCRC& assetId,
		                          const Dia::Core::Containers::String512& resolvedPath,
		                          Dia::AssetRuntime::IAssetLoadCallback* callback)
		{
			std::string pathStr(resolvedPath.AsCStr());

			{
				std::unique_lock<std::shared_mutex> lock(mMutex);

				auto pathIt = mPathToId.find(pathStr);
				if (pathIt != mPathToId.end())
				{
					// Cache hit: texture already loaded — register mapping and fire callback synchronously
					mAssetToTextureId[assetId.Value()] = pathIt->second;
					lock.unlock();
					callback->OnLoadComplete(assetId);
					return;
				}
			}

			// Cache miss: submit async job for disk I/O.
			// Register in mPendingUploads BEFORE Run() so the destructor can wait on
			// any job that is mid-decode when TextureHandler is torn down. The lambda
			// captures only the shared_ptr — no 'this' — so it is safe even if
			// TextureHandler is destroyed while the decode is still running.
			auto upload = std::make_shared<PendingUpload>();
			upload->assetId = assetId;
			upload->resolvedPath = resolvedPath;
			upload->callback = callback;
			upload->success = false;
			upload->failureReason = nullptr;

			Dia::Core::Job* job = Dia::Core::JobSystem::CreateJob([upload](Dia::Core::Job*)
			{
				upload->success = upload->image.loadFromFile(upload->resolvedPath.AsCStr());
				if (!upload->success)
					upload->failureReason = "failed to load texture file";
			});
			upload->job = job;

			{
				std::lock_guard<std::mutex> lock(mPendingUploadsMutex);
				mPendingUploads.push_back(upload);
			}
			Dia::Core::JobSystem::Run(job);
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
				// Wait() blocks until the job is finished, then deletes the job allocation
				if (entry->job)
				{
					Dia::Core::JobSystem::Wait(entry->job);
					entry->job = nullptr;
				}

				if (!entry->success)
				{
					entry->callback->OnLoadFailed(entry->assetId, entry->failureReason);
					continue;
				}

				unsigned int textureId = 0;
				{
					std::unique_lock<std::shared_mutex> lock(mMutex);

					std::string pathStr(entry->resolvedPath.AsCStr());
					auto pathIt = mPathToId.find(pathStr);
					if (pathIt != mPathToId.end() && pathIt->second != 0)
					{
						// Another job already uploaded this path — reuse existing textureId
						textureId = pathIt->second;
					}
					else
					{
						// Upload decoded image to GPU on the main thread
						sf::Texture* texture = DIA_NEW(sf::Texture());
						if (!texture->loadFromImage(entry->image))
						{
							DIA_DELETE(texture);
							lock.unlock();
							entry->callback->OnLoadFailed(entry->assetId, "failed to upload texture to GPU");
							continue;
						}
						textureId = mNextId++;
						mPathToId[pathStr] = textureId;
						mIdToTexture[textureId] = texture;
					}

					mAssetToTextureId[entry->assetId.Value()] = textureId;
				}

				entry->callback->OnLoadComplete(entry->assetId);
			}
		}

		void TextureHandler::Unload(const Dia::Core::StringCRC& assetId)
		{
			// Detach the asset->texture mapping under mMutex (any thread).
			// If this was the last reference to the texture, transfer ownership
			// of the sf::Texture* to the deferred-deletion queue — destruction
			// must run on the GL-context thread (see ProcessGpuDeletions).
			sf::Texture* toDelete = nullptr;
			{
				std::unique_lock<std::shared_mutex> lock(mMutex);
				auto it = mAssetToTextureId.find(assetId.Value());
				if (it == mAssetToTextureId.end())
					return;

				unsigned int textureId = it->second;
				mAssetToTextureId.erase(it);

				bool stillReferenced = false;
				for (const auto& pair : mAssetToTextureId)
				{
					if (pair.second == textureId)
					{
						stillReferenced = true;
						break;
					}
				}

				if (stillReferenced)
					return;

				auto texIt = mIdToTexture.find(textureId);
				if (texIt != mIdToTexture.end())
				{
					toDelete = texIt->second;
					mIdToTexture.erase(texIt);
				}

				// Remove the path→textureId cache so re-loads go through the
				// full async path rather than getting a dead textureId.
				for (auto pathIt = mPathToId.begin(); pathIt != mPathToId.end(); ++pathIt)
				{
					if (pathIt->second == textureId)
					{
						mPathToId.erase(pathIt);
						break;
					}
				}
			}

			if (toDelete)
			{
				std::lock_guard<std::mutex> delLock(mPendingDeletionsMutex);
				mPendingDeletions.push_back(toDelete);
			}
		}

		void TextureHandler::ProcessGpuDeletions()
		{
			// Caller must own the GL context. Swap-drain so deletes run outside
			// the lock (sf::Texture destruction can be slow and we don't want to
			// block Unload callers on the GL thread).
			std::vector<sf::Texture*> toDelete;
			{
				std::lock_guard<std::mutex> lock(mPendingDeletionsMutex);
				toDelete.swap(mPendingDeletions);
			}
			for (sf::Texture* tex : toDelete)
			{
				DIA_DELETE(tex);
			}
		}

		void TextureHandler::UnloadAll()
		{
			// Last-chance teardown. Caller (Shutdown) holds the GL context,
			// so any textures still in the live map plus anything queued for
			// deletion can be destroyed inline.
			{
				std::unique_lock<std::shared_mutex> lock(mMutex);
				for (auto& entry : mIdToTexture)
				{
					DIA_DELETE(entry.second);
				}
				mIdToTexture.clear();
				mPathToId.clear();
				mAssetToTextureId.clear();
			}

			std::vector<sf::Texture*> toDelete;
			{
				std::lock_guard<std::mutex> lock(mPendingDeletionsMutex);
				toDelete.swap(mPendingDeletions);
			}
			for (sf::Texture* tex : toDelete)
			{
				DIA_DELETE(tex);
			}
		}
	}
}
