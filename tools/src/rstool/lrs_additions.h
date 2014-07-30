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
 * lrs_additions - special handling for ListRecordStores to get around their
 *                 inherent read-only nature.
 */

#ifndef __RSTOOL_LRS_ADDITIONS_H__
#define __RSTOOL_LRS_ADDITIONS_H__

#include <string>

#include <memory>

#include <be_io_listrecstore.h>

#include <ordered_set.h>

/**
 * @brief
 * Determine if a directory on disk is a ListRecordStore.
 *
 * @param rsPath
 *	Path to the RecordStore in question.
 *
 * @return
 *	Whether or not the given path is a ListRecordStore.
 */
bool
isListRecordStore(
    const std::string &rsPath);

/**
 * @brief
 * Create a ListRecordStore.
 *
 * @param lrsPath
 *	Pathame of the ListRecordStore to create.
 * @param rsPath
 *	Path to existing RecordStore pointed to by the newly created
 *	ListRecordStore.
 *
 * @throw Error::ObjectDoesNotExist
 *	No RecordStore at rsPath.
 * @throw Error::ObjectExists
 *	lrsName already exists within lrsDir.
 * @throw Error::StrategyError
 *	System error while creating ListRecordStore.
 */
void
constructListRecordStore(
    std::string lrsPath,
    std::string rsPath)
    throw (BiometricEvaluation::Error::ObjectDoesNotExist,
    BiometricEvaluation::Error::ObjectExists,
    BiometricEvaluation::Error::StrategyError);

/**
 * @brief
 * Update the Count Property for a ListRecordStore.
 *
 * @param rsPath
 *	Path to the ListRecordStore to update.
 * @param newCount
 *	Value to set for the Count Property.
 *
 * @throw Error::StrategyError
 *	rsPath is not a ListRecordStore.
 */
void
updateListRecordStoreCount(
    const std::string &rsPath,
    uint64_t newCount)
    throw (BiometricEvaluation::Error::StrategyError);

/**
 * @brief
 * Read keys from a ListRecordStore into a list of strings.
 *
 * @param[in] rsPath
 *	Path to the RecordStore in question.
 * @param[out] srs
 *	Pointer to the open Source RecordStore of the ListRecordStore.
 * @param[out] keys
 *	Pointer to a list of the string keys from the ListRecordStore.
 *
 * @throw Error::FileError
 *	File error condition (name in information string).
 * @throw Error::StrategyError
 *	Errors manipulating RecordStores.
 */
void
readListRecordStoreKeys(
    const std::string &rsPath,
    std::shared_ptr<BiometricEvaluation::IO::RecordStore> &srs,
    std::shared_ptr<OrderedSet<std::string>> &keys)
    throw (BiometricEvaluation::Error::FileError,
    BiometricEvaluation::Error::StrategyError);

/**
 * @brief
 * Write keys from a list of strings to a ListRecordStore's KeyList.
 *
 * @param[in] rsPath
 *	Path to the RecordStore in question.
 * @param[out] keys
 *	Pointer to a list of the string keys from the ListRecordStore.
 *
 * @throw Error::FileError
 *	File error condition (name in information string).
 */
void
writeListRecordStoreKeys(
    const std::string &rsPath,
    const std::shared_ptr<OrderedSet<std::string>> &keys)
    throw (BiometricEvaluation::Error::FileError);

/**
 * @brief
 * Insert keys into the KeyList of a ListRecordStore.
 *
 * @param rsPath
 *	Path to the ListRecordStore in which a key should be inserted.
 * @param key
 *	The keys to insert into the ListRecordStore.
 *
 * @throw Error::FileError
 *	File error condition (name in information string).
 * @throw Error::ObjectDoesNotExist
 *	key does not exist in the ListRecordStore's source RecordStore.
 * @throw Error::ObjectExists
 *	key already exists in ListRecordStore.
 * @throw Error::StrategyError
 *	Invalid key format or errors manipulating RecordStores.
 *
 * @note
 *	This is not an atomic operation.  All possible keys will be inserted,
 *	and any keys that could not be inserted will be listed in an 
 *	exception's information string.
 */
void
insertKeysIntoListRecordStore(
    const std::string &rsPath,
    std::shared_ptr<OrderedSet<std::string>> keys)
    throw (BiometricEvaluation::Error::FileError,
    BiometricEvaluation::Error::ObjectDoesNotExist,
    BiometricEvaluation::Error::ObjectExists,
    BiometricEvaluation::Error::StrategyError);

/**
 * @brief
 * Remove a key from the KeyList of a ListRecordStore.
 *
 * @param rsPath
 *	Path to the ListRecordStore in which a key should be inserted.
 * @param keys
 *	The keys to remove from the ListRecordStore.
 *
 * @throw Error::FileError
 *	File error condition (name in information string).
 * @throw Error::StrategyError
 *	Invalid key format or errors manipulating RecordStores.
 */
void
removeKeysFromListRecordStore(
    const std::string &rsPath,
    std::shared_ptr<OrderedSet<std::string>> keys)
    throw (BiometricEvaluation::Error::FileError,
    BiometricEvaluation::Error::StrategyError);

#endif /* __RSTOOL_LRS_ADDITIONS_H__ */
