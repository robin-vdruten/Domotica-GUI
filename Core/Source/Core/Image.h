#pragma once

#include <string>
#include <filesystem>
#include <cstdint>

namespace Core {

	enum class ImageFormat
	{
		None = 0,
		RGBA,
		RGBA32F
	};

	class Image
	{
	public:
		Image(const std::filesystem::path& path);
		Image(uint32_t width, uint32_t height, ImageFormat format, const void* data = nullptr);
		~Image();

		void SetData(const void* data);
		void Resize(uint32_t width, uint32_t height);

		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }

		uint32_t GetTextureID() const { return m_Texture; }

		static void* Decode(const void* data, uint64_t length, uint32_t& outWidth, uint32_t& outHeight);
		static void  FreeDecoded(void* data);

	private:
		void Allocate();
		void Release();

	private:
		uint32_t m_Width = 0, m_Height = 0;
		uint32_t m_Texture = 0;

		ImageFormat m_Format = ImageFormat::None;

		std::filesystem::path m_Filepath;
	};

}