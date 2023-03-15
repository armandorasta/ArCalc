#include "Str.h"

namespace ArCalc::Str {
	namespace Secret {
		IndexPair GetFirstTokenIndices(std::string_view line) {
			// Skip the spaces before the token.
			IndexPair res{};
			while (res.StartIndex < line.length() && std::isspace(line[res.StartIndex])) {
				res.StartIndex += 1;
			}

			// Go until end of token.
			res.EndIndex = res.StartIndex;
			while (res.EndIndex < line.length() && !std::isspace(line[res.EndIndex])) {
				res.EndIndex += 1;
			}

			return res;
		}
	}

	std::string FileToString(fs::path const& filePath) {
		std::ifstream file{filePath};
		ARCALC_EXPECT(file.is_open(), "Tried to parse non-existant file [{}]", filePath.string());

		auto const fileSize{fs::file_size(filePath)};
		if (!fileSize) { // File is empty? an empty string will do.
			return {};
		}

		auto bytes = std::string{};
		bytes.resize(fileSize);
		file.read(bytes.data(), bytes.size());

		// Chop-off the null characters at the end
		for (size_t i : view::iota(0U, bytes.size()) | view::reverse) {
			if (bytes[i] != '\0') {
				bytes = bytes.substr(0, i + 1);
				break;
			}
		}

		return bytes;
	}

	std::string Mangle(std::string_view paramName, size_t paramIndex) {
		return std::format("__{}__{}__", paramName, paramIndex);
	}
}