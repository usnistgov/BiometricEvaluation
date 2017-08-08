/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */

/** Arguments parsed from command line */
struct Arguments
{
	/** Input file path */
	std::string inputEFSFilePath{};
	/** Output file path */
	std::string outputINCITSFilePath{};
	/** Dimensions of image represented by input */
	BiometricEvaluation::Image::Size size{};
	/** Resolution of image represented by input */
	BiometricEvaluation::Image::Resolution resolution{500, 500,
	    BiometricEvaluation::Image::Resolution::Units::PPI};
	/** Finger position of image represented by input */
	BiometricEvaluation::Finger::Position fingerPosition{
	    BiometricEvaluation::Finger::Position::Unknown};
	/** Impression type of image represented by input */
	BiometricEvaluation::Finger::Impression impressionType{
	    BiometricEvaluation::Finger::Impression::Unknown};
	/** If multiple EFS records in input, which to use (1 based) */
	uint64_t recordNumber{1};
};

/**
 * @brief
 * Parse command line arguments.
 *
 * @param argc
 * argc from main().
 * @param argv
 * argv from main().
 *
 * @return
 * Parsed arguments.
 *
 * @throw BE::Error::StrategyError
 * Invalid arguments parsed.
 */
Arguments
procargs(
    int argc,
    char *argv[]);

/**
 * @brief
 * Retrieve EFS data from a record.
 *
 * @param path
 * Path to AN2K file from which to retrieve EFS data.
 * @param recordNumber
 * EFS record to retrieve, if multiple records in file
 *
 * @return
 * Pointer to EFS data
 *
 * @throw BE::Error::StrategyError
 * No EFS data at recordNumber.
 */
std::shared_ptr<BiometricEvaluation::Feature::AN2K11EFS::ExtendedFeatureSet>
getEFS(
    const std::string &path,
    const uint64_t recordNumber);

/**
 * @brief
 * Convert EFS MinutiaPoint representation into INCITS MinutiaPoint
 * representation.
 * @details
 * Converts theta values into 2-degree units and coordinates into pixel values
 * relative to origin.
 *
 * @param efs
 * EFS data to convert (@see getEFS())
 * @param resolution
 * Resolution of image `efs` is based on (for coordinate conversion).
 *
 * @return
 * INCITS representation of minutia point set from `efs`.
 */
BiometricEvaluation::Feature::MinutiaPointSet
EFSToINCITSMinutia(
    const std::shared_ptr<
        BiometricEvaluation::Feature::AN2K11EFS::ExtendedFeatureSet> &efs,
    const BiometricEvaluation::Image::Resolution &resolution);

/**
 * @brief
 * Convert EFS CorePoint representation into INCITS CorePoint representation.
 *
 * @param efs
 * EFS data to convert (@see getEFS())
 * @param resolution
 * Resolution of image `efs` is based on (for coordinate conversion).
 *
 * @return
 * INCITS representation of CorePoints from `efs`.
 */
BE::Feature::CorePointSet
EFSToINCITSCore(
    const std::shared_ptr<BE::Feature::AN2K11EFS::ExtendedFeatureSet> &efs,
    const BE::Image::Resolution &resolution);

/**
 * @brief
 * Convert EFS DeltaPoint representation into INCITS DeltaPoint representation.
 *
 * @param efs
 * EFS data to convert (@see getEFS())
 * @param resolution
 * Resolution of image `efs` is based on (for coordinate conversion).
 *
 * @return
 * INCITS representation of DeltaPoint from `efs`.
 */
BE::Feature::DeltaPointSet
EFSToINCITSDelta(
    const std::shared_ptr<BE::Feature::AN2K11EFS::ExtendedFeatureSet> &efs,
    const BE::Image::Resolution &resolution);

/**
 * @brief
 * Convert EFS MinutiaRidgeCount representation into INCITS RidgeCount
 * representation.
 *
 * @param efs
 * EFS data to convert (@see getEFS())
 * @param resolution
 * Resolution of image `efs` is based on (for coordinate conversion).
 *
 * @return
 * INCITS representation of MinutiaRidgeCount from `efs`.
 */
BE::Feature::RidgeCountItemSet
EFSToINCITSRidgeCounts(
    const std::shared_ptr<BE::Feature::AN2K11EFS::ExtendedFeatureSet> &efs,
    const BE::Image::Resolution &resolution);

/**
 * @brief
 * Create an INCITS 378 template.
 *
 * @param incitsMinutia
 * Minutia points to encode in template.
 * @param incitsCores
 * Core points to encode in template.
 * @param deltaMinutia
 * Delta points to encode in template.
 * @param dimensions
 * Dimensions to encode in template.
 * @param resolution
 * Resolution to encode in template.
 * @param fgp
 * Finger position to encode in template.
 * @param imp
 * Impression type to encode in template.
 *
 * @return
 * Empty INCITS 378 template.
 */
BiometricEvaluation::Memory::uint8Array
createINCITSTemplate(
    const BiometricEvaluation::Feature::MinutiaPointSet &incitsMinutia,
    const BE::Feature::RidgeCountItemSet &incitsRidgeCounts,
    const BE::Feature::CorePointSet &incitsCores,
    const BE::Feature::DeltaPointSet &incitsDeltas,
    const BiometricEvaluation::Image::Size &dimensions,
    const BiometricEvaluation::Image::Resolution &resolution,
    const BiometricEvaluation::Finger::Position &fgp,
    const BiometricEvaluation::Finger::Impression &imp);


/**
 * @brief
 * Create an INCITS 378 template with 0 minutia.
 *
 * @param dimensions
 * Dimensions to encode in template.
 * @param resolution
 * Resolution to encode in template.
 * @param fgp
 * Finger position to encode in template.
 * @param imp
 * Impression type to encode in template.
 *
 * @return
 * Empty INCITS 378 template.
 */
BiometricEvaluation::Memory::uint8Array
createEmptyTemplate(
    const BiometricEvaluation::Image::Size &dimensions,
    const BiometricEvaluation::Image::Resolution &resolution,
    const BiometricEvaluation::Finger::Position &fgp,
    const BiometricEvaluation::Finger::Impression &imp);
