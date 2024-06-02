/**
 * @file nav.h
 * @author Miku AuahDark
 * @brief NPad's Audio Video decoding library.
 * @version 0.0
 * @copyright Copyright (c) 2024 Miku AuahDark License TBA
 */

#ifndef _NAV_H_
#define _NAV_H_

#ifdef _NAV_IMPLEMENTATION_
#	if defined(_MSC_VER)
#		define NAV_API __declspec(dllexport)
#	elif defined(__GNUC__) || defined(__clang__)
#		define NAV_API __attribute__((visibility("default")))
#	else
#		define NAV_API
#	endif
#else
#	ifdef _MSC_VER
#		define NAV_API __declspec(dllimport)
#	else
#		define NAV_API
#	endif
#endif /* _NAV_IMPLEMENTATION_ */

#include "audioformat.h"
#include "input.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Populate pointer of nav_input to read input data from memory.
 * @param input Allocated, but uninitialized pointer to nav_input.
 * @param buf Memory buffer.
 * @param size Size of memory buffer, in bytes.
 * @note Ensure `buf` exist during the whole lifetime of the nav_input.
 */
NAV_API void nav_input_populate_from_memory(nav_input *input, void *buf, size_t size);

/**
 * @brief Populate pointer of nav_input to read input data from file using C's stdio file operations.
 * @param input Allocated, but uninitialized pointer to nav_input.
 * @param filename Path to the UTF-8 encoded file.
 * @return 1 if success, 0 otherwise.
 */
NAV_API nav_bool nav_input_populate_from_file(nav_input *input, const char *filename);

/**
 * @brief Open new NAV instance.
 * @param input Pointer to "input data". This function will take the ownership of the input.
 * @return Pointer to NAV instance, or NULL on failure.
 * @note When the function errors, the input onership will be given back to the caller.
 */
NAV_API nav_t *nav_open(nav_input *input);

/**
 * @brief Close existing NAV instance.
 * @param nav Pointer to NAV instance.
 * @warning Ensure all existing NAV audio and video instances were closed before calling this function!
 * @sa nav_audio_close
 * @sa nav_video_close
 */
NAV_API void nav_close(nav_t *nav);

/**
 * @brief Get last error message from last invocation of NAV functions.
 * @return Pointer to the error message, or NULL if there are no errors.
 * @note Error messages are local to the caller thread.
 */
NAV_API const char *nav_error();

/**
 * @brief Get amount of audio streams.
 * @param nav Pointer to NAV instance.
 * @return Amount of audio streams in the instance.
 */
NAV_API size_t nav_naudio(nav_t *nav);

/**
 * @brief Get amount of video streams.
 * @param nav Pointer to NAV instance.
 * @return Amount of video streams in the instance.
 */
NAV_API size_t nav_nvideo(nav_t *nav);

/**
 * @brief Open new audio instance of existing NAV instance.
 * @param nav Pointer to NAV instance.
 * @param index Audio index.
 * @return Pointer to NAV audio instance, or NULL on failure.
 */
NAV_API nav_audio_t *nav_open_audio(nav_t *nav, size_t index);

/**
 * @brief Calculate the size in bytes of single audio sample frame.
 * @param nchannels Amount of audio channels.
 * @param format Audio format from nav_audio_format.
 * @return Size of 1 sample frame, in bytes.
 */
NAV_API size_t nav_audio_size(int nchannels, nav_audioformat format);

/**
 * @brief Get audio sample rate.
 * @param a Pointer to NAV audio instance.
 * @return Audio sample rate.
 */
NAV_API uint32_t nav_audio_sample_rate(nav_audio_t *a);

/**
 * @brief Get number of channels.
 * @param a Pointer to NAV audio instance.
 * @return Number of audio channels.
 */
NAV_API uint32_t nav_audio_nchannels(nav_audio_t *a);

/**
 * @brief Get amount of audio samples.
 * @param a Pointer to NAV audio instance.
 * @return Amount of audio samples, or (uint64_t)-1 if unknown.
 * @note This function may be an approximate.
 * @note To get the audio duration in seconds, divide the return value of this function by the sample rate.
 * @sa nav_audio_sample_rate
 */
NAV_API uint64_t nav_audio_nsamples(nav_audio_t *a);

/**
 * @brief Get the bitwise audio format.
 * @param a Pointer to NAV audio instance.
 * @return Bitwise audio format.
 * @note The bitwise audio format is same as [SDL's AudioFormat](https://wiki.libsdl.org/SDL3/SDL_AudioFormat)
 */
NAV_API nav_audioformat nav_audio_format(nav_audio_t *a);

/**
 * @brief Get current sample position.
 * @param a Pointer to NAV audio instance.
 * @return current sample position or (uint64_t)-1 if unknown.
 */
NAV_API uint64_t nav_audio_tell(nav_audio_t *a);

/**
 * @brief Seek to specific sample position.
 * @param a Pointer to NAV audio instance.
 * @param off Sample offset, relative to the beginning of the audio.
 * @return 1 if success, 0 otherwise.
 */
NAV_API nav_bool nav_audio_seek(nav_audio_t *a, uint64_t off);

/**
 * @brief Decode samples.
 * @param a Pointer to NAV audio instance.
 * @param buffer Destination buffer. The data type depends on `nav_audioformat`.
 * @param nsamples Size of the buffer in **samples**.
 * @return Amount of sample written to buffer, or 0 on EOF, or (size_t)-1 on failure.
 */
NAV_API size_t nav_audio_get_samples(nav_audio_t *a, void *buffer, size_t nsamples);

/**
 * @brief Close the audio stream instance.
 * @param a Pointer to NAV audio instance.
 */
NAV_API void nav_audio_close(nav_audio_t *a);

/**
 * @brief Open new video instance of existing NAV instance.
 * @param nav Pointer to NAV instance.
 * @param index Video index.
 * @return Pointer to NAV video instance, or NULL on failure.
 */
NAV_API nav_video_t *nav_open_video(nav_t *nav, size_t index);

/**
 * @brief Calculate the size of uncompressed video frame.
 * 
 * @param width Video width.
 * @param height Video height.
 * @param format Video format from nav_video_pixel_format.
 * @return Size of 1 video frame, in bytes.
 */
NAV_API size_t nav_video_size(uint32_t width, uint32_t height, nav_pixelformat format);

/**
 * @brief Get video dimensions.
 * @param v Pointer to NAV video instance.
 * @param width Pointer to store the video width.
 * @param height Pointer to store the video height.
 */
NAV_API void nav_video_dimensions(nav_video_t *v, uint32_t *width, uint32_t *height);

/**
 * @brief Get video decode pixel format.
 * @param v Pointer to NAV video instance.
 * @return Video pixel format.
 */
NAV_API nav_pixelformat nav_video_pixel_format(nav_video_t *v);

/**
 * @brief Get video duration in seconds.
 * @param v Pointer to NAV video instance.
 * @return Video duration in seconds, or -1 if unknown.
 * @note This function may be an approximate.
 */
NAV_API double nav_video_duration(nav_video_t *v);

/**
 * @brief Get video position in seconds.
 * @param v Pointer to NAV video instance.
 * @return Current video position or -1 if unknown.
 */
NAV_API double nav_video_tell(nav_video_t *v);

/**
 * @brief Seek to specific position in seconds.
 * @param v Pointer to NAV video instance.
 * @param off Position in seconds, relative to the beginning of the video.
 * @return 1 if success, 0 otherwise.
 * @note nav_video_tell() may not return same value as `off` as this function will try to find nearest (next) video
 *       frame.
 */
NAV_API nav_bool nav_video_seek(nav_video_t *v, double off);

/**
 * @brief Decode video frame.
 * @param v Pointer to NAV video instance.
 * @param dst Pointer to destination video frame buffer. Use nav_video_size() to calculate buffer size.
 * @return Current video position or -1 if unknown or failure. nav_error() will return non-null string on failure.
 * @sa nav_video_size
 */
NAV_API double nav_video_get_frame(nav_video_t *v, void *dst);

/**
 * @brief Close the video stream instance.
 * @param v Pointer to NAV video instance.
 */
NAV_API void nav_video_close(nav_video_t *v);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _NAV_H_ */
