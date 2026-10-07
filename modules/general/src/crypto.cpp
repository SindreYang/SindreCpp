#include <sindre/general/core.h>
#include <sindre/general/system.h>

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// 密码学实现集中在本翻译单元内，避免把 OpenSSL 类型和内部格式暴露到公共头。
// 字节编码与文件加密共用同一套容器格式、密钥派生和 AES-GCM 初始化流程，
// 这样可以保证两条公共 API 路径的安全参数和错误语义始终一致。
namespace sindre::general::crypto_detail {
namespace {

constexpr std::array<std::uint8_t, 8> kMagic{
    {'S', 'I', 'N', 'D', 'R', 'E', 'C', 'R'}};
constexpr std::uint8_t kVersion = 1;
constexpr std::uint8_t kAlgorithmAes256Gcm = 1;
constexpr std::uint32_t kPbkdf2Iterations = 600000;
constexpr std::size_t kSaltSize = 16;
constexpr std::size_t kNonceSize = 12;
constexpr std::size_t kTagSize = 16;
constexpr std::size_t kHeaderSize = 8 + 1 + 1 + 4 + kSaltSize + kNonceSize + 8;
constexpr std::size_t kMaximumChunkSize = 64 * 1024 * 1024;

using Byte = std::uint8_t;
using Key = std::array<Byte, 32>;
using Salt = std::array<Byte, kSaltSize>;
using Nonce = std::array<Byte, kNonceSize>;
using Tag = std::array<Byte, kTagSize>;
using Header = std::array<Byte, kHeaderSize>;

struct CipherContext {
    EVP_CIPHER_CTX *value = nullptr;
    CipherContext() noexcept = default;
    ~CipherContext() { if (value) EVP_CIPHER_CTX_free(value); }
    CipherContext(const CipherContext &) = delete;
    CipherContext &operator=(const CipherContext &) = delete;
    CipherContext(CipherContext &&other) noexcept
        : value(std::exchange(other.value, nullptr)) {}
    CipherContext &operator=(CipherContext &&other) noexcept {
        if (this == &other) return *this;
        if (value) EVP_CIPHER_CTX_free(value);
        value = std::exchange(other.value, nullptr);
        return *this;
    }
};

template <class T, class Function>
Result<T> boundary(const char *context, Function &&function) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
        return std::forward<Function>(function)();
    } catch (const std::bad_alloc &) {
        return Result<T>::failure(std::make_error_code(std::errc::not_enough_memory),
                                  "Not enough memory", context);
    } catch (const std::exception &error) {
        return Result<T>::failure(std::make_error_code(std::errc::io_error),
                                  error.what(), context);
    } catch (...) {
        return Result<T>::failure(std::make_error_code(std::errc::io_error),
                                  "Unknown crypto failure", context);
    }
#else
    static_cast<void>(context);
    return std::forward<Function>(function)();
#endif
}

template <class T>
Result<T> failure(std::errc code, const char *message, const char *context) {
    return Result<T>::failure(std::make_error_code(code), message, context);
}

void write_u32(Header &header, std::size_t offset, std::uint32_t value) noexcept {
    header[offset] = static_cast<Byte>((value >> 24) & 0xff);
    header[offset + 1] = static_cast<Byte>((value >> 16) & 0xff);
    header[offset + 2] = static_cast<Byte>((value >> 8) & 0xff);
    header[offset + 3] = static_cast<Byte>(value & 0xff);
}

void write_u64(Header &header, std::size_t offset, std::uint64_t value) noexcept {
    for (int index = 7; index >= 0; --index) {
        header[offset + static_cast<std::size_t>(7 - index)] =
            static_cast<Byte>((value >> (index * 8)) & 0xff);
    }
}

std::uint32_t read_u32(const Header &header, std::size_t offset) noexcept {
    return (static_cast<std::uint32_t>(header[offset]) << 24) |
           (static_cast<std::uint32_t>(header[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(header[offset + 2]) << 8) |
           static_cast<std::uint32_t>(header[offset + 3]);
}

std::uint64_t read_u64(const Header &header, std::size_t offset) noexcept {
    std::uint64_t value = 0;
    for (int index = 0; index < 8; ++index)
        value = (value << 8) | header[offset + static_cast<std::size_t>(index)];
    return value;
}

Header make_header(const Salt &salt, const Nonce &nonce,
                   std::uint64_t plaintext_size) noexcept {
    Header header{};
    std::copy(kMagic.begin(), kMagic.end(), header.begin());
    header[8] = kVersion;
    header[9] = kAlgorithmAes256Gcm;
    write_u32(header, 10, kPbkdf2Iterations);
    std::copy(salt.begin(), salt.end(), header.begin() + 14);
    std::copy(nonce.begin(), nonce.end(), header.begin() + 30);
    write_u64(header, 42, plaintext_size);
    return header;
}

Result<std::pair<Salt, Nonce>> parse_header(const Header &header,
                                             std::uint64_t payload_size,
                                             const char *context) {
    if (!std::equal(kMagic.begin(), kMagic.end(), header.begin()) ||
        header[8] != kVersion || header[9] != kAlgorithmAes256Gcm)
        return failure<std::pair<Salt, Nonce>>(std::errc::invalid_argument,
                                               "Unsupported encrypted data format",
                                               context);
    const auto iterations = read_u32(header, 10);
    if (iterations < 100000 || iterations > 10000000)
        return failure<std::pair<Salt, Nonce>>(std::errc::invalid_argument,
                                               "Invalid encryption parameters",
                                               context);
    const auto plaintext_size = read_u64(header, 42);
    if (payload_size < kHeaderSize + kTagSize ||
        plaintext_size != payload_size - kHeaderSize - kTagSize)
        return failure<std::pair<Salt, Nonce>>(std::errc::invalid_argument,
                                               "Encrypted data size is invalid",
                                               context);
    Salt salt{};
    Nonce nonce{};
    std::copy_n(header.begin() + 14, salt.size(), salt.begin());
    std::copy_n(header.begin() + 30, nonce.size(), nonce.begin());
    return Result<std::pair<Salt, Nonce>>::success({salt, nonce});
}

Result<Key> derive_key(std::string_view password, const Salt &salt,
                       std::uint32_t iterations, const char *context) {
    if (password.empty())
        return failure<Key>(std::errc::invalid_argument,
                            "Password is empty", context);
    if (password.size() > static_cast<std::size_t>(INT_MAX))
        return failure<Key>(std::errc::value_too_large,
                            "Password is too large", context);
    Key key{};
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
                          salt.data(), static_cast<int>(salt.size()),
                          static_cast<int>(iterations), EVP_sha256(),
                          static_cast<int>(key.size()), key.data()) != 1)
        return failure<Key>(std::errc::io_error,
                            "Key derivation failed", context);
    return Result<Key>::success(key);
}

Result<CipherContext> create_context(const Key &key, const Nonce &nonce,
                                     const Header &header, bool encrypting,
                                     const char *context) {
    CipherContext result;
    result.value = EVP_CIPHER_CTX_new();
    if (!result.value)
        return failure<CipherContext>(std::errc::not_enough_memory,
                                      "Cannot allocate cipher context", context);
    const auto *cipher = EVP_aes_256_gcm();
    int length = 0;
    const auto initialized = encrypting
        ? EVP_EncryptInit_ex(result.value, cipher, nullptr, nullptr, nullptr)
        : EVP_DecryptInit_ex(result.value, cipher, nullptr, nullptr, nullptr);
    if (initialized != 1 ||
        EVP_CIPHER_CTX_ctrl(result.value, EVP_CTRL_GCM_SET_IVLEN,
                            static_cast<int>(nonce.size()), nullptr) != 1)
        return failure<CipherContext>(std::errc::io_error,
                                      "Cannot initialize cipher", context);
    const auto keyed = encrypting
        ? EVP_EncryptInit_ex(result.value, nullptr, nullptr, key.data(), nonce.data())
        : EVP_DecryptInit_ex(result.value, nullptr, nullptr, key.data(), nonce.data());
    if (keyed != 1 ||
        (encrypting
             ? EVP_EncryptUpdate(result.value, nullptr, &length, header.data(),
                                 static_cast<int>(header.size()))
             : EVP_DecryptUpdate(result.value, nullptr, &length, header.data(),
                                 static_cast<int>(header.size()))) != 1)
        return failure<CipherContext>(std::errc::io_error,
                                      "Cannot initialize authenticated cipher", context);
    return Result<CipherContext>::success(std::move(result));
}

Result<std::vector<Byte>> encrypt_bytes_impl(const std::vector<Byte> &data,
                                             std::string_view password) {
    if (data.size() > (std::numeric_limits<std::uint64_t>::max)() -
                          kHeaderSize - kTagSize)
        return failure<std::vector<Byte>>(std::errc::value_too_large,
                                          "Input is too large", "codec.encrypt");
    Salt salt{};
    Nonce nonce{};
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1 ||
        RAND_bytes(nonce.data(), static_cast<int>(nonce.size())) != 1)
        return failure<std::vector<Byte>>(std::errc::io_error,
                                          "Cannot generate encryption random values",
                                          "codec.encrypt");
    const auto header = make_header(salt, nonce, data.size());
    auto key = derive_key(password, salt, kPbkdf2Iterations, "codec.encrypt");
    if (!key) return Result<std::vector<Byte>>::failure(key.error());
    auto cipher = create_context(key.value(), nonce, header, true, "codec.encrypt");
    if (!cipher) return Result<std::vector<Byte>>::failure(cipher.error());

    std::vector<Byte> result(kHeaderSize + data.size() + kTagSize);
    std::copy(header.begin(), header.end(), result.begin());
    std::size_t offset = kHeaderSize;
    std::size_t input_offset = 0;
    while (input_offset < data.size()) {
        const auto chunk = (std::min)(data.size() - input_offset,
                                      static_cast<std::size_t>(INT_MAX));
        int written = 0;
        if (EVP_EncryptUpdate(cipher.value().value, result.data() + offset, &written,
                              data.data() + input_offset, static_cast<int>(chunk)) != 1)
            return failure<std::vector<Byte>>(std::errc::io_error,
                                              "Encryption failed", "codec.encrypt");
        if (written < 0 || static_cast<std::size_t>(written) > result.size() - offset)
            return failure<std::vector<Byte>>(std::errc::io_error,
                                              "Encryption output is invalid", "codec.encrypt");
        offset += static_cast<std::size_t>(written);
        input_offset += chunk;
    }
    int written = 0;
    if (EVP_EncryptFinal_ex(cipher.value().value, result.data() + offset, &written) != 1 ||
        written < 0 || static_cast<std::size_t>(written) > result.size() - offset - kTagSize)
        return failure<std::vector<Byte>>(std::errc::io_error,
                                          "Encryption finalization failed", "codec.encrypt");
    offset += static_cast<std::size_t>(written);
    if (EVP_CIPHER_CTX_ctrl(cipher.value().value, EVP_CTRL_GCM_GET_TAG,
                            static_cast<int>(kTagSize), result.data() + offset) != 1)
        return failure<std::vector<Byte>>(std::errc::io_error,
                                          "Cannot create authentication tag", "codec.encrypt");
    offset += kTagSize;
    result.resize(offset);
    return Result<std::vector<Byte>>::success(std::move(result));
}

Result<std::vector<Byte>> decrypt_bytes_impl(const std::vector<Byte> &data,
                                             std::string_view password) {
    if (data.size() < kHeaderSize + kTagSize)
        return failure<std::vector<Byte>>(std::errc::invalid_argument,
                                          "Encrypted data is too small", "codec.decrypt");
    Header header{};
    std::copy_n(data.begin(), header.size(), header.begin());
    const auto parsed = parse_header(header, data.size(), "codec.decrypt");
    if (!parsed) return Result<std::vector<Byte>>::failure(parsed.error());
    const auto iterations = read_u32(header, 10);
    auto key = derive_key(password, parsed.value().first, iterations, "codec.decrypt");
    if (!key) return Result<std::vector<Byte>>::failure(key.error());
    auto cipher = create_context(key.value(), parsed.value().second, header, false,
                                 "codec.decrypt");
    if (!cipher) return Result<std::vector<Byte>>::failure(cipher.error());

    const auto plaintext_size = read_u64(header, 42);
    if (plaintext_size > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()))
        return failure<std::vector<Byte>>(std::errc::value_too_large,
                                          "Decrypted data is too large", "codec.decrypt");
    std::vector<Byte> result(static_cast<std::size_t>(plaintext_size));
    const auto ciphertext_size = data.size() - kHeaderSize - kTagSize;
    std::size_t input_offset = kHeaderSize;
    std::size_t output_offset = 0;
    while (input_offset < kHeaderSize + ciphertext_size) {
        const auto chunk = (std::min)(kHeaderSize + ciphertext_size - input_offset,
                                      static_cast<std::size_t>(INT_MAX));
        int written = 0;
        if (EVP_DecryptUpdate(cipher.value().value, result.data() + output_offset, &written,
                              data.data() + input_offset, static_cast<int>(chunk)) != 1 ||
            written < 0 || static_cast<std::size_t>(written) > result.size() - output_offset)
            return failure<std::vector<Byte>>(std::errc::io_error,
                                              "Decryption failed", "codec.decrypt");
        input_offset += chunk;
        output_offset += static_cast<std::size_t>(written);
    }
    Tag tag{};
    std::copy_n(data.begin() + kHeaderSize + ciphertext_size, tag.size(), tag.begin());
    if (EVP_CIPHER_CTX_ctrl(cipher.value().value, EVP_CTRL_GCM_SET_TAG,
                            static_cast<int>(tag.size()), tag.data()) != 1)
        return failure<std::vector<Byte>>(std::errc::io_error,
                                          "Cannot set authentication tag", "codec.decrypt");
    int written = 0;
    if (EVP_DecryptFinal_ex(cipher.value().value, result.data() + output_offset, &written) != 1)
        return failure<std::vector<Byte>>(std::errc::permission_denied,
                                          "Authentication failed", "codec.decrypt");
    if (written < 0 || static_cast<std::size_t>(written) > result.size() - output_offset)
        return failure<std::vector<Byte>>(std::errc::io_error,
                                          "Decryption output is invalid", "codec.decrypt");
    output_offset += static_cast<std::size_t>(written);
    if (output_offset != result.size())
        return failure<std::vector<Byte>>(std::errc::invalid_argument,
                                          "Decrypted data size is invalid", "codec.decrypt");
    return Result<std::vector<Byte>>::success(std::move(result));
}

} // namespace
} // namespace sindre::general::crypto_detail

namespace sindre::general::codec {
using namespace crypto_detail;

Result<std::vector<std::uint8_t>> encrypt_bytes(
    const std::vector<std::uint8_t> &data, std::string_view password) noexcept {
    return boundary<std::vector<std::uint8_t>>("codec.encrypt", [&] {
        return encrypt_bytes_impl(data, password);
    });
}

Result<std::vector<std::uint8_t>> decrypt_bytes(
    const std::vector<std::uint8_t> &data, std::string_view password) noexcept {
    return boundary<std::vector<std::uint8_t>>("codec.decrypt", [&] {
        return decrypt_bytes_impl(data, password);
    });
}

Result<std::string> encrypt(std::string_view plaintext,
                            std::string_view password) noexcept {
    return boundary<std::string>("codec.encrypt", [&] {
        const std::vector<Byte> data(plaintext.begin(), plaintext.end());
        auto encrypted = encrypt_bytes_impl(data, password);
        if (!encrypted) return Result<std::string>::failure(encrypted.error());
        return base64_encode(encrypted.value());
    });
}

Result<std::string> decrypt(std::string_view ciphertext,
                            std::string_view password) noexcept {
    return boundary<std::string>("codec.decrypt", [&] {
        auto encoded = base64_decode(ciphertext);
        if (!encoded) return Result<std::string>::failure(encoded.error());
        auto decrypted = decrypt_bytes_impl(encoded.value(), password);
        if (!decrypted) return Result<std::string>::failure(decrypted.error());
        return Result<std::string>::success(std::string(
            reinterpret_cast<const char *>(decrypted.value().data()),
            decrypted.value().size()));
    });
}

} // namespace sindre::general::codec

namespace sindre::general::file {
using namespace crypto_detail;
namespace {

Result<std::filesystem::path> make_temp_path(
    const std::filesystem::path &destination, const char *context) {
    std::error_code error;
    auto directory = destination.parent_path();
    if (directory.empty()) directory = std::filesystem::current_path(error);
    if (error) return Result<std::filesystem::path>::failure(error, "Cannot get destination directory", context);
    if (!std::filesystem::is_directory(directory, error) || error)
        return failure<std::filesystem::path>(error ? std::errc::io_error : std::errc::not_a_directory,
                                              "Destination directory is invalid", context);
    auto filename = path::try_to_utf8(destination.filename());
    if (!filename) return Result<std::filesystem::path>::failure(filename.error().with_context(context));
    for (int attempt = 0; attempt < 100; ++attempt) {
        auto id = codec::uuid4();
        if (!id) return Result<std::filesystem::path>::failure(id.error().with_context(context));
        auto name = path::try_from_utf8("." + filename.value() + ".sindre-" + id.value() + ".tmp");
        if (!name) return Result<std::filesystem::path>::failure(name.error().with_context(context));
        auto candidate = directory / name.value();
        const auto candidate_exists = std::filesystem::exists(candidate, error);
        if (error) return Result<std::filesystem::path>::failure(error, "Cannot inspect temporary path", context);
        if (!candidate_exists) return Result<std::filesystem::path>::success(candidate);
    }
    return failure<std::filesystem::path>(std::errc::file_exists,
                                          "Cannot create temporary file path", context);
}

void remove_quietly(const std::filesystem::path &path_value) noexcept {
    std::error_code error;
    std::filesystem::remove(path_value, error);
}

Result<void> validate_options(const CryptoOptions &options, const char *context) {
    if (options.buffer_size == 0 || options.buffer_size > kMaximumChunkSize)
        return failure<void>(std::errc::invalid_argument,
                             "Invalid crypto buffer size", context);
    return Result<void>::success();
}

Result<void> report_progress(const CryptoOptions &options,
                             std::uint64_t current, std::uint64_t total,
                             const char *context) {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        if (options.progress) options.progress(current, total);
        return Result<void>::success();
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return Result<void>::failure(std::make_error_code(std::errc::io_error),
                                     error.what(), context);
    } catch (...) {
        return Result<void>::failure(std::make_error_code(std::errc::io_error),
                                     "Progress callback failed", context);
    }
#else
    static_cast<void>(context);
#endif
}

Result<void> check_cancelled(const CryptoOptions &options, const char *context) {
    if (options.token.is_cancelled())
        return failure<void>(std::errc::operation_canceled,
                             "File crypto operation cancelled", context);
    return Result<void>::success();
}

Result<void> replace_file(const std::filesystem::path &temporary,
                          const std::filesystem::path &destination,
                          bool overwrite, const char *context) {
#if defined(_WIN32)
    if (!overwrite) {
        if (!::MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH))
            return Result<void>::failure(
                std::error_code(static_cast<int>(::GetLastError()), std::system_category()),
                "Cannot move encrypted temporary file", context);
    } else if (!::MoveFileExW(temporary.c_str(), destination.c_str(),
                              MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return Result<void>::failure(
            std::error_code(static_cast<int>(::GetLastError()), std::system_category()),
            "Cannot replace destination file", context);
    }
    return Result<void>::success();
#else
    std::error_code error;
    if (!overwrite && std::filesystem::exists(destination, error))
        return failure<void>(std::errc::file_exists, "Destination file already exists", context);
    if (error) return Result<void>::failure(error, "Cannot inspect destination file", context);
    std::filesystem::rename(temporary, destination, error);
    if (error) return Result<void>::failure(error, "Cannot replace destination file", context);
    return Result<void>::success();
#endif
}

Result<void> validate_paths(const std::filesystem::path &source,
                            const std::filesystem::path &destination,
                            std::string_view password,
                            const CryptoOptions &options, const char *context,
                            std::uint64_t &source_size) {
    if (source.empty() || destination.empty() || password.empty())
        return failure<void>(std::errc::invalid_argument,
                             "Source, destination and password are required", context);
    auto options_status = validate_options(options, context);
    if (!options_status) return options_status;
    std::error_code error;
    if (!std::filesystem::is_regular_file(source, error))
        return failure<void>(error ? std::errc::io_error : std::errc::no_such_file_or_directory,
                             "Source is not a regular file", context);
    const auto source_absolute = std::filesystem::absolute(source, error).lexically_normal();
    if (error) return Result<void>::failure(error, "Cannot resolve source path", context);
    const auto destination_absolute = std::filesystem::absolute(destination, error).lexically_normal();
    if (error) return Result<void>::failure(error, "Cannot resolve destination path", context);
    if (source_absolute == destination_absolute)
        return failure<void>(std::errc::invalid_argument,
                             "Source and destination must differ", context);
    source_size = std::filesystem::file_size(source, error);
    if (error) return Result<void>::failure(error, "Cannot read source file size", context);
    if (std::filesystem::exists(destination, error) && !options.overwrite)
        return failure<void>(std::errc::file_exists,
                             "Destination file already exists", context);
    if (error) return Result<void>::failure(error, "Cannot inspect destination file", context);
    return Result<void>::success();
}

Result<void> encrypt_file_impl(const std::filesystem::path &source,
                               const std::filesystem::path &destination,
                               std::string_view password,
                               const CryptoOptions &options) {
    std::uint64_t source_size = 0;
    auto valid = validate_paths(source, destination, password, options,
                                "file.encrypt", source_size);
    if (!valid) return valid;
    auto temporary = make_temp_path(destination, "file.encrypt");
    if (!temporary) return Result<void>::failure(temporary.error());
    auto cleanup = scope_guard([&] { remove_quietly(temporary.value()); });
    std::ifstream input(source, std::ios::binary);
    std::ofstream output(temporary.value(), std::ios::binary | std::ios::trunc);
    if (!input || !output) return failure<void>(std::errc::io_error,
                                                "Cannot open crypto files", "file.encrypt");
    Salt salt{};
    Nonce nonce{};
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1 ||
        RAND_bytes(nonce.data(), static_cast<int>(nonce.size())) != 1)
        return failure<void>(std::errc::io_error, "Cannot generate encryption random values", "file.encrypt");
    const auto header = make_header(salt, nonce, source_size);
    output.write(reinterpret_cast<const char *>(header.data()), static_cast<std::streamsize>(header.size()));
    if (!output) return failure<void>(std::errc::io_error, "Cannot write crypto header", "file.encrypt");
    auto key = derive_key(password, salt, 600000, "file.encrypt");
    if (!key) return Result<void>::failure(key.error());
    auto cipher = create_context(key.value(), nonce, header, true, "file.encrypt");
    if (!cipher) return Result<void>::failure(cipher.error());
    std::vector<Byte> input_buffer(options.buffer_size);
    std::vector<Byte> output_buffer(options.buffer_size + 16);
    std::uint64_t processed = 0;
    for (;;) {
        auto cancelled = check_cancelled(options, "file.encrypt");
        if (!cancelled) return cancelled;
        input.read(reinterpret_cast<char *>(input_buffer.data()),
                   static_cast<std::streamsize>(input_buffer.size()));
        const auto count = input.gcount();
        if (count > 0) {
            int written = 0;
            if (EVP_EncryptUpdate(cipher.value().value, output_buffer.data(), &written,
                                  input_buffer.data(), static_cast<int>(count)) != 1)
                return failure<void>(std::errc::io_error, "Encryption failed", "file.encrypt");
            output.write(reinterpret_cast<const char *>(output_buffer.data()), written);
            if (!output) return failure<void>(std::errc::io_error, "Cannot write encrypted file", "file.encrypt");
            processed += static_cast<std::uint64_t>(count);
            auto progress = report_progress(options, processed, source_size, "file.encrypt.progress");
            if (!progress) return progress;
        }
        if (input.eof()) break;
        if (!input) return failure<void>(std::errc::io_error, "Cannot read source file", "file.encrypt");
    }
    if (processed != source_size)
        return failure<void>(std::errc::io_error, "Source file changed while encrypting", "file.encrypt");
    int written = 0;
    if (EVP_EncryptFinal_ex(cipher.value().value, output_buffer.data(), &written) != 1)
        return failure<void>(std::errc::io_error, "Encryption finalization failed", "file.encrypt");
    output.write(reinterpret_cast<const char *>(output_buffer.data()), written);
    Tag tag{};
    if (EVP_CIPHER_CTX_ctrl(cipher.value().value, EVP_CTRL_GCM_GET_TAG,
                            static_cast<int>(tag.size()), tag.data()) != 1)
        return failure<void>(std::errc::io_error, "Cannot create authentication tag", "file.encrypt");
    output.write(reinterpret_cast<const char *>(tag.data()), static_cast<std::streamsize>(tag.size()));
    output.flush();
    output.close();
    if (!output) return failure<void>(std::errc::io_error, "Cannot finalize encrypted file", "file.encrypt");
    if (source_size == 0) {
        auto progress = report_progress(options, 0, 0, "file.encrypt.progress");
        if (!progress) return progress;
    }
    auto replaced = replace_file(temporary.value(), destination, options.overwrite, "file.encrypt");
    if (!replaced) return replaced;
    cleanup.dismiss();
    return Result<void>::success();
}

Result<void> decrypt_file_impl(const std::filesystem::path &source,
                               const std::filesystem::path &destination,
                               std::string_view password,
                               const CryptoOptions &options) {
    std::uint64_t source_size = 0;
    auto valid = validate_paths(source, destination, password, options,
                                "file.decrypt", source_size);
    if (!valid) return valid;
    if (source_size < kHeaderSize + kTagSize)
        return failure<void>(std::errc::invalid_argument,
                             "Encrypted file is too small", "file.decrypt");

    auto temporary = make_temp_path(destination, "file.decrypt");
    if (!temporary) return Result<void>::failure(temporary.error());
    auto cleanup = scope_guard([&] { remove_quietly(temporary.value()); });
    std::ifstream input(source, std::ios::binary);
    std::ofstream output(temporary.value(), std::ios::binary | std::ios::trunc);
    if (!input || !output)
        return failure<void>(std::errc::io_error, "Cannot open crypto files", "file.decrypt");

    Header header{};
    input.read(reinterpret_cast<char *>(header.data()),
               static_cast<std::streamsize>(header.size()));
    if (input.gcount() != static_cast<std::streamsize>(header.size()))
        return failure<void>(std::errc::io_error, "Cannot read encrypted file header", "file.decrypt");
    const auto parsed = parse_header(header, source_size, "file.decrypt");
    if (!parsed) return Result<void>::failure(parsed.error());

    const auto plaintext_size = read_u64(header, 42);
    const auto ciphertext_size = source_size - kHeaderSize - kTagSize;
    const auto iterations = read_u32(header, 10);
    auto key = derive_key(password, parsed.value().first, iterations, "file.decrypt");
    if (!key) return Result<void>::failure(key.error());
    auto cipher = create_context(key.value(), parsed.value().second, header, false,
                                 "file.decrypt");
    if (!cipher) return Result<void>::failure(cipher.error());

    std::vector<Byte> input_buffer(options.buffer_size);
    std::vector<Byte> output_buffer(options.buffer_size + 16);
    std::uint64_t processed = 0;
    std::uint64_t remaining = ciphertext_size;
    while (remaining > 0) {
        auto cancelled = check_cancelled(options, "file.decrypt");
        if (!cancelled) return cancelled;
        const auto requested = (std::min)(remaining,
                                          static_cast<std::uint64_t>(input_buffer.size()));
        input.read(reinterpret_cast<char *>(input_buffer.data()),
                   static_cast<std::streamsize>(requested));
        const auto count = input.gcount();
        if (count <= 0 || static_cast<std::uint64_t>(count) > remaining)
            return failure<void>(std::errc::io_error, "Cannot read encrypted file", "file.decrypt");
        int written = 0;
        if (EVP_DecryptUpdate(cipher.value().value, output_buffer.data(), &written,
                              input_buffer.data(), static_cast<int>(count)) != 1 ||
            written < 0)
            return failure<void>(std::errc::io_error, "Decryption failed", "file.decrypt");
        if (static_cast<std::size_t>(written) > output_buffer.size())
            return failure<void>(std::errc::io_error, "Decryption output is invalid", "file.decrypt");
        output.write(reinterpret_cast<const char *>(output_buffer.data()), written);
        if (!output)
            return failure<void>(std::errc::io_error, "Cannot write decrypted file", "file.decrypt");
        remaining -= static_cast<std::uint64_t>(count);
        processed += static_cast<std::uint64_t>(written);
        auto progress = report_progress(options, processed, plaintext_size,
                                        "file.decrypt.progress");
        if (!progress) return progress;
        if (!input && !input.eof())
            return failure<void>(std::errc::io_error, "Cannot read encrypted file", "file.decrypt");
    }

    Tag tag{};
    input.read(reinterpret_cast<char *>(tag.data()), static_cast<std::streamsize>(tag.size()));
    if (input.gcount() != static_cast<std::streamsize>(tag.size()))
        return failure<void>(std::errc::io_error, "Cannot read authentication tag", "file.decrypt");
    if (EVP_CIPHER_CTX_ctrl(cipher.value().value, EVP_CTRL_GCM_SET_TAG,
                            static_cast<int>(tag.size()), tag.data()) != 1)
        return failure<void>(std::errc::io_error, "Cannot set authentication tag", "file.decrypt");
    int written = 0;
    if (EVP_DecryptFinal_ex(cipher.value().value, output_buffer.data(), &written) != 1)
        return failure<void>(std::errc::permission_denied,
                             "Authentication failed", "file.decrypt");
    if (written < 0 || static_cast<std::size_t>(written) > output_buffer.size())
        return failure<void>(std::errc::io_error, "Decryption output is invalid", "file.decrypt");
    output.write(reinterpret_cast<const char *>(output_buffer.data()), written);
    if (!output)
        return failure<void>(std::errc::io_error, "Cannot write decrypted file", "file.decrypt");
    processed += static_cast<std::uint64_t>(written);
    if (processed != plaintext_size)
        return failure<void>(std::errc::invalid_argument,
                             "Decrypted file size is invalid", "file.decrypt");
    output.flush();
    output.close();
    if (!output)
        return failure<void>(std::errc::io_error, "Cannot finalize decrypted file", "file.decrypt");
    if (plaintext_size == 0) {
        auto progress = report_progress(options, 0, 0, "file.decrypt.progress");
        if (!progress) return progress;
    }
    auto replaced = replace_file(temporary.value(), destination, options.overwrite,
                                 "file.decrypt");
    if (!replaced) return replaced;
    cleanup.dismiss();
    return Result<void>::success();
}

} // namespace

Result<void> encrypt(const std::filesystem::path &source,
                     const std::filesystem::path &destination,
                     std::string_view password,
                     CryptoOptions options) noexcept {
    return boundary<void>("file.encrypt", [&] {
        return encrypt_file_impl(source, destination, password, options);
    });
}

Result<void> decrypt(const std::filesystem::path &source,
                     const std::filesystem::path &destination,
                     std::string_view password,
                     CryptoOptions options) noexcept {
    return boundary<void>("file.decrypt", [&] {
        return decrypt_file_impl(source, destination, password, options);
    });
}

} // namespace sindre::general::file
