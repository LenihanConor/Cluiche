#ifndef DIA_METALOGIC_H
#define DIA_METALOGIC_H

#include "DiaCore/Type/BasicTypeDefines.h"

namespace Dia
{
	namespace Core
	{
		//-----------------------------------------------------------------------------------------------------------------------------
		// Boolean types
		//-----------------------------------------------------------------------------------------------------------------------------

		typedef IntegralConstant<bool,true>		true_t;
		typedef IntegralConstant<bool,false>	false_t;		

		//-----------------------------------------------------------------------------------------------------------------------------
		// Meta Or test
		//-----------------------------------------------------------------------------------------------------------------------------

		template <
			bool b1,
			bool b2,
			bool b3 = false,
			bool b4 = false,
			bool b5 = false,
			bool b6 = false,
			bool b7 = false,
			bool b8 = false>
		struct MetaOr: true_t
		{};

		template <>
		struct MetaOr<false,false,false,false,false,false,false,false>
			: false_t
		{};

		//-----------------------------------------------------------------------------------------------------------------------------
		// Meta And test
		//-----------------------------------------------------------------------------------------------------------------------------

		template <
			bool b1,
			bool b2,
			bool b3 = true,
			bool b4 = true,
			bool b5 = true,
			bool b6 = true,
			bool b7 = true,
			bool b8 = true>
		struct MetaAnd: false_t
		{};

		template <>
		struct MetaAnd<true,true,true,true,true,true,true,true>
			: true_t
		{};

		//-----------------------------------------------------------------------------------------------------------------------------
		// Meta Not operation
		//-----------------------------------------------------------------------------------------------------------------------------

		template <bool b> struct MetaNot;

		template <> struct MetaNot<true> : false_t {};
		template <> struct MetaNot<false> : true_t {};

		//-----------------------------------------------------------------------------------------------------------------------------
		// MetaIf operation
		//-----------------------------------------------------------------------------------------------------------------------------

		template <bool flag, typename IfType, typename ThenType>
		struct MetaIf
		{
			typedef IfType Type;
		};

		template <typename IfType, typename ThenType>
		struct MetaIf<false,IfType,ThenType>
		{
			typedef ThenType Type;
		};

		//-----------------------------------------------------------------------------------------------------------------------------
		// MetaMax & MetaMin
		//-----------------------------------------------------------------------------------------------------------------------------

		namespace
		{
			template <typename T, T a, T b>
			struct MetaMax_First
			{
				static const T value = a;
			};

			template <typename T, T a, T b>
			struct MetaMax_Second
			{
				static const T value = b;
			};
		}

		template <typename T, T a, T b>
		struct MetaMax
		{
		private:
			typedef typename MetaIf<(a > b),MetaMax_First<T,a,b>,MetaMax_Second<T,a,b> >::Type Selector;

		public:
			static const T value = Selector::value;
		};

		template <typename T, T a, T b>
		struct MetaMin
		{
		private:
			typedef typename MetaIf<(a < b),MetaMax_First<T,a,b>,MetaMax_Second<T,a,b> >::Type Selector;
		public:
			static const T value = Selector::value;
		};
	}
}

#endif // DIA_ASSERT