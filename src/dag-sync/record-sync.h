/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012-2022 University of California, Los Angeles
 *
 * This file is part of ndn-svs, synchronization library for distributed realtime
 * applications for NDN.
 *
 * ndn-svs library is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free Software
 * Foundation, in version 2.1 of the License.
 *
 * ndn-svs library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
 */

#ifndef MNEMOSYNE_RECORD_SYNC_H
#define MNEMOSYNE_RECORD_SYNC_H

#include "mnemosyne/record.hpp"
#include "mnemosyne/backend.hpp"

#include <ndn-svs/svsync-base.hpp>

namespace mnemosyne::dag {

/**
 * @brief SVSync using arbitrary prefix for data delivery
 *
 * The data prefix acts as the node ID in the version vector
 * Sync core runs under <sync-prefix>
 * Data is produced as <data-prefix>/<sync-prefix>/<seq>
 */
class RecordSync : public svs::SVSyncBase {
  public:
    RecordSync(const ndn::Name &syncPrefix,
               const ndn::Name &nodePrefix,
               const ndn::Name &hintPrefix,
               ndn::Face &face,
               const ndn::svs::UpdateCallback &updateCallback,
               std::weak_ptr<Backend> backend,
               const ndn::svs::SecurityOptions &securityOptions = ndn::svs::SecurityOptions::DEFAULT);

    ~RecordSync();

    inline ndn::Name getDataName(const ndn::svs::NodeID &nid, const ndn::svs::SeqNo &seqNo) override {
        return Record::getRecordName(nid, seqNo);
    }

    svs::SeqNo publishData(Record &record, const ndn::time::milliseconds &freshness, const svs::NodeID &id,
                           uint32_t contentType);

    /**
     * @brief Retrieve a data packet with a particular seqNo from a session
     * it will fetch without forwarding hint first, then with forwarding hint
     *
     * @param nid The name of the target node
     * @param seq The seqNo of the data packet.
     * @param onValidated The callback when the retrieved packet has been validated.
     * @param nRetries The number of retries.
     * @param onValidated The callback when the retrieved packet has been validated.
     * @param onValidationFailed The callback when the retrieved packet failed validation.
     */
    void
    fetchRecord(const ndn::svs::NodeID &nid, const ndn::svs::SeqNo &seq,
                const ndn::svs::DataValidatedCallback &onValidated,
                int nRetries = 0, int forwardingHintRetries = 1,
                const ndn::svs::DataValidationErrorCallback &onValidationFailed = [](auto &&...) {},
                const TimeoutCallback &onTimeout = [](auto &&...) {});

    /**
     * @brief Retrieve a data packet with a particular seqNo from a session with the forwarding hint
     *
     * @param nid The name of the target node
     * @param seq The seqNo of the data packet.
     * @param onValidated The callback when the retrieved packet has been validated.
     * @param onValidationFailed The callback when the retrieved packet failed validation.
     * @param onTimeout The callback when data is not retrieved.
     * @param nRetries The number of retries.
     */
    void
    fetchDataWithHint(const ndn::svs::NodeID &nid, const ndn::svs::SeqNo &seq,
                      const ndn::svs::DataValidatedCallback &onValidated,
                      const ndn::svs::DataValidationErrorCallback &onValidationFailed,
                      const TimeoutCallback &onTimeout,
                      int nRetries = 0);

  private:
    void onDataValidated(const Data &data, const svs::DataValidatedCallback &dataCallback);

    void onDataInterest(const Interest &interest);

    bool shouldCache(const Data &data) const override {
        return false;
    }

  private:
    class BackendDataStore : public svs::DataStore {
      public:
        BackendDataStore(std::weak_ptr<Backend> backend);

        shared_ptr<const Data> find(const Interest &interest) override;

        void insert(const Data &data) override;

      private:
        std::weak_ptr<Backend> m_backend;
    };

    ndn::Face &m_face;
    ndn::Name m_hintPrefix;
    ndn::svs::Fetcher m_fetcher;
    ndn::ScopedRegisteredPrefixHandle m_registerHintPrefix;
    ndn::InterestFilterHandle m_registerFilterHandle;
};

} // namespace mnemosyne::dag

#endif // MNEMOSYNE_RECORD_SYNC_H
