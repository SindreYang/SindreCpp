#include <sindre/ai.h>

// Keep the AI facade as a compiled target even while backend-specific
// operations remain available from the public headers.  This translation unit
// also validates the complete public include surface in every AI build.
namespace sindre::ai {
namespace detail {
void static_library_anchor() noexcept {}
} // namespace detail
} // namespace sindre::ai
