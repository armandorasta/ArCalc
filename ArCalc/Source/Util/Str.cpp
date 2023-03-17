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

	std::string Mangle(std::string_view paramName, size_t paramIndex) {
		return std::format("__{}__{}__", paramName, paramIndex);
	}
}