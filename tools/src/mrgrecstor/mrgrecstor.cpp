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
#include <errno.h>
#include <be_io_dbrecstore.h>

#include "toolsutils.h"

#define DEFAULT_BUFFER_SIZE		4096

using namespace BiometricEvaluation;
using namespace BiometricEvaluation::IO;

void 
PrintUsage(char* argv[])
{
	printf("Usage: %s [-b size] [-d dest_dir] [-r] <name> <description> <recstore_list>\n\n", argv[0]);
	printf("   name          = Record store name\n");
	printf("   description   = Record store description\n");
	printf("   recstore_list = File containing list of record stores to merge\n\n");
	printf("Options:\n");
	printf("   -b size       = Buffer size to allocate for reading data from record stores;\n");
	printf("                   default is %d\n", DEFAULT_BUFFER_SIZE);
	printf("   -d dest_dir   = Directory to create record store in; default is current dir\n");
	printf("   -r            = Remove source record stores after merge\n\n");
	
	exit(EXIT_FAILURE);
}

/*
 * This program is used to merge record stores into a single record store.
 */
int 
main (int argc, char* argv[]) 
{
	DBRecordStore *rsout = NULL;
	DBRecordStore *rsin = NULL;
	FILE *fp = NULL;
	struct stat sb;
	unsigned char *pBuffer = NULL;
	string sName, sDescription, sRecStoreList, sDestDir, sKey;
	char szLine[1024];
	char *pszRecStoreName, *pszRecStoreDir;
	uint64_t ui64DataSize;
	int c, iBufferSize = DEFAULT_BUFFER_SIZE;
	bool bRemoveAfterMerge = false;
	int iStatus = EXIT_FAILURE;
	
	opterr = 0;
	
	/* Process optional command line arguments */
	while ((c = getopt (argc, argv, "b:d:r")) != -1)
		switch (c) {
		case 'b':
			iBufferSize = atoi(optarg);
			break;
			
		case 'd':
			sDestDir.assign(optarg);
			break;
			
		case 'r':
			bRemoveAfterMerge = true;
			break;
			
		default:
			PrintUsage(argv);
		}
		
	if ((argc - optind) != 3)
		PrintUsage(argv);
	
	/* Process mandatory command line arguments */
	sName.assign(argv[optind]);
	sDescription.assign(argv[optind+1]);
	sRecStoreList.assign(argv[optind+2]);
	
	/* Allocate buffer to read data from record stores */
	pBuffer = new unsigned char[iBufferSize];
	if (pBuffer == NULL)
		ERR_OUT("Unable to allocate buffer to read data from record stores");
	
	/* Open record store list file */
	fp = fopen(sRecStoreList.c_str(), "r");
	if (fp == NULL)
		OPEN_ERR_OUT(sRecStoreList.c_str());
	
	/* If necessary, create output directory */
	if (!sDestDir.empty()) {
	
		if (stat(sDestDir.c_str(), &sb) == 0) {
			if (!S_ISDIR(sb.st_mode))
				ERR_OUT("Cannot create output directory! A file with the same name already exists.");
		} else {
			if (mkdir(sDestDir.c_str(), S_IRWXU) != 0)
				ERR_OUT("Failed to create output directory!");
		}
	}
	
	/* Create new record store */
	try {
		rsout = new DBRecordStore(sName, sDescription, sDestDir);		
	} catch (Error::ObjectExists) {
		string sInput;
		
		for (;;) {
			printf("The record store '%s' already exist. Overwrite? (y/n) ", sName.c_str());
			getline(cin, sInput);
			
			if (sInput == "y" || sInput == "Y") {
				DBRecordStore::removeRecordStore(sName, sDestDir);
				
				try {
					rsout = new DBRecordStore(sName, sDescription, sDestDir);
				} catch (Error::ObjectExists) {
					ERR_OUT("Failed to create record store %s!", sName.c_str());
				} catch (Error::StrategyError e) {
					ERR_OUT("A strategy error occurred: %s", e.getInfo().c_str());
				}
				break;
			} else if (sInput == "n" || sInput == "N")
				goto err_out;
		}
	} catch (Error::StrategyError e) {
		ERR_OUT("A strategy error occurred: %s", e.getInfo().c_str());
	}
	
	printf("Merging record stores...\n");
		
	/*
	 * Add each record store in our record store list file to the new record 
	 * store.
	 */
	while (!feof(fp)) {
		if (fscanf(fp, "%s\n", szLine) == 1) {
		
			pszRecStoreName = basename(szLine);
			pszRecStoreDir = dirname(szLine);
			
			try {
				/* Open record store */
				rsin = new DBRecordStore(pszRecStoreName, pszRecStoreDir);
				
				/* 
				 * Step through the keys in this record store and add each to
				 * the new record store.
				 */
				for (;;) {
					try {
						ui64DataSize = rsin->sequence(sKey, NULL);
						
						if ((int)ui64DataSize > iBufferSize)
							ERR_OUT("Insufficient buffer to read record store data. Buffer size = %d, data size = %ui.", iBufferSize, ui64DataSize);
							
						rsin->read(sKey, pBuffer);
					} catch (Error::ObjectDoesNotExist) {
						break;
					} catch (Error::StrategyError e) {
						ERR_OUT("A strategy error occurred: %s", e.getInfo().c_str());
					}
					
					try {
						rsout->insert(sKey, pBuffer, ui64DataSize);
					} catch (Error::ObjectExists) {
						ERR_OUT("Attempted to add duplicate key '%s' to record store.", sKey.c_str());
					} catch (Error::StrategyError e) {
						ERR_OUT("A strategy error occurred: %s", e.getInfo().c_str());
					}
				}
			} catch (Error::ObjectDoesNotExist) {
				ERR_OUT("Failed to open record store %s!", szLine);
			} catch (Error::StrategyError e) {
				ERR_OUT("A strategy error occurred: %s", e.getInfo().c_str());
			}
			
			delete rsin;
			rsin = NULL;
		}
	}
	
	printf("Finished merging record stores\n");
	
	/* Delete source record stores */
	if (bRemoveAfterMerge) {
	
		printf("Removing source record stores...\n");
	
		rewind(fp);
		while (!feof(fp)) {
			if (fscanf(fp, "%s\n", szLine) == 1) {
			
				pszRecStoreName = basename(szLine);
				pszRecStoreDir = dirname(szLine);
				
				try {
					DBRecordStore::removeRecordStore(pszRecStoreName, pszRecStoreDir);
				} catch (Error::ObjectDoesNotExist) {
					ERR_OUT("Failed to remove record store %s!", szLine);
				} catch (Error::StrategyError e) {
					ERR_OUT("A strategy error occurred: %s", e.getInfo().c_str());
				}
			}
		}
		
		printf("Finished removing source record stores\n");
	}
	
	iStatus = EXIT_SUCCESS;
	
err_out:
	if (pBuffer)
		delete[] pBuffer;
	if (fp)
		fclose(fp);
	if (rsout)
		delete rsout;
	if (rsin)
		delete rsin;

	return iStatus;
}
