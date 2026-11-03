// =============================================================================
// packet.h — the chat's wire format          (ASSIGNMENT PART A: declarations)
// =============================================================================
//
// This header is DONE. You implement the two functions in packet.cpp.
//
// Wire format of one UDP datagram:
//
//     byte:    0      1      2      3      4 ............ end
//            +------+------+------+------+------------------+
//            |  seq: uint32, big-endian  | text (no '\0')   |
//            +------+------+------+------+------------------+
//
//   Example: Packet{300, "hi"}  →  00 00 01 2c 68 69
//
// =============================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace chat {

inline constexpr std::size_t kHeaderSize = 4;
inline constexpr std::size_t kMaxPacket = 1200;   // stay well under a 1500-byte MTU
inline constexpr std::size_t kMaxText = kMaxPacket - kHeaderSize;

struct Packet {
    std::uint32_t seq = 0;
    std::string text;

    bool operator==(const Packet&) const = default;
};

// Turn a Packet into bytes ready for sendto().
// Throws std::length_error if p.text is longer than kMaxText.
std::vector<std::uint8_t> encode(const Packet& p);

// Turn bytes from recvfrom() back into a Packet.
// Returns std::nullopt if the bytes can't be a valid packet (fewer than
// kHeaderSize bytes, or more than kMaxPacket). NEVER crashes on bad input:
// these bytes come from the network and can be anything.
std::optional<Packet> decode(std::span<const std::uint8_t> bytes);

} // namespace chat
