#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    EthernetAddress ethernetAddress;
    //如果已经知道地址，直接送出去
    EthernetFrame frameToSend;
    EthernetFrame originFrame;
    originFrame.header().src = _ethernet_address;
    originFrame.header().type = EthernetHeader::TYPE_IPv4;
    originFrame.payload().append(dgram.serialize());
    frameToSend.header().src = _ethernet_address;
    if (_cache.find(next_hop_ip) != _cache.end()) {
        ethernetAddress = _cache[next_hop_ip].first;
        frameToSend.header().dst = ethernetAddress;
        frameToSend.header().type = EthernetHeader::TYPE_IPv4;
        frameToSend.payload().append(dgram.serialize());
        _frames_out.push(frameToSend);
    }
    else {
        //已经有这条了，不要重复发送
        if (_ethSet.find(next_hop_ip) == _ethSet.end()) {
            //不知道地址，广播下一跳地址
            frameToSend.header().type = EthernetHeader::TYPE_ARP;
            frameToSend.header().dst = ETHERNET_BROADCAST;
            ARPMessage message;
            message.opcode = ARPMessage::OPCODE_REQUEST;
            message.sender_ip_address = _ip_address.ipv4_numeric();
            message.target_ip_address = next_hop_ip;
            message.sender_ethernet_address = _ethernet_address;
            frameToSend.payload().append(message.serialize());
            _ethSet.insert(make_pair(next_hop_ip, make_pair(originFrame, _tickSend)));
            _frames_out.push(frameToSend);
        }
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    bool isBroadcast = frame.header().dst == ETHERNET_BROADCAST;
    bool isThis = frame.header().dst == _ethernet_address;
    auto type = frame.header().type;
    InternetDatagram internetDatagram;
    if (type == EthernetHeader::TYPE_IPv4) {
        auto parseResult = internetDatagram.parse(frame.payload());
        return (isBroadcast || isThis) && parseResult == ParseResult::NoError ? std::optional<InternetDatagram>(internetDatagram) : nullopt;
    }
    else if (type == EthernetHeader::TYPE_ARP) {
        ARPMessage message;
        auto parseResult = message.parse(frame.payload());
        if (parseResult == ParseResult::NoError) {
            _cache.insert(make_pair(message.sender_ip_address, make_pair(message.sender_ethernet_address, _tickExpire)));
            _cache.insert(make_pair(message.target_ip_address, make_pair(message.target_ethernet_address, _tickExpire)));
            //找到了对应的，发送响应报文
            if (message.target_ip_address == _ip_address.ipv4_numeric()) {
                EthernetFrame frameOut;
                if (frame.header().dst != ETHERNET_BROADCAST) {
                    //重发IP报文
                    frameOut = _ethSet[message.sender_ip_address].first;
                    frameOut.header().dst = message.sender_ethernet_address;
                }
                else {
                    frameOut.header().src = _ethernet_address;
                    frameOut.header().dst = message.sender_ethernet_address;
                    frameOut.header().type = EthernetHeader::TYPE_ARP;
                    ARPMessage reply;
                    reply.opcode = ARPMessage::OPCODE_REPLY;
                    reply.sender_ethernet_address = _ethernet_address;
                    reply.target_ethernet_address = message.sender_ethernet_address;
                    reply.sender_ip_address = _ip_address.ipv4_numeric();
                    reply.target_ip_address = message.sender_ip_address;
                    frameOut.payload().append(reply.serialize());
                }
                _frames_out.push(frameOut);
            }
        }
        else {
            return nullopt;
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _tickSend += ms_since_last_tick;
    _tickExpire += ms_since_last_tick;
    auto tickIter = _ethSet.begin();
    while (tickIter != _ethSet.end()) {
        if (_tickSend - tickIter->second.second >= 5 * 1000) {
            tickIter = _ethSet.erase(tickIter);
        }
        else {
            ++tickIter;
        }
    }
    //30秒清空一次缓存
    auto iter = _cache.begin();
    while (iter != _cache.end()) {
        if (_tickExpire - iter->second.second >= 30 * 1000) {
            iter = _cache.erase(iter);
        }
        else {
            ++iter;
        }
    }
}
