#include "Keyword.h"
#include "../Exception/ArCalcException.h"

namespace ArCalc {
	std::vector<KeywordInfo> Keyword::s_KeywordMap{
		{"_Set",    KT::Set},
		{"_Last",   KT::Last},
		{"_List",   KT::List},
		{"_Func",   KT::Func},
		{"_Return", KT::Return},
		{"_If",     KT::If},
		{"_Else",   KT::Else},
		{"_Elif",   KT::Elif},
		{"_Return", KT::Return},
		{"_Save",   KT::Save},
		{"_Load",   KT::Load},
	};

	std::optional<Keyword::KT> Keyword::FromString(std::string_view glyph) {
		auto const it{range::find(s_KeywordMap, std::string{glyph}, &KeywordInfo::Glyph)};
		return it != s_KeywordMap.end() ? it->Type : std::optional<KT>{};
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

	std::string Keyword::ToString(KT type) {
		auto const it{range::find(s_KeywordMap, type, &KeywordInfo::Type)};
		return it != s_KeywordMap.end() ? it->Glyph : "";
	}

	KeywordInfo const& Keyword::Get(KT type) {
		if (auto const it{range::find(s_KeywordMap, type, &KeywordInfo::Type)};
			it != s_KeywordMap.end())
		{
			return *it;
		}
		else throw ArCalcException{"Getting invalid keyword [{}]", type};
	}

	std::vector<KeywordType> Keyword::GetAllKeywordTypes() {
		static std::vector<KeywordType> res{};

		if (res.empty()) { // Only happens the first time, otherwise the function is constant time.
			res.reserve(s_KeywordMap.size());
			std::ranges::transform(s_KeywordMap, std::back_inserter(res), &KeywordInfo::Type);
		}
		
		return res;
	}
}