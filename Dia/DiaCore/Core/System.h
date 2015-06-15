#ifndef DIA_SYSTEM_H
#define DIA_SYSTEM_H

#define CORE_YES		(1)
#define CORE_NO			(0)

//------------------------------------------------------------------------------------------------------------------------
// Compiler identification
//------------------------------------------------------------------------------------------------------------------------

#ifdef _MSC_VER
#	if _MSC_VER >= 1400
#		define CORE_VISUALC CORE_YES
#	else
#		error "System configuration: Visual Studio version too low!"
#	endif
#elif __GNUC__
#	define CORE_GCC CORE_YES
#elif __CWCC__
#   define CORE_CWCC CORE_YES
#else
#	error "Compiler identification unknown!"
#endif

#define UNUSED_PARAM(x) (void)x
#define ARRAY_COUNT(a)  (sizeof(a) / sizeof(a[0]))

#define DIA_NON_COPYABLE( _Type_ ) \
	private:\
	_Type_( const _Type_& );\
	_Type_& operator=(const _Type_& ) { return *this; }

#define DIA_DEFINE_NOT_EQUALS_USING_EQUALS_OP( _classname_ ) \
	bool operator != ( const _classname_ & xRef ) const { return !( this->operator == ( xRef ) ); }

#endif // DIA_ASSERT