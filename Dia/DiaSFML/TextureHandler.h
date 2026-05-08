#pragma once

#include <DiaAsset/IAssetTypeHandler.h>
#include <DiaCore/CRC/StringCRC.h>

#include <unordered_map>
#include <shared_mutex>
#include <string>

namespace sf
{
	class Texture;
}

namespace Dia
{
	namespace SFML
	{
		class TextureHandler : public Dia::AssetRuntime::IAssetTypeHandler
		{
		public:
			TextureHandler();
			~TextureHandler();

			unsigned int GetTextureId(const Dia::Core::StringCRC& assetId) const;
			const sf::Texture* GetTexture(unsigned int textureId) const;
			unsigned int GetLoadedCount() const;

			virtual void Load(const Dia::Core::StringCRC& assetId,
			                  const Dia::Core::Containers::String512& resolvedPath,
			                  Dia::AssetRuntime::IAssetLoadCallback* callback) override;

			virtual void Unload(const Dia::Core::StringCRC& assetId) override;

		private:
			void UnloadAll();

			mutable std::shared_mutex mMutex;
			std::unordered_map<unsigned int, unsigned int> mAssetToTextureId;
			std::unordered_map<std::string, unsigned int> mPathToId;
			std::unordered_map<unsigned int, sf::Texture*> mIdToTexture;
			unsigned int mNextId;
		};
	}
}
