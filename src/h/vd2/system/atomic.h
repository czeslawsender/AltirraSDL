//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2004 Avery Lee, All Rights Reserved.
//
//	Beginning with 1.6.0, the VirtualDub system library is licensed
//	differently than the remainder of VirtualDub.  This particular file is
//	thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

#ifndef f_VD2_SYSTEM_ATOMIC_H
#define f_VD2_SYSTEM_ATOMIC_H

#include <atomic>
#include <vd2/system/vdtypes.h>

inline void *VDAtomicCompareExchangePointer(void *volatile *pp, void *p, void *compare) {
	std::atomic<void *> *ap = reinterpret_cast<std::atomic<void *> *>(const_cast<void **>(pp));
	ap->compare_exchange_strong(compare, p);
	return compare;
}

///////////////////////////////////////////////////////////////////////////
/// \class VDAtomicInt
/// \brief Wrapped integer supporting thread-safe atomic operations.
///
/// VDAtomicInt allows integer values shared between threads to be
/// modified with several common operations in a lock-less manner and
/// without the need for explicit barriers. This is particularly useful
/// for thread-safe reference counting.
///
class VDAtomicInt {
protected:
	std::atomic<int> n;

public:
	VDAtomicInt() {}
	VDAtomicInt(int v) : n(v) {}

	bool operator!() const { return !n.load(std::memory_order_relaxed); }
	bool operator!=(int v) const  { return n.load(std::memory_order_relaxed)!=v; }
	bool operator==(int v) const { return n.load(std::memory_order_relaxed)==v; }
	bool operator<=(int v) const { return n.load(std::memory_order_relaxed)<=v; }
	bool operator>=(int v) const { return n.load(std::memory_order_relaxed)>=v; }
	bool operator<(int v) const { return n.load(std::memory_order_relaxed)<v; }
	bool operator>(int v) const { return n.load(std::memory_order_relaxed)>v; }

	///////////////////////////////

	/// Atomically exchanges a value with an integer in memory.
	static inline int staticExchange(volatile int *dst, int v) {
		return reinterpret_cast<std::atomic<int> *>(const_cast<int *>(dst))->exchange(v);
	}

	/// Atomically adds one to an integer in memory.
	static inline void staticIncrement(volatile int *dst) {
		reinterpret_cast<std::atomic<int> *>(const_cast<int *>(dst))->fetch_add(1);
	}

	/// Atomically subtracts one from an integer in memory.
	static inline void staticDecrement(volatile int *dst) {
		reinterpret_cast<std::atomic<int> *>(const_cast<int *>(dst))->fetch_sub(1);
	}

	/// Atomically subtracts one from an integer in memory and returns
	/// true if the result is zero.
	static inline bool staticDecrementTestZero(volatile int *dst) {
		return 1 == reinterpret_cast<std::atomic<int> *>(const_cast<int *>(dst))->fetch_sub(1);
	}

	/// Atomically adds a value to an integer in memory and returns the
	/// result.
	static inline int staticAdd(volatile int *dst, int v) {
		return reinterpret_cast<std::atomic<int> *>(const_cast<int *>(dst))->fetch_add(v) + v;
	}

	/// Atomically adds a value to an integer in memory and returns the
	/// old result (post-add).
	static inline int staticExchangeAdd(volatile int *dst, int v) {
		return reinterpret_cast<std::atomic<int> *>(const_cast<int *>(dst))->fetch_add(v);
	}

	/// Atomically compares an integer in memory to a compare value and
	/// swaps the memory location with a second value if the compare
	/// succeeds. The return value is the memory value prior to the swap.
	static inline int staticCompareExchange(volatile int *dst, int v, int compare) {
		reinterpret_cast<std::atomic<int> *>(const_cast<int *>(dst))->compare_exchange_strong(compare, v);
		return compare;
	}

	///////////////////////////////

	int operator=(int v) { n.store(v, std::memory_order_relaxed); return v; }

	int operator++()		{ return n.fetch_add(1) + 1; }
	int operator--()		{ return n.fetch_sub(1) - 1; }
	int operator++(int)		{ return n.fetch_add(1); }
	int operator--(int)		{ return n.fetch_sub(1); }
	int operator+=(int v)	{ return n.fetch_add(v) + v; }
	int operator-=(int v)	{ return n.fetch_sub(v) - v; }

	void operator&=(int v)	{ n.fetch_and(v); }	///< Atomic bitwise AND.
	void operator|=(int v)	{ n.fetch_or(v); }	///< Atomic bitwise OR.
	void operator^=(int v)	{ n.fetch_xor(v); }	///< Atomic bitwise XOR.

	operator int() const {
		return n.load(std::memory_order_relaxed);
	}

	/// Atomic exchange.
	int xchg(int v) {
		return n.exchange(v);
	}

	/// Compare/exchange (486+).
	int compareExchange(int newValue, int oldValue) {
		n.compare_exchange_strong(oldValue, newValue);
		return oldValue;
	}

	// 486 only, but much nicer.  They return the actual result.

	int inc()			{ return operator++(); }				///< Atomic increment.
	int dec()			{ return operator--(); }				///< Atomic decrement.
	int add(int v)		{ return operator+=(v); }				///< Atomic add.

	// These return the result before the operation, which is more inline with
	// what XADD allows us to do.

	int postinc()		{ return operator++(0); }				///< Atomic post-increment.
	int postdec()		{ return operator--(0); }				///< Atomic post-decrement.
	int postadd(int v)	{ return n.fetch_add(v); }				///< Atomic post-add.

};

///////////////////////////////////////////////////////////////////////////

class VDAtomicFloat {
protected:
	std::atomic<int> n;

public:
	VDAtomicFloat() {}
	VDAtomicFloat(float v) { store(v); }

	bool operator!=(float v) const  { return load()!=v; }
	bool operator==(float v) const { return load()==v; }
	bool operator<=(float v) const { return load()<=v; }
	bool operator>=(float v) const { return load()>=v; }
	bool operator<(float v) const { return load()<v; }
	bool operator>(float v) const { return load()>v; }

	float operator=(float v) { store(v); return v; }

	operator float() const {
		return load();
	}

	/// Atomic exchange.
	float xchg(float v) {
		union { int i; float f; } in, out;
		in.f = v;
		out.i = n.exchange(in.i);
		return out.f;
	}

private:
	float load() const {
		union { int i; float f; } u;
		u.i = n.load(std::memory_order_relaxed);
		return u.f;
	}
	void store(float v) {
		union { int i; float f; } u;
		u.f = v;
		n.store(u.i, std::memory_order_relaxed);
	}
};

///////////////////////////////////////////////////////////////////////////

class VDAtomicBool {
protected:
	std::atomic<bool> n;

public:
	VDAtomicBool() {}
	VDAtomicBool(bool v) : n(v) {}

	bool operator!=(bool v) const { return n.load(std::memory_order_relaxed) != v; }
	bool operator==(bool v) const { return n.load(std::memory_order_relaxed) == v; }

	bool operator=(bool v) { n.store(v, std::memory_order_relaxed); return v; }

	operator bool() const {
		return n.load(std::memory_order_relaxed);
	}

	/// Atomic exchange.
	bool xchg(bool v) {
		return n.exchange(v);
	}
};

///////////////////////////////////////////////////////////////////////////
/// \class VDAtomicPtr
/// \brief Wrapped pointer supporting thread-safe atomic operations.
///
/// VDAtomicPtr allows a shared pointer to be safely manipulated by
/// multiple threads without locks. Note that atomicity is only guaranteed
/// for the pointer itself, so any operations on the object must be dealt
/// with in other manners, such as an inner lock or other atomic
/// operations. An atomic pointer can serve as a single entry queue.
///
template<typename T>
class VDAtomicPtr {
protected:
	std::atomic<T *> ptr;

public:
	VDAtomicPtr() {}
	VDAtomicPtr(T *p) : ptr(p) { }

	operator T*() const { return ptr.load(std::memory_order_relaxed); }
	T* operator->() const { return ptr.load(std::memory_order_relaxed); }

	T* operator=(T* p) {
		ptr.store(p, std::memory_order_relaxed);
		return p;
	}

	/// Atomic pointer exchange.
	T *xchg(T* p) {
		return ptr.exchange(p);
	}

	T *compareExchange(T *newValue, T *oldValue) {
		ptr.compare_exchange_strong(oldValue, newValue);
		return oldValue;
	}
};

#endif
