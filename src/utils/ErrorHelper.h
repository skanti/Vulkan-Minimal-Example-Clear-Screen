#pragma once

#include <iostream>
#include <vector>

#include <vulkan/vulkan.h>

namespace ct {
	namespace error {
		inline static void exit(std::string message, int32_t exitcode) {
			std::cerr << message << std::endl;
#if !defined(__ANDROID__)
			std::exit(exitcode);
#endif
		}

	}

}
