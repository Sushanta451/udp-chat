// =============================================================================
// sequence_tracker.cpp — ASSIGNMENT PART B: implement the tracker
// =============================================================================
//
// Relevant chapters: ch13 (enum class, switch), ch14 (classes, invariants),
// ch16 (vector), ch18 (algorithms), ch10 (unsigned math: watch out for highest - 1!).
//
// Suggested order: toString → highest/receivedCount → onReceive → missing.
// Run the tests after each step: more of them pass as you go.
// =============================================================================
#include "sequence_tracker.h"

namespace chat {

const char* toString([[maybe_unused]] Arrival a)
{
    // TODO(B1): a switch over every Arrival value (ch13.4).
    return "?";
}

Event SequenceTracker::onReceive([[maybe_unused]] std::uint32_t seq)
{
    // TODO(B2): handle the five cases in the header, in this order:
    //   1. seq == 0                         → Invalid
    //   2. already received?                → Duplicate
    //   3. seq == highest + 1               → InOrder
    //   4. seq  > highest + 1               → Gap (record + return the newly missing)
    //   5. otherwise                        → Late (it's no longer missing)
    // Update your private state in each case where something new arrived.
    return {};
}

std::uint32_t SequenceTracker::highest() const
{
    return 0;   // TODO(B3)
}

std::size_t SequenceTracker::receivedCount() const
{
    return 0;   // TODO(B4)
}

std::vector<std::uint32_t> SequenceTracker::missing() const
{
    return {};   // TODO(B5)
}

} // namespace chat
