//
// Created by Tyler on 2/12/23.
//

#include "seen-event-set.h"
#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/util/logging.hpp>

NDN_LOG_INIT(mnemosyne.interface.seenEventSet);

const uint32_t mnemosyne::interface::SeenEventSet::SEEN_EVENT_TYPE = 161;

mnemosyne::interface::SeenEventSet::SeenEventSet(std::chrono::seconds ttl)
        : m_ttl(ttl) {

}

bool mnemosyne::interface::SeenEventSet::hasEvent(const ndn::Name &eventName) {
    return m_events.count(eventName);
}

void mnemosyne::interface::SeenEventSet::addEvent(const ndn::Name &eventName) {
    if (hasEvent(eventName)) return;
    m_events.emplace(eventName);
    m_locations.emplace(std::chrono::system_clock::now(), eventName);
    auto del_time = std::chrono::system_clock::now() - m_ttl;
    while (!m_locations.empty()) {
        if (m_locations.front().first <= del_time) {
            m_events.erase(m_locations.front().second);
            m_locations.pop();
        } else break;
    }
}

ndn::Block mnemosyne::interface::SeenEventSet::encode() const {
    ndn::Block b(SEEN_EVENT_TYPE);
    for (const auto &i: m_events) {
        b.push_back(i.wireEncode());
    }
    b.encode();
    return b;
}

void mnemosyne::interface::SeenEventSet::decode(const ndn::Block &b) {
    if (b.type() != SEEN_EVENT_TYPE) NDN_THROW(std::runtime_error("Bad block to decode"));
    b.parse();
    for (const auto &i: b.elements()) {
        addEvent(ndn::Name(i));
    }
}
