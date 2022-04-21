//===--- ThreadLocalStorage.h - Thread-local storage interface. --*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2022 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_THREADING_THREADLOCALSTORAGE_H
#define SWIFT_THREADING_THREADLOCALSTORAGE_H

#include <type_traits>

#include "Impl.h"
#include "Once.h"
#include "Errors.h"

namespace swift {

// -- Low-level TLS functions -------------------------------------------------

#if !SWIFT_THREADING_NONE
using tls_key = threading_impl::tls_key;
using tls_dtor = threading_impl::tls_dtor;

#if SWIFT_THREADING_USE_RESERVED_TLS_KEYS
using threading_impl::tls_init;

/// tls_init_once() - Initialize TLS, once only
inline void tls_init_once(once_t &token, tls_key key, tls_dtor dtor) {
  const struct tls_init_info { tls_key &k; tls_dtor d; } info = { key, dtor };
  once(token, [](void *ctx) {
    const struct tls_init_info *pinfo = static_cast<const struct tls_init_info *>(ctx);
    if (!tls_init(pinfo->k, pinfo->d))
      swift::threading::fatal("tls_init_once() failed to set destructor");
  }, (void *)&info);
}
#endif // SWIFT_THREADING_USE_RESERVED_TLS_KEYS

using threading_impl::tls_alloc;
using threading_impl::tls_get;
using threading_impl::tls_set;

/// tls_alloc_once() - Allocate TLS key, once only
inline void tls_alloc_once(once_t &token, tls_key &key, tls_dtor dtor) {
  const struct tls_init_info { tls_key &k; tls_dtor d; } info = { key, dtor };
  once(token, [](void *ctx) {
    const struct tls_init_info *pinfo = static_cast<const struct tls_init_info *>(ctx);
    if (!tls_alloc(pinfo->k, pinfo->d))
      swift::threading::fatal("tls_alloc_once() failed to allocate key");
  }, (void *)&info);
}
#endif // !SWIFT_THREADING_NONE

// -- High-level TLS classes --------------------------------------------------

// Validate a type stored in thread-local storage, using static asserts. Such
// types must fit in a pointer and be trivially copyable/destructible.
#define VALIDATE_THREAD_LOCAL_TYPE(T)                                          \
  static_assert(sizeof(T) <= sizeof(void *),                                   \
                "cannot store more than a pointer");                           \
  static_assert(std::is_trivially_copyable<T>::value,                          \
                "ThreadLocal values must be trivially copyable");              \
  static_assert(std::is_trivially_destructible<T>::value,                      \
                "ThreadLocal cleanup is not supported, stored types must be "  \
                "trivially destructible");

// A wrapper class for thread-local storage.
//
// - On platforms that report SWIFT_RUNTIME_SUPPORTS_THREAD_LOCAL
//   above, an object of this type is declared with
//   SWIFT_RUNTIME_ATTRIBUTE_THREAD_LOCAL.  This makes the object
//   itself thread-local, and no internal support is required.
//
//   Note that this includes platforms that don't support threading,
//   for which SWIFT_RUNTIME_ATTRIBUTE_THREAD_LOCAL is empty;
//   thread-local declarations then create an ordinary global.
//
// - On platforms that don't report SWIFT_RUNTIME_SUPPORTS_THREAD_LOCAL,
//   we have to simulate thread-local storage.  Fortunately, all of
//   these platforms (at least for now) support pthread_getspecific
//   or similar.
#if SWIFT_THREAD_LOCAL
template <class T>
class ThreadLocal {
  VALIDATE_THREAD_LOCAL_TYPE(T)

  T value;

public:
  constexpr ThreadLocal() {}

  T get() { return value; }

  void set(T newValue) { value = newValue; }
};
#else
// A wrapper around a TLS key that is lazily initialized using swift::once.
class ThreadLocalKey {
  // We rely on the zero-initialization of objects with static storage
  // duration.
  once_t  onceFlag;
  tls_key key;

public:
  threading_impl::tls_key getKey() {
    once(onceFlag, [](void *ctx) {
        tls_key *pkey = reinterpret_cast<tls_key *>(ctx);
        tls_alloc(*pkey, nullptr);
    }, &key);
    return key;
  }
};

// A type representing a constant TLS key, for use on platforms
// that provide reserved keys.
template <tls_key constantKey>
class ConstantThreadLocalKey {
public:
  tls_key getKey() { return constantKey; }
};

template <class T, class Key>
class ThreadLocal {
  VALIDATE_THREAD_LOCAL_TYPE(T)

  Key key;

public:
  constexpr ThreadLocal() {}

  T get() {
    void *storedValue = tls_get(key.getKey());
    T value;
    memcpy(&value, &storedValue, sizeof(T));
    return value;
  }

  void set(T newValue) {
    void *storedValue;
    memcpy(&storedValue, &newValue, sizeof(T));
    tls_set(key.getKey(), storedValue);
  }
};
#endif

} // end namespace swift

/// SWIFT_THREAD_LOCAL_TYPE(TYPE, KEY) - Declare a variable
/// to be a thread-local variable.  The declaration must have static
/// storage duration; it may be prefixed with "static".
///
/// For example
///
///   static SWIFT_THREAD_LOCAL_TYPE(int, SWIFT_RESERVED_TLS_KEY_9) frobble;
///
/// Because of the fallback path, the default-initialization of the
/// type must be equivalent to a bitwise zero-initialization, and the
/// type must be small and trivially copyable and destructible.
#if SWIFT_THREAD_LOCAL
#define SWIFT_THREAD_LOCAL_TYPE(TYPE, KEY) \
  SWIFT_THREAD_LOCAL swift::ThreadLocal<TYPE>
#elif SWIFT_THREADING_USE_RESERVED_TLS_KEYS
#define SWIFT_THREAD_LOCAL_TYPE(TYPE, KEY) \
  swift::ThreadLocal<TYPE, swift::ConstantThreadLocalKey<KEY>>
#else
#define SWIFT_THREAD_LOCAL_TYPE(TYPE, KEY) \
  swift::ThreadLocal<TYPE, swift::ThreadLocalKey>
#endif


#endif // SWIFT_THREADING_THREADLOCALSTORAGE_H
