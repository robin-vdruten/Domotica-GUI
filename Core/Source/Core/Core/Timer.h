#pragma once

#include <chrono>

namespace Core {

	class Timer
	{
	public:
		Timer()
		{
			Reset();
		}

		inline void Reset()
		{
			m_Start = std::chrono::high_resolution_clock::now();
		}

		inline float Elapsed() const
		{
			return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - m_Start).count() * 0.001f * 0.001f * 0.001f;
		}

		inline float ElapsedMillis() const
		{
			return Elapsed() * 1000.0f;
		}

	private:
		std::chrono::time_point<std::chrono::high_resolution_clock> m_Start;
	};

}
