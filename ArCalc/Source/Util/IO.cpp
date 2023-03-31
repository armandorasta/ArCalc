#include "IO.h"
#include "Exception/ArCalcException.h"

namespace ArCalc::IO {
	std::string GetLine(std::istream& is) {
		std::string line{};
		std::getline(is, line);

		return line;
	}

	std::string GetLineStd() {
		return GetLine(std::cin);
	}

	std::string Read(std::istream& is, size_t byteCount) {
		std::string res{};
		res.resize(std::min(byteCount, IStreamSize(is)));
		is.read(res.data(), res.size());
		return res;
	}

	std::string FileToString(fs::path const& filePath, bool bKeepTrailingNulls) {
		std::ifstream file{filePath};
		if (!file.is_open()) {
			throw IOError{"Tried to convert a non-existant file [{}] to string", filePath.string()};
		}
		return IStreamToString(file, bKeepTrailingNulls);
	}

	size_t IStreamSize(std::istream& is) {
		auto const cursorLoc{is.tellg()};
		is.seekg(0U, std::ios::end);
		auto const fileSize{is.tellg()};
		is.seekg(cursorLoc);
		return fileSize;
	}

	std::string IStreamToString(std::istream& is, bool bKeepTrailingNulls) {
		return IStreamToContainer<std::string>(is, bKeepTrailingNulls);
	}

	fs::path GetSerializationPath() {
		return fs::path{"C:"} / "Users" / "USER" / "Documents" / "ArCalc Saves";
	}
}