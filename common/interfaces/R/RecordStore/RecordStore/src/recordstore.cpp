/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */

#include <be_io_recordstore.h>
#include <be_memory_autoarrayiterator.h>

#include <Rcpp.h>

namespace BE = BiometricEvaluation;

/** Thin wrapper around RecordStore shared pointer. */
class RecordStoreContainer
{
public:
	/**
	 * Constructor.
	 *
	 * @param path
	 * Path to RecordStore
	 *
	 * @throw Error::ObjectDoesNotExist
	 * No RecordStore at path.
	 */
	RecordStoreContainer(
	    const std::string &path) :
	    _rs(BE::IO::RecordStore::openRecordStore(path,
	        BE::IO::Mode::ReadOnly))
	{
	}

	/**
	 * @brief
	 * Obtain RecordStore shared pointer.
	 * 
	 * @return
	 * RecordStore shared pointer.
	 */
	inline std::shared_ptr<BE::IO::RecordStore>
	getRecordStore()
	    const
	{
		return (_rs);
	}

private:
	/** Pointer to RecordStore. */
	const std::shared_ptr<BE::IO::RecordStore> _rs;
};

/**
 * @brief
 * Open a RecordStore.
 *
 * @param path
 * Path to RecordStore
 *
 * @return
 * External pointer to RecordStoreContainer object.
 */
// [[Rcpp::export]]
SEXP
openRecordStore(
    const std::string path)
{
	try {
		return (Rcpp::XPtr<RecordStoreContainer>(
		    new RecordStoreContainer(path), true));
	} catch (BE::Error::Exception &e) {
		Rf_error(e.what());
	}
}

/**
 * @brief
 * Invalidate the RecordStoreContainer poiner
 * @details
 * Forces external pointer deleter, which closes the RecordStore objects
 * opened by Biometric Evaluation framework.
 * 
 * @param recordStore
 * R pointer to RecordStoreContainer
 */
// [[Rcpp::export]]
void
closeRecordStore(
    SEXP recordStore)
{
	Rcpp::XPtr<RecordStoreContainer> rsCPtr(recordStore);
	rsCPtr.release();
}

/**
 * @brief
 * Obtain real storage utilization.
 *
 * @param recordStore
 * R pointer to RecordStoreContainer.
 *
 * @return
 * The amount of backing storage used by recordStore
 */
// [[Rcpp::export]]
uint64_t
getSpaceUsed(
    SEXP recordStore)
{
	Rcpp::XPtr<RecordStoreContainer> rsCPtr(recordStore);
	return (rsCPtr->getRecordStore()->getSpaceUsed());
}

/**
 * @brief
 * Determines if the RecordStore contains an element with the specified key.
 *
 * @param recordStore
 * R pointer to RecordStoreContainer.
 * @param key
 * The key to locate.
 *
 * @return
 * TRUE if the RecordStore contains an element with the key, FALSE otherwise.
 */
// [[Rcpp::export]]
bool
containsKey(
    SEXP recordStore,
    const std::string key)
{
	Rcpp::XPtr<RecordStoreContainer> rsCPtr(recordStore);
	return (rsCPtr->getRecordStore()->containsKey(key));
}

/**
 * @brief
 * Obtain the length of a record.
 *
 * @param recordStore
 * R pointer to RecordStoreContainer.
 * @param key
 * The key of the record.
 *
 * @return
 * The length of key.
 */
// [[Rcpp::export]]
uint64_t
lengthOfKey(
    SEXP recordStore,
    const std::string key)
{
	Rcpp::XPtr<RecordStoreContainer> rsCPtr(recordStore);
	try {
		return (rsCPtr->getRecordStore()->length(key));
	} catch (BE::Error::Exception &e) {
		Rf_error(e.what());
	}
}

/**
 * @brief
 * Obtain the number of items in a RecordStore.
 *
 * @param recordStore
 * R pointer to RecordStoreContainer.
 *
 * @return
 * The number of items in recordStore.
 */
// [[Rcpp::export]]
uint64_t
getCount(
    SEXP recordStore)
{
	Rcpp::XPtr<RecordStoreContainer> rsCPtr(recordStore);
	return (rsCPtr->getRecordStore()->getCount());
}

/**
 * @brief
 * Obtain the path name of the RecordStore.
 *
 * @param recordStore
 * R pointer to RecordStoreContainer.
 *
 * @return
 * Where in the file system recordStore is located (relative).
 */
// [[Rcpp::export]]
std::string
getPathname(
    SEXP recordStore)
{
	Rcpp::XPtr<RecordStoreContainer> rsCPtr(recordStore);
	return (rsCPtr->getRecordStore()->getPathname());
}

/**
 * @brief
 * Obtain the description of a RecordStore.
 *
 * @param recordStore
 * R pointer to RecordStoreContainer.
 *
 * @return
 * Description of recordStore.
 */
// [[Rcpp::export]]
std::string
getDescription(
    SEXP recordStore)
{
	Rcpp::XPtr<RecordStoreContainer> rsCPtr(recordStore);
	return (rsCPtr->getRecordStore()->getDescription());
}

/**
 * @brief
 * Read a single record from a RecordStore.
 *
 * @param recordStore
 * R pointer to RecordStoreContainer.
 * @param key
 * Key of a record in recordStore.
 *
 * @return
 * RawVector of bytes of the record associated with key.
 */
// [[Rcpp::export]]
Rcpp::RawVector
read(
    SEXP containerFromR,
    const std::string key)
{
	Rcpp::XPtr<RecordStoreContainer> rsCPtr(containerFromR);

	try {
		const auto data = rsCPtr->getRecordStore()->read(key);
		Rcpp::RawVector rv(data.size());
		std::copy(data.cbegin(), data.cend(), rv.begin());
		return (rv);
	} catch (BE::Error::Exception &e) {
		Rf_error(e.what());
	}
}

/**
 * @brief
 * Read all keys from a RecordStore.
 *
 * @param rs
 * Shared pointer to RecordStore.
 *
 * @return
 * R list of R characters of keys.
 */
Rcpp::List
readAllKeys(
    const std::shared_ptr<BE::IO::RecordStore> &rs)
{
	uint64_t numElements = rs->getCount();
	std::vector<std::string> keys;
	keys.reserve(numElements);

	/* Reset sequence */
	try {
		keys.emplace_back(rs->sequenceKey(
		    BE::IO::RecordStore::BE_RECSTORE_SEQ_START));
	} catch (BE::Error::ObjectDoesNotExist) {
		/* Exhausted an empty list */
	} catch (BE::Error::Exception &e) {
		Rf_error(e.what());
	}

	for (uint64_t i = 1; i < numElements; i++) {
		try {
			keys.emplace_back(rs->sequenceKey(
			    BE::IO::RecordStore::BE_RECSTORE_SEQ_NEXT));
		} catch (BE::Error::ObjectDoesNotExist) {
			/* Exhausted */
			break;
		} catch (BE::Error::Exception &e) {
			Rf_error(e.what());
		}
	}

	return (Rcpp::List::create(Rcpp::Named("key") = keys));
}

/**
 * @brief
 * Read all records from a RecordStore.
 *
 * @param rs
 * Shared pointer to RecordStore.
 *
 * @return
 * R list of an R list of R characters of keys and an R list of R raws of
 * records associated with said keys.
 */
Rcpp::List
readAllKeysAndData(
    const std::shared_ptr<BE::IO::RecordStore> &rs)
{
	const uint64_t numElements = rs->getCount();

	std::vector<std::string> keys;
	keys.reserve(numElements);
	Rcpp::List data(numElements);

	BE::IO::RecordStore::Record record;
	/* Reset sequence */
	try {
		record = rs->sequence(
		    BE::IO::RecordStore::BE_RECSTORE_SEQ_START);

		keys.emplace_back(record.key);
		
		Rcpp::RawVector datum(record.data.size());
		std::copy(record.data.cbegin(), record.data.cend(),
		    datum.begin());
		data[0] = datum;
	} catch (BE::Error::ObjectDoesNotExist) {
		/* Exhausted an empty list */
	} catch (BE::Error::Exception &e) {
		Rf_error(e.what());
	}

	for (uint64_t i = 1; i < rs->getCount(); ++i) {
		try {
			record = rs->sequence(
			    BE::IO::RecordStore::BE_RECSTORE_SEQ_NEXT);

			keys.emplace_back(record.key);
			
			Rcpp::RawVector datum(record.data.size());
			std::copy(record.data.cbegin(), record.data.cend(),
			    datum.begin());
			data[i] = datum;
		} catch (BE::Error::ObjectDoesNotExist) {
			/* Exhausted */
			break;
		} catch (BE::Error::Exception &e) {
			Rf_error(e.what());
		}
	}


	Rcpp::List rv;
	rv["key"] = keys;
	data.names() = keys;
	rv["data"] = data;
	return (rv);
}

/**
 * @brief
 * Read all records from a RecordStore.
 *
 * @param rs
 * Shared pointer to RecordStore.
 * @param readData
 * Whether or not to read data as well as keys.
 *
 * @return
 * R list of an R list of R characters of keys and an R list of R raws of
 * records associated with said keys when readData is true, or an R list
 * of R characters of keys othewise.
 */
// [[Rcpp::export]]
Rcpp::List
readAll(
    SEXP recordStore,
    const bool readData = false)
{
	const Rcpp::XPtr<RecordStoreContainer> rsCPtr(recordStore);
	const std::shared_ptr<BE::IO::RecordStore> rs =
	    rsCPtr->getRecordStore();
	
	if (readData)
		return (readAllKeysAndData(rs));
	else
		return (readAllKeys(rs));
}

