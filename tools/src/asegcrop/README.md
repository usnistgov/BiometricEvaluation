ASEG Crop
=========

`asegcrop` is a tool to create separate images from ANSI/NIST-ITL slap
imagery, given alternate finger segment position (ASEG/14.025) coordinates.

Requirements
------------

This tool requires [Biometric Evaluation Framework]
(https://github.com/usnistgov/BiometricEvaluation) to parse ANSI/NIST-ITL files,
and [OpenCV](http://opencv.org) to perform image rotation.

How It Works
------------

The tool finds a rectangle big enough to hold all coordinates specified in Type
14, Field 25. Pixels outside the polygon created by the original coordinates are
colored white. If the rotation (`-r`) option is specified, the image is rotated
by the angle formed between the last and first alternate finger segment position
coordinates. This option is most useful when there are exactly four coordinates
forming a rectangle, such as when the ASEG coordinates are rotated finger
segment position (SEG/14.021) coordinates.

### Output

Files are output as raw 8-bit grayscale images in the current directory, with
their filename in the format:

[*basename*].[*slap FGP*].[*ASEG FGP*].[*width*]x[*height*].gray

**Note**: Although files are output as raw, image rotation is a lossy operation,
and interpolation will occur. Interpolation methods can be changed in
[asegcrop.cpp](https://github.com/usnistgov/BiometricEvaluation/blob/master/tools/src/asegcrop/asegcrop.cpp).

Usage
-----

    # Crop and save images from left and right slaps
    $ asegcrop left_and_right_slaps.an2
    Wrote left_and_right_slaps.an2.14.7.416x574.gray
    Wrote left_and_right_slaps.an2.14.8.370x501.gray
    Wrote left_and_right_slaps.an2.14.9.390x564.gray
    Wrote left_and_right_slaps.an2.14.10.299x338.gray
    Wrote left_and_right_slaps.an2.13.2.360x532.gray
    Wrote left_and_right_slaps.an2.13.3.358x568.gray
    Wrote left_and_right_slaps.an2.13.4.350x536.gray
    Wrote left_and_right_slaps.an2.13.5.337x330.gray

    # Attempt to rotate the cropped images upright
    $ asegcrop -r left_and_right_slaps.an2
    Wrote left_and_right_slaps.an2.14.7.355x535.gray
    Wrote left_and_right_slaps.an2.14.8.312x505.gray
    Wrote left_and_right_slaps.an2.14.9.329x529.gray
    Wrote left_and_right_slaps.an2.14.10.260x341.gray
    Wrote left_and_right_slaps.an2.13.2.341x519.gray
    Wrote left_and_right_slaps.an2.13.3.337x555.gray
    Wrote left_and_right_slaps.an2.13.4.354x537.gray
    Wrote left_and_right_slaps.an2.13.5.326x331.gray

Communication
-------------

If you found a bug and can provide steps to reliably reproduce it, or if you
have a feature request, please
[open an issue](https://github.com/usnistgov/BiometricEvaluation/issues). Other
questions may be addressed to the [project maintainers]
(mailto:beframework@nist.gov).

License
-------

This tool uses code based on [`pnpoly`]
(https://www.ecse.rpi.edu/Homepages/wrf/Research/Short_Notes/pnpoly.html) by
Wm. Randolph Franklin, whose open source license appears in
[asegcrop.h](https://github.com/usnistgov/BiometricEvaluation/blob/master/tools/src/asegcrop/asegcrop.h).
All other items in this repository are released in the public domain. See the
[LICENSE]
(https://github.com/usnistgov/BiometricEvaluation/blob/master/LICENSE.md)
for details.
