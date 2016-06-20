/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */

#ifndef __RSTOOL_H__
#define __RSTOOL_H__

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

#include <lrs_additions.h>

static std::string oflagval = ".";		/* Output directory */
static std::string sflagval = "";		/* Path to main RecordStore */
static const char optstr[] = "a:cfh:k:m:o:pqr:s:t:zZ:";

/* Possible actions performed by this utility */
static const std::string ADD_ARG = "add";
static const std::string DISPLAY_ARG = "display";
static const std::string DIFF_ARG = "diff";
static const std::string DUMP_ARG = "dump";
static const std::string LIST_ARG = "list";
static const std::string MAKE_ARG = "make";
static const std::string MERGE_ARG = "merge";
static const std::string REMOVE_ARG = "remove";
static const std::string RENAME_ARG = "rename";
static const std::string VERSION_ARG = "version";
static const std::string UNHASH_ARG = "unhash";

enum class Action
{
	ADD,
	DISPLAY,
	DIFF,
	DUMP,
	LIST,
	MAKE,
	MERGE,
	RENAME,
	REMOVE,
	VERSION,
	UNHASH,
	QUIT
};

/* Things that could be hashed when hashing a key */
enum class HashablePart
{
	FILECONTENTS,
	FILENAME,
	FILEPATH,
	NOTHING
};


/* What to print as value in a hash translation RecordStore */
enum class KeyFormat
{
	DEFAULT,
	FILENAME,
	FILEPATH
};


/* Triggers for special-case processing.  Allows multiple cases. */
enum class SpecialProcessing : uint16_t
{
	NA		= 0,
	LISTRECORDSTORE	= 1 << 0
};

/**
 * @brief
 * Read the contents of a text file into a vector (one line per entry).
 *
 * @param[in] filePath
 *	Complete path of the text file to read from.
 * @param[in] ignoreComments
 *	Whether or not to ignore comment lines (lines beginning with '#')
 * @param[in] ignoreBlankLines
 *	Whether or not to add blank entries (lines beginning with '\n')
 *
 * @return
 *	Vector of the lines in the file
 *
 * @throw BiometricEvaluation::Error::FileError
 *	Error opening or reading from filePath.
 * @throw BiometricEvaluation::Error::ObjectDoesNotExist
 *	filePath does not exist.
 */
std::vector<std::string>
readTextFileToVector(
    const std::string &filePath,
    bool ignoreComments = true,
    bool ignoreBlankLines = true);

/**
 * @brief
 * Prompt the user to answer a yes or no question.
 *
 * @param[in] prompt
 *	The yes or no question.
 * @param[in] default_answer
 *	Value that is the default answer if return is pressed but nothing
 *	typed.
 * @param[in] show_options
 *	Whether or not to show valid input values.
 * @param[in] allow_default_answer
 *	Whether or not to allow default answer.
 *
 * @return
 *	Answer to prompt (true if yes, false if no).
 */
bool
yesOrNo(
    const std::string &prompt,
    bool default_answer = true,
    bool show_options = true,
    bool allow_default_answer = true);

/**
 * @brief
 * Display command-line usage for the tool.
 * 
 * @param[in] exe
 *	Name of executable.
 */
void
usage(
    char *exe);

/**
 * @brief 
 * Check access to core RecordStore files.
 *
 * @param[in] name
 *	RecordStore name
 * @param[in] parentDir
 *	Directory holding RecordStore.
 * @param[in] mode
 *	How attempting to open the RecordStore (IO::Mode::ReadOnly, IO::Mode::ReadWrite)
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
bool
isRecordStoreAccessible(
    const std::string &pathname,
    const BiometricEvaluation::IO::Mode mode);

/**
 * @brief
 * Validate a RecordStore type string.
 *
 * @param[in] type
 *	String (likely entered by user) to check for validity.
 *
 * @return
 *	RecordStore Kind.
 *
 * @throw BiometricEvaluation::Error::StrategyError
 *	type is not a valid string representation of a RecordStore.
 */
BiometricEvaluation::IO::RecordStore::Kind
validate_rs_type(
    const std::string &type);

/**
 * @brief
 * Process command-line arguments for the tool.
 *
 * @param[in] argc
 *	argc from main()
 * @param[in] argv
 *	argv from main()
 *
 * @return
 *	The Action the user has indicated on the command-line.
 */
Action
procargs(
    int argc,
    char *argv[]);

/**
 * @brief
 * Process command-line arguments specific to the DISPLAY and DUMP Actions.
 *
 * @param[in] argc
 *	argc from main()
 * @param[in] argv
 *	argv from main()
 * @param[in/out] visualize
 *	Reference to a boolean indicating if we should try to visually 
 *	display an image ("DISPLAY" only).
 * @param[in/out] key
 *	Reference to a string that will be populated with any value
 *	following the "key" flag on the command-line
 * @param[in/out] range
 *	Reference to a string that will be populated with any value
 *	following the "range" flag on the command-line
 * @param[in/out] rs
 *	Reference to a RecordStore shared pointer that will be allocated
 *	to hold the main RecordStore passed with the -s flag on the command-line
 * @param[in/out] hash_rs
 *	Reference to a RecordStore shared pointer that will be allocated
 *	to hold a hash translation RecordStore.  Instantiating this RecordStore
 *	lets the driver know that the user would like the unhashed key to be
 *	used when the extraction takes place.
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE.
 */
int
procargs_extract(
    int argc,
    char *argv[],
    bool &visualize,
    std::string &key,
    std::string &range,
    std::shared_ptr<BiometricEvaluation::IO::RecordStore> &rs,
    std::shared_ptr<BiometricEvaluation::IO::RecordStore> &hash_rs);

/**
 * @brief
 * Print a record to the screen.
 *
 * @param[in] key
 *	The name of the record to display.
 * @param[in] value
 *	The contents of the record to display.
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
int
display(
    const std::string &key,
    BiometricEvaluation::Memory::AutoArray<uint8_t> &value);

/**
 * @brief
 * Visualize a record by displaying on the screen.
 *
 * @param[in] key
 *	The name of the record to visualize.
 * @param[in] value
 *	The contents of the record to visualize.
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 *
 * @note
 *	This function calls others that will fork().
 */
int
visualizeRecord(
    const std::string &key,
    BiometricEvaluation::Memory::uint8Array &value);

/**
 * @brief
 * Write a record to disk.
 *
 * @param[in] key
 *	The name of the file to write.
 * @param[in] value
 *	The contents of the file to write.
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
int
dump(
    const std::string &key,
    BiometricEvaluation::Memory::AutoArray<uint8_t> &value);
/**
 * @brief
 * Facilitates the extraction of a single key or a range of records from a 
 * RecordStore
 *
 * @param[in] argc
 *	argc from main()
 * @param[in] argv
 *	argv from main()
 * @param[in] action
 *	The Action to be performed with the extracted data.
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
int
extract(
    int argc,
    char *argv[],
    Action action);

/**
 * @brief
 * Facilitates the listing of the keys stored within a RecordStore.
 *
 * @param[in] argc
 *	argc from main()
 * @param[in] argv
 *	argv from main()
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
int
listRecordStore(
    int argc,
    char *argv[]);

/**
 * @brief
 * Process command-line arguments specific to the MAKE Action.
 *
 * @param[in] argc
 *	argc from main()
 * @param[in] argv
 *	argv from main()
 * @param[in/out] hash_filename
 *	Reference to a string to store a path to a hash translation RecordStore,
 *	indicating that the keys should be hashed
 * @param[in/out] what_to_hash
 *	Reference to a HashablePart enumeration that indicates what should be
 *	hashed when creating a hash for an entry.
 * @param[in/out] hashed_key_format
 *	Reference to a KeyFormat enumeration that indicates how the key (when
 *	printed as a value) should appear in a hash translation RecordStore.
 * @param[in/out] kind
 *	Kind of RecordStore to create
 * @param[in/out] elements
 *	Reference to a vector that will hold paths to elements that should
 *	be added to the target RecordStore.  The paths may be to text
 *	files, whose contents are lines of paths that should be added, or
 *	directories, whose contents should be added.
 * @param[in/out] compress
 *	Whether or not to compress record contents.
 * @param[in/out] compressorKind
 *	The type of compression used to compress record contents.
 * @param[in] stopOnDuplicate
 *	Whether or not to stop when attempting to add a duplicate key into
 *	the data RecordStore.
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
int
procargs_make(
    int argc,
    char *argv[],
    std::string &hash_filename,
    HashablePart &what_to_hash,
    KeyFormat &hashed_key_format,
    BiometricEvaluation::IO::RecordStore::Kind &kind,
    std::vector<std::string> &elements,
    bool &compress,
    BiometricEvaluation::IO::Compressor::Kind &compressorKind,
    bool &stopOnDuplicate);

/**
 * @brief
 * Helper function to insert the contents of a file into a RecordStore.
 *
 * @param[in] filename
 *	The name of the file whose contents should be inserted into rs.
 * @param[in] rs
 *	The RecordStore into which the contents of filename should be inserted
 * @param[in] hash_rs
 *	The RecordStore into which hash translations should be stored
 * @param[in] what_to_hash
 *	What should be hashed when creating a hash for an entry.
 * @param[in] hashed_key_format
 *	How the key should be displayed in a hash translation RecordStore.
 * @param[in] stopOnDuplicate
 *	Whether or not to stop when attempting to add a duplicate key into
 *	the data RecordStore.
 *
 * @return
 *	An exit status, either EXIT_FAILURE or EXIT_SUCCESS, depending on if
 *	the contents of filename could be inserted.
 */
int make_insert_contents(const std::string &filename,
    const std::shared_ptr<BiometricEvaluation::IO::RecordStore> &rs,
    const std::shared_ptr<BiometricEvaluation::IO::RecordStore> &hash_rs,
    const HashablePart what_to_hash,
    const KeyFormat hashed_key_format,
    bool stopOnDuplicate);

/**
 * @brief
 * Recursive helper function to insert the contents of a directory into
 * a RecordStore.
 * 
 * @param[in] directory
 *	Path of the directory to search through
 * @param[in] prefix
 *	The parent directories of directory
 * @param[in] rs
 *	The RecordStore into which files should be inserted
 * @param[in] hash_rs
 *	The RecordStore into which hash translations should be stored
 * @param[in] what_to_hash
 *	What should be hashed when creating a hash for an entry.
 * @param[in] hashed_key_value
 *	How the key should be displayed in the hash translation RecordStore.
 * @param[in] stopOnDuplicate
 *	Whether or not to stop if a duplicate key is detected.
 *
 * @throws BiometricEvaluation::Error::ObjectDoesNotExist
 *	If the contents of the directory changes during the run
 * @throws BiometricEvaluation::Error::StrategyError
 *	Underlying problem in storage system
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
int
make_insert_directory_contents(
    const std::string &directory,
    const std::string &prefix,
    const std::shared_ptr<BiometricEvaluation::IO::RecordStore> &rs,
    const std::shared_ptr<BiometricEvaluation::IO::RecordStore> &hash_rs,
    const HashablePart what_to_hash,
    const KeyFormat hashed_key_format,
    bool stopOnDuplicate);

/**
 * @brief
 * Facilitates the creation of a ListRecordStore.
 *
 * @param[in] argc
 *	argc from main()
 * @param[in] argv
 *	argv from main()
 * @param[in/out] elements
 *	Reference to a vector that will hold paths to elements that should
 *	be added to the target RecordStore.  The paths may be to text
 *	files, whose contents are lines of paths that should be added, or
 *	directories, whose contents should be added.
 * @param[in] what_to_hash
 *	What should be hashed when creating a hash for an entry.
 * @param[in] hashed_key_value
 *	How the key should be displayed in the hash translation RecordStore.
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
int
makeListRecordStore(
    int argc,
    char *argv[],
    std::vector<std::string> &elements,
    HashablePart what_to_hash,
    KeyFormat hashed_key_format);

/**
 * @brief
 * Facilitates creation of a RecordStore.
 *
 * @param[in] argc
 *	argc from main()
 * @param[in] argv
 *	argv from main()
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
int
make(
    int argc,
    char *argv[]);

/**
 * @brief
 * Process command-line arguments specific to the MERGE Action.
 *
 * @param[in] argc
 *	argc from main()
 * @param[in] argv
 *	argv from main()
 * @param[in/out] kind
 *	Kind of RecordStore to create
 * @param[in/out] child_rs
 * 	Reference to an vector of strings that will hold the paths to the 
 *	RecordStores that should be merged.
 * @param[in/out] hash_filename
 *	Reference to a string that will specify the name of the hash translation
 *	RecordStore, if one is desired.
 * @param[in/out] what_to_hash
 *	Reference to a HashablePart enumeration indicating what should be
 *	hashed when creating a hash for an entry.
 * @param[in/out] hashed_key_format
 *	Reference to a KeyFormat enumerating indicating how the key should be
 *	printed (as a value) in a hash translation RecordStore.
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE.
 */
int
procargs_merge(
    int argc,
    char *argv[],
    BiometricEvaluation::IO::RecordStore::Kind &kind,
    std::vector<std::string> &child_rs,
    std::string &hash_filename,
    HashablePart &what_to_hash,
    KeyFormat &hashed_key_format);

/** 
 * Create a new RecordStore that contains the contents of several RecordStores,
 * hashing the keys of the child RecordStores before adding them to the merged.
 *  
 * @param[in] mergedName
 * 	The name of the new RecordStore that will be created.
 * @param[in] mergedDescription
 * 	The text used to describe the RecordStore.
 * @param[in] hashName
 *	The name of the new hash translation RecordStore that will be created.
 * @param[in] parentDir
 * 	Where, in the file system, the new store should be rooted.
 * @param[in] kind
 * 	The kind of RecordStore that mergedName should be.
 * @param[in] recordStores
 * 	An vector of string paths to RecordStore that should be merged into
 *	mergedName.
 * @param[in] what_to_hash
 *	What should be hashed when creating a hash for an entry.
 * @param[in] hashed_key_format
 *	How the key should be printed in a hash translation RecordStore
 *
 * @throws BiometricEvaluation::Error::ObjectExists
 * 	A RecordStore with mergedNamed in parentDir
 * @throws BiometricEvaluation::Error::StrategyError
 * 	An error occurred when using the underlying storage system
 */
void mergeAndHashRecordStores(
    const std::string &mergedName,
    const std::string &mergedDescription,
    const std::string &hashName,
    const std::string &parentDir,
    const BiometricEvaluation::IO::RecordStore::Kind &kind,
    std::vector<std::string> &recordStores,
    const HashablePart what_to_hash,
    const KeyFormat hashed_key_format);

/**
 * @brief
 * Facilitates the merging of multiple RecordStores.
 *
 * @param[in] argc
 *	argc from main()
 * @param[in] argv
 *	argv from main()
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
int
merge(
    int argc,
    char *argv[]);

/**
 * @brief
 * Display version information.
 *
 * @param[in] argc
 *	argc from main()
 * @param[in] argv
 *	argv from main()
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
int
version(
    int argc,
    char *argv[]);

/**
 * @brief
 * Process command-line arguments specific to the UNHASH Action.
 *
 * @param[in] argc
 *	argc from main()
 * @param[in] argv
 *	argv from main()
 * @param[in/out] hash
 *	Reference to a string that will hold the hash to unhash
 * @param[in/out] hash_rs
 *	Reference to a shared pointer that will hold the open hashed RecordStore
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
int
procargs_unhash(
    int argc,
    char *argv[],
    std::string &hash,
    std::shared_ptr<BiometricEvaluation::IO::RecordStore> &rs);

/**
 * @brief
 * Facilitates the unhashing of an MD5 hashed key from a RecordStore, given
 * a RecordStore of hash values.
 *
 * @param[in] argc
 *	argc from main()
 * @param[in] argv
 *	argv from main()
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
int
unhash(
    int argc,
    char *argv[]);

/**
 * @brief
 * Process command-line arguments specific to the ADD Action.
 *
 * @param[in] argc
 *	argc from main()
 * @param[in] argv
 *	argv from main()
 * @param[in/out] rs
 *	Reference to a RecordStore shared pointer that will be allocated
 *	to hold the main RecordStore passed with the -s flag on the command-line
 * @param[in/out] hash_rs
 *	Reference to a hash translation RecordStore shared pointer that will
 *	be allocated to hold a hash translation of the file's contents.
 * @param[in/out] files
 *	Reference to a vector that will be populated with paths to files that
 *	should be added to rs.
 * @param[in/out] what_to_hash
 *	Reference to a HashablePart enumeration that indicates what should be
 *	hashed when creating a hash for an entry.
 * @param[in/out] hashed_key_format
 *	Reference to a KeyFormat enumeration that indicates how the key (when
 *	printed as a value) should appear in a hash translation RecordStore.
 * @param[in] stopOnDuplicate
 *	Whether or not to stop when attempting to add a duplicate key into
 *	the data RecordStore.
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
int
procargs_add(
    int argc,
    char *argv[],
    std::shared_ptr<BiometricEvaluation::IO::RecordStore> &rs,
    std::shared_ptr<BiometricEvaluation::IO::RecordStore> &hash_rs,
    std::vector<std::string> &files,
    HashablePart &what_to_hash,
    KeyFormat &hashed_key_format,
    bool &stopOnDuplicate);
/**
 * @brief
 * Process command-line arguments specific to the ADD/REMOVE actions when
 * dealing with a ListRecordStore.
 *
 * @param[in] argc
 *	argc from main()
 * @param[in] argv
 *	argv from main()
 * @param[in/out] hash_rs
 *	Reference to a hash translation RecordStore shared pointer that will
 *	be allocated to hold a hash translation of the file's contents.
 * @param[in/out] files
 *	Reference to a vector that will be populated with paths to files that
 *	should be modifed from rs.
 * @param[in/out] what_to_hash
 *	Reference to a HashablePart enumeration that indicates what should be
 *	hashed when creating a hash for an entry.
 * @param[in/out] hashed_key_format
 *	Reference to a KeyFormat enumeration that indicates how the key (when
 *	printed as a value) should appear in a hash translation RecordStore.
 * @param[in] force
 *	For REMOVE, whether or not to prompt for each key (no effect on
 *	ADD).
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
int
procargs_modifyListRecordStore(
    int argc,
    char *argv[],
    std::shared_ptr<BiometricEvaluation::IO::RecordStore> &hash_rs,
    std::vector<std::string> &files,
    HashablePart &what_to_hash,
    bool &force);

/**
 * @brief
 * Facilitate adding and removing keys from a ListRecordStore's KeyList.
 *
 * @param[in] argc
 *	argc from main()
 * @param[in] argv
 *	argv from main()
 * @param[in] action
 *	The action to perform.
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 *
 * @note
 *	Any Action other than ADD or REMOVE will result in an internal
 *	inconsistency.
 */
int
modifyListRecordStore(
    int argc,
    char *argv[],
    Action action);

/**
 * @brief
 * Facilitates the addition of files to an existing RecordStore.
 * 
 * @param[in] argc
 *	argc from main()
 * @param[in] argv
 *	argv from main()
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
int
add(
    int argc,
    char *argv[]);

/**
 * @brief
 * Process command-line arguments specific to the REMOVE Action.
 *
 * @param[in] argc
 *	argc from main().
 * @param[in] argv
 *	argv from main().
 * @param[in/out] keys
 *	Reference to a vector that will hold the keys to remove.
 * @param[in/out] force_removal
 *	Reference to a boolean that when true will not prompt before removal.
 * @param[in/out] rs
 *	Reference to a shared pointer that will hold the open RecordStore.
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
int
procargs_remove(
    int argc,
    char *argv[],
    std::vector<std::string> &keys,
    bool &force_removal,
    std::shared_ptr<BiometricEvaluation::IO::RecordStore> &rs);

/**
 * @brief
 * Facilitates the removal of key/value pairs from an existing RecordStore.
 * 
 * @param[in] argc
 *	argc from main()
 * @param[in] argv
 *	argv from main()
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
int
remove(
    int argc,
    char *argv[]);

/**
 * @brief
 * Process command-line arguments specific to the RENAME Action.
 *
 * @param[in] argc
 *	argc from main()
 * @param[in] argv
 *	argv from main()
 * @param[in/out] newName
 *	Reference to a string that will hold the new name desired for the
 *	existing RecordStore.
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
int
procargs_rename(
    int argc,
    char *argv[],
    std::string &newName);

/**
 * @brief
 * Facilitates changing the name of a RecordStore.
 *
 * @param[in] argc
 *	argc from main()
 * @param[in] argv
 *	argv from main()
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
int
rename(
    int argc,
    char *argv[]);

/**
 * @brief
 * Process command-line arguments specific to the DIFF Action.
 *
 * @param[in] argc
 *	argc from main().
 * @param[in] argv
 *	argv from main().
 * @param[in/out] sourceRS
 *	Reference to a shared pointer that will hold the open
 *	source-RecordStore.
 * @param[in/out] targetRS
 *	Reference to a shared pointer that will hold the open 
 *	target-RecordStore.
 * @param[in/out] keys
 *	Reference to a vector that will hold the keys to compare.
 * @param[in/out] byte_for_byte
 *	Reference to a boolean that, when true, will perform the difference by
 *	comparing buffers byte-for-byte instead of using an MD5 checksum.
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
int
procargs_diff(
    int argc,
    char *argv[],
    std::shared_ptr<BiometricEvaluation::IO::RecordStore> &sourceRS,
    std::shared_ptr<BiometricEvaluation::IO::RecordStore> &targetRS,
    std::vector<std::string> &keys,
    bool &byte_for_byte);


/**
 * @brief
 * Explain in English what's happening with the complicated "make" options.
 *
 * @param[in] argc
 *	argc from main().
 * @param[in] argv
 *	argv from main().
 * @param[in] kind
 *	Type of RecordStore being created
 * @param[in] what_to_hash
 *	What to hash for keys (name, contents, etc)
 * @param[in] hashed_key_format
 *	Values inserted into the hashed translation RecordStore
 * @param[in] hash_filename
 *	Name of the hash translation RecordStore
 * @param[in] textProvided
 *	Whether or not a text file of paths was provided
 * @param[in] dirProvided
 *	Whether or not a directory of files was provided
 * @param[in] otherProvided
 *	Whether or not individual foles were provided
 * @param[in] compress
 *	Whether or not this is a compressed RecordStore
 * @param[in] compressorKind
 *	The kind of compression used.
 * @param[in] stopOnDuplicate
 *	Whether or not to stop on duplicate keys.
 */
int
makeHumanConfirmation(
    int argc,
    char *argv[],
    BiometricEvaluation::IO::RecordStore::Kind kind,
    HashablePart what_to_hash,
    KeyFormat hashed_key_format,
    const std::string &hash_filename,
    bool textProvided,
    bool dirProvided,
    bool otherProvided,
    bool compress,
    BiometricEvaluation::IO::Compressor::Kind compressorKind,
    bool stopOnDuplicate);

/**
 * @brief
 * Facilitates a diff between two RecordStores.
 * 
 * @param[in] argc
 *	argc from main()
 * @param[in] argv
 *	argv from main()
 *
 * @return
 *	An exit status, either EXIT_SUCCESS or EXIT_FAILURE, that can be
 *	returned from main().
 */
int
diff(
    int argc,
    char *argv[]);

#endif /* __RSTOOL_H__ */
