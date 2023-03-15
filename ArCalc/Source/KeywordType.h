#pragma once

namespace ArCalc {
	enum class KeywordType {
		/*
			_Set [Name] [Value]

			Assigns or initializes a variable with name[Name] and value[Value]
		*/
		Set,

		/*
			_List [Prefix, opt]

			Lists all variabls with the specified prefix or all variables if
			no prefix is specified
		*/
		List,

		/*
			_Func [Name] ([Params..., opt]) [Body];

			Defines a function named [Name] and takes [Params].
			The defination ends with a semicolon

			[Params...] {
				Can be passed by value or by reference by prefixing the name
				of the parameter by the & operator.
			}
		*/
		Func,

		/*
			_Return [Expr]

			Returns [Expr] if provided, otherwise just returns nothing.
		*/
		Return,

		/*
			_Last

			Returns the value of the last expression evaluated, returns or zero
			if nothing was evaluated before it.

			- Setting a variable will not update it.
			- Evaluating an invalid expression will not reset it.
		*/
		Last,

		// Control flow
		If,
		Else,
		Elif,
	};
}