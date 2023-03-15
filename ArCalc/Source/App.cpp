#include "App.h"
#include "PostfixMathEvaluator.h"
#include "Util/Util.h"
#include "Util/MathOperator.h"

namespace ArCalc {
	void App::Run() {
		Parser::ParseFile("Program.txt");
	}
}
