#!/usr/bin/env python
# This software was developed at the National Institute of Standards and
# Technology (NIST) by employees of the Federal Government in the course
# of their official duties. Pursuant to title 17 Section 105 of the
# United States Code, this software is not subject to copyright protection
# and is in the public domain. NIST assumes no responsibility whatsoever for
# its use by other parties, and makes no guarantees, expressed or implied,
# about its quality, reliability, or any other characteristic.
 
import sys
import unittest

sys.path.insert(0, '../')
import BiometricEvaluation as BE

RSNAME = "rs_test"

class NewRecordStoreTest(unittest.TestCase):
	def setUp(self):
		self.rs = BE.RecordStore(path = RSNAME, rstype = BE.RecordStore_Default, description = "RW Test")

	def test_CRUD(self):
		key = "firstRec"
		wdata = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		wlen = len(wdata)
	
		# Insert one record
		self.rs.insert(key, wdata)
		self.rs.flush(key)
		self.assertEqual(1, self.rs.count())
		self.assertTrue(self.rs.contains_key(key))
	
		# Don't allow insertion of duplicate records
		self.assertRaises(Exception, self.rs.insert, (key, wdata))
		self.assertEqual(1, self.rs.count())

		# Read test
		rdata = self.rs.read(key)
		self.assertEqual(wlen, len(rdata))
		self.assertEqual(1, self.rs.count())
		self.assertEqual(rdata, wdata)

		# Replace test
		wdata = "ZYXWVUTSRQPONMLKJIHGFEDCBA0123456789";
		wlen = len(wdata)
		self.rs.replace(key, wdata)
		self.assertEqual(1, self.rs.count())

		# Read replacement
		rdata = self.rs.read(key)
		self.assertEqual(wlen, len(rdata))
		self.assertEqual(1, self.rs.count())
		self.assertEqual(rdata, wdata)

		# Remove test
		self.rs.remove(key)
		self.assertEqual(0, self.rs.count())

		# Read deleted record
		self.assertRaises(Exception, self.rs.read, (key))
	
	def test_sequence(self):
		# Insert some data
		for i in range(1,10):
			self.rs.insert("key" + str(i), str(i))

		# Iterate
		lastVal = 1;
		for kv in self.rs:
			self.assertEqual(kv.keys()[0], "key" + str(lastVal))
			self.assertEqual(kv.values()[0], str(lastVal))
			lastVal += 1
		self.assertEqual(lastVal, 10)

		# Remove a key and iterate
		self.rs.remove("key3")
		lastVal = 1
		for kv in self.rs:
			if lastVal < 3:
				self.assertEqual(kv.keys()[0], "key" + str(lastVal))
				self.assertEqual(kv.values()[0], str(lastVal))
			else:
				self.assertEqual(kv.keys()[0], "key" + str(lastVal + 1))
				self.assertEqual(kv.values()[0], str(lastVal + 1))
			lastVal += 1
		self.assertEqual(lastVal, 9)
	
	def zero_length(self):
		key = "key"

		# Add zero-length record
		self.rs.insert(key, None)

		# Read zero-length record
		value = self.rs.read(key)
		self.assertEqual(None, value)

		# Length of zero length record
		self.assertEqual(0, self.rs.length(key))

		# Remove zero-length record
		self.rs.remove(key)

	def test_nonexistents(self):
		bad_key = "l;kdfgkljdsfklgs"

		# Remove
		self.assertRaises(Exception, self.rs.remove, bad_key)
		# Replace
		self.assertRaises(Exception, self.rs.replace, {"value":None, "key":bad_key})
		# Read 
		self.assertRaises(Exception, self.rs.read, bad_key)
		# Length
		self.assertRaises(Exception, self.rs.length, bad_key)
		# Flush
		self.assertRaises(Exception, self.rs.flush, bad_key)
		# Contains key
		self.assertFalse(self.rs.contains_key(bad_key))
	
	def test_key_format(self):
		self.assertRaises(Exception, self.rs.insert, {"key":"/Slash/"})
		self.assertRaises(Exception, self.rs.insert, {"key":"\\Back\\slash"})
		self.assertRaises(Exception, self.rs.insert, {"key":"*Asterisk*"})
		self.assertRaises(Exception, self.rs.insert, {"key":"&Ampersand&"})
	
	def test_description(self):
		description = "Changed the description"
		self.assertNotEqual(description, self.rs.description())
		self.rs.change_description(description)
		self.assertEqual(description, self.rs.description())

	def test_other(self):
		self.assertNotEqual(0, self.rs.space_used())
		self.rs.sync()

	def tearDown(self):
		self.rs = None
		BE.RecordStore.delete(RSNAME)

if __name__ == '__main__':
	unittest.main()

