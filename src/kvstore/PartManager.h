/* Copyright (c) 2018 - present, VE Software Inc. All rights reserved
 *
 * This source code is licensed under Apache 2.0 License
 *  (found in the LICENSE.Apache file in the root directory)
 */

#ifndef KVSTORE_PARTMANAGER_H_
#define KVSTORE_PARTMANAGER_H_

#include <gtest/gtest_prod.h>
#include "base/Base.h"
#include "meta/client/MetaClient.h"

namespace nebula {
namespace kvstore {


class Handler {
public:
    virtual void addSpace(GraphSpaceID spaceId) = 0;
    virtual void addPart(GraphSpaceID spaceId, PartitionID partId) = 0;
    virtual void removeSpace(GraphSpaceID spaceId) = 0;
    virtual void removePart(GraphSpaceID spaceId, PartitionID partId) = 0;
};

/**
 * This class manages all meta information one storage host needed.
 * */
class PartManager {
public:
    PartManager() = default;

    virtual ~PartManager() = default;

    /**
     * return PartsMap for host
     * */
    virtual PartsMap parts(const HostAddr& host) = 0;

    /**
     * return PartMeta for <spaceId, partId>
     * */
    virtual PartMeta partMeta(GraphSpaceID spaceId, PartitionID partId) = 0;

    /**
     * Check current part exist or not on host.
     * */
    virtual bool partExist(const HostAddr& host, GraphSpaceID spaceId, PartitionID partId) = 0;

    /**
     * Check current space exist or not.
     * */
    virtual bool spaceExist(const HostAddr& host, GraphSpaceID spaceId) = 0;

    /**
     * Register Handler
     * */
    void registerHandler(Handler* handler) {
        handler_ = handler;
    }

protected:
    Handler* handler_ = nullptr;
};

/**
: * Memory based PartManager, it is used in UTs now.
 * */
class MemPartManager final : public PartManager {
    FRIEND_TEST(KVStoreTest, SimpleTest);
    FRIEND_TEST(KVStoreTest, PartsTest);

public:
    MemPartManager() = default;

    ~MemPartManager() = default;

    PartsMap parts(const HostAddr& host) override;

    PartMeta partMeta(GraphSpaceID spaceId, PartitionID partId) override;

    void addPart(GraphSpaceID spaceId, PartitionID partId, std::vector<HostAddr> peers = {}) {
        if (partsMap_.find(spaceId) == partsMap_.end()) {
            if (handler_) {
                handler_->addSpace(spaceId);
            }
        }
        auto& p = partsMap_[spaceId];
        if (p.find(partId) == p.end()) {
            if (handler_) {
                handler_->addPart(spaceId, partId);
            }
        }
        p[partId] = PartMeta();
        auto& pm = p[partId];
        pm.spaceId_ = spaceId;
        pm.partId_  = partId;
        pm.peers_ = std::move(peers);
    }

    void removePart(GraphSpaceID spaceId, PartitionID partId) {
        auto it = partsMap_.find(spaceId);
        CHECK(it != partsMap_.end());
        if (it->second.find(partId) != it->second.end()) {
            it->second.erase(partId);
            if (handler_) {
                handler_->removePart(spaceId, partId);
                if (it->second.empty()) {
                    handler_->removeSpace(spaceId);
                }
            }
        }
    }

    bool partExist(const HostAddr& host, GraphSpaceID spaceId, PartitionID partId) override;

    bool spaceExist(const HostAddr& host, GraphSpaceID spaceId) override {
        UNUSED(host);
        return partsMap_.find(spaceId) != partsMap_.end();
    }

    PartsMap& partsMap() {
        return partsMap_;
    }

private:
    PartsMap partsMap_;
};

class MetaServerBasedPartManager : public PartManager, public meta::MetaChangedListener {
public:
     explicit MetaServerBasedPartManager(HostAddr host);

     ~MetaServerBasedPartManager() {
        VLOG(3) << "~MetaServerBasedPartManager";
     }

     PartsMap parts(const HostAddr& host) override;

     PartMeta partMeta(GraphSpaceID spaceId, PartitionID partId) override;

     bool partExist(const HostAddr& host, GraphSpaceID spaceId, PartitionID partId) override;

     bool spaceExist(const HostAddr& host, GraphSpaceID spaceId) override;

     /**
      * Implement the interfaces in MetaChangedListener
      * */
     void onSpaceAdded(GraphSpaceID spaceId) override;

     void onSpaceRemoved(GraphSpaceID spaceId) override;

     void onPartAdded(const PartMeta& partMeta) override;

     void onPartRemoved(GraphSpaceID spaceId, PartitionID partId) override;

     void onPartUpdated(const PartMeta& partMeta) override;

     HostAddr getLocalHost() override {
        return localHost_;
     }

private:
     std::unique_ptr<meta::MetaClient> client_;
     HostAddr localHost_;
};

}  // namespace kvstore
}  // namespace nebula
#endif  // KVSTORE_PARTMANAGER_H_
