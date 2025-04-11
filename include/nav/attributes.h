#ifndef _NAV_ATTRIBUTES_H_
#define _NAV_ATTRIBUTES_H_

#if (defined(__cplusplus) && __cplusplus >= 201703L) || \
    (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || \
	(defined(__STDC_VERSION__ ) && __STDC_VERSION__ >= 202311L)
#	define NAV_NODISCARD [[nodiscard]]
#	define NAV_DEPRECATED [[deprecated]]
#elif defined(__GNUC__) || defined(__clang__)
#	define NAV_NODISCARD __attribute__((warn_unused_result))
#	define NAV_DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#	include <sal.h>
#	define NAV_NODISCARD _Check_return_
#	define NAV_DEPRECATED __declspec(deprecated)
#else
#	define NAV_NODISCARD
#	define NAV_DEPRECATED
#endif

#endif /* _NAV_ATTRIBUTES_H_ */
