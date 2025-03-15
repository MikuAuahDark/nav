#ifndef _NAV_FFMPEG_COMMON_HPP_
#define _NAV_FFMPEG_COMMON_HPP_

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

inline std::string getLibName(const char *compname, int ver)
{
	char buf[64];

#if defined(_WIN32)
	sprintf(buf, "%s-%d", compname, ver);
#elif defined(__ANDROID__)
	sprintf(buf, "lib%s.so", compname);
#else
	sprintf(buf, "lib%s.so.%d", compname, ver);
#endif

	return std::string(buf);
}

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
