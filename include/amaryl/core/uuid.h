#pragma once

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <memory>
#include <optional>
#include <ostream>
#include <random>
#include <string>
#include <string_view>
#include <type_traits>

#include "amaryl/core/hash_combine.h"
#include "amaryl/core/span.h"
#include "amaryl/core/types.h"

namespace amaryl {

namespace detail {

/// Lowercase hexadecimal digits, indexed by their 4-bit value.
inline constexpr char kHexChars[17] = "0123456789abcdef";

/// Convert a single hexadecimal character to its 4-bit numeric value.
/// Non-hexadecimal characters yield 0.
template <typename CharT>
[[nodiscard]] constexpr uint8 HexToValue(CharT ch) noexcept {
  if (ch >= static_cast<CharT>('0') && ch <= static_cast<CharT>('9')) {
    return static_cast<uint8>(ch - static_cast<CharT>('0'));
  }
  if (ch >= static_cast<CharT>('a') && ch <= static_cast<CharT>('f')) {
    return static_cast<uint8>(10 + ch - static_cast<CharT>('a'));
  }
  if (ch >= static_cast<CharT>('A') && ch <= static_cast<CharT>('F')) {
    return static_cast<uint8>(10 + ch - static_cast<CharT>('A'));
  }
  return 0;
}

/// Test whether a single character is a hexadecimal digit.
template <typename CharT>
[[nodiscard]] constexpr bool IsHexDigit(CharT ch) noexcept {
  return (ch >= static_cast<CharT>('0') && ch <= static_cast<CharT>('9')) ||
         (ch >= static_cast<CharT>('a') && ch <= static_cast<CharT>('f')) ||
         (ch >= static_cast<CharT>('A') && ch <= static_cast<CharT>('F'));
}

/// Convert a pointer or a string-like object to a std::basic_string_view.
template <typename CharT>
[[nodiscard]] constexpr std::basic_string_view<CharT> ToStringView(
    const CharT* str) noexcept {
  return str ? std::basic_string_view<CharT>(str) : std::basic_string_view<CharT>{};
}

template <typename StringType>
[[nodiscard]] constexpr std::basic_string_view<typename StringType::value_type,
                                               typename StringType::traits_type>
ToStringView(const StringType& str) noexcept {
  return str;
}

/// Minimal SHA-1 implementation, used only to build name-based (v5) UUIDs.
class Sha1 {
 public:
  static constexpr uint32 kBlockBytes = 64;

  Sha1() noexcept { Reset(); }

  void Reset() noexcept {
    digest_[0] = 0x67452301;
    digest_[1] = 0xEFCDAB89;
    digest_[2] = 0x98BADCFE;
    digest_[3] = 0x10325476;
    digest_[4] = 0xC3D2E1F0;
    block_byte_index_ = 0;
    byte_count_ = 0;
  }

  void ProcessByte(uint8 octet) noexcept {
    block_[block_byte_index_++] = octet;
    ++byte_count_;
    if (block_byte_index_ == kBlockBytes) {
      block_byte_index_ = 0;
      ProcessBlock();
    }
  }

  void ProcessBytes(const void* data, size_t len) noexcept {
    const uint8* block = static_cast<const uint8*>(data);
    for (size_t i = 0; i < len; ++i) {
      ProcessByte(block[i]);
    }
  }

  void GetDigestBytes(uint8 (&out)[20]) noexcept {
    uint32 d32[5];
    GetDigest(d32);

    size_t i = 0;
    out[i++] = static_cast<uint8>(d32[0] >> 24);
    out[i++] = static_cast<uint8>(d32[0] >> 16);
    out[i++] = static_cast<uint8>(d32[0] >> 8);
    out[i++] = static_cast<uint8>(d32[0]);
    out[i++] = static_cast<uint8>(d32[1] >> 24);
    out[i++] = static_cast<uint8>(d32[1] >> 16);
    out[i++] = static_cast<uint8>(d32[1] >> 8);
    out[i++] = static_cast<uint8>(d32[1]);
    out[i++] = static_cast<uint8>(d32[2] >> 24);
    out[i++] = static_cast<uint8>(d32[2] >> 16);
    out[i++] = static_cast<uint8>(d32[2] >> 8);
    out[i++] = static_cast<uint8>(d32[2]);
    out[i++] = static_cast<uint8>(d32[3] >> 24);
    out[i++] = static_cast<uint8>(d32[3] >> 16);
    out[i++] = static_cast<uint8>(d32[3] >> 8);
    out[i++] = static_cast<uint8>(d32[3]);
    out[i++] = static_cast<uint8>(d32[4] >> 24);
    out[i++] = static_cast<uint8>(d32[4] >> 16);
    out[i++] = static_cast<uint8>(d32[4] >> 8);
    out[i++] = static_cast<uint8>(d32[4]);
  }

 private:
  static uint32 LeftRotate(uint32 value, size_t count) noexcept {
    return (value << count) ^ (value >> (32 - count));
  }

  void GetDigest(uint32 (&digest)[5]) noexcept {
    const size_t bit_count = byte_count_ * 8;

    ProcessByte(0x80);
    if (block_byte_index_ > 56) {
      while (block_byte_index_ != 0) {
        ProcessByte(0);
      }
    }
    while (block_byte_index_ < 56) {
      ProcessByte(0);
    }

    ProcessByte(0);
    ProcessByte(0);
    ProcessByte(0);
    ProcessByte(0);
    ProcessByte(static_cast<uint8>((bit_count >> 24) & 0xFF));
    ProcessByte(static_cast<uint8>((bit_count >> 16) & 0xFF));
    ProcessByte(static_cast<uint8>((bit_count >> 8) & 0xFF));
    ProcessByte(static_cast<uint8>(bit_count & 0xFF));

    std::memcpy(digest, digest_, 5 * sizeof(uint32));
  }

  void ProcessBlock() noexcept {
    uint32 w[80];
    for (size_t i = 0; i < 16; ++i) {
      w[i] = static_cast<uint32>(block_[i * 4 + 0] << 24);
      w[i] |= static_cast<uint32>(block_[i * 4 + 1] << 16);
      w[i] |= static_cast<uint32>(block_[i * 4 + 2] << 8);
      w[i] |= static_cast<uint32>(block_[i * 4 + 3]);
    }
    for (size_t i = 16; i < 80; ++i) {
      w[i] = LeftRotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32 a = digest_[0];
    uint32 b = digest_[1];
    uint32 c = digest_[2];
    uint32 d = digest_[3];
    uint32 e = digest_[4];

    for (size_t i = 0; i < 80; ++i) {
      uint32 f;
      uint32 k;
      if (i < 20) {
        f = (b & c) | (~b & d);
        k = 0x5A827999;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }

      const uint32 temp = LeftRotate(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = LeftRotate(b, 30);
      b = a;
      a = temp;
    }

    digest_[0] += a;
    digest_[1] += b;
    digest_[2] += c;
    digest_[3] += d;
    digest_[4] += e;
  }

  uint32 digest_[5];
  uint8 block_[64];
  size_t block_byte_index_;
  size_t byte_count_;
};

}  // namespace detail

/// Uuid variant, encoded in the top bits of octet 8
/// (xxxxxxxx-xxxx-xxxx-Nxxx-xxxxxxxxxxxx).
enum class UuidVariant {
  /// NCS backward compatibility (obsolete Apollo NCS 1.5 format), 0xxx.
  Ncs,
  /// RFC 4122 / DCE 1.1, 10xx.
  Rfc,
  /// Microsoft COM backward compatibility, 110x.
  Microsoft,
  /// Reserved for future definition, 111x.
  Reserved,
};

/// Uuid version, encoded in the top bits of octet 6
/// (xxxxxxxx-xxxx-Mxxx-xxxx-xxxxxxxxxxxx).
enum class UuidVersion {
  None = 0,               ///< Only possible for nil or invalid UUIDs.
  TimeBased = 1,          ///< Time-based, RFC 4122.
  DceSecurity = 2,        ///< DCE Security, with embedded POSIX UIDs.
  NameBasedMd5 = 3,       ///< Name-based with MD5 hashing.
  RandomNumberBased = 4,  ///< Random or pseudo-random, RFC 4122.
  NameBasedSha1 = 5,      ///< Name-based with SHA-1 hashing.
};

/// A 128-bit universally unique identifier (RFC 4122).
class [[nodiscard]] Uuid {
 public:
  using ValueType = uint8;

  /// Default-constructs the nil Uuid (all bytes zero).
  constexpr Uuid() noexcept = default;

  constexpr Uuid(const std::array<ValueType, 16>& arr) noexcept : data_{arr} {}

  template <typename ForwardIt>
  explicit Uuid(ForwardIt first, ForwardIt last) {
    if (std::distance(first, last) == 16) {
      std::copy(first, last, data_.begin());
    }
  }

  [[nodiscard]] constexpr UuidVariant Variant() const noexcept {
    if ((data_[8] & 0x80) == 0x00) return UuidVariant::Ncs;
    if ((data_[8] & 0xC0) == 0x80) return UuidVariant::Rfc;
    if ((data_[8] & 0xE0) == 0xC0) return UuidVariant::Microsoft;
    return UuidVariant::Reserved;
  }

  [[nodiscard]] constexpr UuidVersion Version() const noexcept {
    if ((data_[6] & 0xF0) == 0x10) return UuidVersion::TimeBased;
    if ((data_[6] & 0xF0) == 0x20) return UuidVersion::DceSecurity;
    if ((data_[6] & 0xF0) == 0x30) return UuidVersion::NameBasedMd5;
    if ((data_[6] & 0xF0) == 0x40) return UuidVersion::RandomNumberBased;
    if ((data_[6] & 0xF0) == 0x50) return UuidVersion::NameBasedSha1;
    return UuidVersion::None;
  }

  [[nodiscard]] constexpr bool IsNil() const noexcept {
    for (size_t i = 0; i < data_.size(); ++i) {
      if (data_[i] != 0) return false;
    }
    return true;
  }

  void Swap(Uuid& other) noexcept { data_.swap(other.data_); }

  /// View the Uuid as its 16 raw bytes.
  [[nodiscard]] Span<const byte> AsBytes() const noexcept {
    return Span<const byte>(data_.data(), data_.size());
  }

  [[nodiscard]] constexpr uint8 operator[](size_t index) const noexcept {
    return data_[index];
  }

  /// Writes the 36 characters of the canonical textual representation to an
  /// output iterator, without allocating a string of its own.
  template <class OutputIt>
  OutputIt WriteTo(OutputIt out) const {
    for (size_t i = 0; i < 16; ++i) {
      if (i == 4 || i == 6 || i == 8 || i == 10) {
        *out++ = '-';
      }
      *out++ = detail::kHexChars[(data_[i] >> 4) & 0x0f];
      *out++ = detail::kHexChars[data_[i] & 0x0f];
    }
    return out;
  }

  /// Returns the canonical textual representation, e.g.
  /// "00112233-4455-6677-8899-aabbccddeeff".
  [[nodiscard]] std::string ToString() const {
    char buffer[36];
    WriteTo(buffer);
    return std::string(buffer, 36);
  }

  /// Tests whether a string is a valid Uuid textual representation, accepting
  /// an optional enclosing pair of braces.
  [[nodiscard]] static bool IsValid(std::string_view str) noexcept {
    bool first_digit = true;
    size_t has_braces = 0;
    size_t index = 0;

    if (str.empty()) return false;
    if (str.front() == '{') has_braces = 1;
    if (has_braces != 0 && str.back() != '}') return false;

    for (size_t i = has_braces; i < str.size() - has_braces; ++i) {
      if (str[i] == '-') continue;
      if (index >= 16 || !detail::IsHexDigit(str[i])) return false;
      if (first_digit) {
        first_digit = false;
      } else {
        ++index;
        first_digit = true;
      }
    }

    return index == 16;
  }

  /// Parses a Uuid from its textual representation, returning std::nullopt on
  /// failure. Accepts an optional enclosing pair of braces.
  [[nodiscard]] static std::optional<Uuid> FromString(std::string_view str) noexcept {
    bool first_digit = true;
    size_t has_braces = 0;
    size_t index = 0;
    std::array<ValueType, 16> data{};

    if (str.empty()) return std::nullopt;
    if (str.front() == '{') has_braces = 1;
    if (has_braces != 0 && str.back() != '}') return std::nullopt;

    for (size_t i = has_braces; i < str.size() - has_braces; ++i) {
      if (str[i] == '-') continue;
      if (index >= 16 || !detail::IsHexDigit(str[i])) return std::nullopt;
      if (first_digit) {
        data[index] = static_cast<ValueType>(detail::HexToValue(str[i]) << 4);
        first_digit = false;
      } else {
        data[index] = static_cast<ValueType>(data[index] | detail::HexToValue(str[i]));
        ++index;
        first_digit = true;
      }
    }

    if (index != 16) return std::nullopt;
    return Uuid{data};
  }

  friend bool operator==(const Uuid& lhs, const Uuid& rhs) noexcept {
    return lhs.data_ == rhs.data_;
  }

  friend bool operator<(const Uuid& lhs, const Uuid& rhs) noexcept {
    return lhs.data_ < rhs.data_;
  }

 private:
  std::array<ValueType, 16> data_{};
};

[[nodiscard]] inline bool operator!=(const Uuid& lhs, const Uuid& rhs) noexcept {
  return !(lhs == rhs);
}

inline void Swap(Uuid& lhs, Uuid& rhs) noexcept { lhs.Swap(rhs); }

template <class Elem, class Traits>
std::basic_ostream<Elem, Traits>& operator<<(std::basic_ostream<Elem, Traits>& os,
                                             const Uuid& id) {
  Elem buffer[36];
  id.WriteTo(buffer);
  os.write(buffer, 36);
  return os;
}

/// Predefined Uuid namespaces for name-based Uuid generation (RFC 4122 §4.3).

/// Name string is a fully-qualified domain name.
inline constexpr Uuid kUuidNamespaceDns{
    std::array<Uuid::ValueType, 16>{0x6b, 0xa7, 0xb8, 0x10, 0x9d, 0xad, 0x11, 0xd1, 0x80,
                                    0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8}};

/// Name string is a URL.
inline constexpr Uuid kUuidNamespaceUrl{
    std::array<Uuid::ValueType, 16>{0x6b, 0xa7, 0xb8, 0x11, 0x9d, 0xad, 0x11, 0xd1, 0x80,
                                    0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8}};

/// Name string is an ISO OID.
inline constexpr Uuid kUuidNamespaceOid{
    std::array<Uuid::ValueType, 16>{0x6b, 0xa7, 0xb8, 0x12, 0x9d, 0xad, 0x11, 0xd1, 0x80,
                                    0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8}};

/// Name string is an X.500 DN, in DER or a text output format.
inline constexpr Uuid kUuidNamespaceX500{
    std::array<Uuid::ValueType, 16>{0x6b, 0xa7, 0xb8, 0x14, 0x9d, 0xad, 0x11, 0xd1, 0x80,
                                    0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8}};

/// Random (version 4) Uuid generator, driven by an external random engine.
template <typename UniformRandomNumberGenerator>
class BasicUuidRandomGenerator {
 public:
  using EngineType = UniformRandomNumberGenerator;

  explicit BasicUuidRandomGenerator(EngineType& gen)
      : generator_(&gen, [](EngineType*) {}) {}
  explicit BasicUuidRandomGenerator(EngineType* gen)
      : generator_(gen, [](EngineType*) {}) {}

  [[nodiscard]] Uuid operator()() {
    std::array<Uuid::ValueType, 16> bytes{};
    for (size_t i = 0; i < bytes.size(); i += sizeof(uint32)) {
      const uint32 value = distribution_(*generator_);
      std::memcpy(bytes.data() + i, &value, sizeof(value));
    }

    // Variant must be 10xxxxxx.
    bytes[8] &= 0xBF;
    bytes[8] |= 0x80;

    // Version must be 0100xxxx.
    bytes[6] &= 0x4F;
    bytes[6] |= 0x40;

    return Uuid{bytes};
  }

 private:
  std::uniform_int_distribution<uint32> distribution_;
  std::shared_ptr<UniformRandomNumberGenerator> generator_;
};

using UuidRandomGenerator = BasicUuidRandomGenerator<std::mt19937>;

/// Name-based (version 5) Uuid generator, using SHA-1 hashing of a namespace
/// Uuid and a name (RFC 4122 §4.3).
class UuidNameGenerator {
 public:
  explicit UuidNameGenerator(const Uuid& namespace_uuid) noexcept
      : namespace_uuid_(namespace_uuid) {}

  template <typename StringType>
  [[nodiscard]] Uuid operator()(const StringType& name) {
    Reset();
    ProcessCharacters(detail::ToStringView(name));
    return MakeUuid();
  }

 private:
  void Reset() noexcept {
    hasher_.Reset();
    const Span<const byte> bytes = namespace_uuid_.AsBytes();
    hasher_.ProcessBytes(bytes.data(), bytes.size());
  }

  template <typename CharT, typename Traits>
  void ProcessCharacters(std::basic_string_view<CharT, Traits> str) noexcept {
    for (uint32 c : str) {
      hasher_.ProcessByte(static_cast<uint8>(c & 0xFF));
      if constexpr (!std::is_same_v<CharT, char>) {
        hasher_.ProcessByte(static_cast<uint8>((c >> 8) & 0xFF));
        hasher_.ProcessByte(static_cast<uint8>((c >> 16) & 0xFF));
        hasher_.ProcessByte(static_cast<uint8>((c >> 24) & 0xFF));
      }
    }
  }

  [[nodiscard]] Uuid MakeUuid() noexcept {
    uint8 digest[20];
    hasher_.GetDigestBytes(digest);

    // Variant must be 10xxxxxx.
    digest[8] &= 0xBF;
    digest[8] |= 0x80;

    // Version must be 0101xxxx.
    digest[6] &= 0x5F;
    digest[6] |= 0x50;

    return Uuid{digest, digest + 16};
  }

  Uuid namespace_uuid_;
  detail::Sha1 hasher_;
};

}  // namespace amaryl

namespace std {

template <>
struct hash<amaryl::Uuid> {
  [[nodiscard]] size_t operator()(const amaryl::Uuid& id) const noexcept {
    const auto bytes = id.AsBytes();
    return static_cast<size_t>(amaryl::hashBytes(bytes.data(), uint32(bytes.size())));
  }
};

}  // namespace std
