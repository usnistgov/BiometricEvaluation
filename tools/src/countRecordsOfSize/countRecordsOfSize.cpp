/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */

/*
 * Print keys of records in a RecordStore of a particular size.  Useful
 * for finding empty keys.
 */

#include <stdint.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include <be_text.h>
#include <be_io_recordstore.h>

namespace BE = BiometricEvaluation;

static const std::string ARG_STRING = "<RecordStore> [<size> (default = 0)]";

/**
 * @brief
 * Print keys that are sized a particular value.
 *
 * @param rs
 *	Shared pointer to RecordStore.
 * @param targetSize
 *	Keys that are this size will be printed.
 */
void
printKeys(
    std::shared_ptr<BiometricEvaluation::IO::RecordStore> rs,
    const uint64_t targetSize);

int
main(
    int argc,
    char *argv[])
{
	/* Check arguments */
	if ((argc < 2) || (argc > 3)) {
		std::cerr << "Usage: " << argv[0] << " " << ARG_STRING <<
		    std::endl;
		return (1);
	}

	std::shared_ptr<BE::IO::RecordStore> rs;
	try {
		rs = BE::IO::RecordStore::openRecordStore(
	    	    std::string(argv[1]), BE::IO::Mode::ReadOnly);
	} catch (BE::Error::Exception &e) {
		std::cerr << e.what() << std::endl;
		return (1);
	}

	switch (argc) {
	case 2:
		printKeys(rs, 0);
		break;
	case 3:
		printKeys(rs, atoi(argv[2]));
		break;
	default:
		std::cerr << "Usage: " << argv[0] << " " << ARG_STRING <<
		    std::endl;
		return (1);
	}
	
	return (0);
}

void
printKeys(
    std::shared_ptr<BiometricEvaluation::IO::RecordStore> rs,
    const uint64_t targetSize)
{
	std::string key;

	for (;;) {
		try {
			key = rs->sequenceKey();
		} catch (BE::Error::ObjectDoesNotExist) {
			break;
		} catch (BE::Error::Exception &e) {
			std::cerr << e.what() << std::endl;
			return;
		}

		if (rs->length(key) == targetSize)
			std::cout << key << std::endl;
	}
}

