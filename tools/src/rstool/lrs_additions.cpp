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
#include <be_io_recordstore.h>
#include <be_text.h>

#include <lrs_additions.h>
#include <ordered_set.h>

namespace BE = BiometricEvaluation;

bool
isListRecordStore(
    const std::string &rsPath)
{
	/* RecordStore existance */
	if (!BE::IO::Utility::fileExists(rsPath))
		return (false);
	
	/* RecordStore control file existence */
	std::string filePath = rsPath + '/' +
	    BE::IO::RecordStore::CONTROLFILENAME;;
	if (!BE::IO::Utility::fileExists(filePath))
		return (false);
	
	/* RecordStore control file lists a RecordStore type of LISTTYPE */
	try {
		BE::IO::PropertiesFile props(filePath, BE::IO::Mode::ReadOnly);
		if (props.getProperty(BE::IO::RecordStore::TYPEPROPERTY) !=
		    to_string(BE::IO::RecordStore::Kind::List))
			return (false);
	} catch (BE::Error::Exception) {
		return (false);
	}
	
	/* KeyList file existance */
	filePath = rsPath + '/' + BE::IO::ListRecordStore::KEYLISTFILENAME;
	if (!BE::IO::Utility::fileExists(filePath))
		return (false);
			
	/* Fairly confident it's a ListRecordStore at this point */
	return (true);
}

void
updateListRecordStoreCount(
    const std::string &rsPath,
    uint64_t newCount)
    throw (BiometricEvaluation::Error::StrategyError)
{
	if (isListRecordStore(rsPath) == false)
		throw BE::Error::StrategyError(rsPath + " is not "
		    " a ListRecordStore");
	
	std::string propsFilePath = rsPath + '/' +
	    BE::IO::RecordStore::CONTROLFILENAME;

	BE::IO::PropertiesFile props(propsFilePath);
	props.setPropertyFromInteger(BE::IO::RecordStore::COUNTPROPERTY,
	     newCount);
	props.sync();
}

void
constructListRecordStore(
    std::string lrsPath,
    std::string rsPath)
    throw (BiometricEvaluation::Error::ObjectDoesNotExist,
    BiometricEvaluation::Error::ObjectExists,
    BiometricEvaluation::Error::StrategyError)
{
	/* Make sure rsPath is actually a RecordStore (exceptions float out) */
	std::shared_ptr<BE::IO::RecordStore> rs =
	    BE::IO::RecordStore::openRecordStore(rsPath, BE::IO::Mode::ReadOnly);

	/* LRS directory */
	if (BE::IO::Utility::fileExists(lrsPath))
		throw BE::Error::ObjectExists(lrsPath);

	if (mkdir(lrsPath.c_str(), S_IRWXU | S_IRWXG | S_IRWXO))
		throw BE::Error::StrategyError("Could not create " + lrsPath +
		    BE::Error::errorStr());
	    
	/* LRS Properties file */
	std::string controlFilePath = lrsPath + '/' +
	    BE::IO::RecordStore::CONTROLFILENAME;

	BE::IO::PropertiesFile props(controlFilePath);
	props.setPropertyFromInteger(BE::IO::RecordStore::COUNTPROPERTY, 0);
	props.setProperty(BE::IO::RecordStore::DESCRIPTIONPROPERTY,
	    "<Description>");
	props.setProperty(BE::IO::RecordStore::TYPEPROPERTY,
	    to_string(BE::IO::RecordStore::Kind::List));
	props.setProperty(BE::IO::ListRecordStore::SOURCERECORDSTOREPROPERTY,
	    rsPath);
	props.sync();
	
	/* KeyList file */
	std::string keyListFile = lrsPath + '/' +
	    BE::IO::ListRecordStore::KEYLISTFILENAME;;
	BE::IO::Utility::writeFile(NULL, 0, keyListFile);
}

void
readListRecordStoreKeys(
    const std::string &rsPath,
    std::shared_ptr<BiometricEvaluation::IO::RecordStore> &srs,
    std::shared_ptr<OrderedSet<std::string>> &existingKeys)
    throw (BiometricEvaluation::Error::FileError,
    BiometricEvaluation::Error::StrategyError)
{
	if (isListRecordStore(rsPath) == false)
		throw BE::Error::StrategyError(rsPath + " is not "
		    "a ListRecordStore");

	/* Open Source RecordStore */
	std::string controlFilePath = rsPath + '/' +
	    BE::IO::RecordStore::CONTROLFILENAME;;
	std::shared_ptr<BE::IO::PropertiesFile> props(
	    new BE::IO::PropertiesFile(controlFilePath, BE::IO::Mode::ReadOnly));

	std::string sourceRSPath;
	try {
		sourceRSPath = props->getProperty(
		    BE::IO::ListRecordStore::SOURCERECORDSTOREPROPERTY);
	} catch (BE::Error::Exception &e) {
		throw BE::Error::StrategyError("Could not read " +
		    BE::IO::ListRecordStore::SOURCERECORDSTOREPROPERTY +
		    " property (" + e.what() + ")");
	}
	try {
		srs = BE::IO::RecordStore::openRecordStore(
		    std::string(sourceRSPath), BE::IO::Mode::ReadOnly);
	} catch (BE::Error::Exception &e) {
		throw BE::Error::StrategyError("Could not open source "
		    "RecordStore " + sourceRSPath + " (" + e.what() + ")");
	}
	
	/* Open KeyList */
	std::string keyListFilePath = rsPath + '/' +
	    BE::IO::ListRecordStore::KEYLISTFILENAME;
	std::fstream keyListFile(keyListFilePath.c_str());
	if (keyListFile.is_open() == false || !keyListFile)
		throw BE::Error::FileError("Error opening " +
		    BE::IO::ListRecordStore::KEYLISTFILENAME);
	
	/* Read existing keys */
	try {
		existingKeys.reset(new OrderedSet<std::string>());
	} catch (BE::Error::Exception &e) {
		keyListFile.close();
		throw BE::Error::StrategyError("Could not allocate key list (" +
		    std::string(e.what()) + ")");
	}
	std::string line;
	for (;;) {
		std::getline(keyListFile, line);
		if (keyListFile.eof())
			break;
		if (!keyListFile) {
			keyListFile.close();
			throw BE::Error::FileError("Error reading " +
			    BE::IO::ListRecordStore::KEYLISTFILENAME);
		}
		BE::Text::removeLeadingTrailingWhitespace(line);
		existingKeys->push_back(line);
	}
	keyListFile.close();
}

void
writeListRecordStoreKeys(
    const std::string &rsPath,
    const std::shared_ptr<OrderedSet<std::string>> &keys)
    throw (BiometricEvaluation::Error::FileError)
{
        /* Write key list to temporary file */
	std::string newListPath;
	try {
		newListPath = BE::IO::Utility::createTemporaryFile(
		    BE::IO::ListRecordStore::KEYLISTFILENAME, rsPath);
	} catch (BE::Error::MemoryError &e) {
		throw BE::Error::FileError(e.what());
	}
	
        std::ofstream newListStream(newListPath.c_str());
        if (!newListStream)
                throw BE::Error::FileError("Could not open " + newListPath);

        for (OrderedSet<std::string>::const_iterator i = keys->begin();
	    i != keys->end(); i++) {
                newListStream << *i << '\n';
                if (!newListStream)
                        throw BE::Error::FileError("Could not write " +
                            newListPath);
        }

        newListStream.close();
        if (!newListStream)
                throw BE::Error::FileError("Could not close " + newListPath);

        /* Atomically replace contents of key list with temporary file */
	std::string existingListPath = rsPath +'/' +
	    BE::IO::ListRecordStore::KEYLISTFILENAME;
        if (std::rename(newListPath.c_str(), existingListPath.c_str()) != 0)
                throw BE::Error::FileError("Could not replace key list: " +
                    BE::Error::errorStr());

	updateListRecordStoreCount(rsPath, keys->size());
}

void
insertKeysIntoListRecordStore(
    const std::string &rsPath,
    std::shared_ptr<OrderedSet<std::string>> keys)
    throw (BiometricEvaluation::Error::FileError,
    BiometricEvaluation::Error::ObjectDoesNotExist,
    BiometricEvaluation::Error::ObjectExists,
    BiometricEvaluation::Error::StrategyError)
{
	std::shared_ptr<OrderedSet<std::string>> existingKeys;
	std::shared_ptr<BE::IO::RecordStore> srs;
	readListRecordStoreKeys(rsPath, srs, existingKeys);
	
	std::shared_ptr<BE::IO::RecordStore> lrs =
	    BE::IO::RecordStore::openRecordStore(
	    std::string(rsPath), BE::IO::Mode::ReadOnly);
	
	/* Add unique keys */
	std::string badKeys;
	for (OrderedSet<std::string>::const_iterator it = keys->begin();
	    it != keys->end(); it++) {
		if (!srs->containsKey(*it))
			badKeys += *it + ", ";
		else
			existingKeys->push_back(*it);
	}
	
	/* Write modified key list */
	writeListRecordStoreKeys(rsPath, existingKeys);
		    
	/* Report any keys that could not be inserted (removing last ", ") */
	if (badKeys.empty() == false)
		throw BE::Error::ObjectDoesNotExist(badKeys.substr(0,
		    badKeys.size() - 2));
}

void
removeKeysFromListRecordStore(
    const std::string &rsPath,
    std::shared_ptr<OrderedSet<std::string>> keys)
    throw (BiometricEvaluation::Error::FileError,
    BiometricEvaluation::Error::StrategyError)
{
	std::shared_ptr<OrderedSet<std::string>> existingKeys;
	std::shared_ptr<BE::IO::RecordStore> srs;
	readListRecordStoreKeys(rsPath, srs, existingKeys);
	
	/* Remove keys */
	for (OrderedSet<std::string>::const_iterator it = keys->begin();
	    it != keys->end(); it++)
		existingKeys->erase(*it);
	
	/* Write modified key list */
	writeListRecordStoreKeys(rsPath, existingKeys);
}
