/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */

#include <sys/stat.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <dirent.h>
#include <getopt.h>

#include <be_error.h>
#include <be_error_exception.h>
#include <be_framework.h>
#include <be_io_archiverecstore.h>
#include <be_io_dbrecstore.h>
#include <be_io_compressedrecstore.h>
#include <be_io_compressor.h>
#include <be_io_filerecstore.h>
#include <be_io_recordstore.h>
#include <be_io_sqliterecstore.h>
#include <be_io_utility.h>
#include <be_text.h>
#include <be_memory_autoarrayutility.h>

#include <image_additions.h>
#include <lrs_additions.h>
#include <rstool.h>

uint16_t specialProcessingFlags = static_cast<std::underlying_type<
    SpecialProcessing>::type>(SpecialProcessing::NA);

namespace BE = BiometricEvaluation;

std::vector<std::string>
readTextFileToVector(
    const std::string &filePath,
    bool ignoreComments,
    bool ignoreBlankLines)
    throw (BiometricEvaluation::Error::FileError,
    BiometricEvaluation::Error::ObjectDoesNotExist)
{
	if (BE::IO::Utility::fileExists(filePath) == false)
		throw BE::Error::ObjectDoesNotExist();
	if (BE::IO::Utility::pathIsDirectory(filePath))
		throw BE::Error::FileError(filePath + " is a directory");
		
	std::string line;
	std::ifstream input;
	std::vector<std::string> rv;
	input.open(filePath.c_str(), std::ifstream::in);
	for (;;) {
		/* Read one path from text file */
		input >> line;
		if (input.eof())
			break;
		else if (input.bad())
			throw BE::Error::FileError(filePath + " bad bit set");
		
		/* Ignore comments and newlines */
		try {
			if (ignoreComments && line.at(0) == '#')
				continue;
			if (line.at(0) == '\n') {
				if (ignoreBlankLines)
					continue;
				/* 
				 * The newlines are stripped from the other
				 * entries, so this should be the empty string.
				 */
				line = "";
			}
		} catch (std::out_of_range) {
			continue;
		}
		
		/* Add the line */
		rv.push_back(line);
	}
	
	return (rv);
}

bool
yesOrNo(
    const std::string &prompt,
    bool default_answer,
    bool show_options,
    bool allow_default_answer)
{	
	std::string input;
	for (;;) {
		std::cout << prompt;
		if (show_options) {
			if (allow_default_answer) {
				if (default_answer == true)
					std::cout << " ([Y]/n)";
				else
					std::cout << " (y/[N])";
			} else
				std::cout << " (y/n)";
		}
		std::cout << ": ";
		std::getline(std::cin, input);
		try {
			switch (input.at(0)) {
			case 'Y':
			case 'y':
				return (true);
			case 'n':
			case 'N':
				return (false);
			}
		} catch (std::out_of_range) {
			if (allow_default_answer)
				return (default_answer);
		}
	}
}

void usage(char *exe)
{
	std::cerr << "Usage: " << exe << " <action> -s <RS> [options]" <<
	    std::endl;
	std::cerr << "Actions: add, display, dump, list, make, merge, remove, "
	    "rename, version, unhash" << std::endl;

	std::cerr << std::endl;

	std::cerr << "Common:" << std::endl;
	std::cerr << "\t-c\t\tIf hashing, hash file/record contents" <<
	    std::endl;
	std::cerr << "\t-p\t\tIf hashing, hash file path" << std::endl;
	std::cerr << "\t-s <path>\tRecordStore" << std::endl;

	std::cerr << std::endl;

	std::cerr << "Add Options:" << std::endl;
	std::cerr << "\t-a <file/dir>\tFile/directory contents to add" <<
	    std::endl;
	std::cerr << "\t-f\t\tForce insertion even if the same key already "
	    "exists" << std::endl;
	std::cerr << "\t-h <hash_rs>\tExisting hash translation RecordStore" <<
	    std::endl;
	std::cerr << "\t-k(fp)\t\tPrint 'f'ilename or file'p'ath of key as "
	    "value " << std::endl << "\t\t\tin hash translation RecordStore" <<
	    std::endl;

	std::cerr << "\tfile/dir ...\tFiles/directory contents to add as a " <<
	    "record" << std::endl;

	std::cerr << std::endl;

	std::cerr << "Diff Options:" << std::endl;
	std::cerr << "\t-a <file>\tText file with keys to compare" << std::endl;
	std::cerr << "\t-f\t\tCompare files byte for byte " <<
	    "(as opposed to checksum)" << std::endl;
	std::cerr << "\t-k <key>\tKey to compare" << std::endl;

	std::cerr << std::endl;

	std::cerr << "Display/Dump Options:" << std::endl;
	std::cerr << "\t-h <hash_rs>\tUnhash keys as found in existing "
	    "translation RecordStore" << std::endl;
	std::cerr << "\t-k <key>\tKey to dump" << std::endl;
	std::cerr << "\t-r <#-#>\tRange of keys" << std::endl;
	std::cerr << "\t-o <dir>\tOutput directory" << std::endl;
	std::cerr << "\t-f\t\tVisualize image/AN2K record (display only)" <<
	    std::endl;

	std::cerr << std::endl;

	std::cerr << "Make Options:" << std::endl;
	std::cerr << "\t-a <text>\tText file with paths to files or "
	    "directories to\n\t\t\tinitially add as records (multiple)" <<
	    std::endl;
	std::cerr << "\t-h <hash_rs>\tHash keys and save translation "
	    "RecordStore" << std::endl;
	std::cerr << "\t-f\t\tForce insertion even if the same key already "
	   "exists" << std::endl;
	std::cerr << "\t-k(fp)\t\tPrint 'f'ilename or file'p'ath of key as "
	   "value " << std::endl << "\t\t\tin hash translation RecordStore" <<
	    std::endl;
	std::cerr << "\t-t <type>\tType of RecordStore to make" << std::endl;
	std::cerr << "\t\t\tWhere <type> is Archive, BerkeleyDB, File, List, "
	    "SQLite" << std::endl;
	std::cerr << "\t-s <sourceRS>\tSource RecordStore, if -t is List" <<
	    std::endl;
	std::cerr << "\t-z\t\tCompress records with default strategy\n" <<
	    "\t\t\t(same as -Z GZIP)" << std::endl;
	std::cerr << "\t-Z <type>\tCompress records with <type> compression" <<
	    "\n\t\t\tWhere type is GZIP" << std::endl;
	std::cerr << "\t<file> ...\tFiles/dirs to add as a record" << std::endl;
	std::cerr << "\t-q\t\tSkip the confirmation step" << std::endl;

	std::cerr << std::endl;

	std::cerr << "Merge Options:" << std::endl;
	std::cerr << "\t-a <RS>\t\tRecordStore to be merged (multiple)" <<
	    std::endl;
	std::cerr << "\t-h <RS>\t\tHash keys and store a hash translation "
	    "RecordStore" << std::endl;
	std::cerr << "\t-t <type>\tType of RecordStore to make" << std::endl;
	std::cerr << "\t\t\tWhere <type> is Archive, BerkeleyDB, File" <<
	    std::endl;
	std::cerr << "\t<RS> ...\tRecordStore(s) to be merged " << std::endl;

	std::cerr << std::endl;
	
	std::cerr << "Remove Options:" << std::endl;
	std::cerr << "\t-f\t\tForce removal, do not prompt" << std::endl;
	std::cerr << "\t-k <key>\tThe key to remove" << std::endl;
	
	std::cerr << std::endl;

	std::cerr << "Rename Options:" << std::endl;
	std::cerr << "\t-s <new_name>\tNew name for the RecordStore" <<
	    std::endl;

	std::cerr << std::endl;

	std::cerr << "Unhash Options:" << std::endl;
	std::cerr << "\t-h <hash>\tHash to unhash" << std::endl;

	std::cerr << std::endl;
}

bool
isRecordStoreAccessible(
    const std::string &pathname,
    const BiometricEvaluation::IO::Mode mode)
{
	bool (*isAccessible)(const std::string&) = NULL;
	switch (mode) {
	case BE::IO::Mode::ReadOnly:
		isAccessible = BE::IO::Utility::isReadable;
		break;
	case BE::IO::Mode::ReadWrite:
		isAccessible = BE::IO::Utility::isWritable;
		break;
	}
	
	/* Test if the generic RecordStore files have the correct permissions */
	return (isAccessible(pathname) && 
	    isAccessible(pathname + '/'
		+ BE::IO::RecordStore::CONTROLFILENAME));
}

BiometricEvaluation::IO::RecordStore::Kind
validate_rs_type(
    const std::string &type)
{
	if (BE::Text::caseInsensitiveCompare(type,
	    to_string(BE::IO::RecordStore::Kind::File)))
		return (BE::IO::RecordStore::Kind::File);
	else if (BE::Text::caseInsensitiveCompare(type,
	    to_string(BE::IO::RecordStore::Kind::BerkeleyDB)))
		return (BE::IO::RecordStore::Kind::BerkeleyDB);
	else if (BE::Text::caseInsensitiveCompare(type,
	    to_string(BE::IO::RecordStore::Kind::Archive)))
		return (BE::IO::RecordStore::Kind::Archive);
	else if (BE::Text::caseInsensitiveCompare(type,
	    to_string(BE::IO::RecordStore::Kind::SQLite)))
		return (BE::IO::RecordStore::Kind::SQLite);
	else if (BE::Text::caseInsensitiveCompare(type,
	    to_string(BE::IO::RecordStore::Kind::List)))
		return (BE::IO::RecordStore::Kind::List);

	throw BE::Error::StrategyError("Invalid RecordStore Type: " + type);
}

Action procargs(int argc, char *argv[])
{
	if (argc == 1) {
		usage(argv[0]);
		return (Action::QUIT);
	}

	Action action;
	if (strcasecmp(argv[1], DUMP_ARG.c_str()) == 0)
		action = Action::DUMP;
	else if (strcasecmp(argv[1], DIFF_ARG.c_str()) == 0)
		action = Action::DIFF;
	else if (strcasecmp(argv[1], DISPLAY_ARG.c_str()) == 0)
		action = Action::DISPLAY;
	else if (strcasecmp(argv[1], LIST_ARG.c_str()) == 0)
		action = Action::LIST;
	else if (strcasecmp(argv[1], MAKE_ARG.c_str()) == 0)
		action = Action::MAKE;
	else if (strcasecmp(argv[1], MERGE_ARG.c_str()) == 0)
		action = Action::MERGE;
	else if (strcasecmp(argv[1], REMOVE_ARG.c_str()) == 0)
		action = Action::REMOVE;
	else if (BE::Text::caseInsensitiveCompare(argv[1], RENAME_ARG))
		action = Action::RENAME;
	else if (strcasecmp(argv[1], VERSION_ARG.c_str()) == 0)
		return Action::VERSION;
	else if (strcasecmp(argv[1], UNHASH_ARG.c_str()) == 0)
		action = Action::UNHASH;
	else if (strcasecmp(argv[1], ADD_ARG.c_str()) == 0)
		action = Action::ADD;
	else {
		usage(argv[0]);
		return (Action::QUIT);
	}

	/* Parse out common options first */
	char c;
	optind = 2;
	while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
		case 's':	/* Main RecordStore */
			sflagval = std::string(optarg);
			break;
		}
	}
	/* Reset getopt() */
	optind = 2;

	/* Sanity check */
	if (sflagval.empty()) {
		std::cerr << "Missing required option (-s)." << std::endl;
		return (Action::QUIT);
	}
	
	/* Special processing needed for ListRecordStores */
	if (isListRecordStore(sflagval))
		specialProcessingFlags |= static_cast<
		    std::underlying_type<SpecialProcessing>::type>(
		    SpecialProcessing::LISTRECORDSTORE);

	return (action);
}

int
procargs_extract(
    int argc,
    char *argv[],
    bool &visualize,
    std::string &key,
    std::string &range,
    std::shared_ptr<BiometricEvaluation::IO::RecordStore> &rs,
    std::shared_ptr<BiometricEvaluation::IO::RecordStore> &hash_rs)
{	
	char c;
        while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
		case 'f':	/* Visualize */
			visualize = true;
			break;
		case 'h':	/* Existing hash translation RecordStore */
			try {
				hash_rs = BE::IO::RecordStore::openRecordStore(
				    std::string(optarg),
				    BE::IO::Mode::ReadOnly);
			} catch (BE::Error::Exception &e) {
				if (isRecordStoreAccessible(
				    std::string(optarg),
				    BE::IO::Mode::ReadOnly))
    					std::cerr << "Could not open " << 
					    optarg <<
					    " -- " << e.what() << std::endl;
				else
					std::cerr << optarg << ": Permission "
					    "denied." << std::endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'k':	/* Extract key */
			key.assign(optarg);
			break;
		case 'o':	/* Output directory */
			oflagval = std::string(optarg);
			if (BE::IO::Utility::fileExists(oflagval)) {
				if (!BE::IO::Utility::pathIsDirectory(
				    oflagval)) {
					std::cerr << optarg << " is not a "
					    "directory." << std::endl;
					return (EXIT_FAILURE);
				}
			} else {
				if (mkdir(oflagval.c_str(), S_IRWXU) != 0) {
					std::cerr << "Could not create " <<
					    oflagval << " - " <<
					    BE::Error::errorStr();
					return (EXIT_FAILURE);
				}
			}
			break;
		case 'r':	/* Extract range of keys */
			range.assign(optarg);
			break;
		}
	}

	if (!key.empty() && !range.empty()) {
		std::cerr << "Choose only one (-k or -r)." << std::endl;
		return (EXIT_FAILURE);
	}

	/* -s option in dump context refers to the source RecordStore */
	if (!BE::IO::Utility::fileExists(sflagval)) {
		std::cerr << sflagval << " was not found." << std::endl;
		return (EXIT_FAILURE);
	}
	try {
		rs = BE::IO::RecordStore::openRecordStore(
		    std::string(sflagval), BE::IO::Mode::ReadOnly);
	} catch (BE::Error::Exception &e) {
  		if (isRecordStoreAccessible(std::string(sflagval),
		    BE::IO::Mode::ReadOnly))
			std::cerr << "Could not open " << sflagval << ".  " <<
			    e.what() << std::endl;
		else
		    	std::cerr << sflagval << ": Permission denied." <<
			    std::endl;
		return (EXIT_FAILURE);
	}

	/* If user didn't specify, dump the entire RecordStore */
	if (range.empty() && key.empty()) {
		std::stringstream out;
		out << "1-" << rs->getCount();
		range = out.str();
	}

	return (EXIT_SUCCESS);
}

int
display(
    const std::string &key,
    BiometricEvaluation::Memory::AutoArray<uint8_t> &value)
{
	value.resize(value.size() + 1);
	value[value.size() - 1] = '\0';

	std::cout << key << " = " << value << std::endl;

	return (EXIT_SUCCESS);
}

int
visualizeRecord(
    const std::string &key,
    BiometricEvaluation::Memory::uint8Array &value)
{
	/* 
	 * We can visualize supported images and AN2K files. Figure out
	 * what we're dealing with by trying formats until we don't throw
	 * an exception.
	 */
	bool isImage = false;
	std::shared_ptr<BiometricEvaluation::Image::Image> image;
	try {
		image = BiometricEvaluation::Image::Image::openImage(value);
		isImage = true;
	} catch (BiometricEvaluation::Error::Exception &e) {
		/* Ignore */
	}

	if (isImage) {
		try {
			ImageAdditions::displayImage(image);
			return (EXIT_SUCCESS);
		} catch (BiometricEvaluation::Error::Exception) {
			return (EXIT_FAILURE);
		}
	}

	/* At this point, we're not an Image */
	try {
		ImageAdditions::displayAN2K(value);
		return (EXIT_SUCCESS);
	} catch (BiometricEvaluation::Error::Exception) {
		return (EXIT_FAILURE);
	}

	/* This data is nothing we know about... */
	std::cerr << "Data for key \"" << key << "\" cannot be visualized." <<
	    std::endl;
	return (EXIT_FAILURE);
}

int
dump(
    const std::string &key,
    BiometricEvaluation::Memory::AutoArray<uint8_t> &value)
{
	/* Possible that keys could have slashes */
	if (key.find('/') != std::string::npos) {
		if (BE::IO::Utility::makePath(oflagval + "/" +
		    BE::Text::dirname(key), S_IRWXU)) {
			std::cerr << "Could not create path to store file (" <<
			    oflagval + "/" + BE::Text::dirname(key) << ")." <<
			    std::endl;
			return (EXIT_FAILURE);
		}
	}

	FILE *fp = std::fopen((oflagval + "/" + key).c_str(), "wb");
	if (fp == NULL) {
		std::cerr << "Could not create file." << std::endl;
		return (EXIT_FAILURE);
	}
	if (fwrite(value, 1, value.size(), fp) != value.size()) {
		std::cerr << "Could not write entry." << std::endl;
		if (fp != NULL)
			std::fclose(fp);
		return (EXIT_FAILURE);
	}
	std::fclose(fp);

	return (EXIT_SUCCESS);
}


int extract(int argc, char *argv[], Action action)
{
	bool visualize = false;
	std::string key = "", range = "";
	std::shared_ptr<BE::IO::RecordStore> rs, hash_rs;
	if (procargs_extract(argc, argv, visualize, key, range, rs, hash_rs) !=
	    EXIT_SUCCESS)
		return (EXIT_FAILURE);

	BE::Memory::AutoArray<uint8_t> hash_buf;
	if (!key.empty()) {
		BE::Memory::AutoArray<uint8_t> buf;
		try {
			buf = rs->read(key);
		} catch (BE::Error::ObjectDoesNotExist) {
			 /* It's possible the key should be hashed */
			try {
				std::string hash = BE::Text::digest(key);
				buf.resize(rs->length(hash));
				buf = rs->read(hash);
			} catch (BE::Error::Exception &e) {
				std::cerr << "Error extracting " << key <<
				    " - " << e.what() << std::endl;	
				return (EXIT_FAILURE);
			}
		} catch (BE::Error::Exception &e) {
			std::cerr << "Error extracting " << key << std::endl;	
			return (EXIT_FAILURE);
		}
	
		/* Unhash, if desired */
		if (hash_rs.get() != NULL) {
			try {
				hash_buf = hash_rs->read(key);
				key = BE::Memory::AutoArrayUtility::getString(
					hash_buf, hash_buf.size());
			} catch (BE::Error::Exception &e) {
				std::cerr << "Could not unhash " << key <<
				    " - " << e.what() << std::endl;
				return (EXIT_FAILURE);
			}
		}

		switch (action) {
		case Action::DUMP:
			if (dump(key, buf) != EXIT_SUCCESS)
				return (EXIT_FAILURE);
			break;
		case Action::DISPLAY:
			if (visualize) {
				if (visualizeRecord(key, buf) != EXIT_SUCCESS)
					return (EXIT_FAILURE);
				break;
			} else {
				if (display(key, buf) != EXIT_SUCCESS)
					return (EXIT_FAILURE);
			}
			break;
		default:
			std::cerr << "Invalid action received (" <<
			    static_cast<std::underlying_type<Action>::type>(
			    action) << ')' << std::endl;
			return (EXIT_FAILURE);
		}
	} else {
		/* TODO: Visualize multiple records */
		if ((action == Action::DISPLAY) && visualize) {
			if (rs->getCount() > 10) {
				std::cerr << "Cowardly refusing to "
				    "visualize " << rs->getCount() <<
				    " records.  Please use -k or dump." <<
				    std::endl;
				return (EXIT_FAILURE);
			}

			std::cerr << "Visualizing multiple records is not "
			    "implemented yet." << std::endl;
			return (EXIT_FAILURE);
		}

		std::vector<std::string> ranges = BE::Text::split(range, '-');
		if (ranges.size() != 2) {
			std::cerr << "Invalid value (-r)." << std::endl;
			return (EXIT_FAILURE);
		}

		std::string next_key;
		for (int i = 0; i < atoi(ranges[0].c_str()) - 1; i++) {
			try {
				next_key = rs->sequenceKey();
			} catch (BE::Error::Exception &e) {
				std::cerr << "Could not sequence to record " <<
				    ranges[0] << std::endl;
				return (EXIT_FAILURE);
			}
		}
		BE::IO::RecordStore::Record record;
		for (int i = atoi(ranges[0].c_str());
		    i <= atoi(ranges[1].c_str()); i++) {
			try {
				record = rs->sequence();
			} catch (BE::Error::Exception &e) {
				std::cerr << "Could not read key " << i <<
				    " - " << e.what() << "." << std::endl;
				return (EXIT_FAILURE);
			}

			/* Unhash, if desired */
			if (hash_rs.get() != NULL) {
				try {
					hash_buf = hash_rs->read(record.key);
					next_key =
					    BE::Memory::AutoArrayUtility::getString(
					    hash_buf, hash_buf.size());
				} catch (BE::Error::Exception &e) {
					std::cerr << "Could not unhash " << 
					    record.key << " - " << e.what() <<
					    std::endl;
					return (EXIT_FAILURE);
				}
			} else {
				next_key = record.key;
			}

			switch (action) {
			case Action::DUMP:
				if (dump(next_key, record.data) != EXIT_SUCCESS)
					return (EXIT_FAILURE);
				break;
			case Action::DISPLAY:
				if (display(next_key, record.data)
				    != EXIT_SUCCESS)
					return (EXIT_FAILURE);
				break;
			default:
				std::cerr << "Invalid action received (" <<
				    static_cast<std::underlying_type<
				    Action>::type>(action) << ')' << std::endl;
				return (EXIT_FAILURE);
			}
		}
	}

	return (EXIT_SUCCESS);
}

int listRecordStore(int argc, char *argv[])
{
	std::shared_ptr<BE::IO::RecordStore> rs;
	try {
		rs = BE::IO::RecordStore::openRecordStore(
		    std::string(sflagval), BE::IO::Mode::ReadOnly);
	} catch (BE::Error::Exception &e) {
		if (isRecordStoreAccessible(std::string(sflagval),
		    BE::IO::Mode::ReadOnly))
			std::cerr << "Could not open RecordStore - " <<
			    e.what() << std::endl;
		else
		    	std::cerr << sflagval << ": Permission denied." <<
			    std::endl;
		return (EXIT_FAILURE);
	}

	std::string key;
	try {
		for (;;) {
			key = rs->sequenceKey();
			std::cout << key << std::endl;
		}
	} catch (BE::Error::ObjectDoesNotExist) {
		/* RecordStore has been exhausted */
		return (EXIT_SUCCESS);
	} catch (BE::Error::Exception &e) {
		std::cerr << "Could not list RecordStore - " << e.what() <<
		    std::endl;
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);	/* Not reached */
}

int
procargs_make(
    int argc,
    char *argv[],
    std::string &hash_pathname,
    HashablePart &what_to_hash,
    KeyFormat &hashed_key_format,
    BiometricEvaluation::IO::RecordStore::Kind &kind,
    std::vector<std::string> &elements,
    bool &compress,
    BiometricEvaluation::IO::Compressor::Kind &compressorKind,
    bool &stopOnDuplicate)
{
	what_to_hash = HashablePart::NOTHING;
	hashed_key_format = KeyFormat::DEFAULT;
	compress = false;
	compressorKind = BE::IO::Compressor::Kind::GZIP;
	stopOnDuplicate = true;
	kind = BE::IO::RecordStore::Kind::Default;

	char c;
	bool textProvided = false, dirProvided = false, otherProvided = false;
	bool quiet = false;
        while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
		case 'a': {	/* Text file with paths to be added */
			std::string path = BE::Text::dirname(optarg) + '/' + 
			    BE::Text::basename(optarg);
			if (!BE::IO::Utility::fileExists(path)) {
				std::cerr << optarg << " does not exist." <<
				    std::endl;
				return (EXIT_FAILURE);
			}
			
			/* -a used to take a directory (backwards compat) */
			if (BE::IO::Utility::pathIsDirectory(path)) {
				elements.push_back(path);
				dirProvided = true;
				break;
			}
			
			/* Parse the paths in the text file */
			std::ifstream input;
			std::string line;
			input.open(optarg, std::ifstream::in);
			textProvided = true;
			for (;;) {
				/* Read one path from text file */
				input >> line;
				if (input.eof())
					break;
				else if (input.bad()) {
					std::cerr << "Error reading paths from "
					    << optarg << std::endl;
					return (EXIT_FAILURE);
				}
				
				/* Ignore comments and newlines */
				try {
					if (line.at(0) == '#' ||
					    line.at(0) == '\n')
					    	continue;
				} catch (std::out_of_range) {
					continue;
				}
				
				elements.push_back(BE::Text::dirname(line) +
				    '/' + BE::Text::basename(line));
			}
			input.close();
			break;
		}
		case 'c':	/* Hash contents */
			if (what_to_hash == HashablePart::NOTHING)
				what_to_hash = HashablePart::FILECONTENTS;
			else {
				std::cerr << "More than one hash method "
				   "selected." << std::endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'f':	/* Force -- don't stop on duplicate */
			stopOnDuplicate = false;
			break;
		case 'h':	/* Hash translation RecordStore */
			hash_pathname.assign(optarg);
			break;
		case 'k':	/* Hash key display type */
			switch (optarg[0]) {
			case 'f':	/* Display as file's name */
				hashed_key_format = KeyFormat::FILENAME;
				break;
			case 'p':	/* Display as file's path */
				hashed_key_format = KeyFormat::FILEPATH;
				break;
			default:
				std::cerr << "Invalid format specifier for "
				   "hashed key." << std::endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'p':	/* Hash file path */
			if (what_to_hash == HashablePart::NOTHING)
				what_to_hash = HashablePart::FILEPATH;
			else {
				std::cerr << "More than one hash method "
				   "selected." << std::endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'q':	/* Don't show confirmation */
			quiet = true;
			break;
		case 't':	/* Destination RecordStore type */
			try {
				kind = validate_rs_type(optarg);
			} catch (BE::Error::StrategyError) {
				std::cerr << "Invalid type format (-t): " <<
				    optarg << std::endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'z':	/* Compress */
			compress = true;
			compressorKind = BE::IO::Compressor::Kind::GZIP;
			break;
		case 'Z':	/* Compression strategy */
			compress = true;
			if (strcasecmp("GZIP", optarg) == 0)
				compressorKind = BE::IO::Compressor::Kind::GZIP;
			else {
				std::cerr << "Invalid compression kind -- " <<
				    optarg << std::endl;
				return (EXIT_FAILURE);
			}
			break;
		}
	}
	/* Remaining arguments are files or directories to add */
	for (int i = optind; i < argc; i++) {
		otherProvided = true;
		elements.push_back(BE::Text::dirname(argv[i]) + "/" +
		    BE::Text::basename(argv[i]));
	}
	
	if (hashed_key_format == KeyFormat::DEFAULT) {
		switch (what_to_hash) {
		case HashablePart::FILEPATH:
			hashed_key_format = KeyFormat::FILEPATH; break;
		default:
			hashed_key_format = KeyFormat::FILENAME; break;
		}
	}

	/* Sanity check -- don't hash without recording a translation */
	if (hash_pathname.empty() && (what_to_hash != HashablePart::NOTHING)) {
		std::cerr << "Specified hash method without -h." << std::endl;
		return (EXIT_FAILURE);
	}
	
	/* Sanity check -- don't compress and make a ListRecordStore */
	if (compress && (kind == BE::IO::RecordStore::Kind::List)) {
		std::cerr << "Can't compress ListRecordStore entries." <<
		    std::endl;
		    return (EXIT_FAILURE);
	}

	/* Choose to hash filename by default */
	if ((hash_pathname.empty() == false) && (what_to_hash ==
	    HashablePart::NOTHING))
		what_to_hash = HashablePart::FILENAME;

	if (quiet)
		return (EXIT_SUCCESS);

	return (makeHumanConfirmation(argc, argv, kind, what_to_hash,
	    hashed_key_format, hash_pathname, textProvided, dirProvided,
	    otherProvided, compress, compressorKind, stopOnDuplicate));
}

int
makeHumanConfirmation(
    int argc,
    char *argv[],
    BiometricEvaluation::IO::RecordStore::Kind kind,
    HashablePart what_to_hash,
    KeyFormat hashed_key_format,
    const std::string &hash_pathname,
    bool textProvided,
    bool dirProvided,
    bool otherProvided,
    bool compress,
    BiometricEvaluation::IO::Compressor::Kind compressorKind,
    bool stopOnDuplicate)
{
	/* Verbose sanity check */
	std::cout << "* Make a new ";
	if (compress)
		std::cout << "\"compressed\" ";
	std::cout << to_string(kind) << " RecordStore named \"";

	std::string rsName = "";
	if (kind == BE::IO::RecordStore::Kind::List) {
		char c;
		optind = 2;
	        while ((c = getopt(argc, argv, optstr)) != EOF) {
			switch (c) {
			case 's':
				rsName = optarg;
				break;
			}
			if (rsName != "")
				break;
		}
	} else {
		rsName = sflagval;
	}

	std::cout << rsName << '"' << std::endl;
	if (kind == BE::IO::RecordStore::Kind::List) {
		std::cout << "* \"" << rsName << "\" will refer to keys " <<
		    "from \"" << sflagval << "\"" << std::endl;
	} else {
		std::cout << "* \"" << rsName << "\" will be created"
		    << std::endl;
	}
	if (textProvided)
		std::cout << "* You provided one or more text files of file "
		    "paths whose contents will be added" << std::endl;
	if (dirProvided)
		std::cout << "* You provided one or more directories whose "
		    "contents will be added" << std::endl;
	if (otherProvided)
		std::cout << "* You provided one or more arguments of "
		    "individual files that will be added" << std::endl;
	if (compress)
		std::cout << "* Files will always be compressed with " <<
		    to_string(compressorKind) << " "
		    "as they're added to the RecordStore" << std::endl;
	switch (what_to_hash) {
	case HashablePart::FILECONTENTS:
		std::cout << "* Keys will be the MD5 checksum of the "
		    "contents of the files added" << std::endl;
		break;
	case HashablePart::FILEPATH:
		std::cout << "* Keys will be the MD5 checksum of the "
		    "relative paths of the files added" << std::endl;
		break;
	case HashablePart::FILENAME:
		std::cout << "* Keys will be the MD5 checksum of the "
		    "names of the files added" << std::endl;
		break;
	case HashablePart::NOTHING:
		std::cout << "* Keys will be the name of the files added" <<
		    std::endl;
	default:
		break;
	}

	if (what_to_hash != HashablePart::NOTHING) {
		std::cout << "* The hash translation RecordStore ";
		if (kind == BE::IO::RecordStore::Kind::List)
			std::cout << "is ";
		else
			std::cout << "will be ";

		std::cout << "named \"" << hash_pathname << '"' << std::endl;
		std::cout << "* The values in \"" << hash_pathname <<
		    "\" will be the ";
		switch (hashed_key_format) {
		case KeyFormat::FILEPATH:
			std::cout << "relative paths ";
			break;
		case KeyFormat::FILENAME:
			/* FALLTHROUGH */
		default:
			std::cout << "file names ";
			break;
		}
		std::cout << "of the original files" << std::endl;
	}
	if (kind != BE::IO::RecordStore::Kind::List) {
		std::cout << "* If a duplicate key is encountered, " <<
		    argv[0] << " will ";
		if (stopOnDuplicate)
			std::cout << "stop and exit" << std::endl;
		else
			std::cout << "overwrite the key/value pair" << std::endl;
	} else
		std::cout << "* Keys added to \"" << rsName << "\" must "
		    "already exist in \"" << sflagval << "\"" << std::endl;

	std::cout << std::endl;
	if (!yesOrNo("Sound good?", false, true, false))
		return (EXIT_FAILURE);

	return (EXIT_SUCCESS);
}

int make_insert_contents(const std::string &filename,
    const std::shared_ptr<BiometricEvaluation::IO::RecordStore> &rs,
    const std::shared_ptr<BiometricEvaluation::IO::RecordStore> &hash_rs,
    const HashablePart what_to_hash,
    const KeyFormat hashed_key_format,
    bool stopOnDuplicate)
{
	static BE::Memory::uint8Array buffer;
	static uint64_t buffer_size = 0;
	static std::ifstream buffer_file;

	try {
		buffer_size = BE::IO::Utility::getFileSize(filename);
		buffer.resize(buffer_size);
	} catch (BE::Error::Exception &e) {
		std::cerr << "Could not get file size for " << filename <<
		    std::endl;
		return (EXIT_FAILURE);
	}

	/* Extract file into buffer */
	buffer_file.open(filename.c_str(), std::ifstream::binary);
	buffer_file.read((char *)&(*buffer), buffer_size);
	if (buffer_file.bad()) {
		std::cerr << "Error reading file (" << filename << ')' <<
		    std::endl;
		return (EXIT_FAILURE);
	}
	buffer_file.close();
	
	static std::string key = "", hash_value = "";
	try {
		key = BE::Text::basename(filename);
		if (hash_rs.get() == NULL) {
			try {
				rs->insert(key, buffer);
			} catch (BE::Error::ObjectExists &e) {
				if (stopOnDuplicate)
					throw;
					
				std::cerr << "Could not insert " + filename +
				    " (key = \"" + key + "\") because "
				    "it already exists.  Replacing..." <<
				    std::endl;
				rs->replace(key, buffer);
			}
		} else {
			switch (what_to_hash) {
			case HashablePart::FILECONTENTS:
				hash_value = BE::Text::digest(buffer,
				    buffer_size);
				break;
			case HashablePart::FILENAME:
				hash_value = BE::Text::digest(key);
				break;
			case HashablePart::FILEPATH:
				hash_value = BE::Text::digest(filename);
				break;
			case HashablePart::NOTHING:
				/* FALLTHROUGH */
			default:
				/* Don't hash */
				break;
			}

			switch (hashed_key_format) {
			case KeyFormat::FILENAME:
				/* Already done */
				break;
			case KeyFormat::FILEPATH:
				key = filename;
				break;
			default:
				std::cerr << "Invalid key format received (" <<
				    static_cast<std::underlying_type<
				    KeyFormat>::type>(hashed_key_format) <<
				    ')' << std::endl;
				return (EXIT_FAILURE);
			}

			try {
				rs->insert(hash_value, buffer);
			} catch (BE::Error::ObjectExists &e) {
				if (stopOnDuplicate)
					throw;
					
				std::cerr << "Could not insert " + filename +
				    " (key: \"" + hash_value + "\") because "
				    "it already exists.  Replacing..." << std::endl;
				rs->replace(hash_value, buffer);
			}

			try {
				hash_rs->insert(hash_value, key.c_str(),
				    key.size());
			} catch (BE::Error::ObjectExists &e) {
				if (stopOnDuplicate)
					throw;
					
				std::cerr << "Could not insert " + filename +
				    " (key: \"" + hash_value + "\") into hash "
				    "translation RecordStore because it "
				    "already exists.  Replacing..." <<
				    std::endl;
				hash_rs->replace(hash_value, key.c_str(),
				    key.size());
			}
		}
	} catch (BE::Error::Exception &e) {
		std::cerr << "Could not add contents of " << filename <<
		    " to RecordStore - " << e.what() << std::endl;
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}

int
make_insert_directory_contents(
    const std::string &directory,
    const std::string &prefix,
    const std::shared_ptr<BiometricEvaluation::IO::RecordStore> &rs,
    const std::shared_ptr<BiometricEvaluation::IO::RecordStore> &hash_rs,
    const HashablePart what_to_hash,
    const KeyFormat hashed_key_format,
    bool stopOnDuplicate)
    throw (BiometricEvaluation::Error::ObjectDoesNotExist,
    BiometricEvaluation::Error::StrategyError)
{
	struct dirent *entry;
	DIR *dir = NULL;
	std::string dirpath, filename;

	dirpath = prefix + "/" + directory;
	if (!BE::IO::Utility::fileExists(dirpath))
		throw BE::Error::ObjectDoesNotExist(dirpath + " does not exist");
	dir = opendir(dirpath.c_str());
	if (dir == NULL)
		throw BE::Error::StrategyError(dirpath + " could not be opened");
	
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_ino == 0)
			continue;
		if ((strcmp(entry->d_name, ".") == 0) ||
		    (strcmp(entry->d_name, "..") == 0))
			continue;

		filename = dirpath + "/" + entry->d_name;

		/* Recursively remove subdirectories and files */
		if (BE::IO::Utility::pathIsDirectory(filename)) {
			if (make_insert_directory_contents(entry->d_name,
			    dirpath, rs, hash_rs, what_to_hash,
			    hashed_key_format, stopOnDuplicate) !=
			    EXIT_SUCCESS)
				return (EXIT_FAILURE);
		} else {
			if (make_insert_contents(filename, rs, hash_rs,
			    what_to_hash, hashed_key_format,
			    stopOnDuplicate) != EXIT_SUCCESS)
				return (EXIT_FAILURE);
		}
	}

	if (dir != NULL)
		if (closedir(dir))
			throw BE::Error::StrategyError("Could not close " + 
			    dirpath + " (" + BE::Error::errorStr() + ")");

	return (EXIT_SUCCESS);
}

int
makeListRecordStore(
    int argc,
    char *argv[],
    std::vector<std::string> &elements,
    HashablePart what_to_hash,
    KeyFormat hashed_key_format)
{
	std::string newRSPath, existingRSPath;
	std::shared_ptr<BE::IO::RecordStore> targetRS;
	
	/*
	 * Rescan for -s (rstool make -s <newRS> -s <existingRS>) and -h (must
	 * exist).
	 */
	char c;
	uint8_t rsCount = 0;
	std::shared_ptr<BE::IO::RecordStore> hash_rs;
	optind = 2;
        while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
		case 'h':
			try {
				hash_rs = BE::IO::RecordStore::openRecordStore(
				    std::string(optarg), BE::IO::Mode::ReadOnly);
			} catch (BE::Error::Exception &e) {
				std::cerr << "Could not open hash RecordStore, "
				    "but it must exist when creating a hashed "
				    "ListRecordStore." << std::endl;
				return (EXIT_FAILURE);
			}
			break;
 		case 's':
			switch (rsCount) {
			case 0:
				newRSPath = optarg;
				sflagval = newRSPath;
				rsCount++;
				break;
			case 1:
				try {
					targetRS =
					    BE::IO::RecordStore::openRecordStore
					    (std::string(optarg),
					     BE::IO::Mode::ReadOnly);
				} catch (BE::Error::Exception &e) {
					if (isRecordStoreAccessible(
					     std::string(optarg),
					     BE::IO::Mode::ReadOnly))
						std::cerr << "Could not open "<<
						BE::Text::basename(optarg) <<
						" - " << e.what() << std::endl;
					else
						std::cerr << optarg <<
						    ": Permission denied." <<
						    std::endl;
					return (EXIT_FAILURE);
				}
				existingRSPath = optarg;
				rsCount++;
				break;
			default:
				std::cerr << "Too many -s options while making "
				"a ListRecordStore" << std::endl;
				return (EXIT_FAILURE);
			}
			break;
		}
	}
	if (rsCount != 2) {
		std::cerr << "Not enough -s options while making a "
		    "ListRecordStore." << std::endl;
		return (EXIT_FAILURE);
	}
	
	try {
		constructListRecordStore(newRSPath, existingRSPath);
	} catch (BE::Error::Exception &e) {
		std::cerr << e.what() << std::endl;
		return (EXIT_FAILURE);
	}
	
	return (modifyListRecordStore(argc, argv, Action::ADD));
}

int make(int argc, char *argv[])
{
	std::string hash_pathname = "";
	BE::IO::RecordStore::Kind type = BE::IO::RecordStore::Kind::Default;
	std::vector<std::string> elements;
	HashablePart what_to_hash = HashablePart::NOTHING;
	KeyFormat hashed_key_format = KeyFormat::DEFAULT;
	bool compress = false;
	BE::IO::Compressor::Kind compressorKind;
	bool stopOnDuplicate = true;

	if (procargs_make(argc, argv, hash_pathname, what_to_hash,
	    hashed_key_format, type, elements, compress, compressorKind,
	    stopOnDuplicate) != EXIT_SUCCESS)
		return (EXIT_FAILURE);
		
	if (type == BE::IO::RecordStore::Kind::List) {
		specialProcessingFlags |= static_cast<
		std::underlying_type<SpecialProcessing>::type>(
		SpecialProcessing::LISTRECORDSTORE);
		return (makeListRecordStore(argc, argv, elements, what_to_hash,
		    hashed_key_format));
	}

	std::shared_ptr<BE::IO::RecordStore> rs;
	std::shared_ptr<BE::IO::RecordStore> hash_rs;
	try {
		if (compress)
			rs.reset(new BE::IO::CompressedRecordStore(sflagval,
			    "<Description>", type, compressorKind));
		else
			rs = BE::IO::RecordStore::createRecordStore(sflagval,
			    "<Description>", type);
		if (!hash_pathname.empty())
			/* No need to compress hash translation RS */
			hash_rs = BE::IO::RecordStore::createRecordStore(
			    hash_pathname,
			    "Hash Translation for " + sflagval, type);
	} catch (BE::Error::Exception &e) {
		std::cerr << "Could not create " << sflagval << " - " <<
		    e.what() << std::endl;
		return (EXIT_FAILURE);
	}

	for (unsigned int i = 0; i < elements.size(); i++) {
		if (BE::IO::Utility::pathIsDirectory(elements[i])) {
			try {
				/* Recursively insert contents of directory */
				if (make_insert_directory_contents(
				    BE::Text::basename(elements[i]),
				    BE::Text::dirname(elements[i]),
				    rs, hash_rs, what_to_hash,
				    hashed_key_format,
				    stopOnDuplicate) != EXIT_SUCCESS)
			    		return (EXIT_FAILURE);
			} catch (BE::Error::Exception &e) {
				std::cerr << "Could not add contents of dir " <<
				    elements[i] << " - " << e.what() <<
				    std::endl;
				return (EXIT_FAILURE);
			}
		} else {
			if (make_insert_contents(elements[i], rs, hash_rs,
			    what_to_hash, hashed_key_format,
			    stopOnDuplicate) != EXIT_SUCCESS)
				return (EXIT_FAILURE);
		}
	}

	return (EXIT_SUCCESS);
}

int
procargs_merge(
    int argc,
    char *argv[],
    BiometricEvaluation::IO::RecordStore::Kind &kind,
    std::vector<std::string> &child_rs,
    std::string &hash_pathname,
    HashablePart &what_to_hash,
    KeyFormat &hashed_key_format)
{
	what_to_hash = HashablePart::NOTHING;
	hashed_key_format = KeyFormat::DEFAULT;
	kind = BiometricEvaluation::IO::RecordStore::Kind::Default;

	char c;
	child_rs.empty();
        while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
		case 't':	/* Destination RecordStore type */
			try {
				kind = validate_rs_type(optarg);
			} catch (BE::Error::StrategyError) {
				std::cerr << "Invalid type format (-t): " <<
				    optarg << std::endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'a':	/* RecordStore to be merged */
			child_rs.push_back(optarg);
			break;
		case 'c':	/* Hash contents */
			if (what_to_hash == HashablePart::NOTHING)
				what_to_hash = HashablePart::FILECONTENTS;
			else {
				std::cerr << "More than one hash method "
				   "selected." << std::endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'h':	/* Hash translation RecordStore */
			hash_pathname.assign(optarg);
			break;
		case 'k':	/* Hash key display type */
			switch (optarg[0]) {
			case 'f':	/* Display as file's name */
				hashed_key_format = KeyFormat::FILENAME;
				break;
			case 'p':	/* Display as file's path */
				hashed_key_format = KeyFormat::FILEPATH;
				break;
			default:
				std::cerr << "Invalid format specifier for "
				   "hashed key." << std::endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'p':	/* Hash file path */
			/* Sanity check */
			std::cerr << "Cannot hash file path when merging "
			    "RecordStores -- there are no paths." << std::endl;
			return (EXIT_FAILURE);
		}
	}
	
	/* Remaining arguments are RecordStores to merge */
	if ((argc - optind) > 0)
		for (int i = optind; i < argc; i++)
			child_rs.push_back(argv[i]);

	if (hashed_key_format == KeyFormat::DEFAULT)
		hashed_key_format = KeyFormat::FILENAME;

	if (child_rs.size() == 0) {
		std::cerr << "Missing required option (-a)." << std::endl;
		return (EXIT_FAILURE);
	}

	/* -s option in dump context refers to the source RecordStore */
	if (BE::IO::Utility::fileExists(sflagval)) {
		std::cerr << sflagval << " already exists." << std::endl;
		return (EXIT_FAILURE);
	}

	/* Sanity check -- don't hash without recording a translation */
	if (hash_pathname.empty() && (what_to_hash !=
	    HashablePart::NOTHING)) {
		std::cerr << "Specified hash method without -h." << std::endl;
		return (EXIT_FAILURE);
	}

	/* Choose to hash filename by default */
	if ((hash_pathname.empty() == false) && (what_to_hash ==
	    HashablePart::NOTHING))
		what_to_hash = HashablePart::FILENAME;

	return (EXIT_SUCCESS);
}

void mergeAndHashRecordStores(
    const std::string &mergedName,
    const std::string &mergedDescription,
    const std::string &hashName,
    const BiometricEvaluation::IO::RecordStore::Kind &kind,
    std::vector<std::string> &recordStores,
    const HashablePart what_to_hash,
    const KeyFormat hashed_key_format)
    throw (BiometricEvaluation::Error::ObjectExists,
    BiometricEvaluation::Error::StrategyError)
{
	std::unique_ptr<BE::IO::RecordStore> merged_rs, hash_rs;
	std::string hash_description = "Hash translation of " + mergedName;
	if (kind == BE::IO::RecordStore::Kind::BerkeleyDB) {
		merged_rs.reset(new BE::IO::DBRecordStore(mergedName,
		    mergedDescription));
		hash_rs.reset(new BE::IO::DBRecordStore(hashName,
		    hash_description));
	} else if (kind == BE::IO::RecordStore::Kind::Archive) {
		merged_rs.reset(new BE::IO::ArchiveRecordStore(mergedName, 
		    mergedDescription));
		hash_rs.reset(new BE::IO::ArchiveRecordStore(hashName,
		    hash_description));
	} else if (kind == BE::IO::RecordStore::Kind::File) {
		merged_rs.reset(new BE::IO::FileRecordStore(mergedName,
		    mergedDescription));
		hash_rs.reset(new BE::IO::FileRecordStore(hashName,
		    hash_description));
	} else if (kind == BE::IO::RecordStore::Kind::SQLite) {
		merged_rs.reset(new BE::IO::SQLiteRecordStore(mergedName,
		    mergedDescription));
    		hash_rs.reset(new BE::IO::SQLiteRecordStore(hashName,
		    hash_description));
	} else if (kind == BE::IO::RecordStore::Kind::Compressed)
		throw BE::Error::StrategyError("Invalid RecordStore type");
	else
		throw BE::Error::StrategyError("Unknown RecordStore type");

	bool exhausted;
	std::string hash;
	std::shared_ptr<BE::IO::RecordStore> rs;
	BE::IO::RecordStore::Record record;
	for (size_t i = 0; i < recordStores.size(); i++) {
		try {
			rs = BE::IO::RecordStore::openRecordStore(
			    recordStores[i], BE::IO::Mode::ReadOnly);
		} catch (BE::Error::Exception &e) {
			throw BE::Error::StrategyError(e.what());
		}
		
		exhausted = false;
		while (!exhausted) {
			try {
				record = rs->sequence();
				switch (what_to_hash) {
				case HashablePart::FILECONTENTS:
					hash = BE::Text::digest(
					    record.data, record.data.size());
					break;
				case HashablePart::FILEPATH:
					/*
					 * We don't have a file's path
					 * here since we're going from
					 * RecordStore to RecordStore.
					 */
					/* FALLTHROUGH */
				case HashablePart::FILENAME:
					hash = BE::Text::digest(record.key);
					break;
				case HashablePart::NOTHING:
					/* FALLTHROUGH */
				default:
					/* Don't hash */
					break;
				}
				
				switch (hashed_key_format) {
				case KeyFormat::FILENAME:
					/* FALLTHROUGH */
				case KeyFormat::FILEPATH:
					/* FALLTHROUGH */
				default:
					/*
					 * We don't have a file's path
					 * here since we're going from
					 * RecordStore to RecordStore,
					 * so not much we can do.
					 */
					break;
				}
				merged_rs->insert(hash, record.data);
				hash_rs->insert(hash, record.key.c_str(),
				    record.key.size() + 1);
			} catch (BE::Error::ObjectDoesNotExist) {
				exhausted = true;
			}
		}
	}
}

int merge(int argc, char *argv[])
{
	HashablePart what_to_hash = HashablePart::NOTHING;
	KeyFormat hashed_key_format = KeyFormat::DEFAULT;
	std::string hash_pathname = "";
	BiometricEvaluation::IO::RecordStore::Kind kind =
	    BiometricEvaluation::IO::RecordStore::Kind::Default;
	std::vector<std::string> child_rs;
	if (procargs_merge(argc, argv, kind, child_rs, hash_pathname,
	    what_to_hash, hashed_key_format) != EXIT_SUCCESS)
		return (EXIT_FAILURE);

	std::string description = "A merge of ";
	for (std::size_t i = 0; i < child_rs.size(); i++) {
		description += BE::Text::basename(child_rs[i]);
		if (i != (child_rs.size() - 1)) description += ", ";
	}

	try {
		if (hash_pathname.empty()) {
			BE::IO::RecordStore::mergeRecordStores(
			    sflagval, description, kind, child_rs);
		} else {
			mergeAndHashRecordStores(
			    sflagval, description, hash_pathname,
			    kind, child_rs, what_to_hash, hashed_key_format);
		}
	} catch (BE::Error::Exception &e) {
		std::cerr << "Could not create " << sflagval << " - " <<
		    e.what() << std::endl;
	}

	return (EXIT_SUCCESS);
}

int version(int argc, char *argv[])
{
	std::cout << argv[0] << " v" << MAJOR_VERSION << "." << MINOR_VERSION <<
	    " (Compiled " << __DATE__ << " " << __TIME__ << ")" << std::endl;
	std::cout << "BiometricEvaluation Framework v" << 
	    BE::Framework::getMajorVersion() << "." <<
	    BE::Framework::getMinorVersion() << " (" <<
	    BE::Framework::getCompiler() << " v" <<
	    BE::Framework::getCompilerVersion() << ")" << std::endl;
	
	return (EXIT_SUCCESS);
}

int procargs_unhash(int argc, char *argv[], std::string &hash,
    std::shared_ptr<BE::IO::RecordStore> &rs)
{
	char c;
        while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
		case 'h':	/* Hash value to convert */
			hash.assign(optarg);
			break;
		}
	}

	if (hash.empty()) {
		std::cerr << "Missing required option (-h)." << std::endl;
		return (EXIT_FAILURE);
	}

	/* sflagval here is the hashed RecordStore */
	try {
		rs = BE::IO::RecordStore::openRecordStore(
		    std::string(sflagval), BE::IO::Mode::ReadOnly);
	} catch (BE::Error::Exception &e) {
		if (isRecordStoreAccessible(std::string(sflagval),
		    BE::IO::Mode::ReadOnly))
			std::cerr << "Could not open " << sflagval << " - " << 
			    e.what() << std::endl;
		else
			std::cerr << sflagval << ": Permission denied." <<
			    std::endl;
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}

int unhash(int argc, char *argv[])
{
	std::string hash = "";
	std::shared_ptr<BE::IO::RecordStore> rs;
	if (procargs_unhash(argc, argv, hash, rs) != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	
	BE::Memory::uint8Array buffer;
	try {
		buffer = rs->read(hash);
		std::cout << buffer << std::endl;
	} catch (BE::Error::ObjectDoesNotExist) {
		std::cerr << hash << " was not found in " << rs->getPathname()
		    << std::endl;
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}

int
procargs_add(
    int argc,
    char *argv[],
    std::shared_ptr<BiometricEvaluation::IO::RecordStore> &rs,
    std::shared_ptr<BiometricEvaluation::IO::RecordStore> &hash_rs,
    std::vector<std::string> &files,
    HashablePart &what_to_hash,
    KeyFormat &hashed_key_format,
    bool &stopOnDuplicate)
{
	what_to_hash = HashablePart::NOTHING;
	hashed_key_format = KeyFormat::DEFAULT;
	stopOnDuplicate = true;

	char c;
	while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
		case 'a':	/* File to add */
			if (!BE::IO::Utility::fileExists(optarg))
				std::cerr << optarg << " does not exist and "
				    "will be skipped." << std::endl;
			else
				files.push_back(optarg);
			break;
		case 'c':	/* Hash contents */
			if (what_to_hash == HashablePart::NOTHING)
				what_to_hash = HashablePart::FILECONTENTS;
			else {
				std::cerr << "More than one hash method "
				    "selected." << std::endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'f':	/* Force -- don't stop on duplicate */
			stopOnDuplicate = false;
			break;
		case 'k':	/* Hash key display type */
			switch (optarg[0]) {
			case 'f':	/* Display as file's name */
				hashed_key_format = KeyFormat::FILENAME;
				break;
			case 'p':	/* Display as file's path */
				hashed_key_format = KeyFormat::FILEPATH;
				break;
			default:
				std::cerr << "Invalid format specifier for "
				    "hashed key." << std::endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'p':	/* Hash file path */
			if (what_to_hash == HashablePart::NOTHING)
				what_to_hash = HashablePart::FILEPATH;
			else {
				std::cerr << "More than one hash method "
				    "selected." << std::endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'h':	/* Existing hash translation RecordStore */
			try {
				hash_rs = BE::IO::RecordStore::openRecordStore(
				    std::string(optarg));
			} catch (BE::Error::Exception &e) {
				if (isRecordStoreAccessible(
				    std::string(optarg), BE::IO::Mode::ReadOnly))
					std::cerr << "Could not open " <<
					     optarg << " -- " << e.what() <<
					     std::endl;
				else
				    	std::cerr << optarg << ": Permission "
					    "denied." << std::endl;
				return (EXIT_FAILURE);
			}
			break;
		}
	}
	/* Remaining arguments are files to add (same as -a) */
	for (int i = optind; i < argc; i++) {
		if (BE::IO::Utility::fileExists(argv[i]) == false)
			std::cerr << optarg << " does not exist and will be " <<
			    "skipped." << std::endl;
		else
			files.push_back(argv[i]);
	}

	if (hashed_key_format == KeyFormat::DEFAULT) {
		switch (what_to_hash) {
		case HashablePart::FILEPATH:
			hashed_key_format = KeyFormat::FILEPATH; break;
		default:
			hashed_key_format = KeyFormat::FILENAME; break;
		}
	}

	/* sflagval is the RecordStore we will be adding to */
	try {
		rs = BE::IO::RecordStore::openRecordStore(
		    std::string(sflagval));
	} catch (BE::Error::Exception &e) {
		if (isRecordStoreAccessible(std::string(sflagval)))
			std::cerr << "Could not open " << sflagval << " -- " <<
			    e.what() << std::endl;
		else
		    	std::cerr << sflagval << ": Permission denied." <<
			     std::endl;
		return (EXIT_FAILURE);
	}

	/* Sanity check -- don't hash without recording a translation */
	if ((hash_rs.get() == NULL) && (what_to_hash !=
	    HashablePart::NOTHING)) {
		std::cerr << "Specified hash method without -h." << std::endl;
			return (EXIT_FAILURE);
	}

	/* Choose to hash filename by default */
	if ((hash_rs.get() != NULL) && (what_to_hash == HashablePart::NOTHING))
		what_to_hash = HashablePart::FILENAME;

	return (EXIT_SUCCESS);
}

int
procargs_modifyListRecordStore(
    int argc,
    char *argv[],
    std::shared_ptr<BiometricEvaluation::IO::RecordStore> &hash_rs,
    std::vector<std::string> &files,
    HashablePart &what_to_hash,
    bool &prompt)
{
	what_to_hash = HashablePart::NOTHING;

	char c;
	optind = 2;
	while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
		case 'a':	/* File to add */
			if (!BE::IO::Utility::fileExists(optarg))
				std::cerr << optarg << " does not exist and "
				    "will be skipped." << std::endl;
			else
				files.push_back(optarg);
			break;
		case 'c':	/* Hash contents */
			if (what_to_hash == HashablePart::NOTHING)
				what_to_hash = HashablePart::FILECONTENTS;
			else {
				std::cerr << "More than one hash method "
				    "selected." << std::endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'f':	/* Force */
			prompt = false;
			break;
		case 'p':	/* Hash file path */
			if (what_to_hash == HashablePart::NOTHING)
				what_to_hash = HashablePart::FILEPATH;
			else {
				std::cerr << "More than one hash method "
				    "selected." << std::endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'h':	/* Existing hash translation RecordStore */
			try {
				hash_rs = BE::IO::RecordStore::openRecordStore(
				    std::string(optarg));
			} catch (BE::Error::Exception &e) {
				if (isRecordStoreAccessible(
				    std::string(optarg)))
					std::cerr << "Could not open " <<
					    optarg << " -- " << e.what() <<
					    std::endl;
				else
				    	std::cerr << optarg << ": Permission "
					    "denied." << std::endl;
				return (EXIT_FAILURE);
			}
			break;
		}
	}
	/* Remaining arguments are files to add (same as -a) */
	for (int i = optind; i < argc; i++)
		files.push_back(argv[i]);

	/* Sanity check -- don't hash without recording a translation */
	if ((hash_rs.get() == NULL) && (what_to_hash !=
	    HashablePart::NOTHING)) {
		std::cerr << "Specified hash method without -h." << std::endl;
		return (EXIT_FAILURE);
	}

	/* Choose to hash filename by default */
	if ((hash_rs.get() != NULL) && (what_to_hash == HashablePart::NOTHING))
		what_to_hash = HashablePart::FILENAME;

	/* Sanity check -- inserting into ListRecordStore */
	if (specialProcessingFlags & static_cast<std::underlying_type<
	    SpecialProcessing>::type>(SpecialProcessing::LISTRECORDSTORE))
		if (!yesOrNo("You are about to modify a ListRecordStore, "
		    "which means that you are only\nmodifying keys in the "
		    "KeyList, not values in the backing RecordStore.\n"
		    "Is this correct?", false))
			return (EXIT_FAILURE);
			
	return (EXIT_SUCCESS);
}

int
modifyListRecordStore(
    int argc,
    char *argv[],
    Action action)
{
	HashablePart what_to_hash = HashablePart::NOTHING;
	std::shared_ptr<BE::IO::RecordStore> hash_rs;
	std::vector<std::string> files;
	bool prompt = true;
	
	if (procargs_modifyListRecordStore(argc, argv, hash_rs, files,
	    what_to_hash, prompt) != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	
	std::shared_ptr<OrderedSet<std::string>> keys(
	    new OrderedSet<std::string>);
	BE::Memory::uint8Array buffer;
	std::string hash, invalidHashKeys = "";
	for (std::vector<std::string>::const_iterator file_path = files.begin();
	    file_path != files.end(); file_path++) {
		switch (what_to_hash) {
		case HashablePart::FILECONTENTS:
			try {
				buffer = BE::IO::Utility::readFile(*file_path);
			} catch (BE::Error::Exception &e) {
				std::cerr << e.what() << std::endl;
				return (EXIT_FAILURE);
			}
			hash = BE::Text::digest(buffer, buffer.size());
			break;
		case HashablePart::FILENAME:
			hash = BE::Text::digest(BE::Text::basename(*file_path));
			break;
		case HashablePart::FILEPATH:
			hash = BE::Text::digest(*file_path);
			break;
		case HashablePart::NOTHING:
			hash = BE::Text::basename(*file_path);
			break;
		}
		
		/* Sanity check -- the hash value we're modifying must exist */
		if (what_to_hash != HashablePart::NOTHING) {
			if (hash_rs->containsKey(hash)) {
				switch (action) {
				case Action::REMOVE:
					if (prompt) {
						if (yesOrNo("Remove \"" + hash +
						    "\"?"))
							keys->push_back(hash);
					} else
						keys->push_back(hash);
					break;
				default:
					keys->push_back(hash);
					break;
				}
			} else
				invalidHashKeys += " * " + *file_path + '\n';
		} else {
			switch (action) {
			case Action::REMOVE:
				if (prompt) {
					if (yesOrNo("Remove \"" + hash + "\"?"))
						keys->push_back(hash);
				} else
					keys->push_back(hash);
				break;
			default:
				keys->push_back(hash);
				break;
			}
		}
	}
	
	try {
		switch (action) {
		case Action::ADD:
			insertKeysIntoListRecordStore(sflagval, keys);
			break;
		case Action::REMOVE:
			removeKeysFromListRecordStore(sflagval, keys);
			break;
		default:
			throw BE::Error::StrategyError("Internal inconsistency:"
			    " Can't perform this action on ListRecordStore");
		}
	} catch (BE::Error::Exception &e) {
		std::cerr << e.what() << std::endl;
		return (EXIT_FAILURE);
	}
	
	if (invalidHashKeys.empty() == false) {
		std::cerr << "The following keys were not added because their "
		    "hash translation does not\nexist in the hash translation "
		    "RecordStore: " << '\n' << invalidHashKeys << std::endl;
		    return (EXIT_FAILURE);
	}
	
	return (EXIT_SUCCESS);
}

int
add(
    int argc,
    char *argv[])
{
	HashablePart what_to_hash = HashablePart::NOTHING;
	KeyFormat hashed_key_format = KeyFormat::DEFAULT;
	std::shared_ptr<BE::IO::RecordStore> rs, hash_rs;
	std::vector<std::string> files;
	bool stopOnDuplicate = true;

	if (procargs_add(argc, argv, rs, hash_rs, files, what_to_hash,
	    hashed_key_format, stopOnDuplicate) != EXIT_SUCCESS)
		return (EXIT_FAILURE);

	for (std::vector<std::string>::const_iterator file_path = files.begin();
	    file_path != files.end(); file_path++) {
		/*
		 * Conscious decision to not care about the return value
		 * when inserting because we may have multiple files we'd
		 * like to add and there's no point in quitting halfway.
		 */
		if (BE::IO::Utility::pathIsDirectory(*file_path))
			make_insert_directory_contents(
			    BE::Text::basename(*file_path),
			    BE::Text::dirname(*file_path), rs, hash_rs,
			    what_to_hash, hashed_key_format,
			    stopOnDuplicate);
		else
			make_insert_contents(*file_path, rs, hash_rs,
			    what_to_hash, hashed_key_format,
			    stopOnDuplicate);
	}

	return (EXIT_SUCCESS);
}

int
procargs_remove(
    int argc,
    char *argv[],
    std::vector<std::string> &keys,
    bool &force_removal,
    std::shared_ptr<BiometricEvaluation::IO::RecordStore> &rs)
{
	char c;
        while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
		case 'f':	/* Force removal (do not prompt) */
			force_removal = true;
			break;
		case 'k':	/* Keys to remove */
			keys.push_back(optarg);
			break;
		}
	}

	if (keys.empty()) {
		std::cerr << "Missing required option (-k)." << std::endl;
		return (EXIT_FAILURE);
	}

	/* sflagval here is the RecordStore */
	try {
		rs = BE::IO::RecordStore::openRecordStore(
		    std::string(sflagval), BE::IO::Mode::ReadWrite);
	} catch (BE::Error::Exception &e) {
		if (isRecordStoreAccessible(
		    std::string(sflagval), BE::IO::Mode::ReadWrite))
			std::cerr << "Could not open " << sflagval << " - " <<
			    e.what() << std::endl;
		else
			std::cerr << sflagval << ": Permission denied." <<
			    std::endl;
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}

int
removeFromListRecordStore(
    int argc,
    char *argv[])
{
	bool force_removal = false;
	int status = EXIT_SUCCESS;
	std::shared_ptr<BE::IO::RecordStore> rs;
	std::vector<std::string> keys;
	std::shared_ptr<OrderedSet<std::string>> approvedKeys(
	    new OrderedSet<std::string>());
	
	if (procargs_remove(argc, argv, keys, force_removal, rs) != 
	    EXIT_SUCCESS)
		return (EXIT_FAILURE);
		
	for (std::vector<std::string>::const_iterator key = keys.begin();
	    key != keys.end(); key++)
		if ((force_removal == false && yesOrNo("Remove " + *key + "?",
		    false)) || force_removal == true)
			approvedKeys->push_back(*key);
			
	removeKeysFromListRecordStore(sflagval, approvedKeys);
	
	return (status);
}

int
remove(
    int argc,
    char *argv[])
{
	bool force_removal = false;
	int status = EXIT_SUCCESS;
	std::shared_ptr<BE::IO::RecordStore> rs;
	std::vector<std::string> keys;
		
	if (procargs_remove(argc, argv, keys, force_removal, rs) !=
	    EXIT_SUCCESS)
		return (EXIT_FAILURE);
		
	for (std::vector<std::string>::const_iterator key = keys.begin();
	    key != keys.end(); key++) {
		if ((force_removal == false && yesOrNo("Remove " + *key + "?",
		    false)) || force_removal == true) {
			try {
				rs->remove(*key);
			} catch (BE::Error::Exception &e) {
				std::cerr << "Could not remove " << *key <<
				     ": " << e.what() << std::endl;
				status = EXIT_FAILURE;
			}
		}
	}

	return (status);
}

int
procargs_diff(
    int argc,
    char *argv[],
    std::shared_ptr<BE::IO::RecordStore> &sourceRS,
    std::shared_ptr<BE::IO::RecordStore> &targetRS,
    std::vector<std::string> &keys,
    bool &byte_for_byte)
{
	int rsCount = 0;
	char c;
        while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
		case 'a': {	/* Text file of keys to diff */
			std::vector<std::string> fileKeys;
			try {
				std::string path = std::string(optarg);;
				if (!BE::IO::Utility::fileExists(path))
					throw BE::Error::ObjectDoesNotExist(
					    path);
				fileKeys = readTextFileToVector(path);
			} catch (BE::Error::Exception &e) {
				std::cerr << e.what() << std::endl;
				return (EXIT_FAILURE);
			}

			/* Merge the two lists */
			size_t capacity = keys.size();
			keys.resize(capacity + fileKeys.size());
			/* Use copy().  merge() implies sorted order */
			copy(fileKeys.begin(), fileKeys.end(),
			    keys.begin() + capacity);
			break;
		} case 'f':	/* Byte-for-byte diff method */
			byte_for_byte = true;
			break;
		case 'k':	/* Individual keys to diff */
			keys.push_back(optarg);
			break;
		case 's':	/* Source/target RecordStores */
			/* 
			 * Rescan for -s instead of using sflagval so that
			 * we keep the source and target RecordStores in order.
			 */
			try {
				/* Assign sourceRS first */
				((rsCount == 1) ? sourceRS : targetRS) =
				    BE::IO::RecordStore::openRecordStore(
				    std::string(optarg), BE::IO::Mode::ReadOnly);
				rsCount++;
			} catch (BE::Error::Exception &e) {
				if (isRecordStoreAccessible(
				    std::string(optarg),
				    BE::IO::Mode::ReadOnly))
					std::cerr << "Could not open " <<
					    BE::Text::basename(optarg) <<
					    " - " << e.what() << std::endl;
				else
				    	std::cerr << optarg << ": Permission "
					    "denied." << std::endl;
				return (EXIT_FAILURE);
			}
		}
	}

	if (rsCount != 2) {
		std::cerr << "Must specify only two RecordStores " <<
		    "(-s <rs> -s <rs>)." << std::endl;
		return (EXIT_FAILURE);
	}
	
	return (EXIT_SUCCESS);
}

int
diff(
    int argc,
    char *argv[])
{
	bool byte_for_byte = false;
	int status = EXIT_SUCCESS;
	std::shared_ptr<BE::IO::RecordStore> sourceRS, targetRS;
	std::vector<std::string> keys;
	if (procargs_diff(argc, argv, sourceRS, targetRS, keys,
	    byte_for_byte) != EXIT_SUCCESS)
		return (EXIT_FAILURE);

	std::string sourcePath = sourceRS->getPathname();
	std::string targetPath = targetRS->getPathname();

	/* Don't attempt diff if either RecordStore is empty */
	if (sourceRS->getCount() == 0) {
		std::cerr << "No entries in " << sourcePath << '.' << std::endl;
		return (EXIT_FAILURE);
	} else if (targetRS->getCount() == 0) {
		std::cerr << "No entries in " << targetPath << '.' << std::endl;
		return (EXIT_FAILURE);
	}

	/* If no keys passed, compare every key */
	if (keys.empty()) {	
		std::string key;
		for (;;) {
			try {
				key = sourceRS->sequenceKey();
				keys.push_back(key);
			} catch (BE::Error::ObjectDoesNotExist) {
				/* End of sequence */
				break;
			}
		}
	}
	
	bool sourceExists, targetExists;
	BE::Memory::AutoArray<uint8_t> sourceBuf, targetBuf;
	uint64_t sourceLength, targetLength;
	std::vector<std::string>::const_iterator key;
	for (key = keys.begin(); key != keys.end(); key++) {		
		/* Get sizes to check existence */
		try {
			sourceLength = sourceRS->length(*key);
			sourceExists = true;
		} catch (BE::Error::ObjectDoesNotExist) {
			sourceExists = false;
		}
		try {
			targetLength = targetRS->length(*key);
			targetExists = true;
		} catch (BE::Error::ObjectDoesNotExist) {
			targetExists = false;
		}
		
		/* Difference based on existence */
		if ((sourceExists == false) && (targetExists == false)) {
			std::cout << *key << ": not found." << std::endl;
			status = EXIT_FAILURE;
			continue;
		} else if ((sourceExists == true) && (targetExists == false)) {
			std::cout << *key << ": only in " << sourcePath << std::endl;
			status = EXIT_FAILURE;
			continue;
		} else if ((sourceExists == false) && (targetExists == true)) {
			std::cout << *key << ": only in " << targetPath << std::endl;
			status = EXIT_FAILURE;
			continue;
		}
		
		/* Difference based on size */
		if (sourceLength != targetLength) {
			std::cout << *key << ':' << sourcePath << " and " <<
			    *key << ':' << targetPath << " differ (size)" <<
			    std::endl;
			status = EXIT_FAILURE;
			continue;
		}
		
		/* Difference based on content */
		try {
			sourceBuf = sourceRS->read(*key);
			if (sourceBuf.size() != sourceLength)
				throw BE::Error::StrategyError("Source size");
			targetBuf = targetRS->read(*key);
			if (targetBuf.size() != targetLength)
				throw BE::Error::StrategyError("Target size");
		} catch (BE::Error::Exception &e) {
			std::cerr << "Could not diff " << *key << " (" << 
			    e.what() << ')' << std::endl;
			status = EXIT_FAILURE;
			continue;
		}
		
		if (byte_for_byte) {
			for (uint64_t i = 0; i < sourceLength; i++) {
				if (sourceBuf[i] != targetBuf[i]) {
					std::cout << *key << ':' << sourcePath
					    << " and " << *key << ':' <<
					    targetPath << " differ " <<
					    "(byte for byte)" << std::endl;
					status = EXIT_FAILURE;
					break;
				}
			}
		} else { /* MD5 */
			try {
				if (BE::Text::digest(sourceBuf, sourceLength) !=
				    BE::Text::digest(targetBuf, targetLength)) {
					std::cout << *key << ':' << sourcePath <<
					    " and " << *key << ':' <<
					    targetPath << " differ " <<
					    "(MD5)" << std::endl;
					status = EXIT_FAILURE;
				}
			} catch (BE::Error::Exception &e) {
				std::cerr << "Could not diff " << *key << " ("
				    << e.what() << ')' << std::endl;
				status = EXIT_FAILURE;
				continue;
			}
		}
	}

	return (status);
}

int
procargs_rename(
    int argc,
    char *argv[],
    std::string &newPath)
{
	int rsCount = 0;
	char c;
        while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
		case 's':	/* Source/target RecordStores */
			switch (rsCount) {
			case 0:
				/* Ensure first RecordStore exists */
				try {
					BE::IO::RecordStore::openRecordStore(
					    std::string(optarg),
					    BE::IO::Mode::ReadWrite);
				} catch (BE::Error::Exception &e) {
					if (isRecordStoreAccessible(
					    std::string(optarg),
					    BE::IO::Mode::ReadWrite))
						std::cerr << "Could not "
						    "open " << optarg <<
						    " - " << e.what() <<
						    std::endl;
					else
						std::cerr << optarg <<
						": Permission denied." <<
						std::endl;
					return (EXIT_FAILURE);
				}
				sflagval = optarg;
				break;
			default:
				newPath = optarg;
				break;
			}
			rsCount++;
			break;
		}
	}

	if (rsCount != 2) {
		std::cerr << "Must specify only two RecordStores " <<
		    "(-s <existing_rs> -s <new_rs>)." << std::endl;
		return (EXIT_FAILURE);
	}

	/* Ensure new path doesn't already exist */
	if (BE::IO::Utility::fileExists(newPath)) {
		std::cerr << newPath << " already exists." << std::endl;
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}

int
rename(
    int argc,
    char *argv[])
{
	std::string newPath;
	if (procargs_rename(argc, argv, newPath) != EXIT_SUCCESS)
		return (EXIT_FAILURE);

	/* Change name in same directory */
	try {
		std::shared_ptr<BE::IO::RecordStore> rs = BE::IO::RecordStore::
		    openRecordStore(std::string(sflagval), BE::IO::Mode::ReadWrite);
		rs->move(newPath);
	} catch (BE::Error::Exception &e) {
		std::cerr << e.what() << std::endl;
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	Action action = procargs(argc, argv);
	switch (action) {
	case Action::ADD:
		if (specialProcessingFlags & static_cast<
		    std::underlying_type<SpecialProcessing>::type>(
		    SpecialProcessing::LISTRECORDSTORE))
			return (modifyListRecordStore(argc, argv, action));
		return (add(argc, argv));
	case Action::DIFF:
		return (diff(argc, argv));
	case Action::DISPLAY:
		/* FALLTHROUGH */
	case Action::DUMP:
		return (extract(argc, argv, action));
	case Action::LIST:
		return (listRecordStore(argc, argv));
	case Action::MAKE:
		return (make(argc, argv));
	case Action::MERGE:
		return (merge(argc, argv));
	case Action::REMOVE:
		if (specialProcessingFlags & static_cast<
		    std::underlying_type<SpecialProcessing>::type>(
		    SpecialProcessing::LISTRECORDSTORE))
			return (modifyListRecordStore(argc, argv, action));
		return (remove(argc, argv));
	case Action::RENAME:
		return (rename(argc, argv));
	case Action::VERSION:
		return (version(argc, argv));
	case Action::UNHASH:
		return (unhash(argc, argv));
	case Action::QUIT:
	default:
		return (EXIT_FAILURE);
	}
}
