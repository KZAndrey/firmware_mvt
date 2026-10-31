static portMUX_TYPE mvt_diag_lock = portMUX_INITIALIZER_UNLOCKED;
static MvtWssDiagnostic mvt_last_diag;

bool mvt_wss_last_diagnostic(MvtWssDiagnostic *out)
{
    if (!out) return false;
    portENTER_CRITICAL(&mvt_diag_lock);
    *out = mvt_last_diag;
    portEXIT_CRITICAL(&mvt_diag_lock);
    return out->sequence != 0;
}

static void mvt_wss_save_diagnostic(MvtWssDiagnostic *value)
{
    const uint32_t boot_id = esp_random();
    portENTER_CRITICAL(&mvt_diag_lock);
    // Preserve the session failure through subsequent failed handshakes.
    if (value->duringSession || !mvt_last_diag.duringSession) {
        value->bootId = mvt_last_diag.sequence ? mvt_last_diag.bootId : boot_id;
        value->sequence = mvt_last_diag.sequence + 1;
        if (!value->sequence) value->sequence = 1;
        mvt_last_diag = *value;
    }
    portEXIT_CRITICAL(&mvt_diag_lock);
}
