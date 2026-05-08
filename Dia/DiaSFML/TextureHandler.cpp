#include "DiaSFML/TextureHandler.h"

#include <DiaCore/Core/Log.h>
#include <DiaCore/Memory/Memory.h>
#include <SFML/Graphics/Texture.hpp>

namespace Dia
{
	namespace SFML
	{
		TextureHandler::TextureHandler()
			: mNextId(1)
		{}

		TextureHandler::~TextureHandler()
		{
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
			std::unique_lock<std::shared_mutex> lock(mMutex);

			std::string pathStr(resolvedPath.AsCStr());
			auto pathIt = mPathToId.find(pathStr);
			unsigned int textureId = 0;

			if (pathIt != mPathToId.end())
			{
				textureId = pathIt->second;
			}
			else
			{
				sf::Texture* texture = DIA_NEW(sf::Texture());
				if (!texture->loadFromFile(resolvedPath.AsCStr()))
				{
					Dia::Core::Log::OutputVaradicLine("[ERROR][TextureHandler] Failed to load texture '%s'", resolvedPath.AsCStr());
					DIA_DELETE(texture);
					callback->OnLoadFailed(assetId, "failed to load texture file");
					return;
				}

				textureId = mNextId++;
				mPathToId[pathStr] = textureId;
				mIdToTexture[textureId] = texture;
			}

			mAssetToTextureId[assetId.Value()] = textureId;
			callback->OnLoadComplete(assetId);
		}

		void TextureHandler::Unload(const Dia::Core::StringCRC& assetId)
		{
			std::unique_lock<std::shared_mutex> lock(mMutex);
			auto it = mAssetToTextureId.find(assetId.Value());
			if (it != mAssetToTextureId.end())
			{
				unsigned int textureId = it->second;
				mAssetToTextureId.erase(it);

				// Check if any other asset still references this texture
				bool stillReferenced = false;
				for (const auto& pair : mAssetToTextureId)
				{
					if (pair.second == textureId)
					{
						stillReferenced = true;
						break;
					}
				}

				if (!stillReferenced)
				{
					auto texIt = mIdToTexture.find(textureId);
					if (texIt != mIdToTexture.end())
					{
						DIA_DELETE(texIt->second);
						mIdToTexture.erase(texIt);
					}
				}
			}
		}

		void TextureHandler::UnloadAll()
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
	}
}
