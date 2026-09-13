#pragma once

#include <memory>
#include <utility>

#if defined(SINDRECPP_WITH_POINTER)
#include <cs_shared_pointer.h>
#include <cs_unique_pointer.h>
#include <cs_weak_pointer.h>
#endif

namespace sindrecpp::pointer {

template <class T> using unique_ptr = std::unique_ptr<T>;
template <class T> using shared_ptr = std::shared_ptr<T>;
template <class T> using weak_ptr = std::weak_ptr<T>;
using std::make_shared;
using std::make_unique;

#if defined(SINDRECPP_WITH_POINTER)
namespace native = CsPointer;
#endif

} // namespace sindrecpp::pointer
