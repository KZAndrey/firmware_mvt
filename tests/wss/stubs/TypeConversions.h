#pragma once
#include "configuration.h"
struct TypeConversions {
    static meshtastic_User ConvertToUser(uint32_t id, meshtastic_UserLite lite) {
        meshtastic_User user = meshtastic_User_init_default;
        snprintf(user.id,sizeof(user.id),"!%08x",id);
        strcpy(user.long_name,lite.long_name); strcpy(user.short_name,lite.short_name);
        user.public_key.size=lite.public_key.size;
        memcpy(user.public_key.bytes,lite.public_key.bytes,lite.public_key.size);
        return user;
    }
};
