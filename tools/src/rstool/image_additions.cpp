/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */

#include <string>

#include <be_data_interchange_an2k.h>
#include <be_image_image.h>
#include <be_memory_autoarray.h>

#include <image_additions.h>

/*
 * NOTE: X11 #defines things like "None" and "Status" that will wreak havoc on
 * libbiomeval headers. Include these headers last.
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>

void
grayToBGRA(
    BiometricEvaluation::Memory::uint8Array &grayBytes,
    BiometricEvaluation::Memory::uint8Array &bgraBytes)
{
	uint64_t graySize = grayBytes.size();
	uint64_t bgraSize = graySize * 4;
	bgraBytes.resize(bgraSize);
	
	for (uint64_t grayOff = 0, bgraOff = 0;
	    (grayOff < graySize) && (bgraOff < bgraSize);
	    grayOff++, bgraOff += 4) {
	    	bgraBytes[bgraOff] = grayBytes[grayOff];
	    	bgraBytes[bgraOff + 1] = grayBytes[grayOff];
	    	bgraBytes[bgraOff + 2] = grayBytes[grayOff];
	    	bgraBytes[bgraOff + 3] = 255;
	}
}

void
RGBToBGRA(
    BiometricEvaluation::Memory::uint8Array &rgbBytes,
    BiometricEvaluation::Memory::uint8Array &bgraBytes)
{
	uint64_t rgbSize = rgbBytes.size();
	uint64_t bgraSize = bgraBytes.size();
	
	for (uint64_t rgbOff = 0, bgraOff = 0; 
	    (rgbOff < rgbSize) && (bgraOff < bgraSize);
	    rgbOff += 3, bgraOff += 4) {
		bgraBytes[bgraOff] = rgbBytes[rgbOff + 2];
		bgraBytes[bgraOff + 1] = rgbBytes[rgbOff + 1];
		bgraBytes[bgraOff + 2] = rgbBytes[rgbOff];
		bgraBytes[bgraOff + 3] = 255;
	}
}

void
RGBAToBGRA(
    BiometricEvaluation::Memory::uint8Array &rgbaBytes,
    BiometricEvaluation::Memory::uint8Array &bgraBytes)
{
	bgraBytes.copy(rgbaBytes);

	uint8_t red;
	uint64_t size = rgbaBytes.size();
	for (uint64_t i = 0; i < size; i += 4) {
		red = rgbaBytes[i];
		bgraBytes[i] = rgbaBytes[i + 2];
		bgraBytes[i + 2] = red;
	}
}

void
createWindowAndDisplayImage(
    std::shared_ptr<BiometricEvaluation::Image::Image> image)
{
	/*
	 * Init the X11 environment.
	 */
	Display *display = XOpenDisplay(nullptr);
	if (display == nullptr) {
		std::string displayName = XDisplayName(nullptr);
		throw BiometricEvaluation::Error::StrategyError("Could not "
		    "open DISPLAY (" + displayName + ")");
	}

	int screen = DefaultScreen(display);
	Visual *visual = DefaultVisual(display, screen);
	if (visual == nullptr)
		throw BiometricEvaluation::Error::StrategyError("Could not "
		    "create Visual");

	Window window = XCreateSimpleWindow(display,
	    DefaultRootWindow(display), 0, 0, image->getDimensions().xSize,
	    image->getDimensions().ySize, 0, WhitePixel(display, screen),
	    BlackPixel(display, screen));

	/* 
	 * Create BGRA XImage.
	 */
	BiometricEvaluation::Memory::uint8Array rawBytes{image->getRawData()};
	BiometricEvaluation::Memory::uint8Array bgraBytes(
	    (image->getDimensions().xSize * image->getDimensions().ySize) * 4);
	uint8_t *bgraBytesPtr = NULL;
	switch (image->getDepth()) {
	case 8:
		grayToBGRA(rawBytes, bgraBytes);
		break;
	case 24:
		RGBToBGRA(rawBytes, bgraBytes);
		break;
	case 32:
		RGBAToBGRA(rawBytes, bgraBytes);
		break;
	default:
		throw BiometricEvaluation::Error::NotImplemented("Depth " +
		    std::to_string(image->getDepth()));
		/* Not reached */
		break;
	}
	bgraBytesPtr = bgraBytes;
	XImage *xImage = XCreateImage(display, visual,
	    DefaultDepth(display, screen), ZPixmap, 0, (char *)bgraBytesPtr,
	    image->getDimensions().xSize, image->getDimensions().ySize,
	    BitmapPad(display), 0 /* image->getDimensions().xSize * 4 */);
	GC graphicsContext = DefaultGC(display, screen);

	/*
	 * Event loop: draw the image when needed and wait for exit.
	 */
	bool stop = false;
	KeySym escapeKey = XKeysymToKeycode(display, XK_Escape);
	XEvent event;
	XSelectInput(display, window, ExposureMask | KeyPressMask);
	XMapWindow(display, window);
	while (!stop) {
		XNextEvent(display, &event);
		switch (event.type) {
		case Expose:
			/* 
			 * Expose can be called multiple times, but
			 * we only need to draw once.
			 */
			while (XCheckTypedWindowEvent(display, 
			    event.xexpose.window, Expose, &event)) {
				/* Ignore */
			}

			XPutImage(display, window, graphicsContext, xImage, 
			    0, 0, 0, 0, image->getDimensions().xSize,
			    image->getDimensions().ySize);
			break;
		case KeyPress:
			if (event.xkey.keycode == escapeKey)
				stop = true;
			break;
		default:
			break;
		}
	}

	/*
	 * Clean up.
	 */
	XUnmapWindow(display, window);
	XDestroyWindow(display, window);
	XFree(xImage);
}

const std::string ImageViewerWorker::ImageParameterKey = "image";

int32_t
ImageViewerWorker::workerMain()
{
	std::shared_ptr<BiometricEvaluation::Image::Image> image =
	    std::static_pointer_cast<BiometricEvaluation::Image::Image>(
	    getParameter(ImageViewerWorker::ImageParameterKey));
	try {
		createWindowAndDisplayImage(image);
	} catch (BiometricEvaluation::Error::Exception &e) {
		std::cerr << e.what() << std::endl;
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}

void
displayImage(
    std::shared_ptr<BiometricEvaluation::Image::Image> image)
{
	std::vector<std::shared_ptr<BiometricEvaluation::Image::Image>> images {
	    1};
	images.push_back(image);
	displayImages(images);
}

void
displayImages(
    std::vector<std::shared_ptr<BiometricEvaluation::Image::Image>> &images)
{
	std::shared_ptr<BiometricEvaluation::Process::ForkManager> manager {
	    new BiometricEvaluation::Process::ForkManager()};

	for (auto image = images.begin(); image != images.end(); image++) {
		std::shared_ptr<BiometricEvaluation::Process::
		    WorkerController> worker =
		    manager->addWorker(std::shared_ptr<ImageViewerWorker>(
		    new ImageViewerWorker()));
		worker->setParameter(ImageViewerWorker::ImageParameterKey,
		    *image);
	}

	manager->startWorkers(true);
}

void
displayAN2K(
    BiometricEvaluation::Memory::uint8Array &data)
{
	std::shared_ptr<BiometricEvaluation::DataInterchange::AN2KRecord> an2k {
	    new BiometricEvaluation::DataInterchange::AN2KRecord(data)};

	std::vector<std::shared_ptr<BiometricEvaluation::Image::Image>> images {
	    an2k->getFingerLatentCount() + an2k->getFingerCaptureCount()};

	/* Finger Captures */
	std::vector<BiometricEvaluation::Finger::AN2KViewCapture>
	    fingerCaptures = an2k->getFingerCaptures();
	for (auto view = fingerCaptures.begin(); view != fingerCaptures.end();
	    view++)
		images.push_back(view->getImage());

	/* Latent Captures */
	std::vector<BiometricEvaluation::Finger::AN2KViewLatent>
	    latentCaptures = an2k->getFingerLatents();
	for (auto view = latentCaptures.begin(); view != latentCaptures.end();
	    view++)
		images.push_back(view->getImage());

	/* Other Captures */

	displayImages(images);
}
