#ifndef MNEMOSYNE_INCLUDE_LOGGER_CONFIG_H_
#define MNEMOSYNE_INCLUDE_LOGGER_CONFIG_H_

#include <ndn-cxx/face.hpp>
#include <iostream>
#include <utility>

namespace mnemosyne {

class LoggerConfig {
  public:
    /**
     * Construct a Config instance used for Mnemosyne initialization.
     * @p multicastPrefix, input, the distributed ledger system's multicast prefix.
     * @p hintPrefix, input, the recovery hint prefix.
     * @p peerPrefix, input, the unique prefix of the peer.
     */
    inline LoggerConfig(ndn::Name multicastPrefix, ndn::Name hintPrefix, ndn::Name peerPrefix)
            : syncPrefix(std::move(multicastPrefix)),
              hintPrefix(std::move(hintPrefix)),
              peerPrefix(std::move(peerPrefix)) {}

    /**
     *
     * @param dbType the type of database. currently "leveldb" or "memory"
     * @param dbConfig the config for the type. Typically path of the database.
     * @return the config object's pointer for chaining.
     */
    LoggerConfig &setDatabase(std::string dbType, std::string dbConfig = "") {
        databaseType = std::move(dbType);
        databasePath = std::move(dbConfig);
        if (databaseType == "memory") {
            seqNoBackupFreq = std::numeric_limits<uint32_t>::max(); // no need for backup since memory is volatile anyway
        }
        return *this;
    }

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
    uint32_t seqNoBackupFreq = 10;

    /**
     * max replication count, 0 mean off
     */
    uint32_t maxCountedReplication = 2;
    uint32_t maxSelfReRefCount = 3;

    /**
     * The multicast prefix, under which an Interest can reach to all the peers in the same multicast group.
     */
    ndn::Name syncPrefix;
    /**
     * The hint prefix, the multicast prefix used as forwarding hint in backup fetching
     */
    ndn::Name hintPrefix;
    /**
     * Producer's unique name prefix, under which an Interest can reach to the producer.
     */
    ndn::Name peerPrefix;
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

#endif // define MNEMOSYNE_INCLUDE_LOGGER_CONFIG_H_