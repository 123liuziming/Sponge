#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const {
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const {
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const {
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
    return _time_since_last_segment_received;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    _time_since_last_segment_received = 0;
    //收到了RST，永久终止连接
    if (seg.header().rst) {
        end_connection();
        return;
    }
    //如果ACK置位，告知发送方ackno和窗口大小
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }
    //接收方接收segment
    _receiver.segment_received(seg);
    if (_receiver.stream_out().eof() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }

    //如果收到的不是单纯的ack，则必须要回复
    if (seg.length_in_sequence_space()) {
        if (_sender.next_seqno_absolute()) {
            _sender.send_empty_segment();
        }
        send_wrapped();
    }
}

void TCPConnection::end_connection() {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
}

void TCPConnection::send_reply() {
    if (!active()) {
        return;
    }
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            _last_ack_no_sent = seg.header().ackno;
        }
        seg.header().win = _receiver.window_size();
        segments_out().push(seg);
        _sender.segments_out().pop();
    }
}

void TCPConnection::send_rst() {
    TCPSegment seg;
    seg.header().seqno = _sender.next_seqno();
    seg.header().rst = true;
    segments_out().push(seg);
}

bool TCPConnection::active() const {
    //outbound and inbound are both in the error state
    if (_sender.stream_in().error() && _receiver.stream_out().error()) {
        return false;
    }
    if (_receiver.stream_out().eof() && _sender.stream_in().eof() && !need_send_ack() && _sender.bytes_in_flight() == 0) {
        return _linger_after_streams_finish && _time_since_last_segment_received < 10 * _cfg.rt_timeout;
    }
    return true;
}

size_t TCPConnection::write(const string &data) {
    auto bytesWritten = _sender.stream_in().write(data);
    send_wrapped();
    return bytesWritten;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    //重传次数太多的话直接发送reset报文
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        send_rst();
        end_connection();
    }
    if (_time_since_last_segment_received >= 10 * _cfg.rt_timeout && !active()) {
        _linger_after_streams_finish = false;
    }
    if (!_sender.next_seqno_absolute()) {
        return;
    }
    send_wrapped();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    send_wrapped();
}

void TCPConnection::connect() {
    //发送SYN请求
    send_wrapped();
}

bool TCPConnection::need_send_ack() const {
    //判断是否需要发送ack
    //如果收到的不是空的ack报文，就要发送
    return _receiver.ackno().has_value() && (!_last_ack_no_sent.has_value() || _last_ack_no_sent.value() != _receiver.ackno().value());
}

void TCPConnection::send_wrapped() {
    _sender.fill_window();
    //可能需要发送空的ack
    if (_sender.segments_out().empty() && need_send_ack()) {
        _sender.send_empty_segment();
    }
    //报文需要加ack
    send_reply();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            send_rst();
            end_connection();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
