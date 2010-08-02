/******************************************************************************
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 ******************************************************************************/
#ifndef __BE_DBRECSTORE_H__
#define __BE_DBRECSTORE_H__

#include <string>
#include <vector>
#include <be_recordstore.h>

#ifdef DB1X
#include <db_185.h>
#else
#include <db.h>
#endif

using namespace std;

/*
 * This file contains the class declaration for an implementation of a
 * RecordStore using a on-disk database.
 */
namespace BiometricEvaluation {

	/*
	 * Class to represent the data storage mechanism.
	 */
	class DBRecordStore : public RecordStore {
		public:
			
			/*
			 * Construct a new DB record store.
			 */
			DBRecordStore(
			    const string &name,
			    const string &description,
			    const string &parentDir)
			    throw (ObjectExists, StrategyError);

			/*
			 * Construct an existing DB record store.
			 */
			DBRecordStore(
			    const string &name,
			    const string &parentDir)
			    throw (ObjectDoesNotExist, StrategyError);

			/*
			 * Destructor.
			 */
			~DBRecordStore();

			uint64_t getSpaceUsed()
			    throw (StrategyError);

			void sync()
			    throw (StrategyError);

			void insert(
			    const string &key,
			    const void *const data,
			    const uint64_t size)
			    throw (ObjectExists, StrategyError);

			void remove(
			    const string &key)
			    throw (ObjectDoesNotExist, StrategyError);

			uint64_t read(
			    const string &key,
			    void *const data)
			    throw (ObjectDoesNotExist, StrategyError);

			virtual void replace(
			    const string &key,
			    const void *const data,
			    const uint64_t size)
			    throw (ObjectDoesNotExist, StrategyError);

			virtual uint64_t length(
			    const string &key)
			    throw (ObjectDoesNotExist, StrategyError);

			void flush(
			    const string &key)
			    throw (ObjectDoesNotExist, StrategyError);

			uint64_t sequence(
			    string &key,
			    void *const data,
			    int cursor = BE_RECSTORE_SEQ_NEXT)
			    throw (ObjectDoesNotExist, StrategyError);

			void changeName(
			    const string &name)
			    throw (ObjectExists, StrategyError);

		protected:

		private:
			/* The file name of the underlying database */
			string _dbname;
			/* The handle to the underlying database */
			DB *_db;
			void internalRead(
			    const string &key,
			    DBT *dbtdata)
			    throw (ObjectDoesNotExist, StrategyError);
	};
}
#endif	/* __BE_DBRECSTORE_H__ */
