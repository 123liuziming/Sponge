#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] routePrefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefixLength For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t routePrefix,
                       const uint8_t prefixLength,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(routePrefix).ip() << "/" << int(prefixLength)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";
    _route_list.push_back(RouteItem{routePrefix, prefixLength, next_hop, interface_num});
}

bool Router::prefix_equal(uint32_t ip1, uint32_t ip2, uint8_t len) {
    uint32_t offset = (len == 0) ? 0 : 0xffffffff << (32 - len);
    return (ip1 & offset) == (ip2 & offset);
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    bool found = false;
    RouteItem item;
    uint32_t dst_ip = dgram.header().dst;
    const size_t len = _route_list.size();
    for (size_t i = 0; i < len; i++) {
        if (prefix_equal(dst_ip, _route_list[i].route_prefix, _route_list[i].prefix_length)) {
            if (!found || item.prefix_length < _route_list[i].prefix_length) {
                item = _route_list[i];
                found = true;
            }
        }
    }
    if (!found) {
        return;
    }
    if (dgram.header().ttl <= 1) {
        return;
    }
    --dgram.header().ttl;
    if (item.next_hop.has_value()) {
        _interfaces[item.interface_num].send_datagram(dgram, item.next_hop.value());
    }
    else {
        _interfaces[item.interface_num].send_datagram(dgram, Address::from_ipv4_numeric(dgram.header().dst));
    }
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
