#pragma once

#include "Core.h"
#include "KeywordType.h"

namespace ArCalc {
	struct KeywordInfo {
		std::string Glyph;
		KeywordType Type;
	};

	class Keyword {
	private:
		// Used by ToString for invalid inputs.
		inline static const std::string sc_EmptyString{};

	private:
		using KT = KeywordType;
		static std::vector<KeywordInfo> s_KeywordMap;

	public:
		static std::optional<KT> FromString(std::string_view glyph);
		static std::string const& ToString(KT type);
		static bool IsValid(std::string_view glyph);
		static void Add(std::string_view glyph, KT type);
	};
}

namespace std {
	template <> 
	struct formatter<ArCalc::KeywordType, char> : formatter<string> {
		using Base = formatter<string>;
		using Base::parse;
		auto format(ArCalc::KeywordType kt, format_context& fc) const {
			return Base::format(ArCalc::Keyword::ToString(kt), fc);
		}
	};
}