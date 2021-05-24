#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;
#include <iostream>

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity), _unorderedItem(), _unorderedBytes(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    //能写入的长度
    auto lengthWritable = std::min(_output.remaining_capacity(), data.size());
    //如果正好能接上
    if (index == _indexNow) {
        auto bytesWritten = _output.write(data.substr(0, lengthWritable));
        if (eof) {
            _output.end_input();
        }
        if (bytesWritten == 0) {
            return;
        }
        for (size_t i = 0; i < lengthWritable; ++i) {
            if (_unorderedItem.count(index + i)) {
                _unorderedItem.erase(index + i);
                --_unorderedBytes;
            }
        }
        _indexNow += bytesWritten;
    }
    //完全接不上，放在map里面
    else if (index > _indexNow) {
        for (size_t i = 0; i < lengthWritable; ++i) {
            if (_unorderedItem.count(index + i) || _unorderedBytes >= _capacity) {
                continue;
            }
            bool realEof = i == lengthWritable - 1 && eof;
            _unorderedItem.insert(std::make_pair(index + i, std::make_pair(data[i], realEof)));
            ++_unorderedBytes;
        }
        return;
    }
    //有一部分可以接上
    else {
        if (data.size() + index > _indexNow) {
            for (size_t i = _indexNow - index; i < data.size(); ++i) {
                if (_unorderedItem.count(index + i) || _unorderedBytes >= _capacity) {
                    continue;
                }
                bool realEof = i == data.size() - 1 && eof;
                _unorderedItem.insert(std::make_pair(index + i, std::make_pair(data[i], realEof)));
                ++_unorderedBytes;
            }
        }
        //被完全覆盖，不用管了
        else {
            return;
        }
    }

    //补全
    std::string fix;
    while (_unorderedItem.count(_indexNow)) {
        auto item = _unorderedItem[_indexNow];
        _unorderedItem.erase(_indexNow);
        --_unorderedBytes;
        fix += item.first;
        if (item.second) {
            _output.end_input();
        }
        if (fix.length() >= _output.remaining_capacity()) {
            break;
        }
        ++_indexNow;
    }
    while (fix.length() >= _output.remaining_capacity() && _unorderedItem.count(_indexNow + 1)) {
        ++_indexNow;
        _unorderedItem.erase(_indexNow);
    }
    cout << "_indexNow: " << _indexNow << endl;
    _output.write(fix);
}


size_t StreamReassembler::unassembled_bytes() const {
    return _unorderedBytes;
}

bool StreamReassembler::empty() const {
    return stream_out().buffer_empty();
}

//给tcp receiver使用
size_t StreamReassembler::getIndexNow() const {
    return _indexNow;
}

