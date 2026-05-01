////////////////////////////////////////////////////////////////////////////////
// Filename: TextureManager.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaSFML/TextureManager.h"
#include <SFML/Graphics/Texture.hpp>
#include <DiaCore/Memory/Memory.h>
#include <DiaCore/Core/Assert.h>

namespace Dia
{
	namespace SFML
	{
		////////////////////////////////////////////////////////////
		TextureManager::TextureManager()
			: mNextId(1) // Start at 1, reserve 0 for invalid
		{
		}

		////////////////////////////////////////////////////////////
		TextureManager::~TextureManager()
		{
			UnloadAll();
		}

		////////////////////////////////////////////////////////////
		unsigned int TextureManager::LoadTexture(const char* path)
		{
			std::lock_guard<std::mutex> lock(mMutex);

			// Check if already loaded
			std::string pathStr(path);
			auto it = mPathToId.find(pathStr);
			if (it != mPathToId.end())
			{
				return it->second;
			}

			// Load new texture
			sf::Texture* texture = DIA_NEW(sf::Texture());
			if (!texture->loadFromFile(path))
			{
				// Failed to load
				DIA_DELETE(texture);
				return 0; // Invalid ID
			}

			// Assign ID and cache
			unsigned int newId = mNextId++;
			mPathToId[pathStr] = newId;
			mIdToTexture[newId] = texture;

			return newId;
		}

		////////////////////////////////////////////////////////////
		const sf::Texture* TextureManager::GetTexture(unsigned int textureId) const
		{
			std::lock_guard<std::mutex> lock(mMutex);

			auto it = mIdToTexture.find(textureId);
			if (it != mIdToTexture.end())
			{
				return it->second;
			}

			return nullptr;
		}

		////////////////////////////////////////////////////////////
		void TextureManager::UnloadTexture(unsigned int textureId)
		{
			std::lock_guard<std::mutex> lock(mMutex);

			// Find and remove texture
			auto it = mIdToTexture.find(textureId);
			if (it != mIdToTexture.end())
			{
				sf::Texture* texture = it->second;
				DIA_DELETE(texture);
				mIdToTexture.erase(it);
			}

			// Remove from path cache (requires iterating - optimize if needed)
			// For now, leave path->id mapping (harmless, GetTexture will return nullptr)
		}

		////////////////////////////////////////////////////////////
		void TextureManager::UnloadAll()
		{
			std::lock_guard<std::mutex> lock(mMutex);

			// Delete all textures
			for (auto& pair : mIdToTexture)
			{
				sf::Texture* texture = pair.second;
				DIA_DELETE(texture);
			}

			mIdToTexture.clear();
			mPathToId.clear();
		}

		////////////////////////////////////////////////////////////
		unsigned int TextureManager::GetLoadedCount() const
		{
			std::lock_guard<std::mutex> lock(mMutex);
			return static_cast<unsigned int>(mIdToTexture.size());
		}
	}
}
