// TODO: Rename to `interp.h`?
#ifndef COY_VM_VM_H_
#define COY_VM_VM_H_

struct coy_context;

void coy_vm_exec_frame_(struct coy_context* ctx);
bool coy_vm_call_(struct coy_context* ctx, struct coy_function_* function, bool segmented);

#endif /* COY_VM_VM_H_ */
