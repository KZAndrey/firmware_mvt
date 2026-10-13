#pragma once
#include "configuration.h"
struct Router {
    uint32_t nextId = 1000;
    std::vector<meshtastic_MeshPacket> sent;
    bool fail = false;
    meshtastic_MeshPacket *allocForSending() {
        auto *p = new meshtastic_MeshPacket(); p->which_payload_variant = meshtastic_MeshPacket_decoded_tag;
        p->id = nextId++; return p;
    }
    int sendLocal(meshtastic_MeshPacket *p, int) {
        if (!fail) sent.push_back(*p);
        delete p; return fail ? 32 : 0;
    }
};
extern Router *router;
