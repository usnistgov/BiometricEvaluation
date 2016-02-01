\name{RecordStore-package}
\alias{RecordStore-package}
\alias{RecordStore}
\docType{package}
\title{
\packageTitle{RecordStore}
}
\description{
\packageDescription{RecordStore}
}
\details{

The DESCRIPTION file:
\packageDESCRIPTION{RecordStore}
\packageIndices{RecordStore}
}
\author{
\packageAuthor{RecordStore}

Maintainer: \packageMaintainer{RecordStore}
}
\references{
Build documentation in https://github.com/usnistgov/BiometricEvaluation/tree/master/doccommon for more information
}
\keyword{ package }
\examples{
# Open the RecordStore instance
rs <- RecordStore::openRecordStore("templates")

# Get names of all keys without reading data
keys <- RecordStore::readAll(rs)$key

# Copy all key/data pairs into R
data <- RecordStore::readAll(rs, readData = TRUE)

# Data is a list of a list of keys (character) and a list of data (raw)
names(data)
[1] "key"  "data"

# Access the 10th element
tenthKey <- data$key[[10]]
tenthData <- data$data[[10]]
# Access by key
tenthData <- data$data[[tenthKey]]

# Read just one key
key1 <- RecordStore::read(rs, "foo")

# Close RecordStore files early (automatically closed when R exits)
RecordStore::closeRecordStore(rs)
# Variable 'rs' is now undefined
remove(rs)

#
# Other methods
#

# Size of RecordStore on disk
bytes <- RecordStore::getSpaceUsed(rs)

# Whether or not key exists in RecordStore
keyExists <- RecordStore::containsKey(rs, "foo")

# The size of the data stored in RecordStore for key
size <- RecordStore::lengthOfKey(rs, "foo")

# Path passed to openRecordStore
pathname <- RecordStore::getPathname(rs)

# Description of RecordStore
description <- RecordStore::getDescription(rs)

# Number of entries in RecordStore
count <- RecordStore::getCount(rs)

}
