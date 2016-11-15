/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties.  Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain.  NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */

#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include <utility>

#include <opencv2/features2d/features2d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <be_data_interchange_an2k.h>
#include <be_image_raw.h>
#include <be_io_utility.h>
#include <be_memory_autoarrayiterator.h>
#include <be_text.h>

#include <asegcrop.h>

namespace BE = BiometricEvaluation;

int
main(
    int argc,
    char *argv[])
{
	if (argc <= 1) {
		std::cerr << "Usage: " << argv[0] << "[-r] type14.an2 "
		    "[type14.an2 ...]\n";
		return (EXIT_FAILURE);
	}

	bool rotate{std::string(argv[1]) == "-r"};
	int rv = EXIT_SUCCESS;
	int firstArg = (rotate ? 2 : 1);

	for (int i = firstArg; i < argc; ++i) {
		std::unique_ptr<BE::DataInterchange::AN2KRecord> an2k;
		try {
			an2k.reset(new BE::DataInterchange::AN2KRecord(
			    argv[i]));
		} catch (BE::Error::Exception &e) {
			std::cerr << argv[i] << ": " << e.whatString() <<
			    std::endl;
			continue;
		}

		for (const auto &capture : an2k->getFingerCaptures()) {
			if (capture.getImageColorDepth() != 8)
				continue;
			try {
				writeSegments(std::string(argv[i]) + "." +
				    std::to_string(to_int_type(
				    capture.getPositions().front())),
				    cropASEG(capture, rotate));
			} catch (BE::Error::Exception &e) {
				std::cerr << e.whatString() << std::endl;
				rv = EXIT_FAILURE;
			}
		}
	}

	return (rv);
}

void
writeSegments(
    const std::string &name,
    const std::vector<std::pair<BE::Finger::Position, BE::Image::Raw>>
    &segments)
{
	std::string segmentName;
	for (const auto &segment : segments) {
		const auto dim = segment.second.getDimensions();
		segmentName = BE::Text::basename(name) + "." +
		    std::to_string(to_int_type(segment.first)) + "." +
		    std::to_string(dim.xSize) + "x" +
		    std::to_string(dim.ySize) + ".gray";
		try {
			BE::IO::Utility::writeFile(segment.second.getRawData(),
			    segmentName);
			std::cout << "Wrote " << segmentName << std::endl;
		} catch (BE::Error::Exception &e) {
			std::cerr << "Failed to write \"" << segmentName <<
			    "\" (" << e.whatString() << ")" << std::endl;
		}
	}
}

std::vector<std::pair<BE::Finger::Position, BE::Image::Raw>>
cropASEG(
    const BE::Finger::AN2KViewCapture &capture,
    bool rotate)
{
	const auto aseg = capture.getAlternateFingerSegmentPositionSet();
	if (aseg.size() == 0)
		return {};

	std::vector<std::pair<BE::Finger::Position, BE::Image::Raw>> ret;
	for (const auto segment : aseg)
		ret.push_back({segment.fingerPosition,
		    cropSingleASEG(capture, segment, rotate)});

	return (ret);
}

float
getRotationAngle(
    const std::vector<BE::Image::Coordinate> &coords,
    const BE::Image::Size &dims)
{
	const auto first = coords.front();
	const auto last = coords.back();

	if (first == last)
		throw BE::Error::StrategyError("Can't determine angle when "
		    "first and last coordinates are the same.");

	if ((first.y == last.y) || (first.x == last.x))
		return (0);

	const auto ix = lineIntersection({0, first.y}, {dims.xSize, first.y},
	    {last.x, 0}, {last.x, dims.ySize}, dims);
	if (!ix.first)
		throw BE::Error::StrategyError("Cannot find intersection");

	const auto rF = realCoordinate(first, dims);
	const auto rL = realCoordinate(last, dims);
	const auto rX = realCoordinate(ix.second, dims);

	const auto a = distance(rF.first, rF.second, rL.first, rL.second);
	const auto b = distance(rL.first, rL.second, rX.first, rX.second);
	const auto c = distance(rX.first, rX.second, rF.first, rF.second);

	float rad = acos(((c * c) + (a * a) - (b * b)) / (2 * c * a));
	float deg = rad * (180.0 / M_PI);

	return (clockwise(coords, dims) ? -1 * (90 - deg) : (90 - deg));
}

BE::Image::Raw
cropSingleASEG(
    const BE::Finger::AN2KViewCapture &capture,
    const BE::Finger::AN2KViewCapture::FingerSegmentPosition &segment,
    bool rotate)
{
	/* Ensure all coordinates are within the image */
	const auto dimensions = capture.getImageSize();
	auto segs = correctSegmentCoordinates(segment.coordinates, dimensions);

	/* Find rectangular bounding box to contain all points */
	uint32_t minX = UINT32_MAX, minY = UINT32_MAX;
	uint32_t maxX = 0, maxY = 0;
	for (const auto &c : segs) {
		maxX = std::max(maxX, c.x);
		maxY = std::max(maxY, c.y);
		minX = std::min(minX, c.x);
		minY = std::min(minY, c.y);
	}
	if ((minX == UINT32_MAX) || (minY == UINT32_MAX) ||
	    (maxX == 0) || (maxY == 0))
		throw BE::Error::StrategyError("Could not find bounding box");
	if ((minX >= maxX) || (minY >= maxY))
		throw BE::Error::StrategyError("Insane bounding box [(" +
		    std::to_string(minX) + "," + std::to_string(minY) + ") (" +
		    std::to_string(maxX) + "," + std::to_string(maxY) + ")]");

	const auto originalRawData = capture.getImage()->getRawData();
	BE::Memory::uint8Array data((maxY - minY) * (maxX - minX));

	/* Extract raw pixels */
	uint64_t croppedOffset = 0;
	for (uint32_t row = minY; row < maxY; ++row) {
		for (uint32_t col = minX; col < maxX; ++col) {
			if (pointInPolygon({col, row}, segs))
				data[croppedOffset++] =
				    originalRawData[col +
				    (row * dimensions.xSize)];
			else
				data[croppedOffset++] = 0xFF;
		}
	}

	if (rotate) {
		float angleOfRotation = getRotationAngle(segment.coordinates,
		    capture.getImageSize());

		std::shared_ptr<BE::Image::Raw> rawImage(
		    new BE::Image::Raw(data, data.size(),
		    {static_cast<uint32_t>(maxX - minX),
		    static_cast<uint32_t>(maxY - minY)},
		    capture.getImageColorDepth(), 8,
		    capture.getImageResolution()));
		return (rotateImage(rawImage, angleOfRotation));
	} else
		return {data, data.size(), {static_cast<uint32_t>(maxX - minX),
		    static_cast<uint32_t>(maxY - minY)},
		    capture.getImageColorDepth(), 8,
		    capture.getImageResolution()};
}

BE::Image::Raw
trim(
    const BE::Memory::uint8Array &data,
    const BE::Image::Size &dims,
    const BE::Image::Resolution &res)
{
	/* Find first row with a non-white pixel */
	uint32_t minY{0};
	for (uint32_t r = 0; r < dims.ySize; ++r) {
		for (uint32_t c = 0; c < dims.xSize; ++c) {
			if (data[c + (dims.xSize * r)] != 255) {
				minY = r;
				break;
			}
		}

		if (minY != 0)
			break;
	}

	/* Find last row with a non-white pixel */
	uint32_t maxY{dims.ySize};
	for (uint32_t r = (dims.ySize - 1); r > minY && r != 0; --r) {
		for (uint32_t c = 0; c < dims.xSize; ++c) {
			if (data[c + (dims.xSize * r)] != 255) {
				maxY = r;
				break;
			}
		}

		if (maxY != dims.ySize)
			break;
	}

	/* Find leftmost col with a non-white pixel */
	uint32_t minX{UINT32_MAX};
	for (uint32_t r = minY; r < maxY; ++r) {
		for (uint32_t c = 0; c < dims.xSize; ++c) {
			if (data[c + (dims.xSize * r)] != 255) {
				minX = std::min(minX, c);
				break;
			}
		}
	}
	if (minX == UINT32_MAX)
		minX = 0;

	/* Find right col with a non-white pixel */
	uint32_t maxX{0};
	for (uint32_t r = minY; r < maxY; ++r) {
		for (uint32_t c = (dims.xSize - 1); c > minX && c != 0; --c) {
			if (data[c + (dims.xSize * r)] != 255) {
				maxX = std::max(maxX, c);
				break;
			}
		}
	}
	if (maxX == 0)
		maxX = dims.xSize;

	/* Make image */
	BE::Memory::uint8Array trimmed((maxX - minX) * (maxY - minY));
	for (uint32_t r = minY; r < maxY; ++r) {
		for (uint32_t c = minX; c < maxX; ++c) {
			trimmed[(c - minX) + ((maxX - minX) * (r - minY))] =
			    data[c + (dims.xSize * r)];
		}
	}

	return {trimmed, trimmed.size(), {maxX - minX, maxY - minY}, 8, 8, res};
}

BiometricEvaluation::Image::Raw
rotateImage(
    const std::shared_ptr<BiometricEvaluation::Image::Image> image,
    const float degrees)
{
	const auto dims = image->getDimensions();
	cv::Mat source(dims.ySize, dims.xSize, cv::DataType<uint8_t>::type);

	const auto rawData = image->getRawData();
	std::copy(rawData.begin(), rawData.end(), source.begin<uint8_t>());

	const cv::Point2f center((static_cast<float>(dims.xSize) / 2),
	    (static_cast<float>(dims.ySize) / 2));

	cv::Mat rotation = cv::getRotationMatrix2D(center, degrees, 1.0);
	cv::Rect boundingBox = cv::RotatedRect(center, source.size(), degrees).
	    boundingRect();

	/* Add a translation into center of new image instead of cropping */
	rotation.at<double>(0, 2) += boundingBox.width / 2.0 - center.x;
	rotation.at<double>(1, 2) += boundingBox.height / 2.0 - center.y;

	cv::Mat rotatedMat;
	cv::warpAffine(source, rotatedMat, rotation, boundingBox.size(),
	    cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));

	BE::Memory::uint8Array rotatedMemory(boundingBox.width *
	    boundingBox.height);
	rotatedMemory.copy(rotatedMat.data);
	return (trim(rotatedMemory, {static_cast<uint32_t>(boundingBox.width),
	    static_cast<uint32_t>(boundingBox.height)},
	    image->getResolution()));
}

/*
 * Geometric functions.
 */

bool
pointInPolygon(
    const BE::Image::Coordinate &point,
    const std::vector<BE::Image::Coordinate> &coordinates)
{
	bool oddIntersections = false;
	float x1, y1, x2, y2, tX, tY;
	for (uint64_t i = 0, j = coordinates.size() - 1; i < coordinates.size();
	    j = i++) {
		/* Want floating point division */
		x1 = coordinates[i].x;
		y1 = coordinates[i].y;
		x2 = coordinates[j].x;
		y2 = coordinates[j].y;
		tX = point.x;
		tY = point.y;

		if (((y1 > tY) != (y2 > tY)) &&
		     (tX < (x2 - x1) * (tY - y1) / (y2 - y1) + x1))
			oddIntersections = !oddIntersections;
	}

	return (oddIntersections);
}

inline float
distance(
    float x1,
    float y1,
    float x2,
    float y2)
{
	return (sqrtf(powf(x2 - x1, 2.0) + powf(y2 - y1, 2.0)));
}


std::pair<bool, BE::Image::Coordinate>
intersectTop(
    const BE::Image::Coordinate &point1,
    const BE::Image::Coordinate &point2,
    const BE::Image::Size &dim)
{
	const auto realL2P1 = realCoordinate(point1, dim);
	const auto realL2P2 = realCoordinate(point2, dim);

	/* Does not cross top of image */
	if ((realL2P1.second > 0) && (realL2P2.second > 0))
		return {false, {0,0}};

	const auto intersection = lineIntersection({0, 0},
	    {dim.xSize, 0}, point1, point2, dim);
	if (!intersection.first)
		return {false, {0, 0}};

	if (coordinateOutsideImage(intersection.second, dim))
		return {false, {0, 0}};
	return (intersection);
}

std::pair<bool, BE::Image::Coordinate>
intersectBottom(
    const BE::Image::Coordinate &point1,
    const BE::Image::Coordinate &point2,
    const BE::Image::Size &dim)
{
	const auto realL2P1 = realCoordinate(point1, dim);
	const auto realL2P2 = realCoordinate(point2, dim);

	/* Does not cross bottom of image */
	if ((realL2P1.second < dim.ySize) && (realL2P2.second < dim.ySize))
		return {false, {0,0}};

	const auto intersection = lineIntersection({0, dim.ySize},
	    {dim.xSize, dim.ySize}, point1, point2, dim);
	if (!intersection.first)
		return {false, {0, 0}};

	if (coordinateOutsideImage(intersection.second, dim))
		return {false, {0, 0}};
	return (intersection);
}

std::pair<bool, BE::Image::Coordinate>
intersectLeft(
    const BE::Image::Coordinate &point1,
    const BE::Image::Coordinate &point2,
    const BE::Image::Size &dim)
{
	const auto realL2P1 = realCoordinate(point1, dim);
	const auto realL2P2 = realCoordinate(point2, dim);

	/* Does not cross left of image */
	if ((realL2P1.first > 0) && (realL2P2.first > 0))
		return {false, {0,0}};

	const auto intersection = lineIntersection({0, 0},
	    {0, dim.ySize}, point1, point2, dim);
	if (!intersection.first)
		return {false, {0, 0}};

	if (coordinateOutsideImage(intersection.second, dim))
		return {false, {0, 0}};
	return (intersection);
}

std::pair<bool, BE::Image::Coordinate>
intersectRight(
    const BE::Image::Coordinate &point1,
    const BE::Image::Coordinate &point2,
    const BE::Image::Size &dim)
{
	const auto realL2P1 = realCoordinate(point1, dim);
	const auto realL2P2 = realCoordinate(point2, dim);

	/* Does not cross right of image */
	if ((realL2P1.first < dim.xSize) && (realL2P2.first < dim.xSize))
		return {false, {0,0}};

	const auto intersection = lineIntersection({dim.xSize, 0},
	    {dim.xSize, dim.ySize}, point1, point2, dim);
	if (!intersection.first)
		return {false, {0, 0}};

	if (coordinateOutsideImage(intersection.second, dim))
		return {false, {0, 0}};
	return (intersection);
}

std::pair<bool, BE::Image::Coordinate>
lineIntersection(
    const BE::Image::Coordinate &l1p1,
    const BE::Image::Coordinate &l1p2,
    const BE::Image::Coordinate &l2p1,
    const BE::Image::Coordinate &l2p2,
    const BE::Image::Size &dim)
{
	/* Properly represent negative coordinates */
	const auto maxSize = BE::Image::Size(UINT32_MAX, UINT32_MAX);
	const auto realL1P1 = realCoordinate(l1p1, maxSize);
	const auto realL1P2 = realCoordinate(l1p2, maxSize);
	const auto realL2P1 = realCoordinate(l2p1, maxSize);
	const auto realL2P2 = realCoordinate(l2p2, maxSize);

	/*
	 * Using nomenclature from
	 * https://en.wikipedia.org/wiki/Line–line_intersection
	 * "Given two points on each line"
	 */
	const int32_t x1 = realL1P1.first, y1 = realL1P1.second;
	const int32_t x2 = realL1P2.first, y2 = realL1P2.second;
	const int32_t x3 = realL2P1.first, y3 = realL2P1.second;
	const int32_t x4 = realL2P2.first, y4 = realL2P2.second;

	const float denominator =
	    ((x1 - x2) * (y3 - y4)) - ((y1 - y2) * (x3 - x4));

	/* No intersection possible (parallel or coincident) */
	if (denominator == 0)
		return {false, {0, 0}};

	const float xNumerator =
	    (((x1 * y2) - (y1 * x2)) * (x3 - x4)) -
	    ((x1 - x2) * ((x3 * y4) - (y3 * x4)));

	const float yNumerator =
	    (((x1 * y2) - (y1 * x2)) * (y3 - y4)) -
	    ((y1 - y2) * ((x3 * y4) - (y3 * x4)));

	BE::Image::Coordinate intersection{
	    static_cast<uint32_t>((xNumerator / denominator)),
	    static_cast<uint32_t>((yNumerator / denominator))};

	return {true, intersection};
}

/*
 * Coordinate information.
 */

inline bool
coordinateOutsideImage(
    const BE::Image::Coordinate &c,
    const BE::Image::Size &dim)
{
	const auto realC = realCoordinate(c, dim);
	return ((realC.first > dim.xSize) ||
	    (realC.second > dim.ySize) ||
	    (realC.first < 0) ||
	    (realC.second < 0));
}

inline std::pair<int64_t, int64_t>
realCoordinate(
    const BE::Image::Coordinate &c,
    const BE::Image::Size &dimensions)
{
	int64_t x = c.x, y = c.y;

	if (x > (UINT32_MAX - dimensions.xSize))
		x = (x - 1) - UINT32_MAX;
	if (y > (UINT32_MAX - dimensions.ySize))
		y = (y - 1) - UINT32_MAX;

	return {x, y};
}

bool
clockwise(
    const std::vector<BE::Image::Coordinate> &coordinates,
    const BE::Image::Size &dimensions)
{
	for (uint64_t i = 0; i < coordinates.size(); ++i) {
		uint64_t j = i + 1;
		if (j == coordinates.size())
			j = 0;

		auto realCoord1 = realCoordinate(coordinates[i], dimensions);
		auto realCoord2 = realCoordinate(coordinates[j], dimensions);

		if (realCoord1.second == realCoord2.second)
			continue;

		return (realCoord1.second > realCoord2.second);
	}

	throw BE::Error::Exception("Cannot determine polygon direction");
}

std::vector<BE::Image::Coordinate>
correctSegmentCoordinates(
    const std::vector<BE::Image::Coordinate> &coordinates,
    const BE::Image::Size &dim)
{
	/* Check if all coordinates are within the image */
	bool necessary{false};
	for (const auto &c : coordinates) {
		if (coordinateOutsideImage(c, dim)) {
			necessary = true;
			break;
		}
	}
	if (!necessary)
		return (coordinates);

	/* Determine placement direction of coordinates */
	bool isClockwise = clockwise(coordinates, dim);

	std::vector<BE::Image::Coordinate> correctedCoordinates;
	for (uint64_t i = 0; i < coordinates.size(); ++i) {
		/* Line segment formed between this and the next coordinate */
		uint64_t j = i + 1;
		if (j == coordinates.size())
			j = 0;

		/*
		 * Only calculate intersection with boundary if necessary
		 */
		/* If both coordinates are inside image */
		if (!coordinateOutsideImage(coordinates[i], dim) &&
		    !coordinateOutsideImage(coordinates[j], dim)) {
			correctedCoordinates.push_back(coordinates[i]);
			continue;
		/* If both coordinates are outside image */
		} else if (coordinateOutsideImage(coordinates[i], dim) &&
		    coordinateOutsideImage(coordinates[j], dim)) {
			continue;
		}

		auto topIntersection = intersectTop(coordinates[i],
		    coordinates[j], dim);
		auto leftIntersection = intersectLeft(coordinates[i],
		    coordinates[j], dim);
		auto bottomIntersection = intersectBottom(
		    coordinates[i], coordinates[j], dim);
		auto rightIntersection = intersectRight(
		    coordinates[i], coordinates[j], dim);

		/* If segment stays within image */
		if (!topIntersection.first && !leftIntersection.first &&
		    !bottomIntersection.first && !rightIntersection.first) {
			correctedCoordinates.push_back(coordinates[i]);
		} else {
			/* Add original coordinate if it's in the image */
			if (!coordinateOutsideImage(coordinates[i], dim))
				correctedCoordinates.push_back(coordinates[i]);

			if (isClockwise) {
				/* Clockwise intersection order: LBTR */
				if (leftIntersection.first)
					correctedCoordinates.push_back(
					    leftIntersection.second);
				if (bottomIntersection.first)
					correctedCoordinates.push_back(
					    bottomIntersection.second);
				if (topIntersection.first)
					correctedCoordinates.push_back(
					    topIntersection.second);
				if (rightIntersection.first)
					correctedCoordinates.push_back(
					    rightIntersection.second);
			} else {
				/* Counter-clockwise intersection order: RBTL */
				if (rightIntersection.first)
					correctedCoordinates.push_back(
					    rightIntersection.second);
				if (bottomIntersection.first)
					correctedCoordinates.push_back(
					    bottomIntersection.second);
				if (topIntersection.first)
					correctedCoordinates.push_back(
					    topIntersection.second);
				if (leftIntersection.first)
					correctedCoordinates.push_back(
					    leftIntersection.second);
			}
		}
	}

	return (correctedCoordinates);
}
