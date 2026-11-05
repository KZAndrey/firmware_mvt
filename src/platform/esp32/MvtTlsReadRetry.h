#pragma once
#include <stdint.h>
// Retry below the WebSocket parser: its partially consumed header stays on stack.
// ESP-IDF 4.4.7 ssl_read propagates MBEDTLS_ERR_SSL_WANT_READ unchanged.
template<class Read, class Clock, class Yield>
int mvtTlsIoRetry(int timeoutMs, Read read, Clock now, Yield yield, unsigned &retries, bool writing) {
    const uint32_t budget=timeoutMs<0?10000u:uint32_t(timeoutMs);
    const uint32_t started=now();
    int remaining=int(budget);
    retries=0;
    for (;;) {
        const int result=read(remaining);
        if((result!=-0x6900 && (!writing || result!=-0x6880)) || uint32_t(now()-started)>=budget) return result;
        yield(); // bounded wait, never spin while TLS waits for more ciphertext
        const uint32_t elapsed=uint32_t(now()-started);
        if(elapsed>=budget) return result;
        remaining=int(budget-elapsed);
        ++retries;
    }
}

template<class Read, class Clock, class Yield>
int mvtTlsReadRetry(int timeoutMs, Read read, Clock now, Yield yield, unsigned &retries) {
    return mvtTlsIoRetry(timeoutMs,read,now,yield,retries,false);
}
template<class Write, class Clock, class Yield>
int mvtTlsWriteRetry(int timeoutMs, Write write, Clock now, Yield yield, unsigned &retries) {
    return mvtTlsIoRetry(timeoutMs,write,now,yield,retries,true);
}
