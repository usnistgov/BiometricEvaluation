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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <dirent.h>
#include <getopt.h>
#include <libgen.h>

#include <be_error.h>
#include <be_error_exception.h>
#include <be_framework.h>
#include <be_io_archiverecstore.h>
#include <be_io_dbrecstore.h>
#include <be_io_factory.h>
#include <be_io_filerecstore.h>
#include <be_io_recordstore.h>
#include <be_io_utility.h>
#include <be_text.h>
#include <be_utility.h>
#include <be_utility_autoarray.h>

static string oflagval = ".";		/* Output directory */
static string sflagval = "";		/* Path to main RecordStore */
static const char optstr[] = "a:ch:k:m:o:r:s:t:";

/* Possible actions performed by this utility */
static const string ADD_ARG = "add";
static const string DUMP_ARG = "dump";
static const string LIST_ARG = "list";
static const string MAKE_ARG = "make";
static const string MERGE_ARG = "merge";
static const string VERSION_ARG = "version";
static const string UNHASH_ARG = "unhash";
typedef enum { ADD, DUMP, LIST, MAKE, MERGE, VERSION, UNHASH, QUIT } Action;

using namespace BiometricEvaluation;
using namespace std;

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
	cerr << "Actions: add, dump, list, make, merge, version, unhash" <<
	    endl;

	cerr << endl;

	cerr << "Common:" << endl;
	cerr << "\t-c\t\tHash contents, not filename" << endl;
	cerr << "\t-o <...>\tOutput directory" << endl;
	cerr << "\t-s <path>\tRecordStore" << endl;

	cerr << endl;

	cerr << "Add Options:" << endl;
	cerr << "\t-a <file>\tFile to add" << endl;
	cerr << "\t-h <hash_rs>\tExisting hash translation RecordStore" << endl;

	cerr << endl;

	cerr << "Dump Options:" << endl;
	cerr << "\t-k <key>\tKey to dump" << endl;
	cerr << "\t-r <#-#>\tRange of keys" << endl;

	cerr << endl;

	cerr << "Make Options:" << endl;
	cerr << "\t-a <file/dir>\tText file with paths, or a directory "
	    "(multiple)" << endl;
	cerr << "\t-h\t\tUse an MD5 hash for key values" << endl;
	cerr << "\t-t <type>\tType of RecordStore to make" << endl;
	cerr << "\t\t\tWhere <type> is Archive, BerkeleyDB, File" << endl;

	cerr << endl;

	cerr << "Merge Options:" << endl;
	cerr << "\t-a <RS>\t\tRecordStore to be merged (multiple)" << endl;
	cerr << "\t-h <RS>\t\tHash keys and store a hash translation "
	    "RecordStore" << endl;
	cerr << "\t-t <type>\tType of RecordStore to make" << endl;
	cerr << "\t\t\tWhere <type> is Archive, BerkeleyDB, File" << endl;

	cerr << endl;

	cerr << "Unhash Options:" << endl;
	cerr << "\t-h <hash>\tHash to unhash" << endl;

	cerr << endl;
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
static Action procargs(int argc, char *argv[])
{
	if (argc == 1) {
		usage(argv[0]);
		return (QUIT);
	}

	Action action;
	if (strcasecmp(argv[1], DUMP_ARG.c_str()) == 0)
		action = DUMP;
	else if (strcasecmp(argv[1], LIST_ARG.c_str()) == 0)
		action = LIST;
	else if (strcasecmp(argv[1], MAKE_ARG.c_str()) == 0)
		action = MAKE;
	else if (strcasecmp(argv[1], MERGE_ARG.c_str()) == 0)
		action = MERGE;
	else if (strcasecmp(argv[1], VERSION_ARG.c_str()) == 0)
		return VERSION;
	else if (strcasecmp(argv[1], UNHASH_ARG.c_str()) == 0)
		action = UNHASH;
	else if (strcasecmp(argv[1], ADD_ARG.c_str()) == 0)
		action = ADD;
	else {
		usage(argv[0]);
		return (QUIT);
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
					return (QUIT);
				}
			} else {
				if (mkdir(oflagval.c_str(), S_IRWXU) != 0) {
					cerr << "Could not create " <<
					    oflagval << " - " << 
					    Error::errorStr();
					return (QUIT);
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
		return (QUIT);
	}

	return (action);
}

/**
 * @brief
 * Process command-line arguments specific to the DUMP Action.
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
 *
 * @returns
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE.
 */
static int procargs_dump(int argc, char *argv[], string &key, string &range,
    tr1::shared_ptr<IO::RecordStore> &rs)
{
	char c;
        while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
		case 'k':
			key.assign(optarg);
			break;
		case 'r':
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
		rs = IO::Factory::openRecordStore(Text::filename(sflagval),
		    Text::dirname(sflagval), IO::READONLY);
	} catch (Error::Exception &e) {
		cerr << "Could not open " << sflagval << ".  " <<
		    e.getInfo() << endl;
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
 * Facilitates the dumping of a single key or a range of records from a 
 * RecordStore
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
static int dump(int argc, char *argv[])
{
	string key = "", range = "";
	tr1::shared_ptr<IO::RecordStore> rs;
	if (procargs_dump(argc, argv, key, range, rs) != EXIT_SUCCESS)
		return (EXIT_FAILURE);

	Utility::AutoArray<uint8_t> buf;
	FILE *fp;
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
				cerr << "Error dumping " << key << " - " <<
				    e.getInfo() << endl;	
				return (EXIT_FAILURE);
			}
		} catch (Error::Exception &e) {
			cerr << "Error dumping " << key << endl;	
			return (EXIT_FAILURE);
		}
		fp = fopen((oflagval + "/" + key).c_str(), "wb");
		if (fp == NULL) {
			cerr << "Could not create file." << endl;
			return (EXIT_FAILURE);
		}
		if (fwrite(buf, 1, buf.size(), fp) != buf.size()) {
			cerr << "Could not write entry." << endl;
			if (fp != NULL)
				fclose(fp);
			return (EXIT_FAILURE);
		}
		fclose(fp);

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
			fp = fopen((oflagval + "/" + next_key).c_str(), "wb");
			if (fp == NULL) {
				cerr << "Could not create file." << endl;
				return (EXIT_FAILURE);
			}
			if (fwrite(buf, 1, buf.size(), fp) != buf.size()) {
				cerr << "Could not write entry." << endl;
				if (fp != NULL)
					fclose(fp);
				return (EXIT_FAILURE);
			}
			fclose(fp);
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
		rs = IO::Factory::openRecordStore(Text::filename(sflagval),
		    Text::dirname(sflagval), IO::READONLY);
	} catch (Error::Exception &e) {
		cerr << "Could not open RecordStore - " << e.getInfo() << endl;
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
 * @param hash_contents[in]
 *	Reference to a boolean that indicates if the contents or the filename
 *	of a file should be hashed when hashing is to be performed.
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
static int procargs_make(int argc, char *argv[], string &hash_filename,
    bool &hash_contents, string &type, vector<string> &elements)
{
	char c;
        while ((c = getopt(argc, argv, optstr)) != EOF) {
		switch (c) {
		case 'a':	/* RecordStore to be created */
			if (!IO::Utility::fileExists(optarg)) {
				cerr << optarg << " does not exist." << endl;
				return (EXIT_FAILURE);
			}
			elements.push_back(optarg);
			break;
		case 'c':	/* Hash contents, not filename */
			hash_contents = true;
			break;
		case 'h':
			hash_filename.assign(optarg);
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

	if (type.empty())
		type.assign(IO::RecordStore::BERKELEYDBTYPE);

	if (elements.empty()) {
		cerr << "Missing required option (-a)." << endl;
		return (EXIT_FAILURE);
	}

	if (hash_filename.empty() && hash_contents) {
		cerr << "Specified -c without -h." << endl;
		return (EXIT_FAILURE);
	}

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
 * @param hash_contents[in]
 *	Whether the contents or the filename of a file should be hashed when
 *	hashing is to be performed.
 *
 * @returns
 *	An exit status, either EXIT_FAILURE or EXIT_SUCCESS, depending on if
 *	the contents of filename could be inserted.
 */
static int make_insert_contents(const string &filename,
    const tr1::shared_ptr<IO::RecordStore> &rs,
    const tr1::shared_ptr<IO::RecordStore> &hash_rs,
    bool hash_contents)
{
	static Utility::AutoArray<char> buffer;
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
		cerr << "Error reading file." << endl;
		return (EXIT_FAILURE);
	}
	buffer_file.close();
	
	static string key = "", hash_value = "";
	try {
		key = Text::filename(filename);
		if (hash_rs.get() == NULL)
			rs->insert(key, buffer, buffer_size);
		else {
			if (!hash_contents)
				hash_value = Text::digest(key);
			else
				hash_value = Utility::digest(buffer,
				    buffer_size);
			rs->insert(hash_value, buffer, buffer_size);
			hash_rs->insert(hash_value, key.c_str(), key.size());
		}
	} catch (Error::Exception &e) {
		cerr << "Could not add conents of " << filename <<
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
 * @param hash_contents[in]
 *	Boolean to indicate if the contents or the filename should be hashed
 *	when hashing is to be performed.
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
static int make_insert_directory_contents(const string &directory,
    const string &prefix, const tr1::shared_ptr<IO::RecordStore> &rs,
    const tr1::shared_ptr<IO::RecordStore> &hash_rs, bool hash_contents)
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
			    dirpath, rs, hash_rs, hash_contents) !=
			    EXIT_SUCCESS)
				return (EXIT_FAILURE);
		} else {
			if (make_insert_contents(filename, rs, hash_rs,
			    hash_contents) != EXIT_SUCCESS)
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
	bool hash_contents = false;

	if (procargs_make(argc, argv, hash_filename, hash_contents, type,
	    elements) != EXIT_SUCCESS)
		return (EXIT_FAILURE);

	tr1::shared_ptr<IO::RecordStore> rs;
	tr1::shared_ptr<IO::RecordStore> hash_rs;
	try {
		rs = IO::Factory::createRecordStore(sflagval, "<Description>",
		    type, oflagval);
		if (!hash_filename.empty())
			hash_rs = IO::Factory::createRecordStore(hash_filename,
			    "Hash Translation for " + sflagval, type, oflagval);
	} catch (Error::Exception &e) {
		cerr << "Could not create " << sflagval << " - " <<
		    e.getInfo() << endl;
		return (EXIT_FAILURE);
	}

	string line;
	ifstream input;
	for (unsigned int i = 0; i < elements.size(); i++) {
		if (IO::Utility::pathIsDirectory(elements[i])) {
			try {
				/* Recursively insert contents of directory */
				if (make_insert_directory_contents(
				    Text::filename(elements[i]),
				    Text::dirname(elements[i]),
				    rs, hash_rs, hash_contents) != EXIT_SUCCESS)
			    		return (EXIT_FAILURE);
			} catch (Error::Exception &e) {
				cerr << "Could not add contents of dir " <<
				    elements[i] << " - " << e.getInfo() << endl;
				return (EXIT_FAILURE);
			}
		} else {
			input.open(elements[i].c_str(), ifstream::in);
			for (;;) {
				/* Read one path from text file */
				input >> line;
				if (input.eof())
					break;
				else if (input.bad()) {
					cerr << "Error reading " << input <<
					    endl;
					return (EXIT_FAILURE);
				}

				if (make_insert_contents(line, rs, hash_rs,
				    hash_contents) !=
				    EXIT_SUCCESS)
					return (EXIT_FAILURE);
			}
			input.close();
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
 * @param hash_contents[in]
 *	Reference to a boolean that indicates if the contents or the filename
 *	of a file should be hashed when hashing is to be performed.
 *
 * @returns
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE.
 */
static int procargs_merge(int argc, char *argv[], string &type,
    Utility::AutoArray< tr1::shared_ptr< IO::RecordStore > > &child_rs,
    int &num_child_rs, string &hash_filename, bool &hash_contents)
{
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
				    IO::Factory::openRecordStore(
				    Text::filename(optarg),
				    Text::dirname(optarg),
				    IO::READONLY);
			} catch (Error::Exception &e) {
				cerr << "Could not open " << optarg << " - " <<
				    e.getInfo() << endl;
				return (EXIT_FAILURE);
			}
			break;
		case 'c':	/* Hash contents, not filename */
			hash_contents = true;
			break;
		case 'h':	/* Hash translation RecordStore */
			hash_filename.assign(optarg);
			break;
		}
	}

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

	if (hash_filename.empty() && hash_contents) {
		cerr << "Specified -c without -h." << endl;
		return (EXIT_FAILURE);
	}

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
 * @param hash_contents[in]
 *	Whether the contents of the file or the filename should be hashed.
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
    size_t numRecordStores,
    bool hash_contents)
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
	Utility::AutoArray<char> buf;
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

			if (!hash_contents)
				hash = Text::digest(key);
			else
				hash = Utility::digest(buf, record_size);
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
	bool hash_contents = false;
	string type = "", hash_filename = "";
	int num_rs = 0;
	Utility::AutoArray< tr1::shared_ptr<IO::RecordStore> > child_rs;
	if (procargs_merge(argc, argv, type, child_rs, num_rs, hash_filename,
	    hash_contents) != EXIT_SUCCESS)
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
			    type, child_rs, num_rs, hash_contents);
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
	    " (Compiled " << __TIMESTAMP__ << ")" << endl;
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
		rs = IO::Factory::openRecordStore(Text::filename(sflagval),
		    Text::dirname(sflagval), IO::READONLY);
	} catch (Error::Exception &e) {
		cerr << "Could not open " << sflagval << " - " << e.getInfo() <<
		    endl;
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
	
	Utility::AutoArray<char> buffer;
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
 * @param hash_contents[in]
 *	Reference to a boolean that indicates if the contents or the filename
 *	of a file should be hashed when hashing is to be performed.
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
    bool &hash_contents)
{
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
		case 'c':	/* Hash contents, not filename */
			hash_contents = true;
			break;
		case 'h':	/* Existing hash translation RecordStore */
			try {
				hash_rs = IO::Factory::openRecordStore(
				    Text::filename(optarg),
				    Text::dirname(optarg));
			} catch (Error::Exception &e) {
				cerr << "Could not open " << optarg << " -- " <<
				    e.getInfo() << endl;
				return (EXIT_FAILURE);
			}
			break;
		}
	}

	/* sflagval is the RecordStore we will be adding to */
	try {
		rs = IO::Factory::openRecordStore(Text::filename(sflagval),
		    Text::dirname(sflagval));
	} catch (Error::Exception &e) {
		cerr << "Could not open " << sflagval << " -- " <<
		    e.getInfo() << endl;
		return (EXIT_FAILURE);
	}

	if (hash_rs.get() == NULL && hash_contents) {
		cerr << "Specified -c without -h." << endl;
		return (EXIT_FAILURE);
	}

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
	bool hash_contents = false;
	tr1::shared_ptr<IO::RecordStore> rs, hash_rs;
	vector<string> files;
	if (procargs_add(argc, argv, rs, hash_rs, files, hash_contents) !=
	    EXIT_SUCCESS)
		return (EXIT_FAILURE);

	for (vector<string>::const_iterator file_path = files.begin();
	    file_path != files.end(); file_path++) {
		/*
		 * Conscious decision to not care about the return value
		 * when inserting because we may have multiple files we'd
		 * like to add and there's no point in quitting halfway.
		 */
		make_insert_contents(*file_path, rs, hash_rs, hash_contents);
	}

	return (EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	Action action = procargs(argc, argv);
	switch (action) {
	case ADD:
		return (add(argc, argv));
	case DUMP:
		return (dump(argc, argv));
	case LIST:
		return (list(argc, argv));
	case MAKE:
		return (make(argc, argv));
	case MERGE:
		return (merge(argc, argv));
	case VERSION:
		return (version(argc, argv));
	case UNHASH:
		return (unhash(argc, argv));
	case QUIT:
	default:
		return (EXIT_FAILURE);
	}
}
