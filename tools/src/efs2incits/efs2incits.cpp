/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */

#include <getopt.h>

#include <cmath>
#include <iostream>

#include <be_data_interchange_an2k.h>
#include <be_data_interchange_ansi2004.h>
#include <be_memory.h>
#include <be_memory_mutableindexedbuffer.h>
#include <be_feature_an2k11efs.h>
#include <be_io_utility.h>
#include <be_text.h>

#include <efs2incits.h>

namespace BE = BiometricEvaluation;
using namespace BE::Framework::Enumeration;

Arguments
procargs(
    int argc,
    char *argv[])
{
	const std::string usage{"Usage: " + std::string(argv[0]) + "\n"
	    "\t-i\tinput.an2\n"
	    "\t-o\toutput.378\n"
	    "\t-w\twidth\n"
	    "\t-h\theight\n"
	    "\t-x\tX resolution\n"
	    "\t-y\tY resolution\n"
	    "\t-u\tResolution units\n"
	    "\t-f\tFinger position\n"
	    "\t-t\tImpression type\n"
	    "\t-v\tView number"};

	if (argc == 1)
		throw BE::Error::StrategyError(usage);

	Arguments args{};

	uint32_t w{}, h{};
	double xRes{}, yRes{};
	BE::Image::Resolution::Units units{};

	char c{};
	while ((c = getopt(argc, argv, "i:o:w:h:x:y:u:f:t:v:")) != EOF) {
		switch (c) {
		case 'i': /* Input */
			args.inputEFSFilePath = optarg;
			break;
		case 'o': /* Output */
			args.outputINCITSFilePath = optarg;
			break;
		case 'w': /* Width */
			w = std::stol(optarg);
			break;
		case 'h': /* Height */
			h = std::stol(optarg);
			break;
		case 'x': /* X resolution */
			xRes = std::stod(optarg);
			break;
		case 'y': /* Y resolution */
			yRes = std::stod(optarg);
			break;
		case 'u': /* Resolution units */
			/* Numeric conversion */
			try {
				units = to_enum<BE::Image::Resolution::Units>(
				    std::stoi(optarg));
				break;
			} catch (std::invalid_argument) {}

			/* String conversion */
			try {
				units = to_enum<BE::Image::Resolution::Units>(
				    optarg);
			} catch (const BE::Error::Exception &e) {
				throw BE::Error::StrategyError("Invalid value "
				    "for -u: " + std::string(optarg) + " (" +
				    e.whatString() + ")");
			}
			break;
		case 'f': /* Finger position */
			/* Numeric conversion */
			try {
				args.fingerPosition =
				    to_enum<BE::Finger::Position>(
				    std::stoi(optarg));
				break;
			} catch (std::invalid_argument) {}

			/* String conversion */
			try {
				args.fingerPosition =
				    to_enum<BE::Finger::Position>(optarg);
			} catch (const BE::Error::Exception &e) {
				throw BE::Error::StrategyError("Invalid value "
				    "for -f: " + std::string(optarg) + " (" +
				    e.whatString() + ")");
			}
			break;
		case 't': /* Impression type */
			/* Numeric conversion */
			try {
				args.impressionType =
				    to_enum<BE::Finger::Impression>(
				    std::stoi(optarg));
				break;
			} catch (std::invalid_argument) {}

			/* String conversion */
			try {
				args.impressionType =
				    to_enum<BE::Finger::Impression>(optarg);
			} catch (const BE::Error::Exception &e) {
				throw BE::Error::StrategyError("Invalid value "
				    "for -t: " + std::string(optarg) + " (" +
				    e.whatString() + ")");
			}
			break;
		case 'v': /* View number */
			try {
				args.recordNumber = std::stol(optarg);
			} catch (...) {
				throw BE::Error::StrategyError("Invalid value "
				    "for -v: " + std::string(optarg));
			}
			break;
		default:
			throw BE::Error::StrategyError(usage);
		}
	}

	args.size = {w, h};
	args.resolution = {xRes, yRes, units};

	if (args.outputINCITSFilePath.empty())
		throw BE::Error::StrategyError("Missing -o\n\n" + usage);
	if (args.inputEFSFilePath.empty())
		throw BE::Error::StrategyError("Missing -i\n\n" + usage);

	return (args);
}

int
main(
    int argc,
    char *argv[])
{
	Arguments args{};
	try {
		args = procargs(argc, argv);
	} catch (const BE::Error::Exception &e) {
		std::cerr << e.whatString() << std::endl;
		return (EXIT_FAILURE);
	}

	std::shared_ptr<BE::Feature::AN2K11EFS::ExtendedFeatureSet> efs{};
	try {
		efs = getEFS(args.inputEFSFilePath, args.recordNumber);
	} catch (const BE::Error::Exception &e) {
		std::cerr << e.whatString() << std::endl;
		return (EXIT_FAILURE);
	}

	try {
		const auto incitsTemplate = createINCITSTemplate(
		    EFSToINCITSMinutia(efs, args.resolution),
		    EFSToINCITSRidgeCounts(efs, args.resolution),
		    EFSToINCITSCore(efs, args.resolution),
		    EFSToINCITSDelta(efs, args.resolution), args.size,
		    args.resolution, args.fingerPosition, args.impressionType);

		BE::IO::Utility::writeFile(incitsTemplate,
		    args.outputINCITSFilePath);
	} catch (const BE::Error::Exception &e) {
		std::cerr << e.whatString() << std::endl;
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}

std::shared_ptr<BE::Feature::AN2K11EFS::ExtendedFeatureSet>
getEFS(
    const std::string &path,
    const uint64_t recordNumber)
{
	BE::DataInterchange::AN2KRecord an2k(path);
	const auto minutiaRecords = an2k.getMinutiaeDataRecordSet();
	if (minutiaRecords.size() < recordNumber)
		throw BE::Error::Exception("Not enough records to "
		    "retrieve record number " +
		    std::to_string(recordNumber));

	return (minutiaRecords[recordNumber - 1].getAN2K11EFS());
}

BE::Feature::MinutiaPointSet
EFSToINCITSMinutia(
    const std::shared_ptr<BE::Feature::AN2K11EFS::ExtendedFeatureSet> &efs,
    const BE::Image::Resolution &resolution)
{
	/* Standard specifies use of this constant, not (1.0 / 2540.0) */
	constexpr double tenUMToIn = 0.00039;
	/* Resolution in PPI */
	const auto resInch =
	    resolution.toUnits(BE::Image::Resolution::Units::PPI);

	/* Get ROI offsets in pixels */
	const auto roi = efs->getImageInfo().roi;
	const auto xROIOffsetPx = roi.horzOffset * tenUMToIn * resInch.xRes;
	const auto yROIOffsetPx = roi.vertOffset * tenUMToIn * resInch.yRes;

	BE::Feature::MinutiaPointSet incitsMinutia{};
	for (const auto &efsMin : efs->getMPS()) {
		BE::Feature::MinutiaPoint incitsMin{};

		/*
		 * Copy the base class members.
		 */
		incitsMin.index = efsMin.index;
		incitsMin.has_type = efsMin.has_type;
		incitsMin.type = efsMin.type;
		incitsMin.has_quality = efsMin.has_quality;
		incitsMin.quality = efsMin.quality;

		/* INCITS theta are in units of 2 degrees */
		incitsMin.theta = std::ceil(efsMin.theta / 2.0);

		/* Theta should be ignored when uncertainty is 180 */
		if (efsMin.has_mdu && (efsMin.mdu == 180)) {
			incitsMin.theta = 0;
			incitsMin.type = BE::Feature::MinutiaeType::Other;
		}

		/*
		 * Convert coordinate.
		 */
		incitsMin.coordinate = {
		    static_cast<uint32_t>(std::ceil(xROIOffsetPx +
		    (efsMin.coordinate.x * tenUMToIn * resInch.xRes))),
		    static_cast<uint32_t>(std::ceil(yROIOffsetPx +
		    (efsMin.coordinate.y * tenUMToIn * resInch.yRes)))};

		incitsMinutia.push_back(incitsMin);
	}

	return (incitsMinutia);
}

BE::Feature::CorePointSet
EFSToINCITSCore(
    const std::shared_ptr<BE::Feature::AN2K11EFS::ExtendedFeatureSet> &efs,
    const BE::Image::Resolution &resolution)
{
	/* Standard specifies use of this constant, not (1.0 / 2540.0) */
	constexpr double tenUMToIn = 0.00039;
	/* Resolution in PPI */
	const auto resInch =
	    resolution.toUnits(BE::Image::Resolution::Units::PPI);

	/* Get ROI offsets in pixels */
	const auto roi = efs->getImageInfo().roi;
	const auto xROIOffsetPx = roi.horzOffset * tenUMToIn * resInch.xRes;
	const auto yROIOffsetPx = roi.vertOffset * tenUMToIn * resInch.yRes;

	BE::Feature::CorePointSet cores{};
	for (const auto &efsCore : efs->getCPS())
		cores.emplace_back(
		    static_cast<uint32_t>(std::ceil(xROIOffsetPx +
		    (efsCore.location.x * tenUMToIn * resInch.xRes))),
		    static_cast<uint32_t>(std::ceil(yROIOffsetPx +
		    (efsCore.location.y * tenUMToIn * resInch.yRes))));

	return (cores);
}

BE::Feature::DeltaPointSet
EFSToINCITSDelta(
    const std::shared_ptr<BE::Feature::AN2K11EFS::ExtendedFeatureSet> &efs,
    const BE::Image::Resolution &resolution)
{
	/* Standard specifies use of this constant, not (1.0 / 2540.0) */
	constexpr double tenUMToIn = 0.00039;
	/* Resolution in PPI */
	const auto resInch =
	    resolution.toUnits(BE::Image::Resolution::Units::PPI);

	/* Get ROI offsets in pixels */
	const auto roi = efs->getImageInfo().roi;
	const auto xROIOffsetPx = roi.horzOffset * tenUMToIn * resInch.xRes;
	const auto yROIOffsetPx = roi.vertOffset * tenUMToIn * resInch.yRes;

	BE::Feature::DeltaPointSet deltas{};
	for (const auto &efsDelta : efs->getDPS()) {
		BE::Feature::DeltaPoint delta(
		    static_cast<uint32_t>(std::ceil(xROIOffsetPx +
		    (efsDelta.location.x * tenUMToIn * resInch.xRes))),
		    static_cast<uint32_t>(std::ceil(yROIOffsetPx +
		    (efsDelta.location.y * tenUMToIn * resInch.yRes))));

		if (efsDelta.has_dup && efsDelta.has_dlf && efsDelta.has_drt) {
			delta.has_angle = true;
			delta.angle1 = std::ceil(efsDelta.dup / 2.0);
			delta.angle2 = std::ceil(efsDelta.dlf / 2.0);
			delta.angle3 = std::ceil(efsDelta.drt / 2.0);
		}

		deltas.emplace_back(delta);
	}

	return (deltas);
}

BE::Feature::RidgeCountItemSet
EFSToINCITSRidgeCounts(
    const std::shared_ptr<BE::Feature::AN2K11EFS::ExtendedFeatureSet> &efs,
    const BE::Image::Resolution &resolution)
{
	const auto mrci = efs->getMRCI();
	if (!mrci.has_mrcs)
		return {};

	BE::Feature::RidgeCountExtractionMethod method{
	    BE::Feature::RidgeCountExtractionMethod::NonSpecific};
	if (mrci.has_mra) {
		switch (mrci.mra) {
		case BE::Feature::AN2K11EFS::MinutiaeRidgeCountAlgorithm::
		    OCTANT:
			method = BE::Feature::RidgeCountExtractionMethod::
			    EightNeighbor;
			break;
		case BE::Feature::AN2K11EFS::MinutiaeRidgeCountAlgorithm::
		    QUADRANT:
			method = BE::Feature::RidgeCountExtractionMethod::
			    FourNeighbor;
			break;
		case BE::Feature::AN2K11EFS::MinutiaeRidgeCountAlgorithm::EFTS7:
			method = BE::Feature::RidgeCountExtractionMethod::Other;
			break;
		}
	}

	BE::Feature::RidgeCountItemSet ridgeCounts{};
	for (const auto &efsRC : mrci.mrcs)
		ridgeCounts.emplace_back(method, efsRC.mia, efsRC.mib,
		    efsRC.mir);

	return (ridgeCounts);
}

BE::Memory::uint8Array
createEmptyTemplate(
    const BE::Image::Size &dimensions,
    const BE::Image::Resolution &resolution,
    const BE::Finger::Position &fgp,
    const BE::Finger::Impression &imp)
{
        BE::Memory::uint8Array nullTmpl(32);
        BE::Memory::MutableIndexedBuffer buf(nullTmpl);

        buf.pushBeU32Val(0x464D5200);
        buf.pushBeU32Val(0x20323000);
	buf.pushBeU16Val(nullTmpl.size());
        buf.pushBeU32Val(0);
        buf.pushBeU16Val(0);
        buf.pushBeU16Val(dimensions.xSize);
        buf.pushBeU16Val(dimensions.ySize);
        buf.pushBeU16Val(std::ceil(
            resolution.toUnits(BE::Image::Resolution::Units::PPCM).xRes));
        buf.pushBeU16Val(std::ceil(
            resolution.toUnits(BE::Image::Resolution::Units::PPCM).yRes));
        buf.pushU8Val(0);

	/* Finger view */
	buf.pushU8Val(to_int_type(fgp));
        buf.pushU8Val(0);
	buf.pushU8Val(to_int_type(imp));
        buf.pushU8Val(0);
        buf.pushU8Val(0);

        /* Extended data */
        buf.pushBeU16Val(0);

        return (nullTmpl);
}

BE::Memory::uint8Array
createINCITSTemplate(
    const BE::Feature::MinutiaPointSet &incitsMinutia,
    const BE::Feature::RidgeCountItemSet &incitsRidgeCounts,
    const BE::Feature::CorePointSet &incitsCores,
    const BE::Feature::DeltaPointSet &incitsDeltas,
    const BE::Image::Size &dimensions,
    const BE::Image::Resolution &resolution,
    const BE::Finger::Position &fgp,
    const BE::Finger::Impression &imp)
{
	auto incits = BE::DataInterchange::ANSI2004Record(
	    createEmptyTemplate(dimensions, resolution, fgp, imp),
	    BE::Memory::uint8Array());
	incits.setMinutia({{incitsMinutia, incitsRidgeCounts, incitsCores,
	    incitsDeltas}});
	return (incits.getFMR());
}

