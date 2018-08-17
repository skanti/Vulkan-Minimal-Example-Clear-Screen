#pragma once

#include <fstream>
#include <string>

namespace ct {
	inline void load_binary(std::string filename, std::vector<char> &content) {

		size_t size;

#if defined(__ANDROID__)
		// Load shader from compressed asset
		AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, filename.c_str(), AASSET_MODE_STREAMING);
		assert(asset);
		size = AAsset_getLength(asset);
		content.resize(size);

		AAsset_read(asset, content.data(), size);
		AAsset_close(asset);
#else
		std::ifstream is(filename, std::ios::binary | std::ios::in | std::ios::ate);

		if (is.is_open()) {
			size = is.tellg();
			is.seekg(0, std::ios::beg);
			// Copy file contents into a buffer
			content.resize(size);
			is.read(content.data(), size);
			is.close();
			assert(size > 0);
		}
#endif
	}

}
