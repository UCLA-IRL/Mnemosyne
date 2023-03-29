//
// Created by Tyler on 3/2/23.
//

#ifndef MNEMOSYNE_SELF_INSERTED_SET_H
#define MNEMOSYNE_SELF_INSERTED_SET_H

#include <ndn-cxx/name.hpp>
#include <set>

namespace mnemosyne::interface {
class SelfInsertedSet {

  public:
    SelfInsertedSet(uint32_t reset_frequency);

    void insert(const ndn::Name &producer);
    void erase(const ndn::Name &producer);

    bool count(const ndn::Name &producer) const;

    void receivedOther(const ndn::Name &eventName);

  private:
    std::set<ndn::Name> m_selfInsertEventProducers;
    uint32_t m_resetFreq;
    uint32_t m_count;
};
}

#endif //MNEMOSYNE_SELF_INSERTED_SET_H
