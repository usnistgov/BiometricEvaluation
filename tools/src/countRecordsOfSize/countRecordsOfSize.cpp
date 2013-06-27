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
#include <tr1/memory>

#include <be_text.h>
#include <be_io_recordstore.h>

using namespace std;
using namespace BiometricEvaluation;

static const string ARG_STRING = "<RecordStore> [<size> (default = 0)]";

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
    tr1::shared_ptr<IO::RecordStore> rs,
    const uint64_t targetSize);

int
main(
    int argc,
    char *argv[])
{
	/* Check arguments */
	if ((argc < 2) || (argc > 3)) {
		cerr << "Usage: " << argv[0] << " " << ARG_STRING << endl;
		return (1);
	}

	tr1::shared_ptr<IO::RecordStore> rs;
	try {
		rs = IO::RecordStore::openRecordStore(
	    	    Text::filename(argv[1]), Text::dirname(argv[1]),
		    IO::READONLY);
	} catch (Error::Exception &e) {
		cerr << e.getInfo() << endl;
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
		cerr << "Usage: " << argv[0] << " " << ARG_STRING << endl;
		return (1);
	}
	
	return (0);
}

void
printKeys(
    tr1::shared_ptr<IO::RecordStore> rs,
    const uint64_t targetSize)
{
	uint64_t size;
	string key;

	for (;;) {
		try {
			size = rs->sequence(key, NULL);
		} catch (Error::ObjectDoesNotExist) {
			break;
		} catch (Error::Exception &e) {
			cerr << e.getInfo() << endl;
			return;
		}

		if (size == targetSize)
			cout << key << endl;
	}
}

