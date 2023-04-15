#pragma once

#include "Core.h"
#include "Exception/ArCalcException.h"
#include "KeywordType.h"

namespace ArCalc {
	struct KeywordInfo {
		consteval KeywordInfo(std::string_view glyph, KeywordType type)
			: Glyph{glyph}, Type{type}
		{
		}

		std::string_view Glyph;
		KeywordType Type;
	};

	class Keyword {
	private:
		// Used m_By ToString for invalid inputs.
		inline static const std::string sc_EmptyString{};

	private:
		using KT = KeywordType;

	private:
		constexpr static auto sc_KeywordMapSize{static_cast<std::underlying_type_t<KT>>(KT::Set) + 1};
		constexpr static std::array<KeywordInfo, sc_KeywordMapSize> sc_KeywordMap{{
			{ "_List"    ,  KT::List    },
			{ "_Func"    ,  KT::Func    },
			{ "_Return"  ,  KT::Return  },
			{ "_Last"    ,  KT::Last    },
			{ "_If"      ,  KT::If      },
			{ "_Elif"    ,  KT::Elif    },
			{ "_Else"    ,  KT::Else    },
			{ "_Save"    ,  KT::Save    },
			{ "_Load"    ,  KT::Load    },
			{ "_Unscope" ,  KT::Unscope },
			{ "_Err"     ,  KT::Err     },
			{ "_Sum"     ,  KT::Sum     },
			{ "_Mul"     ,  KT::Mul     },
			{ "_Set"     ,  KT::Set     },
		}};

	private:
		/// Contains an iterator pair can be used to iterate through all keywords.
		struct GetAllKeywordTypesResult {
			decltype(sc_KeywordMap)::const_iterator Begin;
			decltype(sc_KeywordMap)::const_iterator End;
		};

	public:
		constexpr static std::optional<KT> FromString(std::string_view glyph) {
			auto const it{range::find(sc_KeywordMap, glyph, &KeywordInfo::Glyph)};
			return it != sc_KeywordMap.end() ? it->Type : std::optional<KT>{};
		}

		constexpr static std::string_view ToStringView(KT type) {
			auto const it{range::find(sc_KeywordMap, type, &KeywordInfo::Type)};
			return it == sc_KeywordMap.end() ? std::string_view{} : it->Glyph;
		}

		static std::string ToString(KT type);

		static bool IsValid(std::string_view glyph) {
			return range::find(sc_KeywordMap, glyph, &KeywordInfo::Glyph) != sc_KeywordMap.end();
		}

		static KeywordInfo const& Get(KT type) {
			auto const it{range::find(sc_KeywordMap, type, &KeywordInfo::Type)};
			ARCALC_DA(it != sc_KeywordMap.end(), "Getting invalid keyword [{}]", type);
			return *it;
		}

		// This design allows for changing the container later on.
		static GetAllKeywordTypesResult GetAllKeywordTypes() {
			return {sc_KeywordMap.cbegin(), sc_KeywordMap.cend()};
		}
	};
}

namespace std {
	template <> 
	struct formatter<ArCalc::KeywordType, char> : formatter<string_view> {
		using Base = formatter<string_view>;
		using Self = ArCalc::KeywordType;
		using Base::parse;
		auto format(Self kt, format_context& fc) const {
			return Base::format(ArCalc::Keyword::ToString(kt), fc);
		}
	};
}