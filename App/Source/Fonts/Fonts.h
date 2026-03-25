#pragma once

#include <cstdint>
#include <imgui.h>

namespace Fonts
{
	extern const uint8_t MainFont[78948];

	extern const uint32_t FontAwesome[234008 / 4];

	inline ImFont* s_DefaultFont = nullptr;
	inline float s_DefaultFontSize = 19.0f;

}