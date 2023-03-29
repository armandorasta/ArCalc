#pragma once

namespace ArCalc {
	enum class KeywordType {
		/*
			_Set [Name] [Make]

			Assigns or initializes a variable with name[Name] and value[Make]
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
				Can be passed m_By value or m_By reference m_By prefixing the name
				of the parameter m_By the & operator.
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

		/*
			_Save [constant or function name] [category]

			Serializes a constant or a function to disc, so it is ready to use in 
			the next session.
		*/
		Save,

		/*
			_Load [category]

			Loads an already serialized set of values from disc.
		*/
		Load,
	};
}