#include <gtest/gtest.h>

#include "kfc/database/sha256.hpp"

using kfc::database::sha256_hex;

// FIPS 180-4 / NIST published test vectors.
TEST(Sha256Test, EmptyStringVector) {
    EXPECT_EQ(sha256_hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(Sha256Test, AbcVector) {
    EXPECT_EQ(sha256_hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Sha256Test, LongerMessageSpanningTwoBlocks) {
    EXPECT_EQ(sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST(Sha256Test, DifferentInputsDifferByAvalanche) {
    EXPECT_NE(sha256_hex("password"), sha256_hex("Password"));
}
