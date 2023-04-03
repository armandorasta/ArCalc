#include "ArCalcException.h"
#include "Util/IO.h"

namespace ArCalc {
	ArCalcException::ArCalcException() : ArCalcException{"No message"} {}

	void ArCalcException::PrintMessage(std::string_view indent) const {
		IO::Print(std::cerr, "\n{}{}. [{} {}]\n\n", indent, GetMessage(), GetType(), GetLineNumber());
	}
	
	std::string const & ArCalcException::GetMessage() const noexcept {
		return m_Message;
	}
}
