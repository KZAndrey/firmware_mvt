#pragma once
#include "configuration.h"
enum class ProcessMessage { CONTINUE, STOP };
class MeshModule {
public:
    MeshModule(const char *) {}
    virtual ~MeshModule() {}
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &) = 0;
    virtual bool wantPacket(const meshtastic_MeshPacket *) = 0;
    bool isPromiscuous = false, encryptedOk = false, loopbackOk = false;
};
