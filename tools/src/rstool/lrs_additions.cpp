/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */

#include <be_error.h>
#include <be_io_propertiesfile.h>
#include <be_io_utility.h>
#include <be_text.h>

#include <lrs_additions.h>
#include <ordered_set.h>

using namespace BiometricEvaluation;

bool
isListRecordStore(
    const string &pathToRecordStore)
{
	/* RecordStore existance */
	string rsPath;
	if (!IO::Utility::constructAndCheckPath(
	    Text::filename(pathToRecordStore),
	    Text::dirname(pathToRecordStore), rsPath))
		return (false);
	
	/* RecordStore control file existance */
	string filePath;
	if (!IO::Utility::constructAndCheckPath(
	    IO::RecordStore::CONTROLFILENAME, rsPath, filePath))
		return (false);
	
	/* RecordStore control file lists a RecordStore type of LISTTYPE */
	try {
		IO::PropertiesFile props(filePath, IO::READONLY);
		if (props.getProperty(IO::RecordStore::TYPEPROPERTY) !=
		    IO::RecordStore::LISTTYPE)
			return (false);
	} catch (Error::Exception) {
		return (false);
	}
	
	/* KeyList file existance */
	if (!IO::Utility::constructAndCheckPath(
	    IO::ListRecordStore::KEYLISTFILENAME, rsPath, filePath))
		return (false);
			
	/* Fairly confident it's a ListRecordStore at this point */
	return (true);
}

void
updateListRecordStoreCount(
    const string &pathToRecordStore,
    uint64_t newCount)
    throw (Error::StrategyError)
{
	if (isListRecordStore(pathToRecordStore) == false)
		throw Error::StrategyError(pathToRecordStore + " is not "
		    " a ListRecordStore");
	
	string propsFilePath;
	if (!IO::Utility::constructAndCheckPath(
	    IO::RecordStore::CONTROLFILENAME, pathToRecordStore, propsFilePath))
		throw Error::StrategyError(propsFilePath + " does not exist");
	IO::PropertiesFile props(propsFilePath);
	props.setPropertyFromInteger(IO::RecordStore::COUNTPROPERTY, newCount);
	props.sync();
}

void
constructListRecordStore(
    string lrsName,
    string lrsDir,
    string rsPath)
    throw (Error::ObjectDoesNotExist,
    Error::ObjectExists,
    Error::StrategyError)
{
	/* Make sure rsPath is actually a RecordStore (exceptions float out) */
	tr1::shared_ptr<IO::RecordStore> rs =
	    IO::RecordStore::openRecordStore(Text::filename(rsPath),
	    Text::dirname(rsPath), IO::READONLY);

	/* LRS directory */
	string lrsPath;
	if (IO::Utility::constructAndCheckPath(lrsName, lrsDir, lrsPath))
		throw Error::ObjectExists(lrsPath);
	if (mkdir(lrsPath.c_str(), S_IRWXU | S_IRWXG | S_IRWXO))
		throw Error::StrategyError("Could not create " + lrsPath +
		    Error::errorStr());
	    
	/* LRS Properties file */
	string controlFilePath;
	if (IO::Utility::constructAndCheckPath(
	    IO::RecordStore::CONTROLFILENAME, lrsPath, controlFilePath))
		throw Error::ObjectExists(controlFilePath);
	string absoluteRSPath;
	if (!IO::Utility::constructAndCheckPath(
	    Text::filename(rsPath), Text::dirname(rsPath), absoluteRSPath))
		throw Error::ObjectDoesNotExist(absoluteRSPath);
		
	IO::PropertiesFile props(controlFilePath);
	props.setPropertyFromInteger(IO::RecordStore::COUNTPROPERTY, 0);
	props.setProperty(IO::RecordStore::NAMEPROPERTY,
	Text::filename(lrsPath));
	props.setProperty(IO::RecordStore::DESCRIPTIONPROPERTY,
	    "<Description>");
	props.setProperty(IO::RecordStore::TYPEPROPERTY,
	    IO::RecordStore::LISTTYPE);
	props.setProperty(IO::ListRecordStore::SOURCERECORDSTOREPROPERTY,
	    absoluteRSPath);
	props.sync();
	
	/* KeyList file */
	string keyListFile;
	if (IO::Utility::constructAndCheckPath(
	    IO::ListRecordStore::KEYLISTFILENAME, lrsPath, keyListFile))
		throw Error::ObjectExists(keyListFile);
	IO::Utility::writeFile(NULL, 0, keyListFile);
}

void
readListRecordStoreKeys(
    const string &pathToRecordStore,
    tr1::shared_ptr<IO::RecordStore> &srs,
    tr1::shared_ptr< OrderedSet<string> > &existingKeys)
    throw (Error::FileError,
    Error::StrategyError)
{
	if (isListRecordStore(pathToRecordStore) == false)
		throw Error::StrategyError(pathToRecordStore + " is not "
		    "a ListRecordStore");

	/* Open Source RecordStore */
	string controlFilePath;
	if (!IO::Utility::constructAndCheckPath(
	    IO::RecordStore::CONTROLFILENAME, pathToRecordStore,
	    controlFilePath)) {
		throw Error::FileError("Error opening " +
		    IO::RecordStore::CONTROLFILENAME);
	}
	tr1::shared_ptr<IO::PropertiesFile> props(
	    new IO::PropertiesFile(controlFilePath, IO::READONLY));
	string sourceRSPath;
	try {
		sourceRSPath = props->getProperty(
		    IO::ListRecordStore::SOURCERECORDSTOREPROPERTY);
	} catch (Error::Exception &e) {
		throw Error::StrategyError("Could not read " +
		    IO::ListRecordStore::SOURCERECORDSTOREPROPERTY +
		    " property (" + e.getInfo() + ")");
	}
	try {
		srs = IO::RecordStore::openRecordStore(
		    Text::filename(sourceRSPath),
		    Text::dirname(sourceRSPath),
		    IO::READONLY);
	} catch (Error::Exception &e) {
		throw Error::StrategyError("Could not open source "
		    "RecordStore " + sourceRSPath + " (" + e.getInfo() + ")");
	}
	
	/* Open KeyList */
	string keyListFilePath;
	if (!IO::Utility::constructAndCheckPath(
	    IO::ListRecordStore::KEYLISTFILENAME, pathToRecordStore,
	    keyListFilePath))
		throw Error::FileError("Cannot find " +
		    IO::ListRecordStore::KEYLISTFILENAME);
	std::fstream keyListFile(keyListFilePath.c_str());
	if (keyListFile.is_open() == false || !keyListFile)
		throw Error::FileError("Error opening " +
		    IO::ListRecordStore::KEYLISTFILENAME);
	
	/* Read existing keys */
	try {
		existingKeys.reset(new OrderedSet<string>());
	} catch (Error::Exception &e) {
		keyListFile.close();
		throw Error::StrategyError("Could not allocate key list (" +
		    e.getInfo() + ")");
	}
	string line;
	for (;;) {
		std::getline(keyListFile, line);
		if (keyListFile.eof())
			break;
		if (!keyListFile) {
			keyListFile.close();
			throw Error::FileError("Error reading " +
			    IO::ListRecordStore::KEYLISTFILENAME);
		}
			
		Text::removeLeadingTrailingWhitespace(line);
		existingKeys->push_back(line);
	}
	keyListFile.close();
}

void
writeListRecordStoreKeys(
    const string &pathToRecordStore,
    const tr1::shared_ptr< OrderedSet<string> > &keys)
    throw (Error::FileError)
{
        /* Write key list to temporary file */
	string newListPath;
	try {
		newListPath = IO::Utility::createTemporaryFile(
		    IO::ListRecordStore::KEYLISTFILENAME);
	} catch (Error::MemoryError &e) {
		throw Error::FileError(e.getInfo());
	}
	
        std::ofstream newListStream(newListPath.c_str());
        if (!newListStream)
                throw Error::FileError("Could not open " + newListPath);

        for (OrderedSet<string>::const_iterator i = keys->begin();
	    i != keys->end(); i++) {
                newListStream << *i << '\n';
                if (!newListStream)
                        throw Error::FileError("Could not write " +
                            newListPath);
        }

        newListStream.close();
        if (!newListStream)
                throw Error::FileError("Could not close " + newListPath);

        /* Atomically replace contents of key list with temporary file */
	string existingListPath;
	IO::Utility::constructAndCheckPath(IO::ListRecordStore::KEYLISTFILENAME,
	    pathToRecordStore, existingListPath);
        if (std::rename(newListPath.c_str(), existingListPath.c_str()) != 0)
                throw Error::FileError("Could not replace key list: " +
                    Error::errorStr());
		    
	updateListRecordStoreCount(pathToRecordStore, keys->size());
}

void
insertKeysIntoListRecordStore(
    const string &pathToRecordStore,
    tr1::shared_ptr< OrderedSet<string> > keys)
    throw (Error::FileError,
    Error::ObjectDoesNotExist,
    Error::ObjectExists,
    Error::StrategyError)
{
	tr1::shared_ptr< OrderedSet<string> > existingKeys;
	tr1::shared_ptr<IO::RecordStore> srs;
	readListRecordStoreKeys(pathToRecordStore, srs, existingKeys);
	
	tr1::shared_ptr<IO::RecordStore> lrs =
	    IO::RecordStore::openRecordStore(Text::filename(pathToRecordStore),
	    Text::dirname(pathToRecordStore), IO::READONLY);
	
	/* Add unique keys */
	string badKeys;
	for (OrderedSet<string>::const_iterator it = keys->begin();
	    it != keys->end(); it++) {
		if (!srs->containsKey(*it))
			badKeys += *it + ", ";
		else
			existingKeys->push_back(*it);
	}
	
	/* Write modified key list */
	writeListRecordStoreKeys(pathToRecordStore, existingKeys);
		    
	/* Report any keys that could not be inserted (removing last ", ") */
	if (badKeys.empty() == false)
		throw Error::ObjectDoesNotExist(badKeys.substr(0,
		    badKeys.size() - 2));
}

void
removeKeysFromListRecordStore(
    const string &pathToRecordStore,
    tr1::shared_ptr< OrderedSet<string> > keys)
    throw (Error::FileError,
    Error::StrategyError)
{
	tr1::shared_ptr< OrderedSet<string> > existingKeys;
	tr1::shared_ptr<IO::RecordStore> srs;
	readListRecordStoreKeys(pathToRecordStore, srs, existingKeys);
	
	/* Remove keys */
	for (OrderedSet<string>::const_iterator it = keys->begin();
	    it != keys->end(); it++)
		existingKeys->erase(*it);
	
	/* Write modified key list */
	writeListRecordStoreKeys(pathToRecordStore, existingKeys);
}
