#ifndef MNEMOSYNE_INCLUDE_CONFIG_H_
#define MNEMOSYNE_INCLUDE_CONFIG_H_

#include "logger-config.hpp"
#include <iostream>
#include <utility>

namespace mnemosyne {

class Config: public LoggerConfig {
  public:
    Config(ndn::Name multicastPrefix, ndn::Name hintPrefix, ndn::Name peerPrefix,
           std::set<Name> PSPrefixes, std::set<Name> syncInterfacePrefixes = {})
           : LoggerConfig(std::move(multicastPrefix), std::move(hintPrefix), std::move(peerPrefix)),
             svsPubSubInterfacePrefixes(std::move(PSPrefixes)),
             svsInterfacePrefixes(std::move(syncInterfacePrefixes))
             {}

  public:

    /**
     * Interface only configs
     */
    uint32_t insertBackoffMaxMs = 1000;
    std::chrono::seconds seenEventTtl = std::chrono::minutes(1);
    /**
     * The interface pub/sub prefix, under which an publication can reach all Mnemosyne loggers.
     */
    std::set<Name> svsPubSubInterfacePrefixes;
    std::set<Name> svsInterfacePrefixes;
};

} // namespace mnemosyne

#endif // define MNEMOSYNE_INCLUDE_CONFIG_H_