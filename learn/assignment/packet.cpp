// =============================================================================
// packet.cpp — ASSIGNMENT PART A: implement encode() and decode()
// =============================================================================
//
// Relevant chapters: ch10 (fixed-width ints, static_cast), ch16 (vector), ch17 (array),
// ch11 (templates, big-endian), ch12.15 (std::optional), net1 (exceptions).
//
// When you start a function, delete its [[maybe_unused]] markers and the
// placeholder return.
// =============================================================================
#include "packet.h"   // always include your own header first

#include <stdexcept>

namespace chat {

std::vector<std::uint8_t> encode([[maybe_unused]] const Packet& p)
{
    // TODO(A1): if p.text is longer than kMaxText, throw std::length_error.
    // TODO(A2): make a vector, reserve kHeaderSize + text size.
    // TODO(A3): push the 4 bytes of p.seq, HIGHEST byte first.
    //           (seq >> 24), (seq >> 16), (seq >> 8), seq — each cast to uint8_t.
    // TODO(A4): append the text's bytes.
    return {};
}

std::optional<Packet> decode([[maybe_unused]] std::span<const std::uint8_t> bytes)
{
    // TODO(A5): reject anything shorter than kHeaderSize or longer than kMaxPacket.
    // TODO(A6): rebuild seq from bytes[0..3]: seq = (seq << 8) | byte, four times.
    //           Careful: do the math in std::uint32_t, not char/int.
    // TODO(A7): the text is everything after the header (span::subspan).
    return std::nullopt;
}

} // namespace chat
