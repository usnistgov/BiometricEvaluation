/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */

#include <fstream>
#include <iostream>

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>

#ifdef USE_DBRECSTORE
#include <be_io_dbrecstore.h>
#else
#include <be_io_filerecstore.h>
#endif

#include "toolsutils.h"

using namespace BiometricEvaluation;
using namespace BiometricEvaluation::IO;

void 
PrintUsage(char* argv[])
{
	printf("Usage: %s [-d parent_dir] <name>\n\n", argv[0]);
	printf("   name          = Must specify record store name\n\n");
	printf("Options:\n");
	printf("   -d parent_dir = Parent directory of record store\n\n");
	
	exit(EXIT_FAILURE);
}

/*
 * This program is used to list the keys in a record store.
 */
int 
main (int argc, char* argv[]) 
{
	DBRecordStore *rs = NULL;
	string sName, sParentDir, sKey;
	int c, iRetCode = EXIT_FAILURE;
	
	opterr = 0;
	
	/* Process optional command line arguments */
	while ((c = getopt (argc, argv, "d:")) != -1)
		switch (c) {
		case 'd':
			sParentDir.assign(optarg);
			break;
			
		default:
			PrintUsage(argv);
		}
		
	if ((argc - optind) != 1)
		PrintUsage(argv);
	
	/* Process mandatory command line arguments */
	sName.assign(argv[optind]);
	
	/* Open record store */
	try {
#ifdef USE_DBRECSTORE
		rs = new DBRecordStore(sName, sParentDir);
#else
		rs = new FileRecordStore(sName, sParentDir);
#endif
	} catch (Error::ObjectDoesNotExist) {
		ERR_OUT("Failed to open record store %s", sName.c_str());
	} catch (Error::StrategyError e) {
		ERR_OUT("A strategy error occurred: %s", e.getInfo().c_str());
	}
	
	printf("%d keys found in record store:\n\n", rs->getCount());
	
	/* List record store keys */
	for (;;) {
		try {
			rs->sequence(sKey, NULL);
		} catch (Error::ObjectDoesNotExist) {
			break;
		} catch (Error::StrategyError e) {
			ERR_OUT("A strategy error occurred: %s", e.getInfo().c_str());
		}
		printf("%s\n", sKey.c_str());
	}
	
	iRetCode = EXIT_SUCCESS;
	
err_out:
	if (rs)
		delete rs;

	return iRetCode;
}
