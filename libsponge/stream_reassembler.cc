#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;
#include <iostream>

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity), _index_now(0), _unordered_item(), _unordered_bytes(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    //能写入的长度
    auto length_writable = std::min(_output.remaining_capacity(), data.size());
    //如果正好能接上
    if (index == _index_now) {
        auto bytes_written = _output.write(data.substr(0, length_writable));
        if (eof) {
            _output.end_input();
        }
        if (bytes_written == 0) {
            return;
        }
        for (size_t i = 0; i < length_writable; ++i) {
            if (_unordered_item.count(index + i)) {
                _unordered_item.erase(index + i);
                --_unordered_bytes;
            }
        }
        _index_now += bytes_written;
    }
    //完全接不上，放在map里面
    else if (index > _index_now) {
        for (size_t i = 0; i < length_writable; ++i) {
            if (_unordered_item.count(index + i) || _unordered_bytes >= _capacity) {
                continue;
            }
            bool real_eof = i == length_writable - 1 && eof;
            _unordered_item.insert(std::make_pair(index + i, std::make_pair(data[i], real_eof)));
            ++_unordered_bytes;
        }
        return;
    }
    //有一部分可以接上
    else {
        if (data.size() + index > _index_now) {
            for (size_t i = _index_now - index; i < data.size(); ++i) {
                if (_unordered_item.count(index + i) || _unordered_bytes >= _capacity) {
                    continue;
                }
                bool real_eof = i == data.size() - 1 && eof;
                _unordered_item.insert(std::make_pair(index + i, std::make_pair(data[i], real_eof)));
                ++_unordered_bytes;
            }
        }
        //被完全覆盖，不用管了
        else {
            return;
        }
    }

    //补全
    std::string fix;
    while (_unordered_item.count(_index_now)) {
        auto item = _unordered_item[_index_now];
        _unordered_item.erase(_index_now);
        --_unordered_bytes;
        fix += item.first;
        if (item.second) {
            _output.end_input();
        }
        if (fix.length() >= _output.remaining_capacity()) {
            break;
        }
        ++_index_now;
    }
    while (fix.length() >= _output.remaining_capacity() && _unordered_item.count(_index_now + 1)) {
        ++_index_now;
        _unordered_item.erase(_index_now);
    }
    //cout << "_index_now: " << _index_now << endl;
    _output.write(fix);
}


size_t StreamReassembler::unassembled_bytes() const {
    return _unordered_bytes;
}

bool StreamReassembler::empty() const {
    return stream_out().buffer_empty();
}

//给tcp receiver使用
size_t StreamReassembler::get_index_now() const {
    return _index_now;
}

