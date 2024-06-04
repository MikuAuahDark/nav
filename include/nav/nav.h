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
 * @brief Get last error message from last invocation of NAV functions.
 * @return Pointer to the error message, or NULL if there are no errors.
 * @note Error messages are local to the caller thread.
 * @warning The error message pointer will be invalid when calling another NAV function!
 */
NAV_API const char *nav_error();

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
 * @param filename Pseudo-filename that will be used to improve probing. The file doesn't have to exist. This can be NULL.
 * @return Pointer to NAV instance, or NULL on failure.
 * @note When the function errors, the input onership will be given back to the caller.
 * @note It's strongly recommended to specify `filename` hint as some backends requires it.
 */
NAV_API nav_t *nav_open(nav_input *input, const char *filename);

/**
 * @brief Close existing NAV instance.
 * @param nav Pointer to NAV instance.
 */
NAV_API void nav_close(nav_t *nav);

/**
 * @brief Get amount of streams in the NAV instance.
 * @param nav Pointer to NAV instance.
 * @return Amount of streams. 
 */
NAV_API size_t nav_nstreams(nav_t *nav);

/**
 * @brief Get stream information
 * @param nav Pointer to NAV instance.
 * @param index Stream index.
 * @return Pointer to NAV stream information.
 */
NAV_API nav_streaminfo_t *nav_stream_info(nav_t *nav, size_t index);

/**
 * @brief Get media position.
 * @param nav Pointer to NAV instance.
 * @return Current media position in seconds, or -1 if unknown.
 */
NAV_API double nav_tell(nav_t *nav);

/**
 * @brief Get media duration.
 * @param nav Pointer to NAV instance.
 * @return Media duration, or -1 on unknown.
 */
NAV_API double nav_duration(nav_t *nav);

/**
 * @brief Set media position
 * @param nav Pointer to NAV instance.
 * @param position Position in seconds, relative to the beginning of the media.
 * @return New (re-adjusted) position, or -1 on unknown or failure.
 * @note nav_error() will return NULL on success but new position can't be determined, non-NULL otherwise.
 */
NAV_API double nav_seek(nav_t *nav, double position);

/**
 * @brief Read NAV packet from NAV instance.
 * @param nav Pointer to NAV instance.
 * @return Pointer to the NAV packet instance.
 * @sa nav_packet_free
 * @sa nav_packet_decode
 */
NAV_API nav_packet_t *nav_read(nav_t *nav);

/**
 * @brief Get stream type.
 * @param streaminfo Pointer to NAV stream information.
 * @return Stream type. 
 */
NAV_API nav_streamtype nav_streaminfo_type(nav_streaminfo_t *streaminfo);

/**
 * @brief Calculate the size in bytes of single audio sample frame.
 * @param streaminfo Pointer to NAV stream information.
 * @return Size of 1 sample frame, in bytes.
 */
NAV_API size_t nav_audio_size(nav_streaminfo_t *streaminfo);

/**
 * @brief Get audio sample rate.
 * @param streaminfo Pointer to NAV stream information.
 * @return Audio sample rate.
 * @note This call only return meaningful value if the stream is an audio.
 */
NAV_API uint32_t nav_audio_sample_rate(nav_streaminfo_t *streaminfo);

/**
 * @brief Get number of audio channels.
 * @param streaminfo Pointer to NAV stream information.
 * @return Number of audio channels.
 * @note This call only return meaningful value if the stream is an audio.
 */
NAV_API uint32_t nav_audio_nchannels(nav_streaminfo_t *streaminfo);

/**
 * @brief Get the bitwise audio format.
 * @param streaminfo Pointer to NAV stream information.
 * @return Bitwise audio format.
 * @note The bitwise audio format is same as [SDL's AudioFormat](https://wiki.libsdl.org/SDL3/SDL_AudioFormat)
 * @note This call only return meaningful value if the stream is an audio.
 */
NAV_API nav_audioformat nav_audio_format(nav_streaminfo_t *streaminfo);

/**
 * @brief Calculate the size of uncompressed video frame.
 * @param streaminfo Pointer to NAV stream information.
 * @return Size of 1 video frame, in bytes.
 * @note This call only return meaningful value if the stream is a video.
 */
NAV_API size_t nav_video_size(nav_streaminfo_t *streaminfo);

/**
 * @brief Get video dimensions.
 * @param streaminfo Pointer to NAV stream information.
 * @param width Pointer to store the video width.
 * @param height Pointer to store the video height.
 * @note This call only return meaningful value if the stream is a video.
 */
NAV_API void nav_video_dimensions(nav_streaminfo_t *streaminfo, uint32_t *width, uint32_t *height);

/**
 * @brief Get video decode pixel format.
 * @param streaminfo Pointer to NAV stream information.
 * @return Video pixel format.
 * @note This call only return meaningful value if the stream is a video.
 */
NAV_API nav_pixelformat nav_video_pixel_format(nav_streaminfo_t *streaminfo);

/**
 * @brief Get video frames per second.
 * @param streaminfo Pointer to NAV stream information.
 * @return Video FPS, or 0 if unknown.
 * @note This function may be an approximate.
 * @note This call only return meaningful value if the stream is a video.
 */
NAV_API double nav_video_fps(nav_streaminfo_t *streaminfo);

/**
 * @brief Free the NAV packet instance.
 * @param packet Pointer to the NAV packet instance.
 */
NAV_API void nav_packet_free(nav_packet_t *packet);

/**
 * @brief Get stream index correspond to the NAV packet instance.
 * @param packet Pointer to the NAV packet instance.
 * @return Stream index of the NAV packet. 
 */
NAV_API size_t nav_packet_streamindex(nav_packet_t *packet);

/**
 * @brief Get stream type correspond to the NAV packet instance.
 * @param packet Pointer to the NAV packet instance.
 * @return Stream type of the NAV packet. 
 */
NAV_API nav_streamtype nav_packet_streamtype(nav_packet_t *packet);

/**
 * @brief Get decoded data size.
 * @param packet Pointer to the NAV packet instance.
 * @return Decoded data size, in bytes.
 * @sa nav_audio_size
 * @sa nav_video_size
 */
NAV_API size_t nav_packet_size(nav_packet_t *packet);

/**
 * @brief Decode NAV packet instance.
 * @param packet Pointer to the NAV packet instance.
 * @param dest Pointer where to store the decoded data. Use nav_packet_size(), nav_audio_size(), or nav_video_size()
 *             to calculate amount of memory to allocate.
 * @return Presentation time of the decoded data in seconds, or -1 on EOS or failure.
 * @note nav_error() will return NULL when EOS is reached, otherwise non-NULL is returned.
 * @warning Most backend only allows you to decode the packet once.
 */
NAV_API double nav_packet_decode(nav_packet_t *packet, void *dest);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _NAV_H_ */
