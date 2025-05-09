/**
 * @file nav.h
 * @author Miku AuahDark
 * @brief NPad's Audio Video decoding library.
 * @version 0.1.0
 * @copyright Copyright (c) 2024 Miku AuahDark Licensed under zlib/libpng license.
 */

#ifndef _NAV_H_
#define _NAV_H_

#define NAV_VERSION_MAJOR 0
#define NAV_VERSION_MINOR 3
#define NAV_VERSION_PATCH 0
#define NAV_VERSION_FORMAT(a, b, c) ((a << 16) | (b << 8) | c)
#define NAV_VERSION NAV_VERSION_FORMAT(NAV_VERSION_MAJOR, NAV_VERSION_MINOR, NAV_VERSION_PATCH)

#ifdef NAV_SHARED
#	if defined(_NAV_IMPLEMENTATION_)
#		if defined(_MSC_VER)
#			define NAV_API __declspec(dllexport)
#		elif defined(__GNUC__) || defined(__clang__)
#			define NAV_API __attribute__((visibility("default")))
#		else
#			define NAV_API
#		endif
#	elif defined(_MSC_VER)
#		define NAV_API __declspec(dllimport)
#	else
#		define NAV_API extern
#	endif
#else
#	define NAV_API extern
#endif /* NAV_SHARED */

#include "attributes.h"
#include "audioformat.h"
#include "input.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get integer version of currently loaded NAV library.
 * @return Integer version of NAV, formatted same as NAV_VERSION.
 */
NAV_API uint32_t nav_version();

/**
 * @brief Get null-terminated string representation of currently loaded NAV library.
 * @return Pointer to null-terminated string (this always points to valid pointer).
 */
NAV_API const char *nav_version_string();

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
 * @brief Get amount of available backends.
 * @return Amount of available backends. 0 means no backends are available.
 */
NAV_API size_t nav_backend_count();

/**
 * @brief Get an unique name identifier of a backend.
 * @param index **1-based index** of the backend. So, 1 is the first backend, 2 is the second backend, and so on.
 * @return Lowercase name of the backend, or NULL on failure.
 * @note A backend name is unlikely to change.
 */
NAV_API const char *nav_backend_name(size_t index);

/**
 * @brief Get the backend type.
 * @param index **1-based index** of the backend. So, 1 is the first backend, 2 is the second backend, and so on.
 * @return Valid backend type enum or NAV_BACKENDTYPE_UNKNOWN on failure.
 */
NAV_API nav_backendtype nav_backend_type(size_t index);

/**
 * @brief Get additional information of a certain backend.
 * @param index **1-based index** of the backend. So, 1 is the first backend, 2 is the second backend, and so on.
 * @return Additional information about the backend, or NULL if there are no additional information or on failure.
 * @note use nav_error() to check if an error occured or a backend has no additional information.
 */
NAV_API const char *nav_backend_info(size_t index);

/**
 * @brief Open new NAV instance.
 * 
 * The `filename` parameter acts as a "hint" to backends to try best possible format or to deduce the media format
 * directly. While it's not required, some backends require it, so it's recommended to specify one if possible. When
 * `filename` is specified, it must be valid UTF-8 string.
 * 
 * @param input Pointer to "input data". This function will take the ownership of the input.
 * @param filename Pseudo-filename that will be used to improve probing. This can be NULL, but it's recommended to
 *                 specify a pseudo-filename (the file doesn't have to exist in filesystem).
 * @param settings Additional settings to specify. This can be NULL.
 * @return Pointer to NAV instance, or NULL on failure.
 * @note When the function errors, the input ownership will be given back to the caller.
 */
NAV_NODISCARD NAV_API nav_t *nav_open(nav_input *input, const char *filename, const nav_settings *settings);

/**
 * @brief Close existing NAV instance.
 * @param nav Pointer to NAV instance.
 */
NAV_API void nav_close(nav_t *nav);

/**
 * @brief Get backend index that crated this NAV instance.
 * @param nav Pointer to NAV instance.
 * @return **1-based index** of the backend that created this NAV instance.
 */
NAV_API size_t nav_backend_index(const nav_t *nav);

/**
 * @brief Get amount of streams in the NAV instance.
 * @param nav Pointer to NAV instance.
 * @return Amount of streams. 
 */
NAV_API size_t nav_nstreams(const nav_t *nav);

/**
 * @brief Get stream information
 * @param nav Pointer to NAV instance.
 * @param index Stream index.
 * @return Pointer to NAV stream information, or NULL on failure.
 */
NAV_API const nav_streaminfo_t *nav_stream_info(const nav_t *nav, size_t index);

/**
 * @brief Check if stream at specific index is enabled.
 * 
 * Disabled stream won't generate data during nav_read().
 * 
 * @param nav Pointer to NAV instance.
 * @param index Stream index.
 * @return 1 if enabled, 0 otherwise.
 * @sa nav_stream_enable
 */
NAV_API nav_bool nav_stream_is_enabled(const nav_t *nav, size_t index);

/**
 * @brief Set if specific stream should be enabled.
 * 
 * Disabled stream won't generate data during nav_read(). This can serve as optimization.
 * 
 * @param nav Pointer to NAV instance.
 * @param index Stream index.
 * @param enable 1 to enable, 0 to disable.
 * @return 1 if the change success, 0 otherwise.
 * @sa nav_stream_enabled
 */
NAV_API nav_bool nav_stream_enable(nav_t *nav, size_t index, nav_bool enable);

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
 * @brief Decode stream from NAV instance.
 * @param nav Pointer to NAV instance.
 * @return Pointer to the NAV decoded frame instance, or NULL on failure.
 * @note nav_error() will return NULL on EOS, non-NULL otherwise.
 * @sa nav_frame_free
 */
NAV_NODISCARD NAV_API nav_frame_t *nav_read(nav_t *nav);

/**
 * @brief Get stream type.
 * @param streaminfo Pointer to NAV stream information.
 * @return Stream type. 
 */
NAV_API nav_streamtype nav_streaminfo_type(const nav_streaminfo_t *streaminfo);


/**
 * @brief Calculate the size in bytes of single audio sample frame.
 * @param streaminfo Pointer to NAV stream information.
 * @return Size of 1 sample frame, in bytes.
 */
NAV_API size_t nav_audio_size(const nav_streaminfo_t *streaminfo);

/**
 * @brief Get audio sample rate.
 * @param streaminfo Pointer to NAV stream information.
 * @return Audio sample rate.
 * @note This call only return meaningful value if the stream is an audio.
 */
NAV_API uint32_t nav_audio_sample_rate(const nav_streaminfo_t *streaminfo);

/**
 * @brief Get number of audio channels.
 * @param streaminfo Pointer to NAV stream information.
 * @return Number of audio channels.
 * @note This call only return meaningful value if the stream is an audio.
 */
NAV_API uint32_t nav_audio_nchannels(const nav_streaminfo_t *streaminfo);

/**
 * @brief Get the bitwise audio format.
 * @param streaminfo Pointer to NAV stream information.
 * @return Bitwise audio format.
 * @note The bitwise audio format is same as [SDL's AudioFormat](https://wiki.libsdl.org/SDL3/SDL_AudioFormat)
 * @note This call only return meaningful value if the stream is an audio.
 */
NAV_API nav_audioformat nav_audio_format(const nav_streaminfo_t *streaminfo);

/**
 * @brief Calculate the size of uncompressed video frame without any padding.
 * @param streaminfo Pointer to NAV stream information.
 * @return Size of 1 video frame, in bytes.
 * @note This call only return meaningful value if the stream is a video.
 */
NAV_API size_t nav_video_size(const nav_streaminfo_t *streaminfo);

/**
 * @brief Get amount of planes for this video format.
 * @param streaminfo Pointer to NAV stream information.
 * @return Amount of planes for this video format.
 */
NAV_API size_t nav_video_plane_count(nav_pixelformat pixfmt);

/**
 * @brief Get the dimension of video plane without any padding, in bytes.
 * 
 * @param streaminfo Pointer to NAV stream information.
 * @param index Plane indices.
 * @param width Pointer to store the plane width in bytes.
 * @param height Pointer to store the plane height in bytes.
 * @note This call only return meaningful value if the stream is a video.
 */
NAV_API void nav_video_plane_dimensions(const nav_streaminfo_t *streaminfo, size_t index, size_t *width, size_t *height);

/**
 * @brief Get video dimensions.
 * @param streaminfo Pointer to NAV stream information.
 * @param width Pointer to store the video width.
 * @param height Pointer to store the video height.
 * @note This call only return meaningful value if the stream is a video.
 */
NAV_API void nav_video_dimensions(const nav_streaminfo_t *streaminfo, uint32_t *width, uint32_t *height);

/**
 * @brief Get video decode pixel format.
 * @param streaminfo Pointer to NAV stream information.
 * @return Video pixel format.
 * @note This call only return meaningful value if the stream is a video.
 */
NAV_API nav_pixelformat nav_video_pixel_format(const nav_streaminfo_t *streaminfo);

/**
 * @brief Get video frames per second.
 * @param streaminfo Pointer to NAV stream information.
 * @return Video FPS, or 0 if unknown.
 * @note This function may be an approximate.
 * @note This call only return meaningful value if the stream is a video.
 */
NAV_API double nav_video_fps(const nav_streaminfo_t *streaminfo);

/**
 * @brief Get stream index correspond to the NAV frame instance.
 * @param frame Pointer to the NAV frame instance.
 * @return Stream index of the NAV frame. 
 */
NAV_API size_t nav_frame_streamindex(const nav_frame_t *frame);

/**
 * @brief Get stream type correspond to the NAV frame instance.
 * @param frame Pointer to the NAV frame instance.
 * @return Stream info of the NAV frame. 
 */
NAV_API const nav_streaminfo_t *nav_frame_streaminfo(const nav_frame_t *frame);

/**
 * @brief Get presentation timestamp of this frame.
 * @param frame Pointer to the NAV frame instance.
 * @return Presentation time of the decoded data in seconds, or -1 if unknown.
 */
NAV_API double nav_frame_tell(const nav_frame_t *frame);

/**
 * @brief Make the current frame available for reading.
 * @param frame Pointer to the NAV frame instance.
 * @param strides Pointer to store the list of strides/pitch of each plane in bytes. Negative stride means plane is bottom-up. For audio, this always positive.
 * @param nplanes Pointer to store the amount of planes. For audio, this always writes 1. This can be NULL.
 * @return Array of pointer to each plane, or NULL on failure. Subsequent calls to this function return same pointer.
 * @note nav_frame_release must only be called once regardless on how many times nav_frame_acquire is called.
 * @sa nav_streaminfo_type_plane_count
 * @sa nav_frame_release
 */
NAV_NODISCARD NAV_API const uint8_t *const *nav_frame_acquire(nav_frame_t *frame, ptrdiff_t **strides, size_t *nplanes);

/**
 * @brief Release the previously-acquired frame, making the acquired pointer invalid.
 * @param frame Pointer to the NAV frame instance.
 * @sa nav_frame_acquire
 */
NAV_API void nav_frame_release(nav_frame_t *frame);

/**
 * @brief Free the NAV frame instance.
 * 
 * If the frame was acquired, it will be released.
 * @param frame Pointer to the NAV frame instance.
 */
NAV_API void nav_frame_free(nav_frame_t *frame);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _NAV_H_ */
