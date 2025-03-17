#ifndef _NAV_FFMPEG_COMMON_HPP_
#define _NAV_FFMPEG_COMMON_HPP_

#if defined(_WIN32)
#	define _NAV_FFMPEG_LIB_NAME(libname, ver) libname "-" NAV_STRINGIZE(ver) ".dll"
#elif defined(__ANDROID__)
#	define _NAV_FFMPEG_LIB_NAME(libname, ver) "lib" libname ".so"
#else
#	define _NAV_FFMPEG_LIB_NAME(libname, ver) "lib" libname ".so." NAV_STRINGIZE(ver)
#endif

namespace nav::ffmpeg_common
{

template<typename T>
struct DoublePointerDeleter
{
	inline void operator()(T *ptr)
	{
		if (ptr)
		{
			T *temp = ptr;
			deleter(&temp);
		}
	}

	void(*deleter)(T**);
};

inline double derationalize(const AVRational &r, double dv0 = 0.0)
{
	return nav::derationalize(r.num, r.den, dv0);
}

template<typename T>
double derationalize(T multipler, const AVRational &r, double dv0 = 0.0)
{
	return nav::derationalize(multipler * r.num, (T) r.den, dv0);
}

}

#endif /* _NAV_FFMPEG_COMMON_HPP_ */
