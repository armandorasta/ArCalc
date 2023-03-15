#include "Keyword.h"

namespace ArCalc {
	std::vector<KeywordInfo> Keyword::s_KeywordMap{
		{"_Set",	KT::Set},
		{"_Last",	KT::Last},
		{"_List",	KT::List},
		{"_Func",	KT::Func},
		{"_Return", KT::Return},
		{"_If",		KT::If},
		{"_Else",	KT::Else},
		{"_Elif",	KT::Elif},
	};

	std::optional<Keyword::KT> Keyword::FromString(std::string_view glyph) {
		auto const it{range::find(s_KeywordMap, std::string{glyph}, &KeywordInfo::Glyph)};
		if (it != s_KeywordMap.end()) {
			return {it->Type};
		}
		else return {};
	}

	bool Keyword::IsValid(std::string_view glyph) {
		auto const it{range::find(s_KeywordMap, std::string{glyph}, &KeywordInfo::Glyph)};
		return it != s_KeywordMap.end();
	}

	void Keyword::Add(std::string_view glyph, KT type) {
		KeywordInfo info{
			.Glyph = std::string{glyph},
			.Type{std::move(type)},
		};
		s_KeywordMap.push_back(std::move(info));
	}

	std::string const& Keyword::ToString(KT type) {
		auto const it{range::find(s_KeywordMap, type, &KeywordInfo::Type)};
		if (it != s_KeywordMap.end()) {
			return {it->Glyph};
		}
		else return sc_EmptyString;
	}
}