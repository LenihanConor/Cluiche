#ifndef DIA_TYPE_TRAITS_H
#define DIA_TYPE_TRAITS_H

//---------------------------------------------------------------------------------------------------------------------------------
// Type traits
//
//		HasTrivialConstructor
//		HasTrivialCopy
//		HasTrivialAssign
//		HasTrivialDestructor
//		HasNoThrowConstructor
//		HasNoThrowCopy
//		HasNoThrowAssign
//		HasVirtualDestructor
//		IsVoid
//		IsIntegral
//		IsFloat
//		IsArithmetic
//		IsReference
//		IsArray
//		IsUnion
//		IsClass
//		IsAbstract
//		IsConvertible<From,To>
//		IsEnum
//		IsPointer
//		IsScalar
//		IsPod
//		IsConstant
//
//	Modifiers: (use Foo<T>::Type)
//		AddReference
//		RemoveReference
//		RemoveCV
//
//  Converters: (use Foo<T>::Convert(var))
//		ToPointer
//		ToReference
//		ToCopy

#include "DiaCore/Core/System.h"
#include "DiaCore/Core/MetaLogic.h"
//-------------------------------------------------------------------------------------------------------------------------------
// Compiler support for type traits
//-------------------------------------------------------------------------------------------------------------------------------

#if CORE_VISUALC
#	define DIA_IS_UNION(T)						__is_union(T)
#	define DIA_IS_POD(T)						__is_pod(T)
#	define DIA_IS_EMPTY(T)						__is_empty(T)
#	define DIA_IS_POLYMORPHIC(T)				__is_polymorphic(T)
#	define DIA_HAS_TRIVIAL_CONSTRUCTOR(T)		__has_trivial_constructor(T)
#	define DIA_HAS_TRIVIAL_COPY(T)				__has_trivial_copy(T)
#	define DIA_HAS_TRIVIAL_ASSIGN(T)			__has_trivial_assign(T)
#	define DIA_HAS_TRIVIAL_DESTRUCTOR(T)		__has_trivial_destructor(T)
#	define DIA_HAS_NOTHROW_CONSTRUCTOR(T)		__has_nothrow_constructor(T)
#	define DOIA_HAS_NOTHROW_COPY(T)				__has_nothrow_copy(T)
#	define DIA_HAS_NOTHROW_ASSIGN(T)			__has_nothrow_assign(T)
#	define DIA_HAS_VIRTUAL_DESTRUCTOR(T)		__has_virtual_destructor(T)
#	define DIA_HAS_TYPETRAIT_INTRINSICS
#endif

#ifndef DIA_IS_UNION
#	define DIA_IS_UNION(T) DIA_ASSERT_STATIC(false)
#endif

#ifndef DIA_IS_POD
#	define DIA_IS_POD(T) DIA_ASSERT_STATIC(false)
#endif

#ifndef DIA_IS_EMPTY
#	define DIA_IS_EMPTY(T) DIA_ASSERT_STATIC(false)
#endif

#ifndef DIA_IS_POLYMORPHIC
#	define DIA_IS_POLYMORPHIC(T) DIA_ASSERT_STATIC(false)
#endif 

#ifndef DIA_HAS_TRIVIAL_CONSTRUCTOR
#	define DIA_HAS_TRIVIAL_CONSTRUCTOR(T) DIA_ASSERT_STATIC(false)
#endif

#ifndef DIA_HAS_TRIVIAL_COPY
#	define DIA_HAS_TRIVIAL_COPY(T) DIA_ASSERT_STATIC(false)
#endif

#ifndef DIA_HAS_TRIVIAL_ASSIGN
#	define DIA_HAS_TRIVIAL_ASSIGN(T) DIA_ASSERT_STATIC(false)
#endif

#ifndef DIA_HAS_TRIVIAL_DESTRUCTOR
#	define DIA_HAS_TRIVIAL_DESTRUCTOR(T) DIA_ASSERT_STATIC(false)
#endif

#ifndef DIA_HAS_NOTHROW_CONSTRUCTOR
#	define DIA_HAS_NOTHROW_CONSTRUCTOR(T) DIA_ASSERT_STATIC(false)
#endif

#ifndef DOIA_HAS_NOTHROW_COPY
#	define DOIA_HAS_NOTHROW_COPY(T) DIA_ASSERT_STATIC(false)
#endif

#ifndef DIA_HAS_NOTHROW_ASSIGN
#	define DIA_HAS_NOTHROW_ASSIGN(T) DIA_ASSERT_STATIC(false)
#endif

#ifndef DIA_HAS_VIRTUAL_DESTRUCTOR
#	define DIA_HAS_VIRTUAL_DESTRUCTOR(T) DIA_ASSERT_STATIC(false)
#endif

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			//---------------------------------------------------------------------------------------------------------------------------------
			// Tests for constructors
			//---------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct HasTrivialConstructor
				: IntegralConstant<bool,DIA_HAS_TRIVIAL_CONSTRUCTOR(T)>
			{};

			template <typename T> struct HasTrivialCopy
				: IntegralConstant<bool,DIA_HAS_TRIVIAL_COPY(T)>
			{};
			template <typename T> struct HasTrivialAssign
				: IntegralConstant<bool,DIA_HAS_TRIVIAL_ASSIGN(T)>
			{};
			template <typename T> struct HasTrivialDestructor
				: IntegralConstant<bool,DIA_HAS_TRIVIAL_DESTRUCTOR(T)>
			{};

			template <typename T> struct HasNoThrowConstructor
				: IntegralConstant<bool,DIA_HAS_NOTHROW_CONSTRUCTOR(T)>
			{};

			template <typename T> struct HasNoThrowCopy
				: IntegralConstant<bool,DOIA_HAS_NOTHROW_COPY(T)>
			{};
			template <typename T> struct HasNoThrowAssign
				: IntegralConstant<bool,DIA_HAS_NOTHROW_ASSIGN(T)>
			{};
			template <typename T> struct HasVirtualDestructor
				: IntegralConstant<bool,DIA_HAS_VIRTUAL_DESTRUCTOR(T)>
			{};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Tests for void qualifier
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsVoid: false_t {};

			template <> struct IsVoid<void>					: true_t {};
			template <> struct IsVoid<void const>			: true_t {};
			template <> struct IsVoid<void volatile>		: true_t {};
			template <> struct IsVoid<void const volatile>	: true_t {};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Test for integral types
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsIntegral: false_t {};

			template <> struct IsIntegral<unsigned char>				: true_t {};
			template <> struct IsIntegral<unsigned short>				: true_t {};
			template <> struct IsIntegral<unsigned int>					: true_t {};
			template <> struct IsIntegral<unsigned long>				: true_t {};
			template <> struct IsIntegral<signed char>					: true_t {};
			template <> struct IsIntegral<signed short>					: true_t {};
			template <> struct IsIntegral<signed int>					: true_t {};
			template <> struct IsIntegral<signed long>					: true_t {};
			template <> struct IsIntegral<bool>							: true_t {};
			template <> struct IsIntegral<char>							: true_t {};
			template <> struct IsIntegral<wchar_t>						: true_t {};
			template <> struct IsIntegral<unsigned long long>			: true_t {};
			template <> struct IsIntegral<signed long long>				: true_t {};

			template <> struct IsIntegral<const unsigned char>			: true_t {};
			template <> struct IsIntegral<const unsigned short>			: true_t {};
			template <> struct IsIntegral<const unsigned int>			: true_t {};
			template <> struct IsIntegral<const unsigned long>			: true_t {};
			template <> struct IsIntegral<const signed char>			: true_t {};
			template <> struct IsIntegral<const signed short>			: true_t {};
			template <> struct IsIntegral<const signed int>				: true_t {};
			template <> struct IsIntegral<const signed long>			: true_t {};
			template <> struct IsIntegral<const bool>					: true_t {};
			template <> struct IsIntegral<const char>					: true_t {};
			template <> struct IsIntegral<const wchar_t>				: true_t {};
			template <> struct IsIntegral<const unsigned long long>		: true_t {};
			template <> struct IsIntegral<const signed long long>		: true_t {};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Test for integral types
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsIntegralPointer: false_t {};

			template <> struct IsIntegralPointer<unsigned char*>				: true_t {};
			template <> struct IsIntegralPointer<unsigned short*>				: true_t {};
			template <> struct IsIntegralPointer<unsigned int*>					: true_t {};
			template <> struct IsIntegralPointer<unsigned long*>				: true_t {};
			template <> struct IsIntegralPointer<signed char*>					: true_t {};
			template <> struct IsIntegralPointer<signed short*>					: true_t {};
			template <> struct IsIntegralPointer<signed int*>					: true_t {};
			template <> struct IsIntegralPointer<signed long*>					: true_t {};
			template <> struct IsIntegralPointer<bool*>							: true_t {};
			template <> struct IsIntegralPointer<char*>							: true_t {};
			template <> struct IsIntegralPointer<wchar_t*>						: true_t {};
			template <> struct IsIntegralPointer<unsigned long long*>			: true_t {};
			template <> struct IsIntegralPointer<signed long long*>				: true_t {};

			template <> struct IsIntegralPointer<const unsigned char*>			: true_t {};
			template <> struct IsIntegralPointer<const unsigned short*>			: true_t {};
			template <> struct IsIntegralPointer<const unsigned int*>			: true_t {};
			template <> struct IsIntegralPointer<const unsigned long*>			: true_t {};
			template <> struct IsIntegralPointer<const signed char*>			: true_t {};
			template <> struct IsIntegralPointer<const signed short*>			: true_t {};
			template <> struct IsIntegralPointer<const signed int*>				: true_t {};
			template <> struct IsIntegralPointer<const signed long*>			: true_t {};
			template <> struct IsIntegralPointer<const bool*>					: true_t {};
			template <> struct IsIntegralPointer<const char*>					: true_t {};
			template <> struct IsIntegralPointer<const wchar_t*>				: true_t {};
			template <> struct IsIntegralPointer<const unsigned long long*>		: true_t {};
			template <> struct IsIntegralPointer<const signed long long*>		: true_t {};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Tests for a floating point type
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsFloat: false_t {};

			template <> struct IsFloat<float>				: true_t {};
			template <> struct IsFloat<const float>			: true_t {};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Tests for a floating point type
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsFloatPointer: false_t {};

			template <> struct IsFloatPointer<float*>				: true_t {};
			template <> struct IsFloatPointer<const float*>			: true_t {};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Tests for a double point type
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsDouble: false_t {};

			template <> struct IsDouble<double>				: true_t {};
			template <> struct IsDouble<long double>			: true_t {};
			template <> struct IsDouble<const double>		: true_t {};
			template <> struct IsDouble<const long double>	: true_t {};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Tests for a double point type
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsDoublePointer: false_t {};

			template <> struct IsDoublePointer<double*>				: true_t {};
			template <> struct IsDoublePointer<long double*>		: true_t {};
			template <> struct IsDoublePointer<const double*>		: true_t {};
			template <> struct IsDoublePointer<const long double*>	: true_t {};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Test for signed types
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsUnsigned: false_t {};

			template <> struct IsUnsigned<bool>						: true_t {};
			template <> struct IsUnsigned<unsigned char>			: true_t {};
			template <> struct IsUnsigned<unsigned short>			: true_t {};
			template <> struct IsUnsigned<unsigned int>				: true_t {};
			template <> struct IsUnsigned<unsigned long>			: true_t {};		
			template <> struct IsUnsigned<unsigned long long>		: true_t {};

			template <> struct IsUnsigned<const unsigned char>		: true_t {};
			template <> struct IsUnsigned<const unsigned short>		: true_t {};
			template <> struct IsUnsigned<const unsigned int>		: true_t {};
			template <> struct IsUnsigned<const unsigned long>		: true_t {};
			template <> struct IsUnsigned<const unsigned long long>	: true_t {};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Test for signed types
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsUnsignedPointer: false_t {};

			template <> struct IsUnsignedPointer<bool*>						: true_t {};
			template <> struct IsUnsignedPointer<unsigned char*>			: true_t {};
			template <> struct IsUnsignedPointer<unsigned short*>			: true_t {};
			template <> struct IsUnsignedPointer<unsigned int*>				: true_t {};
			template <> struct IsUnsignedPointer<unsigned long*>			: true_t {};		
			template <> struct IsUnsignedPointer<unsigned long long*>		: true_t {};

			template <> struct IsUnsignedPointer<const unsigned char*>		: true_t {};
			template <> struct IsUnsignedPointer<const unsigned short*>		: true_t {};
			template <> struct IsUnsignedPointer<const unsigned int*>		: true_t {};
			template <> struct IsUnsignedPointer<const unsigned long*>		: true_t {};
			template <> struct IsUnsignedPointer<const unsigned long long*>	: true_t {};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Test for bool types
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsBool: false_t {};

			template <> struct IsBool<bool>							: true_t {};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Test for bool types
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsBoolPointer: false_t {};

			template <> struct IsBoolPointer<bool*>							: true_t {};
			
			//-------------------------------------------------------------------------------------------------------------------------------
			// Test for char types
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsChar: false_t {};

			template <> struct IsChar<char>					: true_t {};
			template <> struct IsChar<unsigned char>		: true_t {};

			template <> struct IsChar<const char>			: true_t {};
			template <> struct IsChar<const unsigned char>	: true_t {};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Test for char types
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsCharPointer: false_t {};

			template <> struct IsCharPointer<char*>					: true_t {};
			template <> struct IsCharPointer<unsigned char*>		: true_t {};

			template <> struct IsCharPointer<const char*>			: true_t {};
			template <> struct IsCharPointer<const unsigned char*>	: true_t {};


			//-------------------------------------------------------------------------------------------------------------------------------
			// Test for short types
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsShort: false_t {};

			template <> struct IsShort<short>					: true_t {};
			template <> struct IsShort<unsigned short>			: true_t {};

			template <> struct IsShort<const short>				: true_t {};
			template <> struct IsShort<const unsigned short>	: true_t {};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Test for short types
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsShortPointer: false_t {};

			template <> struct IsShortPointer<short*>					: true_t {};
			template <> struct IsShortPointer<unsigned short*>			: true_t {};

			template <> struct IsShortPointer<const short*>				: true_t {};
			template <> struct IsShortPointer<const unsigned short*>	: true_t {};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Test for int types
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsInt: false_t {};

			template <> struct IsInt<int>					: true_t {};
			template <> struct IsInt<unsigned int>			: true_t {};

			template <> struct IsInt<const int>				: true_t {};
			template <> struct IsInt<const unsigned int>	: true_t {};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Test for int types
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsIntPointer: false_t {};

			template <> struct IsIntPointer<int*>					: true_t {};
			template <> struct IsIntPointer<unsigned int*>			: true_t {};

			template <> struct IsIntPointer<const int*>				: true_t {};
			template <> struct IsIntPointer<const unsigned int*>	: true_t {};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Test for long types
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsLong: false_t {};

			template <> struct IsLong<long>					: true_t {};
			template <> struct IsLong<unsigned long>		: true_t {};

			template <> struct IsLong<const long>			: true_t {};
			template <> struct IsLong<const unsigned long>	: true_t {};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Test for long types
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsLongPointer: false_t {};

			template <> struct IsLongPointer<long*>					: true_t {};
			template <> struct IsLongPointer<unsigned long*>		: true_t {};

			template <> struct IsLongPointer<const long*>			: true_t {};
			template <> struct IsLongPointer<const unsigned long*>	: true_t {};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Test for long types
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsLongLong: false_t {};

			template <> struct IsLongLong<long long>				: true_t {};
			template <> struct IsLongLong<unsigned long long>		: true_t {};

			template <> struct IsLongLong<const long long>			: true_t {};
			template <> struct IsLongLong<const unsigned long long>	: true_t {};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Test for long types
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsLongLongPointer: false_t {};

			template <> struct IsLongLongPointer<long long*>				: true_t {};
			template <> struct IsLongLongPointer<unsigned long long*>		: true_t {};

			template <> struct IsLongLongPointer<const long long*>			: true_t {};
			template <> struct IsLongLongPointer<const unsigned long long*>	: true_t {};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Tests for an arithmetic type
			// An arithmetic type is one that is either an integer or float
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T>
			struct IsArithmetic
				: MetaOr<IsIntegral<T>::value,IsFloat<T>::value, IsDouble<T>::value>::Type
			{};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Tests for an arithmetic type
			// An arithmetic type is one that is either an integer or float
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T>
			struct IsArithmeticPointer
				: MetaOr<IsIntegralPointer<T>::value,IsFloatPointer<T>::value, IsDoublePointer<T>::value>::Type
			{};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Makes a type into a reference type
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct AddReference;

			template <typename T> struct AddReference<T&> { typedef T& Type; };

			namespace {
				template <typename T>
				struct AddReferenceImpl
				{
					typedef T& Type;
				};
			}

			template <> struct AddReference<void> { typedef void Type; };

			template <typename T>
			struct AddReference
			{
				typedef typename AddReferenceImpl<T>::Type Type;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// Remove a reference
			//---------------------------------------------------------------------------------------------------------------------------------

			template <typename T>
			struct RemoveReference
			{
				typedef T Type;
			};

			template <typename T>
			struct RemoveReference<T&>
			{
				typedef T Type;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// Remove a const
			//---------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct RemoveConstant;
			template <typename T>
			struct RemoveConstant
			{
				typedef T Type;
			};

			template <typename T>
			struct RemoveConstant<const T>
			{
				typedef T Type;
			};

			template <typename T>
			struct RemoveConstant<T&>
			{
				typedef typename RemoveConstant<T>::Type& Type;
			};

			template <typename T>
			struct RemoveConstant<T*>
			{
				typedef typename RemoveConstant<T>::Type* Type;
			};

			template <typename T>
			struct RemoveConstant<T* const>
			{
				typedef typename RemoveConstant<T>::Type* Type;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// Remove pointer
			//---------------------------------------------------------------------------------------------------------------------------------

			template <typename T>
			struct RemovePointer
			{
				typedef T Type;
			};

			template <typename T>
			struct RemovePointer<T*>
			{
				typedef typename RemovePointer<T>::Type Type;
			};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Removes all attributes from a type
			//-------------------------------------------------------------------------------------------------------------------------------
			template <typename T>
			struct RemoveAttributes
			{
				typedef typename RemoveConstant<T>::Type 					NoConstant; 
				typedef typename RemovePointer<NoConstant>::Type 			NoConstantPointer; 
				typedef typename RemoveReference<NoConstantPointer>::Type 	NoConstantPointerReference;
				// this should be NoVolatileConstantPointerReference
				typedef NoConstantPointerReference Type;
			};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Tests for a reference type
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsReference: false_t {};
			template <typename T> struct IsReference<T&>			: true_t {};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Test for an array type
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsArray: false_t {};

			template <typename T, size_t N> struct IsArray<T[N]>				: true_t {};
			template <typename T, size_t N> struct IsArray<T const[N]>			: true_t {};
			template <typename T, size_t N> struct IsArray<T volatile[N]>		: true_t {};
			template <typename T, size_t N> struct IsArray<T const volatile[N]>	: true_t {};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Const/Volatile trait tests
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct CvTraitsImpl {};

			template <typename T>
			struct CvTraitsImpl<T*>
			{
				static const bool		isConst		= false;
				static const bool		isVolatile	= false;

				typedef T				UnqualifiedType;
			};

			template <typename T>
			struct CvTraitsImpl<const T*>
			{
				static const bool		isConst		= true;
				static const bool		isVolatile	= false;

				typedef T				UnqualifiedType;
			};

			template <typename T>
			struct CvTraitsImpl<volatile T*>
			{
				static const bool		isConst		= false;
				static const bool		isVolatile	= true;

				typedef T				UnqualifiedType;
			};

			template <typename T>
			struct CvTraitsImpl<const volatile T*>
			{
				static const bool		isConst		= true;
				static const bool		isVolatile	= true;

				typedef T				UnqualifiedType;
			};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Convert a type to that without the const volatile qualification
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T>
			struct RemoveCV
			{
				typedef typename CvTraitsImpl<T*>::UnqualifiedType Type;
			};

			template <typename T> struct RemoveCV<T&>			{ typedef T& Type; };

			template <typename T, size_t N>
			struct RemoveCV<T const[N]>
			{
				typedef T Type [N];
			};

			template <typename T, size_t N>
			struct RemoveCV<T volatile[N]>
			{
				typedef T Type [N];
			};

			template <typename T, size_t N>
			struct RemoveCV<T const volatile[N]>
			{
				typedef T Type [N];
			};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Tests for a union type
			//-------------------------------------------------------------------------------------------------------------------------------

			namespace
			{
				template <typename T>
				struct IsUnionImpl
				{
					typedef typename RemoveCV<T>::Type Cvt;
					static const bool value = DIA_IS_UNION(Cvt);
				};
			}

			template <typename T>
			struct IsUnion
				: IntegralConstant<bool,IsUnionImpl<T>::value>
			{};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Tests for a class type
			//-------------------------------------------------------------------------------------------------------------------------------

			template <class U> yes_t IsClassTester (void(U::*)(void));
			template <class U> no_t IsClassTester (...);

			template <typename T>
			struct IsClassImpl
			{
				static const bool value =
					MetaAnd<
					sizeof(IsClassTester<T>(0)) == sizeof(yes_t),	// Allows member functions
					MetaNot<IsUnion<T>::value>::value				// And not a union
					>::value;
			};

			template <typename T>
			struct IsClass
				: IntegralConstant<bool,IsClassImpl<T>::value>
			{};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Tests for an abstract class type
			//-------------------------------------------------------------------------------------------------------------------------------

			namespace {
				template <class T>
				struct IsAbstractImp2
				{
					// Deduction fails if T is void, function type, reference type or an abstract class type.
					template <class U> static no_t CheckSig (U (*)[1]);
					template <class U> static yes_t CheckSig (...);

					// T must be a complete type, further if T is a template then it must be instantiated in order for us to get the
					// right answer.
					//StaticAssert(sizeof(T) != 0);

					static const unsigned s1 = sizeof(CheckSig<T>(0));
					static const bool value = (s1 == sizeof(yes_t));
				};

				template <bool b>
				struct IsAbstractSelect
				{
					template <class T>
					struct Rebind
					{
						typedef IsAbstractImp2<T> Type;
					};
				};
				template <>
				struct IsAbstractSelect<false>
				{
					template <class T>
					struct Rebind
					{
						typedef false_t Type;
					};
				};

				template <class T>
				struct IsAbstractImp
				{
					typedef IsAbstractSelect<IsClass<T>::value> Selector;
					typedef typename Selector::template Rebind<T> Binder;
					typedef typename Binder::Type Type;

					static const bool value = Type::value;
				};
			}

			template <typename T>
			struct IsAbstract
				: IntegralConstant<bool,IsAbstractImp<T>::value>
			{};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Test for convertibility
			// Can one type be converted into another?
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename From, typename To> struct IsConvertible;

			namespace {
				template <typename From, typename To>
				struct IsConvertibleBasicImpl
				{
					static no_t Check(...);
					static yes_t Check(To);
					static From m_from;

					static const bool value = sizeof(Check(m_from)) == sizeof(yes_t);
				};

				template <typename From, typename To>
				struct IsConvertibleImpl
				{
					typedef typename AddReference<From>::Type RefType;
					static const bool value =
						MetaAnd<
						IsConvertibleBasicImpl<RefType,To>::value,
						MetaNot<IsArray<To>::value>::value
						>::value;
				};

				template <bool Trivial1, bool Trivial2, bool AbstractTarget>
				struct IsConvertibleImplSelect
				{
					template <typename From, typename To>
					struct Rebind
					{
						typedef IsConvertibleBasicImpl<From,To> Type;
					};
				};

				template <>
				struct IsConvertibleImplSelect<true,true,false>
				{
					template <typename From, typename To>
					struct Rebind
					{
						typedef true_t Type;
					};
				};

				template <>
				struct IsConvertibleImplSelect<false,false,true>
				{
					template <typename From, typename To>
					struct Rebind
					{
						typedef false_t Type;
					};
				};

				template <>
				struct IsConvertibleImplSelect<true,false,true>
				{
					template <typename From, typename To>
					struct Rebind
					{
						typedef false_t Type;
					};
				};

				template <typename From, typename To>
				struct IsConvertibleImplDispatchBase
				{
					typedef IsConvertibleImplSelect<
						IsArithmetic<From>::value,
						IsArithmetic<To>::value,
						IsAbstract<To>::value
					> Selector;

					typedef typename Selector::template Rebind<From,To> IscBinder;
					typedef typename IscBinder::Type Type;
				};

				template <typename From, typename To>
				struct IsConvertibleImplDispatch
					: IsConvertibleImplDispatchBase<From,To>::Type
				{};

				template <> struct IsConvertibleImpl<void,void>									{ static const bool value = true; };
				template <> struct IsConvertibleImpl<void,void const>							{ static const bool value = true; };
				template <> struct IsConvertibleImpl<void,void volatile>						{ static const bool value = true; };
				template <> struct IsConvertibleImpl<void,void const volatile>					{ static const bool value = true; };
				template <> struct IsConvertibleImpl<void const,void>							{ static const bool value = true; };
				template <> struct IsConvertibleImpl<void const,void const>						{ static const bool value = true; };
				template <> struct IsConvertibleImpl<void const,void volatile>					{ static const bool value = true; };
				template <> struct IsConvertibleImpl<void const,void const volatile>			{ static const bool value = true; };
				template <> struct IsConvertibleImpl<void volatile,void>						{ static const bool value = true; };
				template <> struct IsConvertibleImpl<void volatile,void const>					{ static const bool value = true; };
				template <> struct IsConvertibleImpl<void volatile,void volatile>				{ static const bool value = true; };
				template <> struct IsConvertibleImpl<void volatile,void const volatile>			{ static const bool value = true; };
				template <> struct IsConvertibleImpl<void const volatile,void>					{ static const bool value = true; };
				template <> struct IsConvertibleImpl<void const volatile,void const>			{ static const bool value = true; };
				template <> struct IsConvertibleImpl<void const volatile,void volatile>			{ static const bool value = true; };
				template <> struct IsConvertibleImpl<void const volatile,void const volatile>	{ static const bool value = true; };


				template <typename To>
				struct IsConvertibleImpl<void,To>
				{
					static const bool value = false;
				};
				template <typename From>
				struct IsConvertibleImpl<From,void>
				{
					static const bool value = false;
				};
			}

			template <typename From, typename To>
			struct IsConvertible
				: IntegralConstant<bool,IsConvertibleImplDispatch<From,To>::value>
			{};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Tests to see if two types are the same type
			//-------------------------------------------------------------------------------------------------------------------------------
			template <typename First, typename Second>	struct IsEqual					{	static const bool value = false; };
			template <typename First>					struct IsEqual<First, First>	{	static const bool value = true; };

			//-------------------------------------------------------------------------------------------------------------------------------
			// Tests to see if type is an enumeration
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsEnum;

			namespace {
				template <typename T>
				struct IsClassOrUnion
					: IntegralConstant<bool,
					MetaOr<
					IsClass<T>::value,
					IsUnion<T>::value>::value>
				{};

				struct IntConvertible { IntConvertible(int); };

				template <bool isTypenameArithmeticOrReference = true>
				struct IsEnumHelper
				{
					template <typename T> struct Type
					{
						static const bool value = false;
					};
				};

				template <> struct IsEnumHelper<false>
				{
					template <typename T> struct Type
						: IsConvertible<typename AddReference<T>::Type,IntConvertible>
					{};
				};

				template <typename T>
				struct IsEnumImpl
				{
					static const bool selector = 
						MetaOr<
						IsArithmetic<T>::value,
						IsReference<T>::value,
						IsClassOrUnion<T>::value,
						IsArray<T>::value
						>::value;

					typedef IsEnumHelper<selector> Se;

					typedef typename Se::template Type<T> Helper;
					static const bool value = Helper::value;
				};

				template <> struct IsEnumImpl<void>					{ static const bool value = false; };
				template <> struct IsEnumImpl<void const>			{ static const bool value = false; };
				template <> struct IsEnumImpl<void volatile>		{ static const bool value = false; };
				template <> struct IsEnumImpl<void const volatile>	{ static const bool value = false; };
			}

			template <typename T>
			struct IsEnum
				: IntegralConstant<bool,IsEnumImpl<T>::value>
			{};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Tests that a type is a pointer
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsPointer;

			namespace {
				template <typename> struct IsPointerHelper
				{
					static const bool value = false;
				};

				template <typename T> struct IsPointerHelper<T*>				{ static const bool value = true; };
				template <typename T> struct IsPointerHelper<T* const>			{ static const bool value = true; };
				template <typename T> struct IsPointerHelper<T* volatile>		{ static const bool value = true; };
				template <typename T> struct IsPointerHelper<T* const volatile>	{ static const bool value = true; };

				template <typename T>
				struct IsPointerImpl
				{
					static const bool value = IsPointerHelper<T>::value;
				};
			}

			template <typename T>
			struct IsPointer
				: IntegralConstant<bool,IsPointerImpl<T>::value>
			{};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Tests for a scalar type
			// A scalar type is anything that can hold a simple value.  This includes arithmetic types and pointers
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsScalar;

			namespace {
				template <typename T>
				struct IsScalarImpl
				{
					static const bool value = 
						MetaOr<
						IsArithmetic<T>::value,
						IsEnum<T>::value,
						IsPointer<T>::value
						>::value;
				};

				template <> struct IsScalarImpl<void>					{ static const bool value = false; };
				template <> struct IsScalarImpl<void const>				{ static const bool value = false; };
				template <> struct IsScalarImpl<void volatile>			{ static const bool value = false; };
				template <> struct IsScalarImpl<void const volatile>	{ static const bool value = false; };
			}

			template <typename T>
			struct IsScalar
				: IntegralConstant<bool,IsScalarImpl<T>::value>
			{};

			//-------------------------------------------------------------------------------------------------------------------------------
			// Tests for a POD type
			//-------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsPod;

			namespace {
				template <typename T>
				struct IsPodImpl
				{
					static const bool value = 
						MetaOr<
						IsScalar<T>::value,
						IsVoid<T>::value,
						DIA_IS_POD(T)
						>::value;
				};

				template <> struct IsPodImpl<void> { static const bool value = true; };
				template <> struct IsPodImpl<void const> { static const bool value = true; };
				template <> struct IsPodImpl<void volatile> { static const bool value = true; };
				template <> struct IsPodImpl<void const volatile> { static const bool value = true; };
			}

			template <typename T>
			struct IsPod
				: IntegralConstant<bool,IsPodImpl<T>::value>
			{};

			//---------------------------------------------------------------------------------------------------------------------------------
			// IsConstant
			//---------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsConstant;

			template <typename T>
			struct IsConstant : false_t
			{};

			template <typename T>
			struct IsConstant<T const> : true_t
			{};


			//---------------------------------------------------------------------------------------------------------------------------------
			// IsVolatile
			//---------------------------------------------------------------------------------------------------------------------------------

			template <typename T> struct IsVolatile;

			template <typename T>
			struct IsVolatile : false_t
			{};

			template <typename T>
			struct IsVolatile<T volatile> : true_t
			{};

			//---------------------------------------------------------------------------------------------------------------------------------
			// ReturnPointer
			//---------------------------------------------------------------------------------------------------------------------------------

			template <typename T>	struct ToPointer		{};
			template <typename T>	struct ToPointer<T*>	{ typedef T* Type; static T* Convert(T* value) { return value; } };
			template <typename T>	struct ToPointer<T&>	{ typedef T* Type; static T* Convert(T& value) { return &value; } };

			//---------------------------------------------------------------------------------------------------------------------------------
			// ReturnReference
			//---------------------------------------------------------------------------------------------------------------------------------
			template <typename T>	struct ToReference		{};
			template <typename T>	struct ToReference<T*>	{ typedef T& Type; static T& Convert(T* value) { return *value; } };
			template <typename T>	struct ToReference<T&>	{ typedef T& Type; static T& Convert(T& value) { return value; } };

			//---------------------------------------------------------------------------------------------------------------------------------
			// ReturnCopy
			//---------------------------------------------------------------------------------------------------------------------------------
			template <typename T>	struct ToCopy			{ typedef T Type; static T Convert(T	value) { return value; } };
			template <typename T>	struct ToCopy<T&>		{ typedef T Type; static T Convert(T&	value) { return value; } };

			//---------------------------------------------------------------------------------------------------------------------------------
			// horrible_cast< >
			//---------------------------------------------------------------------------------------------------------------------------------
			namespace
			{
				template <class OutputClass, class InputClass>
				union horrible_union
				{
					OutputClass out;
					InputClass in;
				};
			}

			template <class OutputClass, class InputClass>
			inline OutputClass horrible_cast (const InputClass input)
			{
				horrible_union<OutputClass,InputClass> u;
				typedef int ERROR_CantUseHorribleCast [sizeof(InputClass) == sizeof(u)
					&& sizeof(InputClass) == sizeof(OutputClass) ? 1 : -1];
				u.in = input;
				return u.out;
			}
		}
	}
}

#endif // 