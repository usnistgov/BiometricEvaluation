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
#include <be_io_recordstore.h>
#include "toolsutils.h"


using namespace BiometricEvaluation;
using namespace BiometricEvaluation::IO;

void 
PrintUsage(char* argv[])
{
	printf("Usage: %s [-i input_dir] [-o output_dir] [-r range | -k key] <name>\n\n", argv[0]);
	printf("   name          = Must specify record store name. If '-r' or '-k' option is\n");
	printf("                   not used then all records in this record store are dumped.\n\n");
	printf("Options:\n");
	printf("   -i input_dir  = Input record store directory; default is current dir\n");
	printf("   -o output_dir = Directory to dump contents of record store to; default is\n");
	printf("                   current dir\n");
	printf("   -r range      = Range of records to dump, 1-based index (e.g., 1-30); cannot\n");
	printf("                   be used with '-k' option\n");
	printf("   -k key        = Key name of record to dump; cannot be used with '-r' option\n\n");
	
	exit(EXIT_FAILURE);
}

/*
 * This program is used to dump the contents of a record store.
 */
int 
main (int argc, char* argv[]) 
{
	std::tr1::shared_ptr<RecordStore>rs;
	FILE *fp = NULL;
	unsigned char *pBuffer = NULL;
	string sName, sInputDir, sOutputDir, sSearchKey, sKey, sOutputFile;
	unsigned int uiMin, uiMax;
	uint64_t ui64Length, ui64BytesRead;
	struct stat sb;
	int c, iRetCode = EXIT_FAILURE;
	bool ropt = false;
	bool kopt = false;
	
	opterr = 0;
	
	/* Process optional command line arguments */
	while ((c = getopt (argc, argv, "i:o:r:k:")) != -1)
		switch (c) {
		case 'i':
			sInputDir.assign(optarg);
			break;
			
		case 'o':
			sOutputDir.assign(optarg);
			if (optarg[strlen(optarg)-1] != '/')
				sOutputDir += "/";
			break;
			
		case 'r':
			if (sscanf(optarg, "%u-%u", &uiMin, &uiMax) != 2)
				PrintUsage(argv);
			else {
				if (uiMin == 0 || uiMax == 0 || uiMin > uiMax)
					PrintUsage(argv);
			}
			ropt = true;
			break;
			
		case 'k':
			sSearchKey.assign(optarg);
			kopt = true;
			break;
			
		default:
			PrintUsage(argv);
		}
		
	if ((argc - optind) != 1)
		PrintUsage(argv);
	
	/* Both '-r' and '-k' options cannot be specified at the same time */
	if (ropt && kopt)
		PrintUsage(argv);
		
	/* If necessary, create output directory */
	if (!sOutputDir.empty()) {
	
		if (stat(sOutputDir.c_str(), &sb) == 0) {
			if (!S_ISDIR(sb.st_mode))
				ERR_OUT("Cannot create output directory! A file with the same name already exists.");
		} else {
			if (mkdir(sOutputDir.c_str(), S_IRWXU) != 0)
				ERR_OUT("Failed to create output directory!");
		}
	} else
		sOutputDir.assign("./");
	
	/* Process mandatory command line arguments */
	sName.assign(argv[optind]);

	/* Open record store */
	try {
		rs = RecordStore::openRecordStore(sName, sInputDir, READONLY);
	} catch (Error::ObjectDoesNotExist) {
		ERR_OUT("Failed to open record store %s!", sName.c_str());
	} catch (Error::StrategyError e) {
		ERR_OUT("A strategy error occurred: %s", e.getInfo().c_str());
	}
	
	/* Dump specific record */
	if (kopt) {
		try {
			ui64Length = rs->length(sSearchKey);
			
			sOutputFile = sOutputDir + sSearchKey;
			
			/* Dump specified record using the key for the filename */
			fp = fopen(sOutputFile.c_str(), "wb");
			if (fp == NULL)
				ERR_OUT("Failed to create output file %s!", sOutputFile.c_str());
			
			if (ui64Length > 0) {
				pBuffer = new unsigned char[ui64Length];
				if (pBuffer == NULL)
					ERR_OUT("Failed to allocate buffer to read record! Record size = %u.", ui64Length);
					
				ui64BytesRead = rs->read(sSearchKey, pBuffer);
				if (ui64BytesRead != ui64Length)
					ERR_OUT("Number of bytes read from record does not match expected bytes! Read %u, expected %u.", ui64BytesRead, ui64Length);
					
				fwrite(pBuffer, 1, ui64BytesRead, fp);
			}
		} catch (Error::ObjectDoesNotExist) {
			ERR_OUT("Failed to locate record '%s' in record store!", sSearchKey.c_str());
		} catch (Error::StrategyError e) {
			ERR_OUT("A strategy error occurred: %s", e.getInfo().c_str());
		}
	} else {
		unsigned int uiTotalRecords = rs->getCount();
		unsigned int uiFirst, uiLast, uiCurr = 1;
		
		if (uiTotalRecords == 0)
			ERR_OUT("Empty record store -- nothing to dump!");
		
		/* If dumping range of keys, make sure start and stop index are valid */
		if (ropt) {	
			if (uiMin > uiTotalRecords)
				ERR_OUT("Invalid start index. Record store contains only %u records!", uiTotalRecords);
				
			if (uiMax > uiTotalRecords)
				ERR_OUT("Invalid stop index. Record store contains only %u records!", uiTotalRecords);
				
			uiFirst = uiMin;
			uiLast = uiMax;
		} else {
			uiFirst = 1;
			uiLast = uiTotalRecords;
		}
		
		/* Dump range of records */
		for (;;) {
			try {
				ui64Length = rs->sequence(sKey, NULL);
				
				if (uiCurr >= uiFirst) {
					sOutputFile = sOutputDir + sKey;
					
					/* Dump record using the key for the filename */
					fp = fopen(sOutputFile.c_str(), "wb");
					if (fp == NULL)
						ERR_OUT("Failed to create output file %s!", sOutputFile.c_str());
					
					if (ui64Length > 0) {
						pBuffer = new unsigned char[ui64Length];
						if (pBuffer == NULL)
							ERR_OUT("Failed to allocate buffer to read record '%s'! Record size = %u.", sKey.c_str(), ui64Length);
							
						ui64BytesRead = rs->read(sKey, pBuffer);
						if (ui64BytesRead != ui64Length)
							ERR_OUT("Number of bytes read from record '%s' does not match expected bytes! Read %u, expected %u.", sKey.c_str(), ui64BytesRead, ui64Length);
							
						fwrite(pBuffer, 1, ui64BytesRead, fp);
						
						delete[] pBuffer;
						pBuffer = NULL;
					}
					
					fclose(fp);
					fp = NULL;
				}
			} catch (Error::ObjectDoesNotExist) {
				break;
			} catch (Error::StrategyError e) {
				ERR_OUT("A strategy error occurred: %s", e.getInfo().c_str());
			}
			
			uiCurr++;
			
			if (uiCurr > uiLast)
				break;
		}
	}

	iRetCode = EXIT_SUCCESS;
	
err_out:
	if (pBuffer)
		delete[] pBuffer;
	if (fp)
		fclose(fp);

	return iRetCode;
}
