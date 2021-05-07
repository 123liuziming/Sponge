#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

inline uint64_t abs_diff(uint64_t a, uint64_t b) {
    return (a > b ? a - b : b - a);
}

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    return isn + n;
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    auto diff = n - isn;
    const uint64_t mask = 1ul << 32;
    uint64_t modLeft = checkpoint / mask * mask;
    uint64_t modRight = (checkpoint / mask + 1) * mask;
    auto ptLeft = modLeft >= INT32_MAX || modLeft + diff > INT64_MAX ? modLeft + diff : modLeft + static_cast<uint32_t>(diff);
    auto ptRight = modRight >= INT32_MAX || modRight + diff >= INT64_MAX ? modRight + diff : modRight + static_cast<uint32_t>(diff);
    const uint64_t disLeft = abs_diff(checkpoint, ptLeft);
    const uint64_t disRight = abs_diff(checkpoint, ptRight);
    return disLeft <= disRight ? ptLeft : ptRight;
}
