/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties.  Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain.  NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 *
 * pointInPolygon() is modified from Wm. Randolph Franklin's pnpoly(),
 * distributed under the license below.
 */

#ifndef ASEGCROP_H_
#define ASEGCROP_H_

/**
 * @brief
 * Write cropped segments of a finger capture to disk.
 * @details
 * Writes files in the form of:
 * [basename name].[capture FGP].[cropped FGP].[width]x[height].gray
 *
 * @param name
 * AN2K file containing ASEGed captures.
 * @param segments
 * Return value from cropASEG()
 */
void
writeSegments(
    const std::string &name,
    const std::vector<std::pair<BiometricEvaluation::Finger::Position,
    BiometricEvaluation::Image::Raw>>
    &segments);

/**
 * @brief
 * Crop an image for all ASEG boxes found.
 *
 * @param capture
 * Finger capture to be cropped.
 * @param rotate
 * Whether or not to attempt to rotate cropped fingerprints into an upright
 * position.
 *
 * @return
 * A vector of pairs of finger positions and their associated cropped
 * raw image.
 */
std::vector<std::pair<BiometricEvaluation::Finger::Position,
BiometricEvaluation::Image::Raw>>
cropASEG(
    const BiometricEvaluation::Finger::AN2KViewCapture &capture,
    bool rotate);

/**
 * @brief
 * Crop a single finger position with ASEG boxes from a finger capture.
 *
 * @param capture
 * Finger capture to be cropped.
 * @param segment
 * A single ASEG to crop from capture.
 * @param rotate
 * Whether or not to attempt to rotate cropped fingerprints into an upright
 * position.
 *
 * @return
 * Raw image of segment cropped from capture.
 */
BiometricEvaluation::Image::Raw
cropSingleASEG(
    const BiometricEvaluation::Finger::AN2KViewCapture &capture,
    const BiometricEvaluation::Finger::AN2KViewCapture::FingerSegmentPosition
    &segment,
    bool rotate);

/**
 * @brief
 * Trim whitespace surrounding an 8-bit grayscale image.
 *
 * @param data
 * AutoArray of 8-bit grayscale decompressed raw image data.
 * @param dims
 * Dimensions of the image represented in data.
 * @param res
 * Resolution of the image represented in data.
 * @param hasAlphaChannel
 * Presence of alpha channel.
 *
 * @return
 * Raw image of data without white border.
 */
BiometricEvaluation::Image::Raw
trim(
    const BiometricEvaluation::Memory::uint8Array &data,
    const BiometricEvaluation::Image::Size &dims,
    const BiometricEvaluation::Image::Resolution &res,
    const bool hasAlphaChannel);


/**
 * @brief
 * Rotate an 8-bit grayscale image.
 * 
 * @param image
 * Image to rotate.
 * @param degrees
 * Degrees to rotate.
 *
 * @return
 * Rotated version of image as a Raw.
 */
BiometricEvaluation::Image::Raw
rotateImage(
    const std::shared_ptr<BiometricEvaluation::Image::Image> image,
    const float degrees);

/*
 * Geometric functions.
 */

/**
 * @brief
 * Determine if a point falls within a polygon.
 * @details
 * Copyright (c) 1970-2003, Wm. Randolph Franklin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimers.
 * 2. Redistributions in binary form must reproduce the above copyright notice
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. The name of W. Randolph Franklin may not be used to endorse or promote
 *    products derived from this Software without specific prior written
 *    permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 * @param point
 * The point to test.
 * @param coordinates
 * Coordinates making up the polygon.
 *
 * @return
 * true if point appears to be in the polygon, false otherwise.
 *
 * @url
 * https://www.ecse.rpi.edu/Homepages/wrf/Research/Short_Notes/pnpoly.html
 */
bool
pointInPolygon(
    const BiometricEvaluation::Image::Coordinate &point,
    const std::vector<BiometricEvaluation::Image::Coordinate> &coordinates);

/**
 * @brief
 * Linear istance formula.
 *
 * @param x1
 * X coordinate of first point.
 * @param y2
 * Y coordinate of first point.
 * @param x1
 * X coordinate of second point.
 * @param y2
 * Y coordinate of second point.
 *
 * @return
 * Distance between (x1, y1) and (x2, y2).
 */
inline float
distance(
    float x1,
    float y1,
    float x2,
    float y2);

/**
 * @brief
 * Whether or not a line segment intersects the bottom border of an image.
 *
 * @param point1
 * An endpoint of the line segment to test.
 * @param point2
 * The other end of the line segment to test.
 * @param dim
 * The dimensions of the image.
 *
 * @return
 * A pair consisting of a boolean on whether or not there is an intersection
 * and the point of the intersection if pair.first is true.
 */
std::pair<bool, BiometricEvaluation::Image::Coordinate>
intersectBottom(
    const BiometricEvaluation::Image::Coordinate &point1,
    const BiometricEvaluation::Image::Coordinate &point2,
    const BiometricEvaluation::Image::Size &dim);

/**
 * @brief
 * Whether or not a line segment intersects the top border of an image.
 *
 * @param point1
 * An endpoint of the line segment to test.
 * @param point2
 * The other end of the line segment to test.
 * @param dim
 * The dimensions of the image.
 *
 * @return
 * A pair consisting of a boolean on whether or not there is an intersection
 * and the point of the intersection if pair.first is true.
 */
std::pair<bool, BiometricEvaluation::Image::Coordinate>
intersectTop(
    const BiometricEvaluation::Image::Coordinate &point1,
    const BiometricEvaluation::Image::Coordinate &point2,
    const BiometricEvaluation::Image::Size &dim);

/**
 * @brief
 * Whether or not a line segment intersects the right border of an image.
 *
 * @param point1
 * An endpoint of the line segment to test.
 * @param point2
 * The other end of the line segment to test.
 * @param dim
 * The dimensions of the image.
 *
 * @return
 * A pair consisting of a boolean on whether or not there is an intersection
 * and the point of the intersection if pair.first is true.
 */
std::pair<bool, BiometricEvaluation::Image::Coordinate>
intersectRight(
    const BiometricEvaluation::Image::Coordinate &point1,
    const BiometricEvaluation::Image::Coordinate &point2,
    const BiometricEvaluation::Image::Size &dim);

/**
 * @brief
 * Whether or not a line segment intersects the left border of an image.
 *
 * @param point1
 * An endpoint of the line segment to test.
 * @param point2
 * The other end of the line segment to test.
 * @param dim
 * The dimensions of the image.
 *
 * @return
 * A pair consisting of a boolean on whether or not there is an intersection
 * and the point of the intersection if pair.first is true.
 */
std::pair<bool, BiometricEvaluation::Image::Coordinate>
intersectLeft(
    const BiometricEvaluation::Image::Coordinate &point1,
    const BiometricEvaluation::Image::Coordinate &point2,
    const BiometricEvaluation::Image::Size &dim);

/**
 * @brief
 * Calculate the intersection between two lines (not line segments) formed
 * by a collection of points.
 *
 * @param l1p1
 * A point on the first line.
 * @param l1p2.
 * A second point on the first line.
 * @param l2p1
 * A point on the second line.
 * @param l2p1
 * A second point on the second line.
 *
 * @return
 * A pair of a boolean indicating whether or not lines intersect, and the point
 * of intersection when true. The intersection point is undefined if pair.first
 * is false.
 *
 * @note
 * Although negative intersections can be calculated, one must take into account
 * the unsigned integer values stored in Coordinate.
 * @note
 * Fractional intersection points are rounded due to being stored in an
 * unsigned integer value.
 */
std::pair<bool, BiometricEvaluation::Image::Coordinate>
lineIntersection(
    const BiometricEvaluation::Image::Coordinate &l1p1,
    const BiometricEvaluation::Image::Coordinate &l1p2,
    const BiometricEvaluation::Image::Coordinate &l2p1,
    const BiometricEvaluation::Image::Coordinate &l2p2,
    const BiometricEvaluation::Image::Size &dim);

/*
 * Coordinate information.
 */

/**
 * @brief
 * Obtain if a coordinate falls outside the dimensions of an image.
 * @details
 * Corrects for signed values being treated as unsigned.
 *
 * @param c
 * The coordinate in question.
 * @param dim
 * The dimensions of the image.
 *
 * @return
 * true if c falls outside the dimensions of the image, false otherwise.
 */
bool
coordinateOutsideImage(
    const BiometricEvaluation::Image::Coordinate &c,
    const BiometricEvaluation::Image::Size &dim);

/**
 * @brief
 * Correct for signed values in an unsigned Coordinate.
 *
 * @param c
 * Coordinate to parse.
 * @param dimensions
 * Dimensions of the image.
 *
 * @return
 * Pair of signed X and Y coordinates representing the real value of c.
 */
std::pair<int64_t, int64_t>
realCoordinate(
    const BiometricEvaluation::Image::Coordinate &c,
    const BiometricEvaluation::Image::Size &dimensions);

/**
 * @brief
 * Determine the placement order of ASEG coordinates.
 *
 * @param coords
 * ASEG coordinates.
 *
 * @return
 * true if the coordinates appear to be placed in a clockwise manner.
 */
bool
clockwise(
    const std::vector<BiometricEvaluation::Image::Coordinate> &coordinates,
    const BiometricEvaluation::Image::Size &dimensions);

/**
 * @brief
 * Convert ASEG coordinates that are outside of an image's bounds to be points
 * at which the line segments intersect the bounds.
 *
 * @param coordinates
 * ASEG coordinates.
 * @param dim
 * Dimensions of the image.
 *
 * @return
 * A set of coordinates entirely inside the image bounds representing the
 * enclosing the same image space as coordinates.
 */
std::vector<BiometricEvaluation::Image::Coordinate>
correctSegmentCoordinates(
    const std::vector<BiometricEvaluation::Image::Coordinate> &coordinates,
    const BiometricEvaluation::Image::Size &dim);

#endif /* ASEGCROP_H_ */
