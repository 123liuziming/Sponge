#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <iostream>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _RTO{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    while (_next_seqno < (_window_size == 0 && _window_right_edge == _next_seqno ? _window_right_edge + 1 : _window_right_edge)) {
        TCPSegment segment;
        TCPHeader header;
        header.seqno = next_seqno();
        //取窗口和可取数据的最小值
        auto real_size = TCPConfig::MAX_PAYLOAD_SIZE;
        //说明发送的是SYN
        if (_next_seqno == 0) {
            real_size = 0;
            header.syn = true;
            ++_next_seqno;
        }
        //读入body
        auto real_edge = _window_right_edge + (_window_size == 0 && _window_right_edge == _next_seqno);
        real_size = std::min(real_size, real_edge - _next_seqno);
        segment.payload() = Buffer(_stream.read(real_size));
        real_size = std::min(real_size, segment.payload().size());
        _next_seqno += real_size;
        if (_stream.eof() && _abs_fin == 0 && _next_seqno < real_edge) {
            header.fin = true;
            _abs_fin = _next_seqno;
            ++_next_seqno;
        }
        segment.header() = header;
        _bytes_in_flight += segment.length_in_sequence_space();
        //笨逼报文不配发
        if (segment.length_in_sequence_space() == 0) {
            return;
        }
        //发送报文，将报文添加到重传队列中暂时保存
        _segments_out.push(segment);
        _outstandings.push(segment);
        //如果没有启动计时器，则启动计时器，并重置剩余时间
        if (!_timer_flag) {
            _timer_flag = true;
            _remain_ticks = _RTO;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    //更新窗口大小与右边界
    _window_size = window_size;
    auto ack_no_unwrapped = unwrap(ackno, _isn, _next_seqno);
    auto ack_length = ack_no_unwrapped - _last_ack;
    _window_right_edge = window_size + ack_no_unwrapped;
    //确认了太久之前的报文，应该丢弃
    if (_last_ack >= ack_no_unwrapped || ack_no_unwrapped > _next_seqno) {
        return;
    }
    //已经重传完所有数据，关闭定时器
    _bytes_in_flight -= ack_length;
    if (!_bytes_in_flight) {
        _timer_flag = false;
    }
    while (!_outstandings.empty()) {
        auto &front = _outstandings.front();
        //如果这个报文段被此ack完全覆盖，则说明接收方已经完全收到，不需要再重传
        if (unwrap(front.header().seqno, _isn, _next_seqno) + front.length_in_sequence_space() <= ack_no_unwrapped) {
            _outstandings.pop();
        } else {
            break;
        }
    }
    //设置ack
    _last_ack = ack_no_unwrapped;
    _RTO = _initial_retransmission_timeout;
    _remain_ticks = _RTO;
    //连续重传报文数重置
    _consecutive_retransmissions = 0;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    //没开启计时器，返回
    if (!_timer_flag) {
        return;
    }
    _remain_ticks -= ms_since_last_tick;
    //如果已经到了时间
    if (_remain_ticks <= 0 && !_outstandings.empty()) {
        //重新传输报文段
        _segments_out.push(_outstandings.front());
        if (_window_size > 0 || _window_right_edge == 0) {
            //递增连续重新传输
            ++_consecutive_retransmissions;
            //将RTO翻倍
            _RTO <<= 1;
        }
        _remain_ticks = _RTO;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    //这些报文段不会被重传，也不占用序号
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    _segments_out.push(segment);
}
