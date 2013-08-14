/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */

#ifndef __ORDERED_SET_H__
#define __ORDERED_SET_H__

#include <list>
#include <set>
#include <tr1/memory>

/** A container where insertion order is preserved and elements are unique. */
template<class T>
class OrderedSet
{
public:
	typedef typename std::list<T> container;
	typedef typename container::iterator iterator;
	typedef typename container::const_iterator const_iterator;
	typedef typename container::size_type size_type;

	/** Insertion strategy */
	typedef enum {
		/**
		 * Insertions are amortized O(log(size())), but will take 
		 * twice as much RAM.
		 */
		FASTER,
		/** Uses less RAM, but insertions are amortized O(size()). */
		SMALLER
	} Strategy;
	
	/**
	 * @brief
	 * Constructor.
	 *
	 * @param strategy
	 *	The insertion strategy to use.
	 */
	OrderedSet(
	    Strategy strategy = FASTER);
	
	/**
	 * @brief
	 * Insert an element at the end of the collection.
	 *
	 * @param value
	 *	Value to insert.
	 *
	 * @return
	 *	Whether or not the object was inserted.
	 *
	 * @note
	 *	Complexity is defined by Strategy.
	 */
	bool
	push_back(
	    const T &value);
	    
	/**
	 * @brief
	 * Remove an element from the collection.
	 *
	 * @param pos
	 *	Iterator to element at the position which should be removed.
	 */
	void
	erase(
	    iterator pos);
	    
	/**
	 * @brief
	 * Remove an element from the collection.
	 *
	 * @param pos
	 *	Value of the element to remove.
	 */
	void
	erase(
	    const T &value);
	
	/**
	 * @return
	 *	Iterator at the first element of the collection.
	 */ 
	iterator
	begin();
	
	/**
	 * @return
	 *	Iterator at the first element of the collection.
	 */ 
	const_iterator
	begin()
	    const;
	
	/**
	 * @return
	 *	Iterator beyond the last element of the collection.
	 */ 
	iterator
	end();
	
	/**
	 * @return
	 *	Iterator beyond the last element of the collection.
	 */ 
	const_iterator
	end()
	    const;
	
	/**
	 * @return
	 *	Number of elements in the collection.
	 */
	size_type
	size()
	    const;
	
	/**
	 * @brief
	 * Determine if a value exists in the container.
	 *
	 * @param value
	 *	Value to search the container for.
	 *
	 * @return
	 *	Whether or not value exists in this container.
	 */
	bool
	valueExists(
	    const T &value)
	    const;

private:
	/** Insertion strategy */
	Strategy _strategy;
	/** Backing datastore for elements */
	std::tr1::shared_ptr<container> _elements;
	/** Used when strategy == FASTER to find duplicates */
	std::tr1::shared_ptr< std::set<T> > _uniqueElements;
};

template<class T>
OrderedSet<T>::OrderedSet(
    Strategy strategy) :
    _strategy(strategy),
    _elements(new container())
{
	if (strategy == FASTER)
		_uniqueElements.reset(new std::set<T>());
}

template<class T>
bool
OrderedSet<T>::push_back(
    const T &value)
{
	switch (_strategy) {
	case FASTER:
		if (_uniqueElements->insert(value).second) {
			_elements->push_back(value);
			return (true);
		} else
			return (false);
		break;
	case SMALLER:
		/* FALLTHROUGH */
	default:
		if (!this->valueExists(value)) {
			_elements->push_back(value);
			return (true);
		} else
			return (false);
		break;
	}
}

template<class T>
void
OrderedSet<T>::erase(
    OrderedSet<T>::iterator pos)
{
	if (_strategy == FASTER)
		_uniqueElements->erase(*pos);
	_elements->erase(pos);
}

template<class T>
void
OrderedSet<T>::erase(
    const T &value)
{
	if (_strategy == FASTER)
		_uniqueElements->erase(value);
		
	/* std::list's remove is actually erase */
	_elements->remove(value);
}

template<class T>
typename OrderedSet<T>::iterator
OrderedSet<T>::begin()
{
	return (_elements->begin());
}

template<class T>
typename OrderedSet<T>::const_iterator
OrderedSet<T>::begin()
    const
{
	return (_elements->begin());
}
	
template<class T>
typename OrderedSet<T>::iterator
OrderedSet<T>::end()
{
	return (_elements->end());
}

template<class T>
typename OrderedSet<T>::const_iterator
OrderedSet<T>::end()
    const
{
	return (_elements->end());
}

template<class T>
typename OrderedSet<T>::size_type
OrderedSet<T>::size()
    const
{
	return (_elements->size());
}

template<class T>
bool
OrderedSet<T>::valueExists(
    const T &value)
    const
{
	switch (_strategy) {
	case FASTER:
		return (_uniqueElements->find(value) != _uniqueElements->end());
	case SMALLER:
		/* FALLTHROUGH */
	default:
		return (std::find(_elements->begin(), _elements->end(),
		    value) !=  _elements->end());
	}
}

#endif /* __ORDERED_SET_H__ */

