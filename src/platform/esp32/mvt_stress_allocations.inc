// Test-only, boot-lifetime counters across tasks. No allocation, logging or
// blocking in the allocator hook. A contended last-record update is skipped.
static uint32_t mvt_stress_alloc_counts[4];
static uint32_t mvt_stress_alloc_last[3];
static char mvt_stress_alloc_name[16];
static unsigned char mvt_stress_alloc_busy;
static void mvt_stress_record_alloc(uint32_t size, uint32_t caps, const char *task, bool wss, uint32_t uptime)
{
    __atomic_add_fetch(&mvt_stress_alloc_counts[0],1,__ATOMIC_RELAXED);
    if(task && strncmp(task,"wifi",4)==0)__atomic_add_fetch(&mvt_stress_alloc_counts[1],1,__ATOMIC_RELAXED);
    if(task && (strcmp(task,"tiT")==0 || strncmp(task,"tcpip",5)==0))__atomic_add_fetch(&mvt_stress_alloc_counts[2],1,__ATOMIC_RELAXED);
    if(wss)__atomic_add_fetch(&mvt_stress_alloc_counts[3],1,__ATOMIC_RELAXED);
    if(__atomic_test_and_set(&mvt_stress_alloc_busy,__ATOMIC_ACQUIRE))return;
    mvt_stress_alloc_last[0]=size;mvt_stress_alloc_last[1]=caps;mvt_stress_alloc_last[2]=uptime;
    unsigned i=0;
    if(task)for(;i<15 && task[i];++i)mvt_stress_alloc_name[i]=task[i];
    mvt_stress_alloc_name[i]=0;
    __atomic_clear(&mvt_stress_alloc_busy,__ATOMIC_RELEASE);
}
void mvt_stress_alloc_snapshot(uint32_t values[8],char task[16])
{
    for(unsigned i=0;i<4;++i)values[i]=__atomic_load_n(&mvt_stress_alloc_counts[i],__ATOMIC_RELAXED);
    values[4]=values[5]=values[6]=0;values[7]=mvt_alloc_hook_registered?1:0;task[0]=0;
    if(__atomic_test_and_set(&mvt_stress_alloc_busy,__ATOMIC_ACQUIRE))return;
    for(unsigned i=0;i<3;++i)values[i+4]=mvt_stress_alloc_last[i];
    memcpy(task,mvt_stress_alloc_name,16);task[15]=0;
    __atomic_clear(&mvt_stress_alloc_busy,__ATOMIC_RELEASE);
}
