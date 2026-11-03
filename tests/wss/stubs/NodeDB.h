#pragma once
#include "configuration.h"
#include "meshtastic/admin.pb.h"
#define SEGMENT_CONFIG 1
#define NODEINFO_BITFIELD_IS_KEY_MANUALLY_VERIFIED_MASK 1
struct NodeDB {
    unsigned configSaves = 0;
    bool saveSucceeds = true;
    void addFromContact(const meshtastic_SharedContact &contact) {
        if(!saveSucceeds)return;
        auto *node=getMeshNode(contact.node_num);
        if(!node||!contact.has_user)return;
        node->has_user=true;
        node->user.public_key.size=contact.user.public_key.size;
        memcpy(node->user.public_key.bytes,contact.user.public_key.bytes,contact.user.public_key.size);
        if(contact.manually_verified)node->bitfield|=NODEINFO_BITFIELD_IS_KEY_MANUALLY_VERIFIED_MASK;
    }
    bool saveToDisk(int segment) { if (segment == SEGMENT_CONFIG) ++configSaves; return saveSucceeds; }
    std::vector<meshtastic_NodeInfoLite> nodes;
    uint32_t getNodeNum() { return 0x11223344; }
    size_t getNumMeshNodes() { return nodes.size(); }
    meshtastic_NodeInfoLite *getMeshNodeByIndex(size_t i) { return &nodes.at(i); }
    meshtastic_NodeInfoLite *getMeshNode(uint32_t id) {
        for (auto &n : nodes) if (n.num == id) return &n;
        return nullptr;
    }
    uint8_t getMeshNodeChannel(uint32_t id) { auto *n = getMeshNode(id); return n ? n->channel : 0; }
};
extern NodeDB *nodeDB;
