#ifndef CLICK_FLOWIPMANAGERHHMP_HH
#define CLICK_FLOWIPMANAGERHHMP_HH
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
#include <click/hashtablemp.hh>


class FlowIPManagerHMP: public VirtualFlowIPManagerIMP<Spinlock,false,false,maintainerArgMP> {
    public:

        const char *class_name() const { return "FlowIPManagerHMP"; }
	FlowIPManagerHMP(): VirtualFlowIPManagerIMP() {}

    protected:
	int find(IPFlow5ID &f, int core=0) override;
	int insert(IPFlow5ID &f, int flowid, int core=0) override;
	int alloc(int i) override;
	int delete_flow(FlowControlBlock * fcb, int core) override;
};

CLICK_ENDDECLS
#endif
