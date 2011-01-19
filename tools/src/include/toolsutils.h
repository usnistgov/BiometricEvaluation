/*
* This software was developed at the National Institute of Standards and
* Technology (NIST) by employees of the Federal Government in the course
* of their official duties. Pursuant to title 17 Section 105 of the
* United States Code, this software is not subject to copyright protection
* and is in the public domain. NIST assumes no responsibility whatsoever for
* its use by other parties, and makes no guarantees, expressed or implied,
* about its quality, reliability, or any other characteristic.
*/
#ifndef __TOOLSUTILS_H__
#define __TOOLSUTILS_H__

#define OPEN_ERR_OUT(fn)						\
	do {								\
		fprintf(stderr, "Could not open file %s: ", fn);	\
		fprintf(stderr, "%s\n", strerror(errno));		\
		goto err_out;						\
	} while (0)
	
#define READ_ERR_OUT(...)						\
	do {								\
		fprintf(stderr, "Error reading ");			\
		fprintf(stderr, __VA_ARGS__);				\
		fprintf(stderr, " (line %d in %s).\n", __LINE__, __FILE__);\
		goto err_out;						\
	} while (0)

#define ERR_OUT(...)							\
	do {								\
		fprintf(stderr, "ERROR: ");				\
		fprintf(stderr, __VA_ARGS__);				\
		fprintf(stderr, " (line %d in %s).\n", __LINE__, __FILE__);\
		goto err_out;						\
	} while (0)
	
#define ERR_EXIT(...)							\
	do {								\
		fprintf(stderr, "ERROR: ");				\
		fprintf(stderr, __VA_ARGS__);				\
		fprintf(stderr, " (line %d in %s).\n", __LINE__, __FILE__);\
		exit(EXIT_FAILURE);					\
	} while (0)
	
#endif
