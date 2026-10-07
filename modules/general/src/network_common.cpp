#include <sindre/general/network.h>

#include <array>
#include <string>

namespace sindre::general::network {
namespace {

class NetworkCategory final : public std::error_category {
public:
    const char *name() const noexcept override { return "sindre.network"; }

    std::string message(int value) const override {
        static constexpr std::array<const char *, 16> names{
            "invalid url", "invalid request", "unsupported scheme", "dns failure",
            "connection failed", "connection timeout", "read timeout", "write timeout",
            "tls failed", "deadline exceeded", "operation cancelled", "response too large",
            "callback failed", "retry exhausted", "file io failed", "http status"};
        if (value >= 1 && value <= static_cast<int>(names.size())) return names[static_cast<std::size_t>(value - 1)];
        return "unknown network error";
    }
};

} // namespace

const std::error_category &network_category() noexcept {
    static const NetworkCategory category;
    return category;
}

std::error_code make_error_code(NetworkErrc code) noexcept {
    return {static_cast<int>(code), network_category()};
}

} // namespace sindre::general::network
