static portMUX_TYPE mvt_diag_lock = portMUX_INITIALIZER_UNLOCKED;
static MvtWssDiagnostic mvt_last_diag;
static MvtWssDiagnostic mvt_latest_diag;
static MvtWssBootDiagnostic mvt_boot_diag;
typedef struct { uint32_t count, uptime, duration, intent, checksum; } MvtRetainedPower;
static RTC_NOINIT_ATTR MvtRetainedPower mvt_retained_power;
static uint32_t mvt_power_checksum(const MvtRetainedPower *p) {
    return 0x4d565431u ^ p->count ^ p->uptime ^ p->duration ^ p->intent;
}
void mvt_wss_boot_init(uint32_t resetReason, uint32_t wakeCause, bool retain) {
    portENTER_CRITICAL(&mvt_diag_lock);
    if (!retain || mvt_retained_power.checksum != mvt_power_checksum(&mvt_retained_power))
        memset(&mvt_retained_power, 0, sizeof(mvt_retained_power));
    mvt_boot_diag.bootId = esp_random();
    mvt_boot_diag.resetReason = resetReason; mvt_boot_diag.wakeCause = wakeCause;
    mvt_boot_diag.bootCount = ++mvt_retained_power.count;
    if (!mvt_boot_diag.bootCount) mvt_boot_diag.bootCount = mvt_retained_power.count = 1;
    mvt_boot_diag.previousSleepUptimeMs = mvt_retained_power.uptime;
    mvt_boot_diag.previousSleepDurationMs = mvt_retained_power.duration;
    mvt_boot_diag.sleepIntent = mvt_retained_power.intent;
    mvt_retained_power.uptime = mvt_retained_power.duration = mvt_retained_power.intent = 0;
    mvt_retained_power.checksum = mvt_power_checksum(&mvt_retained_power);
    portEXIT_CRITICAL(&mvt_diag_lock);
}
void mvt_wss_prepare_sleep(uint32_t uptimeMs, uint32_t durationMs) {
    portENTER_CRITICAL(&mvt_diag_lock);
    mvt_retained_power.uptime = uptimeMs; mvt_retained_power.duration = durationMs;
    mvt_retained_power.intent = 1;
    mvt_retained_power.checksum = mvt_power_checksum(&mvt_retained_power);
    portEXIT_CRITICAL(&mvt_diag_lock);
}
bool mvt_wss_boot_diagnostic(MvtWssBootDiagnostic *out) {
    if (!out) return false;
    portENTER_CRITICAL(&mvt_diag_lock); *out = mvt_boot_diag; portEXIT_CRITICAL(&mvt_diag_lock);
    return out->bootCount != 0;
}

bool mvt_wss_last_diagnostic(MvtWssDiagnostic *out)
{
    if (!out) return false;
    portENTER_CRITICAL(&mvt_diag_lock);
    *out = mvt_last_diag;
    portEXIT_CRITICAL(&mvt_diag_lock);
    return out->sequence != 0;
}

// Current transport attempt for recovery; reporting retains the session failure.
bool mvt_wss_latest_diagnostic(MvtWssDiagnostic *out)
{
    if (!out) return false;
    portENTER_CRITICAL(&mvt_diag_lock);
    *out = mvt_latest_diag;
    portEXIT_CRITICAL(&mvt_diag_lock);
    return out->sequence != 0;
}

static void mvt_wss_save_diagnostic(MvtWssDiagnostic *value)
{
    const uint32_t boot_id = mvt_boot_diag.bootCount ? mvt_boot_diag.bootId : esp_random();
    portENTER_CRITICAL(&mvt_diag_lock);
    MvtWssDiagnostic current = *value;
    current.bootId = mvt_latest_diag.sequence ? mvt_latest_diag.bootId : boot_id;
    current.sequence = mvt_latest_diag.sequence + 1;
    if (!current.sequence) current.sequence = 1;
    mvt_latest_diag = current;
    // Preserve the session failure through subsequent failed handshakes.
    if (value->duringSession || !mvt_last_diag.duringSession) {
        value->bootId = mvt_last_diag.sequence ? mvt_last_diag.bootId : boot_id;
        value->sequence = mvt_last_diag.sequence + 1;
        if (!value->sequence) value->sequence = 1;
        mvt_last_diag = *value;
    }
    portEXIT_CRITICAL(&mvt_diag_lock);
}

void mvt_wss_startup_failure(const char *stage, uint32_t uptimeMs, uint32_t freeHeap,
                             uint32_t largestBlock, uint32_t minimumHeap) {
    MvtWssDiagnostic d = {0};
    strncpy(d.stage, stage, sizeof(d.stage)-1); d.ret = -1; d.uptimeMs = uptimeMs;
    d.freeHeap = freeHeap; d.largestBlock = largestBlock; d.minimumHeap = minimumHeap;
    mvt_wss_save_diagnostic(&d);
}
