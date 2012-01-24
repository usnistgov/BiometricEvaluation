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
 * rstool - A tool for manipulating RecordStores.
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
#include <be_io_filerecstore.h>
#include <be_io_recordstore.h>
#include <be_io_utility.h>
#include <be_text.h>
#include <be_memory_autoarray.h>

static string oflagval = ".";		/* Output directory */
static string sflagval = "";		/* Path to main RecordStore */
static const char optstr[] = "a:cfh:k:m:o:pr:s:t:";

/* Possible actions performed by this utility */
static const string ADD_ARG = "add";
static const string DISPLAY_ARG = "display";
static const string DIFF_ARG = "diff";
static const string DUMP_ARG = "dump";
static const string LIST_ARG = "list";
static const string MAKE_ARG = "make";
static const string MERGE_ARG = "merge";
static const string REMOVE_ARG = "remove";
static const string VERSION_ARG = "version";
static const string UNHASH_ARG = "unhash";
namespace Action {
	typedef enum {
	    ADD,
	    DISPLAY,
	    DIFF,
	    DUMP,
	    LIST,
	    MAKE,
	    MERGE,
	    REMOVE,
	    VERSION,
	    UNHASH, 
	    QUIT
	} Type;
}

/* Things that could be hashed when hashing a key */
namespace HashablePart {
	typedef enum {
		FILECONTENTS,
		FILENAME,
		FILEPATH,
		NOTHING
	} Type;
}

/* What to print as value in a hash translation RecordStore */
namespace KeyFormat {
	typedef enum {
		DEFAULT,
		FILENAME,
		FILEPATH
	} Type;
};

using namespace BiometricEvaluation;
using namespace std;

/**
 * @brief
 * Read the contents of a text file into a vector (one line per entry).
 *
 * @param filePath
 *	Complete path of the text file to read from.
 * @param ignoreComments
 *	Whether or not to ignore comment lines (lines beginning with '#')
 * @param ignoreBlankLines
 *	Whether or not to add blank entries (lines beginning with '\n')
 *
 * @return
 *	Vector of the lines in the file
 *
 * @throw Error::FileError
 *	Error opening or reading from filePath.
 * @throw Error::ObjectDoesNotExist
 *	filePath does not exist.
 */
static vector<string>
readTextFileToVector(
    const string &filePath,
    bool ignoreComments = true,
    bool ignoreBlankLines = true)
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

/**
 * @brief
 * Prompt the user to answer a yes or no question.
 *
 * @param prompt
 *	The yes or no question.
 * @param default_answer
 *	Value that is the default answer if return is pressed but nothing
 *	typed.
 * @param show_options
 *	Whether or not to show valid input values.
 * @param allow_default_answer
 *	Whether or not to allow default answer.
 *
 * @return
 *	Answer to prompt (true if yes, false if no).
 */
static bool
yesOrNo(
    const string &prompt,
    bool default_answer = true,
    bool show_options = true,
    bool allow_default_answer = true)
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

/**
 * @brief
 * Display command-line usage for the tool.
 * 
 * @param exe[in]
 *	Name of executable.
 */
static void usage(char *exe)
{
	cerr << "Usage: " << exe << " <action> -s <RS> [options]" << endl;
	cerr << "Actions: add, display, dump, list, make, merge, remove, "
	    "version, unhash" << endl;

	cerr << endl;

	cerr << "Common:" << endl;
	cerr << "\t-c\t\tIf hashing, hash file/record contents" << endl;
	cerr << "\t-p\t\tIf hashing, hash file path" << endl;
	cerr << "\t-o <...>\tOutput directory" << endl;
	cerr << "\t-s <path>\tRecordStore" << endl;

	cerr << endl;

	cerr << "Add Options:" << endl;
	cerr << "\t-a <file>\tFile to add" << endl;
	cerr << "\t-h <hash_rs>\tExisting hash translation RecordStore" << endl;
	cerr << "\t-k(fp)\t\tPrint 'f'ilename or file'p'ath of key as value " <<
	    endl << "\t\t\tin hash translation RecordStore" << endl;

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

	cerr << endl;

	cerr << "Make Options:" << endl;
	cerr << "\t-a <text>\tText file with paths to files or directories "
	    "to\n\t\t\tinitially add as reords (multiple)" << endl;
	cerr << "\t-h <hash_rs>\tHash keys and save translation RecordStore" <<
	    endl;
	cerr << "\t-k(fp)\t\tPrint 'f'ilename or file'p'ath of key as value " <<
	    endl << "\t\t\tin hash translation RecordStore" << endl;
	cerr << "\t-t <type>\tType of RecordStore to make" << endl;
	cerr << "\t\t\tWhere <type> is Archive, BerkeleyDB, File" << endl;
	cerr << "\t[<file>] [...]\tFiles/dirs to add as a record" << endl;

	cerr << endl;

	cerr << "Merge Options:" << endl;
	cerr << "\t-a <RS>\t\tRecordStore to be merged (multiple)" << endl;
	cerr << "\t-h <RS>\t\tHash keys and store a hash translation "
	    "RecordStore" << endl;
	cerr << "\t-t <type>\tType of RecordStore to make" << endl;
	cerr << "\t\t\tWhere <type> is Archive, BerkeleyDB, File" << endl;

	cerr << endl;
	
	cerr << "Remove Options:" << endl;
	cerr << "\t-f\t\tForce removal, do not prompt" << endl;
	cerr << "\t-k <key>\tThe key to remove" << endl;
	
	cout << endl;

	cerr << "Unhash Options:" << endl;
	cerr << "\t-h <hash>\tHash to unhash" << endl;

	cerr << endl;
}

/**
 * @brief 
 * Check access to core RecordStore files.
 *
 * @param name
 *	RecordStore name
 * @param parentDir
 *	Directory holding RecordStore.
 * @param mode
 *	How attempting to open the RecordStore (IO::READONLY, IO::READWRITE)
 *
 * @return
 *	true if access to file exists, false otherwise.
 *
 * @note
 *	This function could return true if core RecordStore files do not
 *	exist.  See related functions for more information.
 *
 * @see BiometricEvaluation::IO::Utility::isReadable()
 * @see BiometricEvaluation::IO::Utility::isWritable()
 */
static bool
isRecordStoreAccessible(
    const string &name,
    const string &parentDir,
    const uint8_t mode = IO::READWRITE)
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

/**
 * @brief
 * Validate a RecordStore type string.
 *
 * @param type[in]
 *	String (likely entered by user) to check for validity.
 *
 * @returns
 *	RecordStore type string, if one can be identified, or "" on error.
 */
static string validate_rs_type(const string &type)
{
	if (strcasecmp(type.c_str(), IO::RecordStore::FILETYPE.c_str()) == 0)
		return (IO::RecordStore::FILETYPE);
	else if (strcasecmp(type.c_str(),
	    IO::RecordStore::BERKELEYDBTYPE.c_str()) == 0)
		return (IO::RecordStore::BERKELEYDBTYPE);
	else if (strcasecmp(type.c_str(),
	    IO::RecordStore::ARCHIVETYPE.c_str()) == 0)
		return (IO::RecordStore::ARCHIVETYPE);

	return ("");
}

/**
 * @brief
 * Process command-line arguments for the tool.
 *
 * @param argc[in]
 *	argc from main()
 * @param argv[in]
 *	argv from main()
 *
 * @returns
 *	The Action the user has indicated on the command-line.
 */
static Action::Type procargs(int argc, char *argv[])
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

	return (action);
}

/**
 * @brief
 * Process command-line arguments specific to the DISPLAY and DUMP Actions.
 *
 * @param argc[in]
 *	argc from main()
 * @param argv[in]
 *	argv from main()
 * @param key[in]
 *	Reference to a string that will be populated with any value
 *	following the "key" flag on the command-line
 * @param range[in]
 *	Reference to a string that will be populated with any value
 *	following the "range" flag on the command-line
 * @param rs[in]
 *	Reference to a RecordStore shared pointer that will be allocated
 *	to hold the main RecordStore passed with the -s flag on the command-line
 * @param hash_rs[in]
 *	Reference to a RecordStore shared pointer that will be allocated
 *	to hold a hash translation RecordStore.  Instantiating this RecordStore
 *	lets the driver know that the user would like the unhashed key to be
 *	used when the extraction takes place.
 *
 * @returns
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE.
 */
static int
procargs_extract(
    int argc,
    char *argv[],
    string &key,
    string &range,
    tr1::shared_ptr<IO::RecordStore> &rs,
    tr1::shared_ptr<IO::RecordStore> &hash_rs)
{
	char c;
        while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
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

/**
 * @brief
 * Print a record to the screen.
 *
 * @param key[in]
 *	The name of the record to display.
 * @param value[in]
 *	The contents of the record to display.
 *
 * @returns
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
static int
display(
    const string &key,
    Memory::AutoArray<uint8_t> &value)
{
	value.resize(value.size() + 1);
	value[value.size() - 1] = '\0';

	cout << key << " = " << value << endl;

	return (EXIT_SUCCESS);
}

/**
 * @brief
 * Write a record to disk.
 *
 * @param key[in]
 *	The name of the file to write.
 * @param value[in]
 *	The contents of the file to write.
 *
 * @returns
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
static int
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

/**
 * @brief
 * Facilitates the extraction of a single key or a range of records from a 
 * RecordStore
 *
 * @param argc[in]
 *	argc from main()
 * @param argv[in]
 *	argv from main()
 * @param action[in]
 *	The Action to be performed with the extracted data.
 *
 * @returns
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
static int extract(int argc, char *argv[], Action::Type action)
{
	string key = "", range = "";
	tr1::shared_ptr<IO::RecordStore> rs, hash_rs;
	if (procargs_extract(argc, argv, key, range, rs, hash_rs) !=
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
			if (display(key, buf) != EXIT_SUCCESS)
				return (EXIT_FAILURE);
			break;
		}
	} else {
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
			}
		}
	}

	return (EXIT_SUCCESS);
}

/**
 * @brief
 * Facilitates the listing of the keys stored within a RecordStore.
 *
 * @param argc[in]
 *	argc from main()
 * @param argv[in]
 *	argv from main()
 *
 * @returns
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
static int list(int argc, char *argv[])
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

/**
 * @brief
 * Process command-line arguments specific to the MAKE Action.
 *
 * @param argc[in]
 *	argc from main()
 * @param argv[in]
 *	argv from main()
 * @param hash_filename[in]
 *	Reference to a string to store a path to a hash translation RecordStore,
 *	indicating that the keys should be hashed
 * @param what_to_hash[in]
 *	Reference to a HashablePart enumeration that indicates what should be
 *	hashed when creating a hash for an entry.
 * @param hashed_key_format[in]
 *	Reference to a KeyFormat enumeration that indicates how the key (when
 *	printed as a value) should appear in a hash translation RecordStore.
 * @param type[in]
 *	Reference to a string that will represent the type of RecordStore to
 *	create.
 * @param elements[in]
 *	Reference to a vector that will hold paths to elements that should
 *	be added to the target RecordStore.  The paths may be to text
 *	files, whose contents are lines of paths that should be added, or
 *	directories, whose contents should be added.
 *
 * @returns
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
static int
procargs_make(
    int argc,
    char *argv[],
    string &hash_filename,
    HashablePart::Type &what_to_hash,
    KeyFormat::Type &hashed_key_format,
    string &type,
    vector<string> &elements)
{
	what_to_hash = HashablePart::NOTHING;
	hashed_key_format = KeyFormat::DEFAULT;

	char c;
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
				break;
			}
			
			/* Parse the paths in the text file */
			ifstream input;
			string line;
			input.open(optarg, ifstream::in);
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
		case 't':	/* Destination RecordStore type */
			type = validate_rs_type(optarg);
			if (type.empty()) {
				cerr << "Invalid type format (-t)." << endl;
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

	/* Choose to hash filename by default */
	if ((hash_filename.empty() == false) && (what_to_hash ==
	    HashablePart::NOTHING))
		what_to_hash = HashablePart::FILENAME;

	return (EXIT_SUCCESS);
}

/**
 * @brief
 * Helper function to insert the contents of a file into a RecordStore.
 *
 * @param filename[in]
 *	The name of the file whose contents should be inserted into rs.
 * @param rs[in]
 *	The RecordStore into which the contents of filename should be inserted
 * @param hash_rs[in]
 *	The RecordStore into which hash translations should be stored
 * @param what_to_hash[in]
 *	What should be hashed when creating a hash for an entry.
 * @param hashed_key_format[in]
 *	How the key should be displayed in a hash translation RecordStore.
 *
 * @returns
 *	An exit status, either EXIT_FAILURE or EXIT_SUCCESS, depending on if
 *	the contents of filename could be inserted.
 */
static int make_insert_contents(const string &filename,
    const tr1::shared_ptr<IO::RecordStore> &rs,
    const tr1::shared_ptr<IO::RecordStore> &hash_rs,
    const HashablePart::Type what_to_hash,
    const KeyFormat::Type hashed_key_format)
{
	static Memory::AutoArray<char> buffer;
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
	buffer_file.read(buffer, buffer_size);
	if (buffer_file.bad()) {
		cerr << "Error reading file (" << filename << ')' << endl;
		return (EXIT_FAILURE);
	}
	buffer_file.close();
	
	static string key = "", hash_value = "";
	try {
		key = Text::filename(filename);
		if (hash_rs.get() == NULL)
			rs->insert(key, buffer, buffer_size);
		else {
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
			}

			rs->insert(hash_value, buffer, buffer_size);
			hash_rs->insert(hash_value, key.c_str(), key.size());
		}
	} catch (Error::Exception &e) {
		cerr << "Could not add contents of " << filename <<
		    " to RecordStore - " << e.getInfo() << endl;
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}

/**
 * @brief
 * Recursive helper function to insert the contents of a directory into
 * a RecordStore.
 * 
 * @param directory[in]
 *	Path of the directory to search through
 * @param prefix[in]
 *	The parent directories of directory
 * @param rs[in]
 *	The RecordStore into which files should be inserted
 * @param hash_rs[in]
 *	The RecordStore into which hash translations should be stored
 * @param what_to_hash[in]
 *	What should be hashed when creating a hash for an entry.
 * @param hashed_key_value[in]
 *	How the key should be displayed in the hash translation RecordStore.
 *
 * @throws Error::ObjectDoesNotExist
 *	If the contents of the directory changes during the run
 * @throws Error::StrategyError
 *	Underlying problem in storage system
 *
 * @returns
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
static int
make_insert_directory_contents(
    const string &directory,
    const string &prefix,
    const tr1::shared_ptr<IO::RecordStore> &rs,
    const tr1::shared_ptr<IO::RecordStore> &hash_rs,
    const HashablePart::Type what_to_hash,
    const KeyFormat::Type hashed_key_format)
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
			    hashed_key_format) != EXIT_SUCCESS)
				return (EXIT_FAILURE);
		} else {
			if (make_insert_contents(filename, rs, hash_rs,
			    what_to_hash, hashed_key_format) != EXIT_SUCCESS)
				return (EXIT_FAILURE);
		}
	}

	if (dir != NULL)
		if (closedir(dir))
			throw Error::StrategyError("Could not close " + 
			    dirpath + " (" + Error::errorStr() + ")");

	return (EXIT_SUCCESS);
}

/**
 * @brief
 * Facilitates creation of a RecordStore.
 *
 * @param argc[in]
 *	argc from main()
 * @param argv[in]
 *	argv from main()
 *
 * @returns
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
static int make(int argc, char *argv[])
{
	string hash_filename = "", type = "";
	vector<string> elements;
	HashablePart::Type what_to_hash = HashablePart::NOTHING;
	KeyFormat::Type hashed_key_format = KeyFormat::DEFAULT;

	if (procargs_make(argc, argv, hash_filename, what_to_hash,
	    hashed_key_format, type, elements) != EXIT_SUCCESS)
		return (EXIT_FAILURE);

	tr1::shared_ptr<IO::RecordStore> rs;
	tr1::shared_ptr<IO::RecordStore> hash_rs;
	try {
		rs = IO::RecordStore::createRecordStore(sflagval,
		    "<Description>", type, oflagval);
		if (!hash_filename.empty())
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
				    hashed_key_format) != EXIT_SUCCESS)
			    		return (EXIT_FAILURE);
			} catch (Error::Exception &e) {
				cerr << "Could not add contents of dir " <<
				    elements[i] << " - " << e.getInfo() << endl;
				return (EXIT_FAILURE);
			}
		} else {
			if (make_insert_contents(elements[i], rs, hash_rs,
			    what_to_hash, hashed_key_format) !=
			    EXIT_SUCCESS)
				return (EXIT_FAILURE);
		}
	}

	return (EXIT_SUCCESS);
}

/**
 * @brief
 * Process command-line arguments specific to the MERGE Action.
 *
 * @param argc[in]
 *	argc from main()
 * @param argv[in]
 *	argv from main()
 * @param type[in]
 *	Reference to a string that will represent the type of RecordStore
 *	to create.
 * @param child_rs[in]
 * 	Reference to an AutoArray of RecordStore pointers that will hold 
 *	open RecordStore shared pointers to the RecordStores that should 
 *	be merged
 * @param num_child_rs[in]
 *	Reference to an integer that will specify the contents of the child_rs
 *	array.
 * @param hash_filename[in]
 *	Reference to a string that will specify the name of the hash translation
 *	RecordStore, if one is desired.
 * @param what_to_hash[in]
 *	Reference to a HashablePart enumeration indicating what should be
 *	hashed when creating a hash for an entry.
 * @param hashed_key_format[in]
 *	Reference to a KeyFormat enumerating indicating how the key should be
 *	printed (as a value) in a hash translation RecordStore.
 *
 * @returns
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE.
 */
static int
procargs_merge(
    int argc,
    char *argv[],
    string &type,
    Memory::AutoArray< tr1::shared_ptr< IO::RecordStore > > &child_rs,
    int &num_child_rs,
    string &hash_filename,
    HashablePart::Type &what_to_hash,
    KeyFormat::Type &hashed_key_format)
{
	what_to_hash = HashablePart::NOTHING;
	hashed_key_format = KeyFormat::DEFAULT;

	char c;
	num_child_rs = 0;
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
			try {
				/* Add to AutoArray */
				child_rs.resize(num_child_rs + 1);
				child_rs[num_child_rs++] =
				    IO::RecordStore::openRecordStore(
				    Text::filename(optarg),
				    Text::dirname(optarg),
				    IO::READONLY);
			} catch (Error::Exception &e) {
    				if (isRecordStoreAccessible(
				    Text::filename(optarg),
				    Text::dirname(optarg),
				    IO::READONLY))
					cerr << "Could not open " << optarg <<
					    " - " << e.getInfo() << endl;
				else
				    	cerr << optarg << ": Permission "
					    "denied." << endl;
				return (EXIT_FAILURE);
			}
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

	if (hashed_key_format == KeyFormat::DEFAULT)
		hashed_key_format = KeyFormat::FILENAME;

	if (num_child_rs == 0) {
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

/** 
 * Create a new RecordStore that contains the contents of several RecordStores,
 * hashing the keys of the child RecordStores before adding them to the merged.
 *  
 * @param mergedName[in]
 * 	The name of the new RecordStore that will be created.
 * @param mergedDescription[in]
 * 	The text used to describe the RecordStore.
 * @hashName[in]
 *	The name of the new hash translation RecordStore that will be created.
 * @param parentDir[in]
 * 	Where, in the file system, the new store should be rooted.
 * @param type[in]
 * 	The type of RecordStore that mergedName should be.
 * @param recordStores[in]
 * 	An array of shared pointers to RecordStore that should be merged into
 *	mergedName.
 * @param numRecordStores[in]
 * 	The number of RecordStore* in recordStores.
 * @param what_to_hash[in]
 *	What should be hashed when creating a hash for an entry.
 * @param hashed_key_format[in]
 *	How the key should be printed in a hash translation RecordStore
 *
 * \throws Error::ObjectExists
 * 	A RecordStore with mergedNamed in parentDir
 * \throws Error::StrategyError
 * 	An error occurred when using the underlying storage system
 */
static void mergeAndHashRecordStores(
    const string &mergedName,
    const string &mergedDescription,
    const string &hashName,
    const string &parentDir,
    const string &type,
    tr1::shared_ptr<IO::RecordStore> recordStores[],
    const size_t numRecordStores,
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
	} else
		throw Error::StrategyError("Unknown RecordStore type");

	bool exhausted;
	uint64_t record_size;
	string key, hash;
	Memory::AutoArray<char> buf;
	for (size_t i = 0; i < numRecordStores; i++) {
		exhausted = false;
		while (!exhausted) {
			try {
				record_size = recordStores[i]->sequence(key);
				buf.resize(record_size);
			} catch (Error::ObjectDoesNotExist) {
				exhausted = true;
				continue;
			}

			try {
				recordStores[i]->read(key, buf);
			} catch (Error::ObjectDoesNotExist) {
				throw Error::StrategyError("Could not read " +
				    key + " from RecordStore");
			}

			switch (what_to_hash) {
			case HashablePart::FILECONTENTS:
				hash = Text::digest(buf, record_size);
				break;
			case HashablePart::FILEPATH:
				/* 
				 * We don't have a file's path here since
				 * we're going from RecordStore to RecordStore.
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
				 * We don't have a file's path here since
				 * we're going from RecordStore to RecordStore,
				 * so not much we can do.
				 */
				break;
			}

			merged_rs->insert(hash, buf, record_size);
			hash_rs->insert(hash, key.c_str(), key.size() + 1);
		}
	}
}

/**
 * @brief
 * Facilitates the merging of multiple RecordStores.
 *
 * @param argc[in]
 *	argc from main()
 * @param argv[in]
 *	argv from main()
 *
 * @returns
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
static int merge(int argc, char *argv[])
{
	HashablePart::Type what_to_hash = HashablePart::NOTHING;
	KeyFormat::Type hashed_key_format = KeyFormat::DEFAULT;
	string type = "", hash_filename = "";
	int num_rs = 0;
	Memory::AutoArray< tr1::shared_ptr<IO::RecordStore> > child_rs;
	if (procargs_merge(argc, argv, type, child_rs, num_rs, hash_filename,
	    what_to_hash, hashed_key_format) != EXIT_SUCCESS)
		return (EXIT_FAILURE);

	string description = "A merge of ";
	for (int i = 0; i < num_rs; i++) {
		if (child_rs[i] != NULL) {
			description += child_rs[i]->getName();
			if (i != (num_rs - 1)) description += ", ";
		}
	}

	try {
		if (hash_filename.empty()) {
			IO::RecordStore::mergeRecordStores(
			    sflagval, description, oflagval, type, child_rs,
			    num_rs);
		} else {
			mergeAndHashRecordStores(
			    sflagval, description, hash_filename, oflagval,
			    type, child_rs, num_rs, what_to_hash,
			    hashed_key_format);
		}
	} catch (Error::Exception &e) {
		cerr << "Could not create " << sflagval << " - " <<
		    e.getInfo() << endl;
	}

	return (EXIT_SUCCESS);
}

/**
 * @brief
 * Display version information.
 *
 * @param argc[in]
 *	argc from main()
 * @param argv[in]
 *	argv from main()
 * @returns
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
static int version(int argc, char *argv[])
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

/**
 * @brief
 * Process command-line arguments specific to the UNHASH Action.
 *
 * @param argc[in]
 *	argc from main()
 * @param argv[in]
 *	argv from main()
 * @param hash[in]
 *	Reference to a string that will hold the hash to unhash
 * @param hash_rs[in]
 *	Reference to a shared pointer that will hold the open hashed RecordStore
 *
 * @returns
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
static int procargs_unhash(int argc, char *argv[], string &hash,
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

/**
 * @brief
 * Facilitates the unhashing of an MD5 hashed key from a RecordStore, given
 * a RecordStore of hash values.
 *
 * @param argc[in]
 *	argc from main()
 * @param argv[in]
 *	argv from main()
 *
 * @returns
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
static int unhash(int argc, char *argv[])
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

/**
 * @brief
 * Process command-line arguments specific to the ADD Action.
 *
 * @param argc[in]
 *	argc from main()
 * @param argv[in]
 *	argv from main()
 * @param rs[in]
 *	Reference to a RecordStore shared pointer that will be allocated
 *	to hold the main RecordStore passed with the -s flag on the command-line
 * @param hash_rs[in]
 *	Reference to a hash translation RecordStore shared pointer that will
 *	be allocated to hold a hash translation of the file's contents.
 * @param files[in]
 *	Reference to a vector that will be populated with paths to files that
 *	should be added to rs.
 * @param what_to_hash[in]
 *	Reference to a HashablePart enumeration that indicates what should be
 *	hashed when creating a hash for an entry.
 * @param hashed_key_format[in]
 *	Reference to a KeyFormat enumeration that indicates how the key (when
 *	printed as a value) should appear in a hash translation RecordStore.
 *
 * @returns
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
static int
procargs_add(
    int argc,
    char *argv[],
    tr1::shared_ptr<IO::RecordStore> &rs,
    tr1::shared_ptr<IO::RecordStore> &hash_rs,
    vector<string> &files,
    HashablePart::Type &what_to_hash,
    KeyFormat::Type &hashed_key_format)
{
	what_to_hash = HashablePart::NOTHING;
	hashed_key_format = KeyFormat::DEFAULT;

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
		if ((hash_rs.get() != NULL) && (what_to_hash ==
		    HashablePart::NOTHING))
			what_to_hash = HashablePart::FILENAME;

	return (EXIT_SUCCESS);
}

/**
 * @brief
 * Facilitates the addition of files to an existing RecordStore.
 * 
 * @param argc[in]
 *	argc from main()
 * @param argv[in]
 *	argv from main()
 *
 * @returns
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
static int
add(
    int argc,
    char *argv[])
{
	HashablePart::Type what_to_hash = HashablePart::NOTHING;
	KeyFormat::Type hashed_key_format = KeyFormat::DEFAULT;
	tr1::shared_ptr<IO::RecordStore> rs, hash_rs;
	vector<string> files;
	if (procargs_add(argc, argv, rs, hash_rs, files, what_to_hash,
	    hashed_key_format) != EXIT_SUCCESS)
		return (EXIT_FAILURE);

	for (vector<string>::const_iterator file_path = files.begin();
	    file_path != files.end(); file_path++) {
		/*
		 * Conscious decision to not care about the return value
		 * when inserting because we may have multiple files we'd
		 * like to add and there's no point in quitting halfway.
		 */
		make_insert_contents(*file_path, rs, hash_rs, what_to_hash,
		    hashed_key_format);
	}

	return (EXIT_SUCCESS);
}

/**
 * @brief
 * Process command-line arguments specific to the REMOVE Action.
 *
 * @param argc[in]
 *	argc from main().
 * @param argv[in]
 *	argv from main().
 * @param keys[in]
 *	Reference to a vector that will hold the keys to remove.
 * @param force_removal
 *	Reference to a boolean that when true will not prompt before removal.
 * @param rs[in]
 *	Reference to a shared pointer that will hold the open RecordStore.
 *
 * @returns
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
static int
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

/**
 * @brief
 * Facilitates the removal of key/value pairs from an existing RecordStore.
 * 
 * @param argc[in]
 *	argc from main()
 * @param argv[in]
 *	argv from main()
 *
 * @returns
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
static int
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

/**
 * @brief
 * Process command-line arguments specific to the DIFF Action.
 *
 * @param argc[in]
 *	argc from main().
 * @param argv[in]
 *	argv from main().
 * @param sourceRS[in]
 *	Reference to a shared pointer that will hold the open
 *	source-RecordStore.
 * @param targetRS[in]
 *	Reference to a shared pointer that will hold the open 
 *	target-RecordStore.
 * @param keys[in]
 *	Reference to a vector that will hold the keys to compare.
 * @param byte_for_byte[in]
 *	Reference to a boolean that, when true, will perform the difference by
 *	comparing buffers byte-for-byte instead of using an MD5 checksum.
 *
 * @returns
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
static int
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

/**
 * @brief
 * Facilitates a diff between two RecordStores.
 * 
 * @param argc[in]
 *	argc from main()
 * @param argv[in]
 *	argv from main()
 *
 * @returns
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
static int
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

int main(int argc, char *argv[])
{
	Action::Type action = procargs(argc, argv);
	switch (action) {
	case Action::ADD:
		return (add(argc, argv));
	case Action::DIFF:
		return (diff(argc, argv));
	case Action::DISPLAY:
		/* FALLTHROUGH */
	case Action::DUMP:
		return (extract(argc, argv, action));
	case Action::LIST:
		return (list(argc, argv));
	case Action::MAKE:
		return (make(argc, argv));
	case Action::MERGE:
		return (merge(argc, argv));
	case Action::REMOVE:
		return (remove(argc, argv));
	case Action::VERSION:
		return (version(argc, argv));
	case Action::UNHASH:
		return (unhash(argc, argv));
	case Action::QUIT:
	default:
		return (EXIT_FAILURE);
	}
}
