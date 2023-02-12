//
// Created by Tyler on 2/12/23.
//

#include "seen-event-set.h"
#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/util/logging.hpp>

NDN_LOG_INIT(mnemosyne.interface.seenEventSet);

mnemosyne::interface::SeenEventSet::SeenEventSet(std::chrono::seconds ttl)
        : m_ttl(ttl){

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
