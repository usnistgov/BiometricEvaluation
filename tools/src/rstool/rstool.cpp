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
#include <libgen.h>

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
#include <be_memory_autoarray.h>

#include <image_additions.h>
#include <lrs_additions.h>
#include <rstool.h>

uint16_t specialProcessingFlags = SpecialProcessing::NA;

using namespace BiometricEvaluation;
using namespace std;

vector<string>
readTextFileToVector(
    const string &filePath,
    bool ignoreComments,
    bool ignoreBlankLines)
    throw (Error::FileError,
    Error::ObjectDoesNotExist)
{
	if (IO::Utility::fileExists(filePath) == false)
		throw Error::ObjectDoesNotExist();
	if (IO::Utility::pathIsDirectory(filePath))
		throw Error::FileError(filePath + " is a directory");
		
	string line;
	ifstream input;
	vector<string> rv;
	input.open(filePath.c_str(), ifstream::in);
	for (;;) {
		/* Read one path from text file */
		input >> line;
		if (input.eof())
			break;
		else if (input.bad())
			throw Error::FileError(filePath + " bad bit set");
		
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
		} catch (out_of_range) {
			continue;
		}
		
		/* Add the line */
		rv.push_back(line);
	}
	
	return (rv);
}

bool
yesOrNo(
    const string &prompt,
    bool default_answer,
    bool show_options,
    bool allow_default_answer)
{	
	string input;
	for (;;) {
		cout << prompt;
		if (show_options) {
			if (allow_default_answer) {
				if (default_answer == true)
					cout << " ([Y]/n)";
				else
					cout << " (y/[N])";
			} else
				cout << " (y/n)";
		}
		cout << ": ";
		getline(cin, input);
		try {
			switch (input.at(0)) {
			case 'Y':
			case 'y':
				return (true);
			case 'n':
			case 'N':
				return (false);
			}
		} catch (out_of_range) {
			if (allow_default_answer)
				return (default_answer);
		}
	}
}


void usage(char *exe)
{
	cerr << "Usage: " << exe << " <action> -s <RS> [options]" << endl;
	cerr << "Actions: add, display, dump, list, make, merge, remove, "
	    "rename, version, unhash" << endl;

	cerr << endl;

	cerr << "Common:" << endl;
	cerr << "\t-c\t\tIf hashing, hash file/record contents" << endl;
	cerr << "\t-p\t\tIf hashing, hash file path" << endl;
	cerr << "\t-o <...>\tOutput directory" << endl;
	cerr << "\t-s <path>\tRecordStore" << endl;

	cerr << endl;

	cerr << "Add Options:" << endl;
	cerr << "\t-a <file/dir>\tFile/directory contents to add" << endl;
	cerr << "\t-f\t\tForce insertion even if the same key already exists" <<
	    endl;
	cerr << "\t-h <hash_rs>\tExisting hash translation RecordStore" << endl;
	cerr << "\t-k(fp)\t\tPrint 'f'ilename or file'p'ath of key as value " <<
	    endl << "\t\t\tin hash translation RecordStore" << endl;
	cerr << "\tfile/dir ...\tFiles/directory contents to add as a " <<
	    "record" << endl;

	cerr << endl;

	cerr << "Diff Options:" << endl;
	cerr << "\t-a <file>\tText file with keys to compare" << endl;
	cerr << "\t-f\t\tCompare files byte for byte " <<
	    "(as opposed to checksum)" << endl;
	cerr << "\t-k <key>\tKey to compare" << endl;

	cerr << endl;

	cerr << "Display/Dump Options:" << endl;
	cerr << "\t-h <hash_rs>\tUnhash keys as found in existing translation "
	    "RecordStore" <<
	    endl;
	cerr << "\t-k <key>\tKey to dump" << endl;
	cerr << "\t-r <#-#>\tRange of keys" << endl;
	cerr << "\t-f\t\tVisualize image or AN2K record" << endl;

	cerr << endl;

	cerr << "Make Options:" << endl;
	cerr << "\t-a <text>\tText file with paths to files or directories "
	    "to\n\t\t\tinitially add as records (multiple)" << endl;
	cerr << "\t-h <hash_rs>\tHash keys and save translation RecordStore" <<
	    endl;
	cerr << "\t-f\t\tForce insertion even if the same key already exists" <<
	    endl;
	cerr << "\t-k(fp)\t\tPrint 'f'ilename or file'p'ath of key as value " <<
	    endl << "\t\t\tin hash translation RecordStore" << endl;
	cerr << "\t-t <type>\tType of RecordStore to make" << endl;
	cerr << "\t\t\tWhere <type> is Archive, BerkeleyDB, File, List, " <<
	    "SQLite" << endl;
	cerr << "\t-s <sourceRS>\tSource RecordStore, if -t is List" << endl;
	cerr << "\t-z\t\tCompress records with default strategy\n" <<
	"\t\t\t(same as -Z GZIP)" << endl;
	cerr << "\t-Z <type>\tCompress records with <type> compression" <<
	    "\n\t\t\tWhere type is GZIP" << endl;
	cerr << "\t<file> ...\tFiles/dirs to add as a record" << endl;
	cerr << "\t-q\t\tSkip the confirmation step" << endl;

	cerr << endl;

	cerr << "Merge Options:" << endl;
	cerr << "\t-a <RS>\t\tRecordStore to be merged (multiple)" << endl;
	cerr << "\t-h <RS>\t\tHash keys and store a hash translation "
	    "RecordStore" << endl;
	cerr << "\t-t <type>\tType of RecordStore to make" << endl;
	cerr << "\t\t\tWhere <type> is Archive, BerkeleyDB, File" << endl;
	cerr << "\t<RS> ...\tRecordStore(s) to be merged " << endl;

	cerr << endl;
	
	cerr << "Remove Options:" << endl;
	cerr << "\t-f\t\tForce removal, do not prompt" << endl;
	cerr << "\t-k <key>\tThe key to remove" << endl;
	
	cout << endl;

	cerr << "Rename Options:" << endl;
	cerr << "\t-s <new_name>\tNew name for the RecordStore" << endl;

	cerr << endl;

	cerr << "Unhash Options:" << endl;
	cerr << "\t-h <hash>\tHash to unhash" << endl;

	cerr << endl;
}

bool
isRecordStoreAccessible(
    const string &name,
    const string &parentDir,
    const uint8_t mode)
{
	bool (*isAccessible)(const string&) = NULL;
	switch (mode) {
	case IO::READONLY:
		isAccessible = IO::Utility::isReadable;
		break;
	case IO::READWRITE:
		/* FALLTHROUGH */
	default:
		isAccessible = IO::Utility::isWritable;
		break;
	}
	
	string path;
	IO::Utility::constructAndCheckPath(name, parentDir, path);
	
	/* Test if the generic RecordStore files have the correct permissions */
	return ((isAccessible(path) && 
	    isAccessible(path + '/' + IO::RecordStore::CONTROLFILENAME) &&
	    isAccessible(path + '/' + name)));
}

string validate_rs_type(const string &type)
{
	if (strcasecmp(type.c_str(), IO::RecordStore::FILETYPE.c_str()) == 0)
		return (IO::RecordStore::FILETYPE);
	else if (strcasecmp(type.c_str(),
	    IO::RecordStore::BERKELEYDBTYPE.c_str()) == 0)
		return (IO::RecordStore::BERKELEYDBTYPE);
	else if (strcasecmp(type.c_str(),
	    IO::RecordStore::ARCHIVETYPE.c_str()) == 0)
		return (IO::RecordStore::ARCHIVETYPE);
	else if (strcasecmp(type.c_str(),
	    IO::RecordStore::SQLITETYPE.c_str()) == 0)
		return (IO::RecordStore::SQLITETYPE);
	else if (strcasecmp(type.c_str(),
	    IO::RecordStore::LISTTYPE.c_str()) == 0)
		return (IO::RecordStore::LISTTYPE);

	return ("");
}

Action::Type procargs(int argc, char *argv[])
{
	if (argc == 1) {
		usage(argv[0]);
		return (Action::QUIT);
	}

	Action::Type action;
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
	else if (strcasecmp(argv[1], RENAME_ARG.c_str()) == 0)
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
		case 'o':	/* Output directory */
			if (IO::Utility::constructAndCheckPath(
			    Text::filename(optarg), Text::dirname(optarg),
			    oflagval)) {
				if (!IO::Utility::pathIsDirectory(optarg)) {
					cerr << optarg << " is not a "
					    "directory." << endl;
					return (Action::QUIT);
				}
			} else {
				if (mkdir(oflagval.c_str(), S_IRWXU) != 0) {
					cerr << "Could not create " <<
					    oflagval << " - " <<
					    Error::errorStr();
					return (Action::QUIT);
				}
			}
			break;
		case 's':	/* Main RecordStore */
			sflagval = optarg;
			break;
		}
	}
	/* Reset getopt() */
	optind = 2;

	/* Sanity check */
	if (sflagval.empty()) {
		cerr << "Missing required option (-s)." << endl;
		return (Action::QUIT);
	}
	
	/* Special processing needed for ListRecordStores */
	if (isListRecordStore(sflagval))
		specialProcessingFlags |= SpecialProcessing::LISTRECORDSTORE;

	return (action);
}

int
procargs_extract(
    int argc,
    char *argv[],
    bool &visualize,
    string &key,
    string &range,
    tr1::shared_ptr<IO::RecordStore> &rs,
    tr1::shared_ptr<IO::RecordStore> &hash_rs)
{	
	char c;
        while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
		case 'f':	/* Visualize */
			visualize = true;
			break;
		case 'h':	/* Existing hash translation RecordStore */
			try {
				hash_rs = IO::RecordStore::openRecordStore(
				    Text::filename(optarg),
				    Text::dirname(optarg), IO::READONLY);
			} catch (Error::Exception &e) {
				if (isRecordStoreAccessible(
				    Text::filename(optarg),
				    Text::dirname(optarg),
				    IO::READONLY))
    					cerr << "Could not open " << optarg <<
					    " -- " << e.getInfo() << endl;
				else
					cerr << optarg << ": Permission "
					    "denied." << endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'k':	/* Extract key */
			key.assign(optarg);
			break;
		case 'r':	/* Extract range of keys */
			range.assign(optarg);
			break;
		}
	}

	if (!key.empty() && !range.empty()) {
		cerr << "Choose only one (-k or -r)." << endl;
		return (EXIT_FAILURE);
	}

	/* -s option in dump context refers to the source RecordStore */
	if (!IO::Utility::fileExists(sflagval)) {
		cerr << sflagval << " was not found." << endl;
		return (EXIT_FAILURE);
	}
	try {
		rs = IO::RecordStore::openRecordStore(Text::filename(sflagval),
		    Text::dirname(sflagval), IO::READONLY);
	} catch (Error::Exception &e) {
  		if (isRecordStoreAccessible(Text::filename(sflagval),
		    Text::dirname(sflagval), IO::READONLY))
			cerr << "Could not open " << sflagval << ".  " <<
			    e.getInfo() << endl;
		else
		    	cerr << sflagval << ": Permission denied." << endl;
		return (EXIT_FAILURE);
	}

	/* If user didn't specify, dump the entire RecordStore */
	if (range.empty() && key.empty()) {
		stringstream out;
		out << "1-" << rs->getCount();
		range = out.str();
	}

	return (EXIT_SUCCESS);
}

int
display(
    const string &key,
    Memory::AutoArray<uint8_t> &value)
{
	value.resize(value.size() + 1);
	value[value.size() - 1] = '\0';

	cout << key << " = " << value << endl;

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
	tr1::shared_ptr<BiometricEvaluation::Image::Image> image;
	try {
		image = BiometricEvaluation::Image::Image::openImage(value);
		isImage = true;
	} catch (BiometricEvaluation::Error::Exception &e) {
		/* Ignore */
	}

	if (isImage) {
		try {
			::displayImage(image);
			return (EXIT_SUCCESS);
		} catch (BiometricEvaluation::Error::Exception) {
			return (EXIT_FAILURE);
		}
	}

	/* At this point, we're not an Image */
	try {
		displayAN2K(value);
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
    const string &key,
    Memory::AutoArray<uint8_t> &value)
{
	/* Possible that keys could have slashes */
	if (key.find('/') != string::npos) {
		if (IO::Utility::makePath(oflagval + "/" + Text::dirname(key),
		    S_IRWXU)) {
			cerr << "Could not create path to store file (" <<
			    oflagval + "/" + Text::dirname(key) << ")." <<
			    endl;
			return (EXIT_FAILURE);
		}
	}

	FILE *fp = fopen((oflagval + "/" + key).c_str(), "wb");
	if (fp == NULL) {
		cerr << "Could not create file." << endl;
		return (EXIT_FAILURE);
	}
	if (fwrite(value, 1, value.size(), fp) != value.size()) {
		cerr << "Could not write entry." << endl;
		if (fp != NULL)
			fclose(fp);
		return (EXIT_FAILURE);
	}
	fclose(fp);

	
	return (EXIT_SUCCESS);
}


int extract(int argc, char *argv[], Action::Type action)
{
	bool visualize = false;
	string key = "", range = "";
	tr1::shared_ptr<IO::RecordStore> rs, hash_rs;
	if (procargs_extract(argc, argv, visualize, key, range, rs, hash_rs) !=
	    EXIT_SUCCESS)
		return (EXIT_FAILURE);

	Memory::AutoArray<uint8_t> buf;
	Memory::AutoArray<char> hash_buf;
	if (!key.empty()) {
		try {
			buf.resize(rs->length(key));
			rs->read(key, buf);
		} catch (Error::ObjectDoesNotExist) {
			 /* It's possible the key should be hashed */
			try {
				string hash = Text::digest(key);
				buf.resize(rs->length(hash));
				rs->read(hash, buf);
			} catch (Error::Exception &e) {
				cerr << "Error extracting " << key << " - " <<
				    e.getInfo() << endl;	
				return (EXIT_FAILURE);
			}
		} catch (Error::Exception &e) {
			cerr << "Error extracting " << key << endl;	
			return (EXIT_FAILURE);
		}
	
		/* Unhash, if desired */
		if (hash_rs.get() != NULL) {
			try {
				hash_buf.resize(hash_rs->length(key) + 1);
				hash_rs->read(key, hash_buf);
				hash_buf[hash_buf.size() - 1] = '\0';
				key.assign(hash_buf);
			} catch (Error::Exception &e) {
				cerr << "Could not unhash " << key << " - " <<
				    e.getInfo() << endl;
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
			cerr << "Invalid action received (" <<
			    action << ')' << endl;
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

		vector<string> ranges = Text::split(range, '-');
		if (ranges.size() != 2) {
			cerr << "Invalid value (-r)." << endl;
			return (EXIT_FAILURE);
		}

		string next_key;
		for (int i = 0; i < atoi(ranges[0].c_str()) - 1; i++) {
			try {
				rs->sequence(next_key, NULL);
			} catch (Error::Exception &e) {
				cerr << "Could not sequence to record " <<
				    ranges[0] << endl;
				return (EXIT_FAILURE);
			}
		}
		for (int i = atoi(ranges[0].c_str());
		    i <= atoi(ranges[1].c_str()); i++) {
			try {
				buf.resize(rs->sequence(next_key, NULL));
				rs->read(next_key, buf);
			} catch (Error::Exception &e) {
				cerr << "Could not read key " << i << " - " <<
				    e.getInfo() << "." << endl;
				return (EXIT_FAILURE);
			}

			/* Unhash, if desired */
			if (hash_rs.get() != NULL) {
				try {
					hash_buf.resize(hash_rs->length(
					    next_key) + 1);
					hash_rs->read(next_key, hash_buf);
					hash_buf[hash_buf.size() - 1] = '\0';
					next_key.assign(hash_buf);
				} catch (Error::Exception &e) {
					cerr << "Could not unhash " << 
					    next_key << " - " << e.getInfo() <<
					    endl;
					return (EXIT_FAILURE);
				}
			}

			switch (action) {
			case Action::DUMP:
				if (dump(next_key, buf) != EXIT_SUCCESS)
					return (EXIT_FAILURE);
				break;
			case Action::DISPLAY:
				if (display(next_key, buf) != EXIT_SUCCESS)
					return (EXIT_FAILURE);
				break;
			default:
				cerr << "Invalid action received (" <<
				    action << ')' << endl;
				return (EXIT_FAILURE);
			}
		}
	}

	return (EXIT_SUCCESS);
}

int listRecordStore(int argc, char *argv[])
{
	tr1::shared_ptr<IO::RecordStore> rs;
	try {
		rs = IO::RecordStore::openRecordStore(Text::filename(sflagval),
		    Text::dirname(sflagval), IO::READONLY);
	} catch (Error::Exception &e) {
		if (isRecordStoreAccessible(Text::filename(sflagval),
		    Text::dirname(sflagval), IO::READONLY))
			cerr << "Could not open RecordStore - " <<
			    e.getInfo() << endl;
		else
		    	cerr << sflagval << ": Permission denied." << endl;
		return (EXIT_FAILURE);
	}

	string key;
	try {
		for (;;) {
			rs->sequence(key, NULL);
			cout << key << endl;
		}
	} catch (Error::ObjectDoesNotExist) {
		/* RecordStore has been exhausted */
		return (EXIT_SUCCESS);
	} catch (Error::Exception &e) {
		cerr << "Could not list RecordStore - " << e.getInfo() << endl;
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);	/* Not reached */
}

int
procargs_make(
    int argc,
    char *argv[],
    string &hash_filename,
    HashablePart::Type &what_to_hash,
    KeyFormat::Type &hashed_key_format,
    string &type,
    vector<string> &elements,
    bool &compress,
    IO::Compressor::Kind &compressorKind,
    bool &stopOnDuplicate)
{
	what_to_hash = HashablePart::NOTHING;
	hashed_key_format = KeyFormat::DEFAULT;
	compress = false;
	compressorKind = IO::Compressor::GZIP;
	stopOnDuplicate = true;

	char c;
        bool textProvided = false, dirProvided = false, otherProvided = false;
        bool quiet = false;
        while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
		case 'a': {	/* Text file with paths to be added */
			string path = Text::dirname(optarg) + '/' + 
			    Text::filename(optarg);
			if (!IO::Utility::fileExists(path)) {
				cerr << optarg << " does not exist." << endl;
				return (EXIT_FAILURE);
			}
			
			/* -a used to take a directory (backwards compat) */
			if (IO::Utility::pathIsDirectory(path)) {
				elements.push_back(path);
				dirProvided = true;
				break;
			}
			
			/* Parse the paths in the text file */
			ifstream input;
			string line;
			input.open(optarg, ifstream::in);
			textProvided = true;

			for (;;) {
				/* Read one path from text file */
				input >> line;
				if (input.eof())
					break;
				else if (input.bad()) {
					cerr << "Error reading paths (" << 
					    input << ')' << endl;
					return (EXIT_FAILURE);
				}
				
				/* Ignore comments and newlines */
				try {
					if (line.at(0) == '#' ||
					    line.at(0) == '\n')
					    	continue;
				} catch (out_of_range) {
					continue;
				}
				
				elements.push_back(Text::dirname(line) + '/' +
				    Text::filename(line));
			}
			input.close();
			break;
		}
		case 'c':	/* Hash contents */
			if (what_to_hash == HashablePart::NOTHING)
				what_to_hash = HashablePart::FILECONTENTS;
			else {
				cerr << "More than one hash method selected." <<
				    endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'f':	/* Force -- don't stop on duplicate */
			stopOnDuplicate = false;
			break;
		case 'h':	/* Hash translation RecordStore */
			hash_filename.assign(optarg);
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
				cerr << "Invalid format specifier for hashed "
				    "key." << endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'p':	/* Hash file path */
			if (what_to_hash == HashablePart::NOTHING)
				what_to_hash = HashablePart::FILEPATH;
			else {
				cerr << "More than one hash method selected." <<
				    endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'q':	/* Don't show confirmation */
			quiet = true;
			break;
		case 't':	/* Destination RecordStore type */
			type = validate_rs_type(optarg);
			if (type.empty()) {
				cerr << "Invalid type format (-t)." << endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'z':	/* Compress */
			compress = true;
			compressorKind = IO::Compressor::GZIP;
			break;
		case 'Z':	/* Compression strategy */
			compress = true;
			if (strcasecmp("GZIP", optarg) == 0)
				compressorKind = IO::Compressor::GZIP;
			else {
				cerr << "Invalid compression kind -- " <<
				    optarg << endl;
				return (EXIT_FAILURE);
			}
			break;
		}
	}
	/* Remaining arguments are files or directories to add */
	for (int i = optind; i < argc; i++)
		elements.push_back(Text::dirname(argv[i]) + "/" +
		    Text::filename(argv[i]));
	
	if (hashed_key_format == KeyFormat::DEFAULT) {
		switch (what_to_hash) {
		case HashablePart::FILEPATH:
			hashed_key_format = KeyFormat::FILEPATH; break;
		default:
			hashed_key_format = KeyFormat::FILENAME; break;
		}
	}

	if (type.empty())
		type.assign(IO::RecordStore::BERKELEYDBTYPE);

	/* Sanity check -- don't hash without recording a translation */
	if (hash_filename.empty() && (what_to_hash != HashablePart::NOTHING)) {
		cerr << "Specified hash method without -h." << endl;
		return (EXIT_FAILURE);
	}
	
	/* Sanity check -- don't compress and make a ListRecordStore */
	if (compress && type == IO::RecordStore::LISTTYPE) {
		cerr << "Can't compress ListRecordStore entries." << endl;
		    return (EXIT_FAILURE);
	}

	/* Choose to hash filename by default */
	if ((hash_filename.empty() == false) && (what_to_hash ==
	    HashablePart::NOTHING))
		what_to_hash = HashablePart::FILENAME;

        if (quiet)
                return (EXIT_SUCCESS);

        return (makeHumanConfirmation(argc, argv, type, what_to_hash,
            hashed_key_format, hash_filename, textProvided, dirProvided,
            otherProvided, compress, compressorKind, stopOnDuplicate));
}

int
makeHumanConfirmation(
    int argc,
    char *argv[],
    const std::string &kind,
    HashablePart::Type what_to_hash,
    KeyFormat::Type hashed_key_format,
    const std::string &hash_filename,
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
	std::cout << kind << " RecordStore named \"";

	std::string rsName = "";
	if (kind == BiometricEvaluation::IO::RecordStore::LISTTYPE) {
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
	} else
		rsName = sflagval;

	std::cout << rsName << '"' << std::endl;
	if (kind == BiometricEvaluation::IO::RecordStore::LISTTYPE)
		std::cout << "* \"" << rsName << "\" will refer to keys " <<
		    "from \"" << sflagval << "\"" << std::endl;
	std::cout << "* \"" << rsName << "\" will be stored in \"" <<
	    oflagval << '"' << std::endl;
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
		    BiometricEvaluation::IO::Compressor::kindToString(
		    compressorKind) << " "
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
		if (kind == BiometricEvaluation::IO::RecordStore::LISTTYPE)
			std::cout << "is ";
		else
			std::cout << "will be ";

		std::cout << "named \"" << hash_filename << '"' << std::endl;
		std::cout << "* The values in \"" << hash_filename <<
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
	if (kind != BiometricEvaluation::IO::RecordStore::LISTTYPE) {
		std::cout << "* If a duplicate key is encountered, " <<
		    argv[0] << " will ";
		if (stopOnDuplicate)
			std::cout << "stop and exit" << std::endl;
		else
			std::cout << "overwrite the key/value pair" <<
			    std::endl;
	} else
		std::cout << "* Keys added to \"" << rsName << "\" must "
		    "already exist in \"" << sflagval << "\"" << std::endl;

	std::cout << std::endl;
	if (!yesOrNo("Sound good?", false, true, false))
		return (EXIT_FAILURE);

	return (EXIT_SUCCESS);
}

int make_insert_contents(const string &filename,
    const tr1::shared_ptr<IO::RecordStore> &rs,
    const tr1::shared_ptr<IO::RecordStore> &hash_rs,
    const HashablePart::Type what_to_hash,
    const KeyFormat::Type hashed_key_format,
    bool stopOnDuplicate)
{
	static Memory::uint8Array buffer;
	static uint64_t buffer_size = 0;
	static ifstream buffer_file;

	try {
		buffer_size = IO::Utility::getFileSize(filename);
		buffer.resize(buffer_size);
	} catch (Error::Exception &e) {
		cerr << "Could not get file size for " << filename << endl;
		return (EXIT_FAILURE);
	}

	/* Extract file into buffer */
	buffer_file.open(filename.c_str(), ifstream::binary);
	buffer_file.read((char *)&(*buffer), buffer_size);
	if (buffer_file.bad()) {
		cerr << "Error reading file (" << filename << ')' << endl;
		return (EXIT_FAILURE);
	}
	buffer_file.close();
	
	static string key = "", hash_value = "";
	try {
		key = Text::filename(filename);
		if (hash_rs.get() == NULL) {
			try {
				rs->insert(key, buffer, buffer_size);
			} catch (Error::ObjectExists &e) {
				if (stopOnDuplicate)
					throw e;
					
				cerr << "Could not insert " + filename + " "
				    "(key = \"" + key + "\") because "
				    "it already exists.  Replacing..." << endl;
				rs->replace(key, buffer, buffer_size);
			}
		} else {
			switch (what_to_hash) {
			case HashablePart::FILECONTENTS:
				hash_value = Text::digest(buffer, buffer_size);
				break;
			case HashablePart::FILENAME:
				hash_value = Text::digest(key);
				break;
			case HashablePart::FILEPATH:
				hash_value = Text::digest(filename);
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
				cerr << "Invalid key format received (" <<
				    hashed_key_format << ')' << endl;
				return (EXIT_FAILURE);
			}

			try {
				rs->insert(hash_value, buffer, buffer_size);
			} catch (Error::ObjectExists &e) {
				if (stopOnDuplicate)
					throw e;
					
				cerr << "Could not insert " + filename + " "
				    "(key: \"" + hash_value + "\") because "
				    "it already exists.  Replacing..." << endl;
				rs->replace(hash_value, buffer, buffer_size);
			}

			try {
				hash_rs->insert(hash_value, key.c_str(),
				    key.size());
			} catch (Error::ObjectExists &e) {
				if (stopOnDuplicate)
					throw e;
					
				cerr << "Could not insert " + filename + " "
				    "(key: \"" + hash_value + "\") into hash "
				    "translation RecordStore because it "
				    "already exists.  Replacing..." << endl;
				hash_rs->replace(hash_value, key.c_str(),
				    key.size());
			}
		}
	} catch (Error::Exception &e) {
		cerr << "Could not add contents of " << filename <<
		    " to RecordStore - " << e.getInfo() << endl;
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}

int
make_insert_directory_contents(
    const string &directory,
    const string &prefix,
    const tr1::shared_ptr<IO::RecordStore> &rs,
    const tr1::shared_ptr<IO::RecordStore> &hash_rs,
    const HashablePart::Type what_to_hash,
    const KeyFormat::Type hashed_key_format,
    bool stopOnDuplicate)
    throw (Error::ObjectDoesNotExist, Error::StrategyError)
{
	struct dirent *entry;
	DIR *dir = NULL;
	string dirpath, filename;

	dirpath = prefix + "/" + directory;
	if (!IO::Utility::fileExists(dirpath))
		throw Error::ObjectDoesNotExist(dirpath + " does not exist");
	dir = opendir(dirpath.c_str());
	if (dir == NULL)
		throw Error::StrategyError(dirpath + " could not be opened");
	
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_ino == 0)
			continue;
		if ((strcmp(entry->d_name, ".") == 0) ||
		    (strcmp(entry->d_name, "..") == 0))
			continue;

		filename = dirpath + "/" + entry->d_name;

		/* Recursively remove subdirectories and files */
		if (IO::Utility::pathIsDirectory(filename)) {
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
			throw Error::StrategyError("Could not close " + 
			    dirpath + " (" + Error::errorStr() + ")");

	return (EXIT_SUCCESS);
}

int
makeListRecordStore(
    int argc,
    char *argv[],
    vector<string> &elements,
    HashablePart::Type what_to_hash,
    KeyFormat::Type hashed_key_format)
{
	string newRSName, existingRSPath;
	tr1::shared_ptr<IO::RecordStore> targetRS;
	
	/*
	 * Rescan for -s (rstool make -s <newRS> -s <existingRS>) and -h (must
	 * exist).
	 */
	char c;
	uint8_t rsCount = 0;
	tr1::shared_ptr<IO::RecordStore> hash_rs;
	optind = 2;
        while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
		case 'h':
			try {
				hash_rs = IO::RecordStore::openRecordStore(
				    Text::filename(optarg),
				    Text::dirname(optarg), IO::READONLY);
			} catch (Error::Exception &e) {
				cerr << "Could not open hash RecordStore, but "
				    "it must exist when creating a hashed "
				    "ListRecordStore." << endl;
				return (EXIT_FAILURE);
			}
			break;
 		case 's':
			switch (rsCount) {
			case 0:
				newRSName = optarg;
				sflagval = newRSName;
				rsCount++;
				break;
			case 1:
				try {
					targetRS =
					    IO::RecordStore::openRecordStore(
					    Text::filename(optarg),
					    Text::dirname(optarg),
					    IO::READONLY);
				} catch (Error::Exception &e) {
					if (isRecordStoreAccessible(
					    Text::filename(optarg),
					    Text::dirname(optarg),
					    IO::READONLY))
						cerr << "Could not open " <<
						Text::filename(optarg) <<
						" - " << e.getInfo() << endl;
					else
						cerr << optarg <<
						    ": Permission denied." <<
						    endl;
					return (EXIT_FAILURE);
				}
				existingRSPath = optarg;
				rsCount++;
				break;
			default:
				cerr << "Too many -s options while making a "
				    "ListRecordStore" << endl;
				return (EXIT_FAILURE);
			}
			break;
		}
	}
	if (rsCount != 2) {
		cerr << "Not enough -s options while making a "
		    "ListRecordStore." << endl;
		return (EXIT_FAILURE);
	}
	
	try {
		constructListRecordStore(newRSName, oflagval, existingRSPath);
	} catch (Error::Exception &e) {
		cerr << e.getInfo() << endl;
		return (EXIT_FAILURE);
	}
	
	return (modifyListRecordStore(argc, argv, Action::ADD));
}

int make(int argc, char *argv[])
{
	string hash_filename = "", type = "";
	vector<string> elements;
	HashablePart::Type what_to_hash = HashablePart::NOTHING;
	KeyFormat::Type hashed_key_format = KeyFormat::DEFAULT;
	bool compress = false;
	IO::Compressor::Kind compressorKind;
	bool stopOnDuplicate = true;

	if (procargs_make(argc, argv, hash_filename, what_to_hash,
	    hashed_key_format, type, elements, compress, compressorKind,
	    stopOnDuplicate) != EXIT_SUCCESS)
		return (EXIT_FAILURE);
		
	if (type == IO::RecordStore::LISTTYPE) {
		specialProcessingFlags |= SpecialProcessing::LISTRECORDSTORE;
		return (makeListRecordStore(argc, argv, elements, what_to_hash,
		    hashed_key_format));
	}

	tr1::shared_ptr<IO::RecordStore> rs;
	tr1::shared_ptr<IO::RecordStore> hash_rs;
	try {
		if (compress)
			rs.reset(new IO::CompressedRecordStore(sflagval,
			    "<Description>", type, oflagval, compressorKind));
		else
			rs = IO::RecordStore::createRecordStore(sflagval,
			    "<Description>", type, oflagval);
		if (!hash_filename.empty())
			/* No need to compress hash translation RS */
			hash_rs = IO::RecordStore::createRecordStore(
			    hash_filename,
			    "Hash Translation for " + sflagval, type, oflagval);
	} catch (Error::Exception &e) {
		cerr << "Could not create " << sflagval << " - " <<
		    e.getInfo() << endl;
		return (EXIT_FAILURE);
	}

	for (unsigned int i = 0; i < elements.size(); i++) {
		if (IO::Utility::pathIsDirectory(elements[i])) {
			try {
				/* Recursively insert contents of directory */
				if (make_insert_directory_contents(
				    Text::filename(elements[i]),
				    Text::dirname(elements[i]),
				    rs, hash_rs, what_to_hash,
				    hashed_key_format,
				    stopOnDuplicate) != EXIT_SUCCESS)
			    		return (EXIT_FAILURE);
			} catch (Error::Exception &e) {
				cerr << "Could not add contents of dir " <<
				    elements[i] << " - " << e.getInfo() << endl;
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
    string &type,
    vector<string> &child_rs,
    string &hash_filename,
    HashablePart::Type &what_to_hash,
    KeyFormat::Type &hashed_key_format)
{
	what_to_hash = HashablePart::NOTHING;
	hashed_key_format = KeyFormat::DEFAULT;

	char c;
	child_rs.empty();
        while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
		case 't':	/* Destination RecordStore type */
			type = validate_rs_type(optarg);
			if (type.empty()) {
				cerr << "Invalid type format (-t)." << endl;
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
				cerr << "More than one hash method selected." <<
				    endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'h':	/* Hash translation RecordStore */
			hash_filename.assign(optarg);
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
				cerr << "Invalid format specifier for hashed "
				    "key." << endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'p':	/* Hash file path */
			/* Sanity check */
			cerr << "Cannot hash file path when merging "
			    "RecordStores -- there are no paths." << endl;
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
		cerr << "Missing required option (-a)." << endl;
		return (EXIT_FAILURE);
	}

	if (type.empty())
		type.assign(IO::RecordStore::BERKELEYDBTYPE);

	/* -s option in dump context refers to the source RecordStore */
	if (IO::Utility::fileExists(sflagval)) {
		cerr << sflagval << " already exists." << endl;
		return (EXIT_FAILURE);
	}

	/* Sanity check -- don't hash without recording a translation */
	if (hash_filename.empty() && (what_to_hash !=
	    HashablePart::NOTHING)) {
		cerr << "Specified hash method without -h." << endl;
		return (EXIT_FAILURE);
	}

	/* Choose to hash filename by default */
	if ((hash_filename.empty() == false) && (what_to_hash ==
	    HashablePart::NOTHING))
		what_to_hash = HashablePart::FILENAME;

	return (EXIT_SUCCESS);
}

void mergeAndHashRecordStores(
    const string &mergedName,
    const string &mergedDescription,
    const string &hashName,
    const string &parentDir,
    const string &type,
    vector<string> &recordStores,
    const HashablePart::Type what_to_hash,
    const KeyFormat::Type hashed_key_format)
    throw (Error::ObjectExists, Error::StrategyError)
{
	auto_ptr<IO::RecordStore> merged_rs, hash_rs;
	string hash_description = "Hash translation of " + mergedName;
	if (type == IO::RecordStore::BERKELEYDBTYPE) {
		merged_rs.reset(new IO::DBRecordStore(mergedName,
		    mergedDescription, parentDir));
		hash_rs.reset(new IO::DBRecordStore(hashName,
		    hash_description, parentDir));
	} else if (type == IO::RecordStore::ARCHIVETYPE) {
		merged_rs.reset(new IO::ArchiveRecordStore(mergedName, 
		    mergedDescription, parentDir));
		hash_rs.reset(new IO::ArchiveRecordStore(hashName,
		    hash_description, parentDir));
	} else if (type == IO::RecordStore::FILETYPE) {
		merged_rs.reset(new IO::FileRecordStore(mergedName,
		    mergedDescription, parentDir));
		hash_rs.reset(new IO::FileRecordStore(hashName,
		    hash_description, parentDir));
	} else if (type == IO::RecordStore::SQLITETYPE) {
		merged_rs.reset(new IO::SQLiteRecordStore(mergedName,
		    mergedDescription, parentDir));
    		hash_rs.reset(new IO::SQLiteRecordStore(hashName,
		    hash_description, parentDir));
	} else if (type == IO::RecordStore::COMPRESSEDTYPE)
		throw Error::StrategyError("Invalid RecordStore type");
	else
		throw Error::StrategyError("Unknown RecordStore type");

	bool exhausted;
	uint64_t record_size;
	string key, hash;
	Memory::AutoArray<char> buf;
	tr1::shared_ptr<IO::RecordStore> rs;
	for (size_t i = 0; i < recordStores.size(); i++) {
		try {
			rs = IO::RecordStore::openRecordStore(
			    Text::filename(recordStores[i]),
			     Text::dirname(recordStores[i]), IO::READONLY);
		} catch (Error::Exception &e) {
			throw Error::StrategyError(e.getInfo());
		}
		
		exhausted = false;
		while (!exhausted) {
			try {
				record_size = rs->sequence(key);
				buf.resize(record_size);
				
				try {
					rs->read(key, buf);
				} catch (Error::ObjectDoesNotExist) {
					throw Error::StrategyError(
					    "Could not read " + key +
					    " from RecordStore");
				}
				
				switch (what_to_hash) {
				case HashablePart::FILECONTENTS:
					hash = Text::digest(buf,
					    record_size);
					break;
				case HashablePart::FILEPATH:
					/*
					 * We don't have a file's path
					 * here since we're going from
					 * RecordStore to RecordStore.
					 */
					/* FALLTHROUGH */
				case HashablePart::FILENAME:
					hash = Text::digest(key);
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
				
				merged_rs->insert(hash, buf, record_size);
				hash_rs->insert(hash, key.c_str(),
				    key.size() + 1);
			} catch (Error::ObjectDoesNotExist) {
				exhausted = true;
			}
		}
	}
}

int merge(int argc, char *argv[])
{
	HashablePart::Type what_to_hash = HashablePart::NOTHING;
	KeyFormat::Type hashed_key_format = KeyFormat::DEFAULT;
	string type = "", hash_filename = "";
	vector<string> child_rs;
	if (procargs_merge(argc, argv, type, child_rs, hash_filename,
	    what_to_hash, hashed_key_format) != EXIT_SUCCESS)
		return (EXIT_FAILURE);

	string description = "A merge of ";
	for (int i = 0; i < child_rs.size(); i++) {
		description += Text::filename(child_rs[i]);
		if (i != (child_rs.size() - 1)) description += ", ";
	}

	try {
		if (hash_filename.empty()) {
			IO::RecordStore::mergeRecordStores(
			    sflagval, description, oflagval, type, child_rs);
		} else {
			mergeAndHashRecordStores(
			    sflagval, description, hash_filename, oflagval,
			    type, child_rs, what_to_hash, hashed_key_format);
		}
	} catch (Error::Exception &e) {
		cerr << "Could not create " << sflagval << " - " <<
		    e.getInfo() << endl;
	}

	return (EXIT_SUCCESS);
}

int version(int argc, char *argv[])
{
	cout << argv[0] << " v" << MAJOR_VERSION << "." << MINOR_VERSION <<
	    " (Compiled " << __DATE__ << " " << __TIME__ << ")" << endl;
	cout << "BiometricEvaluation Framework v" << 
	    Framework::getMajorVersion() << "." <<
	    Framework::getMinorVersion() << " (" <<
	    Framework::getCompiler() << " v" <<
	    Framework::getCompilerVersion() << ")" << endl;
	
	return (EXIT_SUCCESS);
}

int procargs_unhash(int argc, char *argv[], string &hash,
    tr1::shared_ptr<IO::RecordStore> &rs)
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
		cerr << "Missing required option (-h)." << endl;
		return (EXIT_FAILURE);
	}

	/* sflagval here is the hashed RecordStore */
	try {
		rs = IO::RecordStore::openRecordStore(Text::filename(sflagval),
		    Text::dirname(sflagval), IO::READONLY);
	} catch (Error::Exception &e) {
		if (isRecordStoreAccessible(Text::filename(sflagval),
		    Text::dirname(sflagval), IO::READONLY))
			cerr << "Could not open " << sflagval << " - " << 
			    e.getInfo() << endl;
		else
			cerr << sflagval << ": Permission denied." << endl;
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}

int unhash(int argc, char *argv[])
{
	string hash = "";
	tr1::shared_ptr<IO::RecordStore> rs;
	if (procargs_unhash(argc, argv, hash, rs) != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	
	Memory::AutoArray<char> buffer;
	try {
		buffer.resize(rs->length(hash));
		rs->read(hash, buffer);
		cout << buffer << endl;
	} catch (Error::ObjectDoesNotExist) {
		cerr << hash << " was not found in " << rs->getName() << endl;
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}

int
procargs_add(
    int argc,
    char *argv[],
    tr1::shared_ptr<IO::RecordStore> &rs,
    tr1::shared_ptr<IO::RecordStore> &hash_rs,
    vector<string> &files,
    HashablePart::Type &what_to_hash,
    KeyFormat::Type &hashed_key_format,
    bool &stopOnDuplicate)
{
	what_to_hash = HashablePart::NOTHING;
	hashed_key_format = KeyFormat::DEFAULT;
	stopOnDuplicate = true;

	char c;
	while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
		case 'a':	/* File to add */
			if (!IO::Utility::fileExists(optarg))
				cerr << optarg << " does not exist and will "
				    "be skipped." << endl;
			else
				files.push_back(optarg);
			break;
		case 'c':	/* Hash contents */
			if (what_to_hash == HashablePart::NOTHING)
				what_to_hash = HashablePart::FILECONTENTS;
			else {
				cerr << "More than one hash method selected." <<
				    endl;
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
				cerr << "Invalid format specifier for hashed "
				    "key." << endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'p':	/* Hash file path */
			if (what_to_hash == HashablePart::NOTHING)
				what_to_hash = HashablePart::FILEPATH;
			else {
				cerr << "More than one hash method selected." <<
				    endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'h':	/* Existing hash translation RecordStore */
			try {
				hash_rs = IO::RecordStore::openRecordStore(
				    Text::filename(optarg),
				    Text::dirname(optarg));
			} catch (Error::Exception &e) {
				if (isRecordStoreAccessible(
				    Text::filename(optarg),
				    Text::dirname(optarg)))
					cerr << "Could not open " << optarg << 
					    " -- " << e.getInfo() << endl;
				else
				    	cerr << optarg << ": Permission "
					    "denied." << endl;
				return (EXIT_FAILURE);
			}
			break;
		}
	}
	/* Remaining arguments are files to add (same as -a) */
	for (int i = optind; i < argc; i++) {
		if (IO::Utility::fileExists(argv[i]) == false)
			cerr << optarg << " does not exist and will be " <<
			    "skipped." << endl;
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
		rs = IO::RecordStore::openRecordStore(Text::filename(sflagval),
		    Text::dirname(sflagval));
	} catch (Error::Exception &e) {
		if (isRecordStoreAccessible(Text::filename(sflagval),
		    Text::dirname(sflagval)))
			cerr << "Could not open " << sflagval << " -- " <<
			    e.getInfo() << endl;
		else
		    	cerr << sflagval << ": Permission denied." << endl;
		return (EXIT_FAILURE);
	}

	/* Sanity check -- don't hash without recording a translation */
	if ((hash_rs.get() == NULL) && (what_to_hash !=
	    HashablePart::NOTHING)) {
		cerr << "Specified hash method without -h." << endl;
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
    tr1::shared_ptr<IO::RecordStore> &hash_rs,
    vector<string> &files,
    HashablePart::Type &what_to_hash,
    bool &prompt)
{
	what_to_hash = HashablePart::NOTHING;

	char c;
	optind = 2;
	while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
		case 'a':	/* File to add */
			if (!IO::Utility::fileExists(optarg))
				cerr << optarg << " does not exist and will "
				    "be skipped." << endl;
			else
				files.push_back(optarg);
			break;
		case 'c':	/* Hash contents */
			if (what_to_hash == HashablePart::NOTHING)
				what_to_hash = HashablePart::FILECONTENTS;
			else {
				cerr << "More than one hash method selected." <<
				    endl;
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
				cerr << "More than one hash method selected." <<
				    endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'h':	/* Existing hash translation RecordStore */
			try {
				hash_rs = IO::RecordStore::openRecordStore(
				    Text::filename(optarg),
				    Text::dirname(optarg));
			} catch (Error::Exception &e) {
				if (isRecordStoreAccessible(
				    Text::filename(optarg),
				    Text::dirname(optarg)))
					cerr << "Could not open " << optarg << 
					    " -- " << e.getInfo() << endl;
				else
				    	cerr << optarg << ": Permission "
					    "denied." << endl;
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
		cerr << "Specified hash method without -h." << endl;
		return (EXIT_FAILURE);
	}

	/* Choose to hash filename by default */
	if ((hash_rs.get() != NULL) && (what_to_hash == HashablePart::NOTHING))
		what_to_hash = HashablePart::FILENAME;

	/* Sanity check -- inserting into ListRecordStore */
	if (specialProcessingFlags & SpecialProcessing::LISTRECORDSTORE)
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
    Action::Type action)
{
	HashablePart::Type what_to_hash = HashablePart::NOTHING;
	KeyFormat::Type hashed_key_format = KeyFormat::DEFAULT;
	tr1::shared_ptr<IO::RecordStore> hash_rs;
	vector<string> files;
	bool prompt = true;
	
	if (procargs_modifyListRecordStore(argc, argv, hash_rs, files,
	    what_to_hash, prompt) != EXIT_SUCCESS)
		return (EXIT_FAILURE);
	
	tr1::shared_ptr< OrderedSet<string> > keys(
	    new OrderedSet<string>);
	Memory::uint8Array buffer;
	string hash, invalidHashKeys = "";
	for (vector<string>::const_iterator file_path = files.begin();
	    file_path != files.end(); file_path++) {
		switch (what_to_hash) {
		case HashablePart::FILECONTENTS:
			try {
				buffer = IO::Utility::readFile(*file_path);
			} catch (Error::Exception &e) {
				cerr << e.getInfo() << endl;
				return (EXIT_FAILURE);
			}
			hash = Text::digest(buffer, buffer.size());
			break;
		case HashablePart::FILENAME:
			hash = Text::digest(Text::filename(*file_path));
			break;
		case HashablePart::FILEPATH:
			hash = Text::digest(*file_path);
			break;
		case HashablePart::NOTHING:
			hash = Text::filename(*file_path);
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
			throw Error::StrategyError("Internal inconsistency -- "
			    "can't perform this action on ListRecordStore");
		}
	} catch (Error::Exception &e) {
		cerr << e.getInfo() << endl;
		return (EXIT_FAILURE);
	}
	
	if (invalidHashKeys.empty() == false) {
		cerr << "The following keys were not added because their hash "
		    "translation does not\nexist in the hash translation "
		    "RecordStore: " << '\n' << invalidHashKeys << endl;
		    return (EXIT_FAILURE);
	}
	
	return (EXIT_SUCCESS);
}

int
add(
    int argc,
    char *argv[])
{
	HashablePart::Type what_to_hash = HashablePart::NOTHING;
	KeyFormat::Type hashed_key_format = KeyFormat::DEFAULT;
	tr1::shared_ptr<IO::RecordStore> rs, hash_rs;
	vector<string> files;
	bool stopOnDuplicate = true;

	if (procargs_add(argc, argv, rs, hash_rs, files, what_to_hash,
	    hashed_key_format, stopOnDuplicate) != EXIT_SUCCESS)
		return (EXIT_FAILURE);

	for (vector<string>::const_iterator file_path = files.begin();
	    file_path != files.end(); file_path++) {
		/*
		 * Conscious decision to not care about the return value
		 * when inserting because we may have multiple files we'd
		 * like to add and there's no point in quitting halfway.
		 */
		if (IO::Utility::pathIsDirectory(*file_path))
			make_insert_directory_contents(
			    Text::filename(*file_path),
			    Text::dirname(*file_path), rs, hash_rs,
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
    vector<string> &keys,
    bool &force_removal,
    tr1::shared_ptr<IO::RecordStore> &rs)
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
		cerr << "Missing required option (-k)." << endl;
		return (EXIT_FAILURE);
	}

	/* sflagval here is the RecordStore */
	try {
		rs = IO::RecordStore::openRecordStore(Text::filename(sflagval),
		    Text::dirname(sflagval), IO::READWRITE);
	} catch (Error::Exception &e) {
		if (isRecordStoreAccessible(Text::filename(sflagval),
		    Text::dirname(sflagval), IO::READWRITE))
			cerr << "Could not open " << sflagval << " - " <<
			    e.getInfo() << endl;
		else
			cerr << sflagval << ": Permission denied." << endl;
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
	tr1::shared_ptr<IO::RecordStore> rs;
	vector<string> keys;
	tr1::shared_ptr< OrderedSet<string> > approvedKeys(
	    new OrderedSet<string>());
	
	if (procargs_remove(argc, argv, keys, force_removal, rs) != 
	    EXIT_SUCCESS)
		return (EXIT_FAILURE);
		
	for (vector<string>::const_iterator key = keys.begin();
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
	tr1::shared_ptr<IO::RecordStore> rs;
	vector<string> keys;
		
	if (procargs_remove(argc, argv, keys, force_removal, rs) !=
	    EXIT_SUCCESS)
		return (EXIT_FAILURE);
		
	for (vector<string>::const_iterator key = keys.begin();
	    key != keys.end(); key++) {
		if ((force_removal == false && yesOrNo("Remove " + *key + "?",
		    false)) || force_removal == true) {
			try {
				rs->remove(*key);
			} catch (Error::Exception &e) {
				cerr << "Could not remove " << *key << ": " <<
				    e.getInfo() << endl;
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
    tr1::shared_ptr<IO::RecordStore> &sourceRS,
    tr1::shared_ptr<IO::RecordStore> &targetRS,
    vector<string> &keys,
    bool &byte_for_byte)
{
	int rsCount = 0;
	char c;
        while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
		case 'a': {	/* Text file of keys to diff */
			vector<string> fileKeys;
			try {
				string path;
				if (IO::Utility::constructAndCheckPath(
				    Text::filename(optarg),
				    Text::dirname(optarg), path) == false)
					throw Error::ObjectDoesNotExist(optarg);
				fileKeys = readTextFileToVector(path);
			} catch (Error::Exception &e) {
				cerr << e.getInfo() << endl;
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
				    IO::RecordStore::openRecordStore(
				    Text::filename(optarg),
				    Text::dirname(optarg), IO::READONLY);
				rsCount++;
			} catch (Error::Exception &e) {
				if (isRecordStoreAccessible(
				    Text::filename(optarg),
				    Text::dirname(optarg),
				    IO::READONLY))
					cerr << "Could not open " <<
					    Text::filename(optarg) << " - " << 
					    e.getInfo() << endl;
				else
				    	cerr << optarg << ": Permission "
					    "denied." << endl;
				return (EXIT_FAILURE);
			}
		}
	}

	if (rsCount != 2) {
		cerr << "Must specify only two RecordStores " <<
		    "(-s <rs> -s <rs>)." << endl;
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
	tr1::shared_ptr<IO::RecordStore> sourceRS, targetRS;
	vector<string> keys;
	if (procargs_diff(argc, argv, sourceRS, targetRS, keys,
	    byte_for_byte) != EXIT_SUCCESS)
		return (EXIT_FAILURE);

	string sourceName = sourceRS->getName();
	string targetName = targetRS->getName();

	/* Don't attempt diff if either RecordStore is empty */
	if (sourceRS->getCount() == 0) {
		cerr << "No entries in " << sourceName << '.' << endl;
		return (EXIT_FAILURE);
	} else if (targetRS->getCount() == 0) {
		cerr << "No entries in " << targetName << '.' << endl;
		return (EXIT_FAILURE);
	}

	/* If no keys passed, compare every key */
	if (keys.empty()) {	
		string key;
		for (;;) {
			try {
				sourceRS->sequence(key, NULL);
				keys.push_back(key);
			} catch (Error::ObjectDoesNotExist) {
				/* End of sequence */
				break;
			}
		}
	}
	
	bool sourceExists, targetExists;
	Memory::AutoArray<uint8_t> sourceBuf, targetBuf;
	uint64_t sourceLength, targetLength;
	vector<string>::const_iterator key;
	for (key = keys.begin(); key != keys.end(); key++) {		
		/* Get sizes to check existance */
		try {
			sourceLength = sourceRS->length(*key);
			sourceExists = true;
		} catch (Error::ObjectDoesNotExist) {
			sourceExists = false;
		}
		try {
			targetLength = targetRS->length(*key);
			targetExists = true;
		} catch (Error::ObjectDoesNotExist) {
			targetExists = false;
		}
		
		/* Difference based on existance */
		if ((sourceExists == false) && (targetExists == false)) {
			cout << *key << ": not found." << endl;
			status = EXIT_FAILURE;
			continue;
		} else if ((sourceExists == true) && (targetExists == false)) {
			cout << *key << ": only in " << sourceName << endl;
			status = EXIT_FAILURE;
			continue;
		} else if ((sourceExists == false) && (targetExists == true)) {
			cout << *key << ": only in " << targetName << endl;
			status = EXIT_FAILURE;
			continue;
		}
		
		/* Difference based on size */
		if (sourceLength != targetLength) {
			cout << *key << ':' << sourceName << " and " << *key <<
			    ':' << targetName << " differ (size)" << endl;
			status = EXIT_FAILURE;
			continue;
		}
		
		/* Difference based on content */
		sourceBuf.resize(sourceLength);
		targetBuf.resize(targetLength);
		try {
			if (sourceRS->read(*key, sourceBuf) != sourceLength)
				throw Error::StrategyError("Source size");
			if (targetRS->read(*key, targetBuf) != targetLength)
				throw Error::StrategyError("Target size");
		} catch (Error::Exception &e) {
			cerr << "Could not diff " << *key << " (" << 
			    e.getInfo() << ')' << endl;
			status = EXIT_FAILURE;
			continue;
		}
		
		if (byte_for_byte) {
			for (uint64_t i = 0; i < sourceLength; i++) {
				if (sourceBuf[i] != targetBuf[i]) {
					cout << *key << ':' << sourceName <<
					    " and " << *key << ':' <<
					    targetName << " differ " <<
					    "(byte for byte)" << endl;
					status = EXIT_FAILURE;
					break;
				}
			}
		} else { /* MD5 */
			try {
				if (Text::digest(sourceBuf, sourceLength) !=
				    Text::digest(targetBuf, targetLength)) {
					cout << *key << ':' << sourceName <<
					    " and " << *key << ':' <<
					    targetName << " differ " <<
					    "(MD5)" << endl;
					status = EXIT_FAILURE;
				}
			} catch (Error::Exception &e) {
				cerr << "Could not diff " << *key << " (" << 
				    e.getInfo() << ')' << endl;
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
    std::string &newName)
{
	newName = "";
	int rsCount = 0;
	char c;
        while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
		case 's':	/* Source/target RecordStores */
			switch (rsCount) {
			case 0:
				/* Ensure first RecordStore exists */
				try {
					IO::RecordStore::openRecordStore(
					    Text::filename(optarg),
					    Text::dirname(optarg),
					    IO::READWRITE);
				} catch (Error::Exception &e) {
					if (isRecordStoreAccessible(
					    Text::filename(optarg),
					    Text::dirname(optarg),
					    IO::READWRITE))
						std::cerr << "Could not "
						    "open " << Text::
						    filename(optarg) <<
						    " - " << e.getInfo() <<
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
				newName = optarg;
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
	if (IO::Utility::constructAndCheckPath(newName, oflagval,
	    newName)) {
		std::cerr << newName << " already exists." << std::endl;
		return (EXIT_FAILURE);
	}
	newName = Text::filename(newName);

	return (EXIT_SUCCESS);
}

int
rename(
    int argc,
    char *argv[])
{
	std::string newName;
	if (procargs_rename(argc, argv, newName) != EXIT_SUCCESS)
		return (EXIT_FAILURE);

	/* Ensure we can temporarily change the name in the current directory */
	std::string newPath;
	if (IO::Utility::constructAndCheckPath(newName,
	    Text::dirname(sflagval), newPath)) {
		std::cerr << newPath << " exists (needed temporarily)." <<
		    std::endl;
		return (EXIT_FAILURE);
	}

	/* Ensure destination path will allow for name change */
	if (IO::Utility::constructAndCheckPath(newName, oflagval,
	    newPath)) {
		std::cerr << newPath << " exists." << std::endl;
		return (EXIT_FAILURE);
	}

	/* Change name in same directory */
	try {
		tr1::shared_ptr<IO::RecordStore> rs = IO::RecordStore::
		    openRecordStore(Text::filename(sflagval),
		    Text::dirname(sflagval), IO::READWRITE);
		rs->changeName(newName);
	} catch (Error::Exception &e) {
		std::cerr << e.getInfo() << std::endl;
		return (EXIT_FAILURE);
	}

	/* Move the RecordStore */
	std::string existingPath;
	if (!IO::Utility::constructAndCheckPath(newName,
	    Text::dirname(sflagval), existingPath)) {
		std::cerr << existingPath << " does not exist." << std::endl;
		return (EXIT_FAILURE);
	}
	if (rename(existingPath.c_str(), newPath.c_str()) != 0) {
		std::cerr << "Could not move \"" << existingPath << "\" "
		    "to \"" << newPath << "\" (" << Error::errorStr() <<
		    ")" << std::endl;
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	Action::Type action = procargs(argc, argv);
	switch (action) {
	case Action::ADD:
		if (specialProcessingFlags & SpecialProcessing::LISTRECORDSTORE)
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
		if (specialProcessingFlags & SpecialProcessing::LISTRECORDSTORE)
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

