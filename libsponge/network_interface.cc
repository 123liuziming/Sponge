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
    EthernetAddress ethernet_address;
    //如果已经知道地址，直接送出去
    EthernetFrame frame_to_send;
    EthernetFrame origin_frame;
    origin_frame.header().src = _ethernet_address;
    origin_frame.header().type = EthernetHeader::TYPE_IPv4;
    origin_frame.payload().append(dgram.serialize());
    frame_to_send.header().src = _ethernet_address;
    if (_cache.find(next_hop_ip) != _cache.end()) {
        ethernet_address = _cache[next_hop_ip].first;
        frame_to_send.header().dst = ethernet_address;
        frame_to_send.header().type = EthernetHeader::TYPE_IPv4;
        frame_to_send.payload().append(dgram.serialize());
        _frames_out.push(frame_to_send);
    }
    else {
        //已经有这条了，不要重复发送
        if (_eth_set.find(next_hop_ip) == _eth_set.end()) {
            //不知道地址，广播下一跳地址
            frame_to_send.header().type = EthernetHeader::TYPE_ARP;
            frame_to_send.header().dst = ETHERNET_BROADCAST;
            ARPMessage message;
            message.opcode = ARPMessage::OPCODE_REQUEST;
            message.sender_ip_address = _ip_address.ipv4_numeric();
            message.target_ip_address = next_hop_ip;
            message.sender_ethernet_address = _ethernet_address;
            frame_to_send.payload().append(message.serialize());
            _eth_set.insert(make_pair(next_hop_ip, make_pair(origin_frame, _tick_send)));
            _frames_out.push(frame_to_send);
        }
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    bool is_broadcast = frame.header().dst == ETHERNET_BROADCAST;
    bool is_this = frame.header().dst == _ethernet_address;
    auto type = frame.header().type;
    InternetDatagram internet_datagram;
    if (type == EthernetHeader::TYPE_IPv4) {
        auto parse_result = internet_datagram.parse(frame.payload());
        return (is_broadcast || is_this) && parse_result == ParseResult::NoError ? std::optional<InternetDatagram>(internet_datagram) : nullopt;
    }
    else if (type == EthernetHeader::TYPE_ARP) {
        ARPMessage message;
        auto parse_result = message.parse(frame.payload());
        if (parse_result == ParseResult::NoError) {
            _cache.insert(make_pair(message.sender_ip_address, make_pair(message.sender_ethernet_address, _tick_expire)));
            _cache.insert(make_pair(message.target_ip_address, make_pair(message.target_ethernet_address, _tick_expire)));
            //找到了对应的，发送响应报文
            if (message.target_ip_address == _ip_address.ipv4_numeric()) {
                EthernetFrame frame_out;
                if (frame.header().dst != ETHERNET_BROADCAST) {
                    //重发IP报文
                    frame_out = _eth_set[message.sender_ip_address].first;
                    frame_out.header().dst = message.sender_ethernet_address;
                }
                else {
                    frame_out.header().src = _ethernet_address;
                    frame_out.header().dst = message.sender_ethernet_address;
                    frame_out.header().type = EthernetHeader::TYPE_ARP;
                    ARPMessage reply;
                    reply.opcode = ARPMessage::OPCODE_REPLY;
                    reply.sender_ethernet_address = _ethernet_address;
                    reply.target_ethernet_address = message.sender_ethernet_address;
                    reply.sender_ip_address = _ip_address.ipv4_numeric();
                    reply.target_ip_address = message.sender_ip_address;
                    frame_out.payload().append(reply.serialize());
                }
                _frames_out.push(frame_out);
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
    _tick_send += ms_since_last_tick;
    _tick_expire += ms_since_last_tick;
    auto tick_iter = _eth_set.begin();
    while (tick_iter != _eth_set.end()) {
        if (_tick_send - tick_iter->second.second >= 5 * 1000) {
            tick_iter = _eth_set.erase(tick_iter);
        }
        else {
            ++tick_iter;
        }
    }
    //30秒清空一次缓存
    auto iter = _cache.begin();
    while (iter != _cache.end()) {
        if (_tick_expire - iter->second.second >= 30 * 1000) {
            iter = _cache.erase(iter);
        }
        else {
            ++iter;
        }
    }
}
