#ifndef CLICK_FLOWIPMANAGERMUTEX_HH
#define CLICK_FLOWIPMANAGERMUTEX_HH
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
#include <mutex>
#include <shared_mutex>

#define ht_shared_lock_acquire() std::shared_lock<std::shared_mutex> l(_hash_table_write_lock);
#define ht_shared_lock_release() l.unlock(); 
#define ht_exclusive_lock_acquire() std::unique_lock<std::shared_mutex> l(_hash_table_write_lock);
#define ht_exclusive_lock_release() l.unlock();


// DPK hash tables, per core duplication


class FlowIPManagerMutex: public VirtualFlowIPManagerIMP<Spinlock,false,true,maintainerArgMP> {
    public:

        const char *class_name() const { return "FlowIPManagerMutex"; }
	FlowIPManagerMutex(): VirtualFlowIPManagerIMP() {}

    protected:
	int find(IPFlow5ID &f, int core=0) override;
	int insert(IPFlow5ID &f, int flowid, int core=0) override;
	int alloc(int i) override;
	int delete_flow(FlowControlBlock * fcb, int core) override;
	int free_pos(int pos, int core) override;

	std::shared_mutex _hash_table_write_lock;

};

CLICK_ENDDECLS
#endif
