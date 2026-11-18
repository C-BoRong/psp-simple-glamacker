#ifndef ATRI_GPU_BUDGET_H
#define ATRI_GPU_BUDGET_H
#define GPU_LIST_BYTES (256u*1024u)
void gpu_budget_begin(void *list);
void gpu_budget_flush(void);
unsigned gpu_budget_peak(void);
unsigned gpu_budget_splits(void);
#endif
