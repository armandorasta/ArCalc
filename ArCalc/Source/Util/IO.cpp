#include "IO.h"

namespace ArCalc::IO {
	std::string GetLine(std::istream& is) {
		std::string line{};
		std::getline(is, line);

		return line;
	}

	std::string GetLineStd() {
		return GetLine(std::cin);
	}
}