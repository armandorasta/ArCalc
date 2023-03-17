#include "PostfixMathEvaluator.h"
#include "App.h"
#include "Exception/ArCalcException.h"

int main() {
	namespace calc = ArCalc;
	try {
		calc::App app{};
		app.Run();
	} catch (calc::ArCalcException const& err) {
		std::cout << '\n' << err.GetMessage() << ".\n";
	}
}
