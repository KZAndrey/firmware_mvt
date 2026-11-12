// Capture only the first WiFi/DMA allocation failure of this boot. Fixed storage,
// bounded stack walking, no printing or allocation inside the allocator hook.
static uint32_t mvt_wifi_trace_state, mvt_wifi_trace_count;
static uint32_t mvt_wifi_trace_pcs[16], mvt_wifi_trace_meta[3];
static void __attribute__((noinline)) mvt_stress_capture_wifi_trace(uint32_t size,uint32_t caps,const char *task,uint32_t uptime)
{
    if(!task || strncmp(task,"wifi",4) || !(caps & MALLOC_CAP_DMA))return;
    uint32_t expected=0;
    if(!__atomic_compare_exchange_n(&mvt_wifi_trace_state,&expected,1,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))return;
    mvt_wifi_trace_meta[0]=size;mvt_wifi_trace_meta[1]=caps;mvt_wifi_trace_meta[2]=uptime;
    esp_backtrace_frame_t frame={0};
    esp_backtrace_get_start(&frame.pc,&frame.sp,&frame.next_pc);
    for(unsigned i=0;i<16;++i){
        mvt_wifi_trace_pcs[mvt_wifi_trace_count++]=frame.pc;
        if(!frame.next_pc || !esp_backtrace_get_next_frame(&frame))break;
    }
    __atomic_store_n(&mvt_wifi_trace_state,2,__ATOMIC_RELEASE);
}
unsigned mvt_stress_wifi_trace_snapshot(uint32_t pcs[16],uint32_t meta[3])
{
    if(__atomic_load_n(&mvt_wifi_trace_state,__ATOMIC_ACQUIRE)!=2)return 0;
    memcpy(pcs,mvt_wifi_trace_pcs,sizeof(mvt_wifi_trace_pcs));
    memcpy(meta,mvt_wifi_trace_meta,sizeof(mvt_wifi_trace_meta));
    return mvt_wifi_trace_count;
}
