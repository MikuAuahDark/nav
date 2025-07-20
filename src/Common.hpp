#ifndef _NAV_COMMON_H_
#define _NAV_COMMON_H_

#include <numeric>
#include <optional>
#include <string>
#include <vector>
#include <type_traits>

#include "Internal.hpp"

namespace nav
{

struct AcquireData
{
	uint8_t *source; // Note: It may not writable.
	std::vector<uint8_t*> planes;
	std::vector<ptrdiff_t> strides;
};

struct FrameVector: public nav_frame_t
{
	FrameVector(nav_streaminfo_t *streaminfo, size_t streamindex, double position, const void *data, size_t size);
	~FrameVector() override;
	size_t getStreamIndex() const noexcept override;
	nav_streaminfo_t *getStreamInfo() const noexcept override;
	double tell() const noexcept override;
	const uint8_t *const *acquire(ptrdiff_t **strides, size_t *nplanes) override;
	void release() noexcept override;
	// Backward compatibility only
	uint8_t *pointer() noexcept;
	nav_hwacceltype getHWAccelType() const noexcept override;
	void *getHWAccelHandle() override;

private:
	std::vector<uint8_t> buffer;
	std::vector<uint8_t*> data;
	std::vector<ptrdiff_t> planeWidths;
	nav_streaminfo_t *streaminfo;
	size_t streamindex;
	double position;
};

template<typename T>
double derationalize(T num, T den, double dv0 = 0.0)
{
	if (!den)
		return dv0;
	T gcd = std::gcd(num, den);
	num /= gcd;
	den /= gcd;
	return double(num) / double(den);
}

constexpr nav_audioformat makeAudioFormat(uint8_t bps, bool is_float, bool is_signed)
{
	uint16_t floatval = NAV_AUDIOFORMAT_ISFLOAT(0xFFFFu) * is_float;
	uint16_t signedval = NAV_AUDIOFORMAT_ISSIGNED(0xFFFFu) * is_signed;
	return nav_audioformat(uint16_t(bps) | floatval | signedval);
}

bool getEnvvarBool(const std::string &name);
std::optional<int> getEnvvarInt(const std::string &name);
bool checkBackendDisabled(const std::string &backendNameUppercase);
size_t planeCount(nav_pixelformat fmt) noexcept;

#ifdef _WIN32
std::wstring fromUTF8(const std::string &str);
#endif /* _WIN32 */

}

#endif /* _NAV_COMMON_H_ */
