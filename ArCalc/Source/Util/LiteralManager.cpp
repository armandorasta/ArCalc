#pragma once

#include "LiteralManager.h"
#include "Exception/ArCalcException.h"
#include "Keyword.h"
#include "IO.h"

namespace ArCalc {
	LiteralManager::LiteralManager(std::ostream& os) : m_OStream{os} {
		SetLast(0.0);
	}

	void LiteralManager::Add(std::string_view litName, double value) {
		auto const ownedName = std::string{litName};
		ARCALC_DA(!m_LitMap.contains(ownedName), "Adding literal [{}] twice", litName);
		m_LitMap.emplace(ownedName, LiteralData::Make(value));
	}

	void LiteralManager::Add(std::string_view litName, double* ptr) {
		auto const ownedName = std::string{litName};
		ARCALC_DA(!m_LitMap.contains(ownedName), "Adding literal [{}] twice", litName);
		m_LitMap.emplace(ownedName, LiteralData::MakeRef(ptr));
	}

	double LiteralManager::GetLast() const {
		return *m_LitMap.at(Keyword::ToString(KeywordType::Last));
	}

	void LiteralManager::SetLast(double toWhat) {
		m_LitMap.insert_or_assign(Keyword::ToString(KeywordType::Last), LiteralData::Make(toWhat));
	}

	bool LiteralManager::IsVisible(std::string_view litName) const {
		return m_LitMap.contains(std::string{litName});
	}

	void LiteralManager::List(std::string_view prefix) const {
		constexpr auto Tab = "    ";

		for (auto const& [name, data] : m_LitMap) {
			if (name != "_Last" && name.starts_with(prefix)) {
				IO::Print(m_OStream, "\n{}{} = {}", Tab, name, *data);
			}
		}
	}

	LiteralData const& LiteralManager::Get(std::string_view litName) const {
		return const_cast<LiteralManager&>(*this).Get(litName);
	}

	LiteralData& LiteralManager::Get(std::string_view litName) {
		ARCALC_DA(IsVisible(litName), "Getting Invalid literal [{}]", litName);
		return m_LitMap.at(std::string{litName});
	}

	void LiteralManager::Serialize(std::string_view name, std::ostream& os) {
		os << std::format("C {} {}\n\n", name, *Get(name));
	}
	
	void LiteralManager::Deserialize(std::istream& is) {
		auto const name{IO::Input<std::string>(is)};
		auto const value{IO::Input<double>(is)};
		// Clashing names will be overriden for now.
		m_LitMap.insert_or_assign(name, LiteralData::Make(value));
	}

	void LiteralManager::Reset() {
		m_LitMap.clear();
	}
}