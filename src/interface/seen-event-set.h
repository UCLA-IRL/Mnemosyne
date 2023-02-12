//
// Created by Tyler on 2/12/23.
//

#ifndef MNEMOSYNE_SEEN_EVENT_SET_H
#define MNEMOSYNE_SEEN_EVENT_SET_H

#include <ndn-cxx/name.hpp>
#include <set>
#include <queue>
#include <chrono>

namespace mnemosyne::interface {

class SeenEventSet {
  public:
    SeenEventSet(std::chrono::seconds ttl);

    bool hasEvent(const ndn::Name& eventName);

    void addEvent(const ndn::Name& eventName);

  private:
    std::queue<std::pair<std::chrono::time_point<std::chrono::system_clock>, ndn::Name>> m_locations;
    std::set<ndn::Name> m_events;
    ndn::Name m_peerPrefix;
    std::chrono::seconds m_ttl;
};

}

#endif //MNEMOSYNE_SEEN_EVENT_SET_H
