#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    //syn代表建立连接，此时负载里不能有数据
    if (seg.header().syn && !seg.payload().size()) {
        _isn = seg.header().seqno;
        return;
    }
    //要结束连接了
    if (seg.header().fin && !seg.payload().size()) {
        _reassembler.stream_out().end_input();
        return;
    }
    const uint64_t checkpoint = _reassembler.stream_out().bytes_written();
    _reassembler.push_substring(seg.payload().copy(), unwrap(seg.header().seqno, _isn, checkpoint) , seg.header().fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    return std::optional<WrappingInt32>(_reassembler.getIndexNow());
}

size_t TCPReceiver::window_size() const {
    return _reassembler.stream_out().remaining_capacity();
}
