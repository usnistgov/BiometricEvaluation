/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */

#ifndef __RSTOOL_IMAGE_ADDITIONS_H__
#define __RSTOOL_IMAGE_ADDITIONS_H__

#include <vector>

#include <be_image_image.h>
#include <be_memory_autoarray.h>
#include <be_process_forkmanager.h>

/**
 * @brief
 * Obtain a copy of an 8-bit grayscale as a BGRA byte array.
 *
 * @param[in] grayBytes
 *	Raw image bytes, 1 byte per pixel, sequential.
 * @param[out] bgraBytes
 *	Copy of grayBytes as a BGRA byte array.
 */
static void
grayToBGRA(
    BiometricEvaluation::Memory::uint8Array &grayBytes,
    BiometricEvaluation::Memory::uint8Array &bgraBytes);

/**
 * @brief
 * Add an alpha channel and switch blue and green channels of 24-bit RGB data.
 *
 * @param[in] rgbBytes
 *	RGB triplets, 1 byte per color, 24 bits per pixel, sequential.
 * @param[out] bgraBytes
 *	Copy of rgbBytes as a BGRA byte array.
 */
static void
RGBToBGRA(
    BiometricEvaluation::Memory::uint8Array &rgbBytes,
    BiometricEvaluation::Memory::uint8Array &bgraBytes);

/**
 * @brief
 * Obtain a copy of a RGBA array where R and B are swapped.
 *
 * @param[in] rgbaBytes
 *	RGBA triplets, 1 byte per color, 32 bits per pixel, sequential.
 * @param[out] bgraBytes
 *	Copy of rgbBytes as a BGRA byte array.
 */
static void
RGBAToBGRA(
    BiometricEvaluation::Memory::uint8Array &rgbBytes,
    BiometricEvaluation::Memory::uint8Array &bgraBytes);

/**
 * @brief
 * Display an image on screen via X11 in a new Window.
 *
 * @param image
 *	Pointer to image data.
 *
 * @throw Error::NotImplemented
 *	Image with an invalid bit depth was passed.
 * @throw Error::StrategyError
 *	Error initializing X11.
 *
 * @note
 *	Under normal circumstances, this function will not return until
 *	the user presses the "escape" key on their keyboard to close the
 *	window.
 * @warning
 *	This is probably not the function you're looking for. @see displayImage.
 */
static void
createWindowAndDisplayImage(
    std::shared_ptr<BiometricEvaluation::Image::Image> image);

/**
 * @brief
 * Display an image on screen via X11.
 * @details
 * This function calls createWindowAndDisplayImage in a separate process.
 *
 * @param image
 *	Pointer to image data.
 *
 * @throw Error::NotImplemented
 *	Image with an invalid bit depth was passed.
 * @throw Error::StrategyError
 *	Error initializing X11
 *
 * @note
 *	Under normal circumstances, this function will not return until
 *	the user presses the "escape" key on their keyboard to close the
 *	window.
 * @note
 *	This function will create a new process per image.
 */
void
displayImage(
    std::shared_ptr<BiometricEvaluation::Image::Image> image);

/**
 * @brief
 * Display images on screen via X11.
 * @details
 * This function calls createWindowAndDisplayImage in a separate processes.
 *
 * @param images
 *	Vector of pointers to image data.
 *
 * @throw Error::NotImplemented
 *	Image with an invalid bit depth was passed.
 * @throw Error::StrategyError
 *	Error initializing X11
 *
 * @note
 *	Under normal circumstances, this function will not return until
 *	the user presses the "escape" key on their keyboard to close the
 *	window.
 * @note
 *	This function will create a new process per image.
 */
void
displayImages(
    std::vector<std::shared_ptr<BiometricEvaluation::Image::Image>> &images);

/**
 * @brief
 * Display the captures of an AN2K record on screen via X11.
 * @details
 * This function calls createWindowAndDisplayImage in a separate process.
 *
 * @param data
 *	AN2K data in memory.
 *
 * @throw Error::DataError
 *	Error parsing AN2K file.
 * @throw Error::NotImplemented
 *	Image with an invalid bit depth was in the AN2K record.
 * @throw Error::StrategyError
 *	Error initializing X11
 *
 * @note
 *	Under normal circumstances, this function will not return until
 *	the user presses the "escape" key on their keyboard to close the
 *	window.
 * @note
 *	This function will create a new process per image.
 */
void
displayAN2K(
    BiometricEvaluation::Memory::uint8Array &data);

/** Managable Worker for displaying more than image at a time to the screen. */
class ImageViewerWorker : public BiometricEvaluation::Process::Worker
{
public:
	/** Worker implementation */
	int32_t
	workerMain();

	/** Name of the parameter containing an Image */
	static const std::string ImageParameterKey;

	/** Constructor */
	ImageViewerWorker() = default;
	/** Destructor */
	~ImageViewerWorker() = default;
};

#endif /* __RSTOOL_IMAGE_ADDITIONS_H__ */
