#pragma once

#include "Core.h"
#include "Util/Util.h"
#include "Parser.h"

/** Conventions:
 *
 * Keywords  => underscore prefix + PascalCase.
 * Constants => underscore prefix + snake_case (may start with a capital letter).
 * Operators and built in functions => snake_case.
 */

using HANDLE = void*;

namespace ArCalc {
	class App {
	public:
		App();

	public:
		void Run();

	private:
		std::unique_ptr<Parser> m_pParser{};
		HANDLE hConsole{};
	};
}