#include "Image.h"

#include <glad/gl.h>
#include <cstring>
#include <fstream>

#include "stb_image.h"

namespace Core {

	namespace Utils {

		static uint32_t BytesPerPixel(ImageFormat format)
		{
			switch (format)
			{
			case ImageFormat::RGBA:    return 4;
			case ImageFormat::RGBA32F: return 16;
			default: return 0;
			}
		}

		static GLenum ToGLInternalFormat(ImageFormat format)
		{
			switch (format)
			{
			case ImageFormat::RGBA:    return GL_RGBA8;
			case ImageFormat::RGBA32F: return GL_RGBA32F;
			default: return GL_RGBA8;
			}
		}

		static GLenum ToGLType(ImageFormat format)
		{
			switch (format)
			{
			case ImageFormat::RGBA:    return GL_UNSIGNED_BYTE;
			case ImageFormat::RGBA32F: return GL_FLOAT;
			default: return GL_UNSIGNED_BYTE;
			}
		}

	}

	Image::Image(const std::filesystem::path& path)
		: m_Filepath(path)
	{
		if (!std::filesystem::exists(m_Filepath))
			throw std::runtime_error("Image file does not exist: " + m_Filepath.string());

		std::ifstream file(m_Filepath, std::ios::binary | std::ios::ate);
		if (!file)
			throw std::runtime_error("Failed to open image file: " + m_Filepath.string());

		const std::streamsize size = file.tellg();
		if (size <= 0)
			throw std::runtime_error("Image file is empty: " + m_Filepath.string());

		file.seekg(0, std::ios::beg);

		std::vector<unsigned char> buffer(static_cast<size_t>(size));
		if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
			throw std::runtime_error("Failed to read image file: " + m_Filepath.string());

		int width = 0, height = 0, channels = 0;
		void* pixels = nullptr;

		if (stbi_is_hdr_from_memory(buffer.data(), static_cast<int>(buffer.size())))
		{
			float* data = stbi_loadf_from_memory(buffer.data(), static_cast<int>(buffer.size()),
				&width, &height, &channels, 4);
			if (!data)
				throw std::runtime_error(std::string("stbi_loadf_from_memory failed: ") + stbi_failure_reason());
			pixels = static_cast<void*>(data);
			m_Format = ImageFormat::RGBA32F;
		}
		else
		{
			stbi_uc* data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()),
				&width, &height, &channels, 4);
			if (!data)
				throw std::runtime_error(std::string("stbi_load_from_memory failed: ") + stbi_failure_reason());
			pixels = static_cast<void*>(data);
			m_Format = ImageFormat::RGBA;
		}

		m_Width = static_cast<uint32_t>(width);
		m_Height = static_cast<uint32_t>(height);

		Allocate();
		SetData(pixels);
		stbi_image_free(pixels);
	}

	Image::Image(uint32_t width, uint32_t height, ImageFormat format, const void* data)
		: m_Width(width), m_Height(height), m_Format(format)
	{
		Allocate();
		if (data)
			SetData(data);
	}

	Image::~Image()
	{
		Release();
	}

	void Image::Allocate()
	{
		if (m_Texture == 0)
			glGenTextures(1, &m_Texture);

		glBindTexture(GL_TEXTURE_2D, m_Texture);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		const GLenum internalFormat = Utils::ToGLInternalFormat(m_Format);
		const GLenum type = Utils::ToGLType(m_Format);

		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, (GLsizei)m_Width, (GLsizei)m_Height, 0, GL_RGBA, type, nullptr);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void Image::Release()
	{
		if (m_Texture)
		{
			glDeleteTextures(1, &m_Texture);
			m_Texture = 0;
		}
	}

	void Image::SetData(const void* data)
	{
		if (!m_Texture)
			Allocate();

		glBindTexture(GL_TEXTURE_2D, m_Texture);

		const GLenum type = Utils::ToGLType(m_Format);

		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)m_Width, (GLsizei)m_Height, GL_RGBA, type, data);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void Image::Resize(uint32_t width, uint32_t height)
	{
		if (m_Width == width && m_Height == height)
			return;

		m_Width = width;
		m_Height = height;

		if (!m_Texture)
		{
			Allocate();
			return;
		}

		glBindTexture(GL_TEXTURE_2D, m_Texture);
		const GLenum internalFormat = Utils::ToGLInternalFormat(m_Format);
		const GLenum type = Utils::ToGLType(m_Format);
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, (GLsizei)m_Width, (GLsizei)m_Height, 0, GL_RGBA, type, nullptr);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void* Image::Decode(const void* buffer, uint64_t length, uint32_t& outWidth, uint32_t& outHeight)
	{
		int width, height, channels;
		stbi_uc* data = stbi_load_from_memory((const stbi_uc*)buffer, (int)length, &width, &height, &channels, 4);

		outWidth = (uint32_t)width;
		outHeight = (uint32_t)height;

		return data;
	}

	void Image::FreeDecoded(void* data)
	{
		stbi_image_free(data);
	}

}