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
#include <be_io_factory.h>
#include "toolsutils.h"

using namespace BiometricEvaluation;
using namespace BiometricEvaluation::IO;

void 
PrintUsage(char* argv[])
{
	printf("Usage: %s [-d dest_dir] [-v] <name> <description> <recordtype> <filelist>\n\n", argv[0]);
	printf("   name        = Record store name\n");
	printf("   description = Record store description\n");
	printf("   recordtype  = Record store type "
			"(ie. BERKELEYDB, ARCHIVE, or FILE)\n\n");
	printf("   filelist    = File containing list of files to add to the record store\n\n");
	printf("Options:\n");
	printf("   -d dest_dir = Directory to create record store in; default is current\n");
	printf("                 directory\n");
	printf("   -v          = Verify contents of record store after it is created\n\n");
	printf("Examples:\n");
	printf("   %s MyStore \"Sample Store\" filelist.txt\n", argv[0]);
	printf("   %s -d MyDir -v MyStore \"Sample Store\" filelist.txt\n\n", argv[0]);
	
	exit(EXIT_FAILURE);
}

/*
 * Add a file to the record store.
 */
bool 
AddFileToRecordStore(std::tr1::shared_ptr<RecordStore> rs, char *pszFilename)
{
	struct stat sb;
	string sKey;
	FILE *pFile = NULL;
	unsigned char *pData = NULL;
	bool bSuccess = false;
	
	/* Get file size */
	if (stat(pszFilename, &sb) != 0)
		ERR_OUT("Failed to stat file %s", pszFilename);

	/* Open file and read its contents */
	pFile = fopen(pszFilename, "rb");
	if (pFile == NULL)
		OPEN_ERR_OUT(pszFilename);
		
	pData = new unsigned char[sb.st_size];
	fread(pData, 1, sb.st_size, pFile);
	
	sKey.assign(basename(pszFilename));
	
	/* Insert file into record store using the filename as the key */
	try {
		rs->insert(sKey, pData, sb.st_size);
	} catch (Error::ObjectExists) {
		ERR_OUT("File %s already exist in the record store", pszFilename);
	} catch (Error::StrategyError e) {
		ERR_OUT("A strategy error occurred adding %s: %s", pszFilename, e.getInfo().c_str());
	}
	
	bSuccess = true;
		
err_out:
	if (pData)
		delete[] pData;
	if (pFile)
		fclose(pFile);
		
	return bSuccess;
}

/*
 * Verify the file data matches what was inserted into the record store.
 */
bool 
VerifyRecord(std::tr1::shared_ptr<RecordStore> rs, char *pszFilename)
{
	string sKey;
	uint64_t ui64Length;
	struct stat sb;
	FILE *pFile = NULL;
	unsigned char *pRsData = NULL;
	unsigned char *pFileData = NULL;
	bool bIsSame = false;
	
	/* Get file size */
	if (stat(pszFilename, &sb) != 0)
		ERR_OUT("Failed to stat file %s. Verification failed!", pszFilename);
	
	/* Open file */
	pFile = fopen(pszFilename, "rb");
	if (pFile == NULL)
		ERR_OUT("Failed to open file %s. Verification failed!", pszFilename);
		
	sKey.assign(basename(pszFilename));
	
	try {
		ui64Length = rs->length(sKey);
	
		/* Make sure file size matches record size */
		if (ui64Length != sb.st_size)
			ERR_OUT("Mismatched record size for %s. Verification failed!\n", pszFilename);

		if (ui64Length > 0) {
			pRsData = new unsigned char[ui64Length];
			pFileData = new unsigned char[sb.st_size];
			
			/* Read and compare data from record store and file */
			if ((rs->read(sKey, pRsData) == ui64Length) &&
				(fread(pFileData, 1, sb.st_size, pFile) == sb.st_size))	{

				if (memcmp(pRsData, pFileData, sb.st_size) == 0)
					bIsSame = true;
				else
					ERR_OUT("Memory comparison failed for %s. Verification failed!", pszFilename);
			} else
				ERR_OUT("Incorrect number of bytes read for %s. Verification failed!", pszFilename);
		} else
			bIsSame = true;
	} catch (Error::ObjectDoesNotExist) {
		ERR_OUT("Failed to locate %s in the record store. Verification failed!", pszFilename);
	} catch (Error::StrategyError e) {
		ERR_OUT("A strategy error occurred verifying %s: %s", pszFilename, e.getInfo().c_str());
	}
	
err_out:
	if (pRsData)
		delete[] pRsData;
	if (pFileData)
		delete[] pFileData;
	if (pFile)
		fclose(pFile);
	
	return bIsSame;
}

/*
 * This program is used to create a record store from a list of files.
 */
int 
main (int argc, char* argv[]) 
{
	std::tr1::shared_ptr<RecordStore>rs;
	FILE *pFile = NULL;
	struct stat sb;
	string sName, sDescription, sRecordType, sDestDir, sFileList;
	char szFilename[1024];
	int c;
	bool bVerify = false;
	int iRetCode = EXIT_FAILURE;
	
	opterr = 0;
	
	/* Process optional command line arguments */
	while ((c = getopt (argc, argv, "d:v")) != -1)
		switch (c) {
		case 'd':
			sDestDir.assign(optarg);
			break;
			
		case 'v':
			bVerify = true;
			break;
			
		default:
			PrintUsage(argv);
		}
		
	if ((argc - optind) != 4)
		PrintUsage(argv);
	
	/* Process mandatory command line arguments */
	sName.assign(argv[optind]);
	sDescription.assign(argv[optind+1]);
	sRecordType.assign(argv[optind+2]);
	sFileList.assign(argv[optind+3]);
	
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
		rs = Factory::createRecordStore
				(sName,
				sDescription,
				sRecordType,
				sDestDir);
	} catch (Error::ObjectExists) {
		string sInput;
		
		for (;;) {
			printf("The record store already exist. Overwrite? (y/n) ");
			getline(cin, sInput);
			
			if (sInput == "y" || sInput == "Y") {
				RecordStore::removeRecordStore(sName, sDestDir);
				
				try {
					rs = Factory::createRecordStore
							(sName,
							sDescription,
							sRecordType,
							sDestDir);
				} catch (Error::ObjectExists) {
					ERR_OUT("Failed to create record store!");
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
	
	/* Open input file list */
	pFile = fopen(sFileList.c_str(), "r");
	if (pFile == NULL)
		OPEN_ERR_OUT(sFileList.c_str());
	
	/* Add each file in our file list to the record store */
	while (!feof(pFile)) {
		if (fscanf(pFile, "%s\n", szFilename) == 1) {
			if (!AddFileToRecordStore(rs, szFilename))
				ERR_OUT("Failed to add %s to the record store!",  szFilename);
		}
	}
	
	printf("Record store created successfully!\n");
	
	/* Verify data in record store */
	if (bVerify) {
		unsigned int uiFileCount = 0;
		
		printf("Verifying record store...\n");
		
		/* Open record store */
		try {
			rs = Factory::openRecordStore
					(sName,
					sDestDir,
					READONLY);
		} catch (Error::ObjectDoesNotExist) {
			ERR_OUT("Failed to open record store.  Verification failed!");
		}

		rewind(pFile);
		
		/* 
		 * Verify the contents of each file in our file list with the records
		 * in the record store
		 */
		while (!feof(pFile)) {
			if (fscanf(pFile, "%s\n", szFilename) == 1) {
				if (!VerifyRecord(rs, szFilename))
					ERR_OUT("Verification failed for %s in record store!", szFilename);
				
				uiFileCount++;
			}
		}
		
		if (rs->getCount() == uiFileCount)
			printf("Verification succeeded!\n");
		else
			printf("File list contains %u files but record store has %u.\n", uiFileCount, rs->getCount());
	}
	
	iRetCode = EXIT_SUCCESS;
	
err_out:
	if (pFile)
		fclose(pFile);

	return iRetCode;
}
