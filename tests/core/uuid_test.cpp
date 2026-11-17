#include "amaryl/core/uuid.h"

#include <functional>
#include <random>
#include <sstream>
#include <string>

#include "boost/ut.hpp"

using namespace amaryl;
using namespace boost::ut;

int main() {
  "nil uuid is all zero"_test = [] {
    const Uuid nil{};
    expect(nil.IsNil());
    expect(nil.Variant() == UuidVariant::Ncs);
    expect(nil.Version() == UuidVersion::None);
    expect(nil.ToString() == "00000000-0000-0000-0000-000000000000");
  };

  "to_string and from_string roundtrip"_test = [] {
    const auto id = Uuid::FromString("00112233-4455-6677-8899-aabbccddeeff");
    expect(id.has_value());
    expect(id->ToString() == "00112233-4455-6677-8899-aabbccddeeff");
    expect((*id)[0] == 0x00);
    expect((*id)[1] == 0x11);
    expect((*id)[15] == 0xff);

    const auto bytes = id->AsBytes();
    expect(bytes.size() == 16u);
    expect(bytes[0] == 0x00);
    expect(bytes[15] == 0xff);
  };

  "from_string rejects invalid input"_test = [] {
    expect(!Uuid::FromString("").has_value());
    expect(!Uuid::FromString("not-a-uuid").has_value());
    // Too short.
    expect(!Uuid::FromString("00112233-4455-6677-8899-aabbccddee").has_value());
    // Too long.
    expect(!Uuid::FromString("00112233-4455-6677-8899-aabbccddeeff00").has_value());
    // Non-hexadecimal digit.
    expect(!Uuid::FromString("00112233-4455-6677-8899-aabbccddeefg").has_value());
  };

  "is_valid accepts braces and rejects garbage"_test = [] {
    expect(Uuid::IsValid("00112233-4455-6677-8899-aabbccddeeff"));
    expect(Uuid::IsValid("{00112233-4455-6677-8899-aabbccddeeff}"));
    expect(!Uuid::IsValid(""));
    expect(!Uuid::IsValid("00112233-4455-6677-8899-aabbccddee"));  // too short
    expect(!Uuid::IsValid("{00112233-4455-6677-8899-aabbccddeeff"));  // missing '}'
    expect(!Uuid::IsValid("xyz"));
  };

  "comparison operators"_test = [] {
    const Uuid a = *Uuid::FromString("00000000-0000-0000-0000-000000000001");
    const Uuid b = *Uuid::FromString("00000000-0000-0000-0000-000000000002");
    const Uuid c = a;

    expect(a == c);
    expect(a != b);
    expect(a < b);
    expect(!(b < a));
  };

  "random generator produces version 4"_test = [] {
    std::mt19937 engine{42u};
    UuidRandomGenerator gen(engine);

    const Uuid id = gen();
    expect(!id.IsNil());
    expect(id.Variant() == UuidVariant::Rfc);
    expect(id.Version() == UuidVersion::RandomNumberBased);

    // The same seed must produce the same UUID.
    std::mt19937 engine2{42u};
    UuidRandomGenerator gen2(engine2);
    expect(gen2() == id);
  };

  "name generator produces version 5 (known vector)"_test = [] {
    UuidNameGenerator gen(kUuidNamespaceDns);
    const Uuid id = gen("python.org");
    expect(id.Variant() == UuidVariant::Rfc);
    expect(id.Version() == UuidVersion::NameBasedSha1);

    const Uuid expected = *Uuid::FromString("886313e1-3b8a-5372-9b90-0c9aee199e5d");
    expect(id == expected);
  };

  "predefined namespaces are rfc time-based"_test = [] {
    const Uuid namespaces[] = {kUuidNamespaceDns, kUuidNamespaceUrl,
                               kUuidNamespaceOid, kUuidNamespaceX500};
    for (const Uuid& ns : namespaces) {
      expect(ns.Variant() == UuidVariant::Rfc);
      expect(ns.Version() == UuidVersion::TimeBased);
    }
  };

  "hash is consistent for equal uuids"_test = [] {
    const Uuid a = *Uuid::FromString("00112233-4455-6677-8899-aabbccddeeff");
    const Uuid b = *Uuid::FromString("00112233-4455-6677-8899-aabbccddeeff");
    expect(std::hash<Uuid>{}(a) == std::hash<Uuid>{}(b));
  };

  "stream output matches to_string"_test = [] {
    const Uuid id = *Uuid::FromString("00112233-4455-6677-8899-aabbccddeeff");
    std::ostringstream oss;
    oss << id;
    expect(oss.str() == id.ToString());
  };

  return 0;
}
