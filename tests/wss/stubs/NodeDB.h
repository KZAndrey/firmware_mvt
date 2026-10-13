#pragma once
#include "configuration.h"
struct NodeDB {
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
