#ifndef MNEMOSYNE_INCLUDE_CONFIG_H_
#define MNEMOSYNE_INCLUDE_CONFIG_H_

#include "cert-manager.hpp"
#include <ndn-cxx/face.hpp>
#include <iostream>

using namespace ndn;
namespace mnemosyne {

class Config {
  public:

    static shared_ptr<Config> CustomizedConfig(const std::string &multicastPrefix, const std::string &interfacePrefix, const std::string &peerPrefix,
                                               const std::string &databasePath);

    /**
     * Construct a Config instance used for Mnemosyne initialization.
     * @p multicastPrefix, input, the distributed ledger system's multicast prefix.
     * @p peerPrefix, input, the unique prefix of the peer.
     */
    Config(const std::string &multicastPrefix, const std::string &interfacePrefix, const std::string &peerPrefix);

  public:
    /**
     * The number of preceding records that referenced by a later record.
     */
    size_t precedingRecordNum = 2;

    /**
     * The retries for fetching records.
     */
    int recordFetchRetries = 1;
    int hintedFetchRetries = 2;

    /**
     * Frequency of helper sequence number backup
     */
    uint32_t SeqNoBackupFreq = 10;

    /**
     * The multicast prefix, under which an Interest can reach to all the peers in the same multicast group.
     */
    Name syncPrefix;
    /**
     * The hint prefix, the multicast prefix used as forwarding hint in backup fetching
     */
     Name hintPrefix;
    /**
     * The interface pub/sub prefix, under which an publication can reach all Mnemosyne loggers.
     */
    Name interfacePrefix;
    /**
     * Producer's unique name prefix, under which an Interest can reach to the producer.
     */
    Name peerPrefix;
    /**
     * The Database type;
     */
    std::string databaseType = "leveldb";
    /**
     * The path to the Database;
     */
    std::string databasePath;
};

} // namespace mnemosyne

#endif // define MNEMOSYNE_INCLUDE_CONFIG_H_