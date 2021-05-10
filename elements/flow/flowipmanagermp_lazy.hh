#ifndef CLICK_FLOWIPMANAGERMP_lazy_HH
#define CLICK_FLOWIPMANAGERMP_lazy_HH
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

class FlowIPManagerMP_lazy: public VirtualFlowIPManagerIMP<Spinlock,false,true,maintainerArgNone> {
    public:

        const char *class_name() const override { return "FlowIPManagerMP_lazy"; }
	// By default, with no_free_on_delete and deferred free
	FlowIPManagerMP_lazy(): VirtualFlowIPManagerIMP() {
	    _timer = 0;
	    _flags = 0;
	    }

	virtual int parse(Args * args) override;
	virtual int maintainer(int core) override;
	
	// Allow to define additional handlers
	static String read_handler(Element *e, void *thunk);
	virtual void add_handlers() override;
	virtual String get_lazy_counter(int cnt);
	virtual void run_timer(Timer *timer) override;

    protected:
	virtual int find(IPFlow5ID &f, int core=0) override;
	virtual int insert(IPFlow5ID &f, int flowid, int core=0) override;
	virtual int alloc(int i) override;
	virtual int delete_flow(FlowControlBlock * fcb, int core) override;
	virtual int free_pos(int pos, int core) override;

	int _flags;
	bool _lf;
	bool _always_recycle;

	// We are never full (although we may to deletion)
	virtual inline bool insert_if_full() override { return LAZY_INSERT_FULL; }

	// We don't need lastseen for the lazy methods...
	virtual inline void update_lastseen(FlowControlBlock *fcb,
                                      Timestamp &ts) override {};


#if IMP_COUNTERS
	uint32_t _recycled_entries = 0;
	uint32_t _total_flows = 0;
#endif
	uint16_t _recent = 0;
	Timer * _timer;

};


CLICK_ENDDECLS
#endif
