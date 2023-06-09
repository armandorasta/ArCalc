#pragma once

namespace ArCalc {
	enum class KeywordType : size_t {
		/*
			_List [Prefix, opt]

			Lists all variabls with the specified prefix or all variables if
			no prefix is specified
		*/
		List = 0,

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

		/*
			_Unscope 

			Cancels the current scope or does nothing when in global scope.
			--------------------------------------------------------------------
			_Unscope [literal name]

			Deletes the literal, even if it's a function parameter.
			--------------------------------------------------------------------
			_Unscope [function name] [new name, opt]

			Deletes or renames the function.
			--------------------------------------------------------------------
		*/
		Unscope,

		/*
			_Err "[error message]"

			Exists a function with an error message.
		*/
		Err,

		/*
			[_Sig or _Pi] [variable name] [min] [max (inclusive)]
				[function]

			_Sum: passes values of [min, max] to the function and adds them up.
			_Mul: same as _Sum, except it multiplies instead of adding.
		*/
		Sum,
		Mul,

		/*
			_Set [Name] [Make]

			Assigns or initializes a variable with name[Name] and value [Make]
		*/
		Set,
	};
}