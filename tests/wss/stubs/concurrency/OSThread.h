#pragma once
namespace concurrency {
class OSThread {
public:
    OSThread(const char *, int) {}
    virtual ~OSThread() {}
    virtual int32_t runOnce() = 0;
};
}
