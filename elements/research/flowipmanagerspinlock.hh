#ifndef CLICK_FLOWIPMANAGERSL_HH
#define CLICK_FLOWIPMANAGERSL_HH
#include <click/config.h>
#include <click/string.hh>
#include <click/timer.hh>
#include <click/vector.hh>
#include <click/multithread.hh>
#include <click/pair.hh>
#include <click/flow/flowelement.hh>
#include <click/flow/common.hh>
#include <click/batchbuilder.hh>
#include <click/virtualflowipmanagerimp.hh>
#include <rte_hash.h>



// DPK hash tables, per core duplication


class FlowIPManagerSpinlock: public VirtualFlowIPManagerIMP<Spinlock,false,false,maintainerArgMP> {
    public:

        const char *class_name() const { return "FlowIPManagerSpinlock"; }
	FlowIPManagerSpinlock(): VirtualFlowIPManagerIMP() {}

    protected:
	int find(IPFlow5ID &f, int core=0) override;
	int insert(IPFlow5ID &f, int flowid, int core=0) override;
	int alloc(int i) override;
	int delete_flow(FlowControlBlock * fcb, int core) override;
	int free_pos(int pos, int core) override;

        static Spinlock hash_table_lock;

};

CLICK_ENDDECLS
#endif
