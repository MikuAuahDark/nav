#ifndef _NAV_COMMON_H_
#define _NAV_COMMON_H_

#include <numeric>
#include <string>
#include <vector>
#include <type_traits>

#include "nav_internal.hpp"

namespace nav
{

struct FrameVector: public nav_frame_t
{
	FrameVector(nav_streaminfo_t *streaminfo, size_t streamindex, double position, const void *data, size_t size);
	~FrameVector() override;
	size_t getStreamIndex() const noexcept override;
	nav_streaminfo_t *getStreamInfo() const noexcept override;
	double tell() const noexcept override;
	size_t size() const noexcept override;
	void *data() noexcept override;

private:
	std::vector<uint8_t> buffer;
	nav_streaminfo_t *streaminfo;
	size_t streamindex;
	double position;
};

template<typename T>
double derationalize(T num, T den, double dv0 = 0.0)
{
	if (den == 0)
		return dv0;
	T gcd = std::gcd(num, den);
	num /= gcd;
	den /= gcd;
	return double(num) / double(den);
}

#ifdef _WIN32
std::wstring fromUTF8(const std::string &str);
#endif /* _WIN32 */

}

#endif /* _NAV_COMMON_H_ */
