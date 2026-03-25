#pragma once

#include <filesystem>

#include "Core/Renderer/Texture.h"

namespace Renderer {

	struct MSDFData;

	class Font
	{
	public:
		Font(const std::filesystem::path& font);
		~Font();

		const MSDFData* GetMSDFData() const { return m_Data; }
		std::shared_ptr<Texture2D> GetAtlasTexture() const { return m_AtlasTexture; }

		static std::shared_ptr<Font> GetDefault();
	private:
		MSDFData* m_Data;
		std::shared_ptr<Texture2D> m_AtlasTexture;
	};

}
