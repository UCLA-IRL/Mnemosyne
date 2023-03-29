//
// Created by Tyler on 3/2/23.
//

#include "self-inserted-set.h"

mnemosyne::interface::SelfInsertedSet::SelfInsertedSet(uint32_t reset_frequency) :
    m_count(0),
    m_resetFreq(reset_frequency)
{
}

void mnemosyne::interface::SelfInsertedSet::insert(const ndn::Name &producer) {
    m_selfInsertEventProducers.insert(producer);
    m_count ++;
    if (m_count >= m_resetFreq) {
        m_selfInsertEventProducers.clear();
        m_count = 0;
    }
}

void mnemosyne::interface::SelfInsertedSet::erase(const ndn::Name &producer) {
    m_selfInsertEventProducers.erase(producer);
}

bool mnemosyne::interface::SelfInsertedSet::count(const ndn::Name &producer) const {
    return m_selfInsertEventProducers.count(producer) != 0;
}

void mnemosyne::interface::SelfInsertedSet::receivedOther(const ndn::Name &eventName) {
    m_count ++;
    if (m_count >= m_resetFreq) {
        m_selfInsertEventProducers.clear();
        m_count = 0;
        return;
    }
    for (size_t i = eventName.size(); i > 0; i --) {
        auto it = m_selfInsertEventProducers.find(eventName.getPrefix(i));
        if (it != m_selfInsertEventProducers.end()) {
            m_selfInsertEventProducers.erase(it);
            break;
        }
    }
}
