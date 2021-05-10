#ifndef CLICK_FLOWIPMANAGER_CPPIMP_HH
#define CLICK_FLOWIPMANAGER_CPPIMP_HH
#include <click/config.h>
#include <click/string.hh>
#include <click/timer.hh>
#include <click/vector.hh>
#include <click/multithread.hh>
#include <click/pair.hh>
#include <click/flow/flowelement.hh>
#include <click/flow/common.hh>
#include <click/batchbuilder.hh>
#include <click/timerwheel.hh>
#include <click/virtualflowipmanagerimp.hh>

class FlowIPManager_CPPIMP: public VirtualFlowIPManagerIMP<nolock,true,false,maintainerArgNone> {
    public:

        const char *class_name() const override { return "FlowIPManager_CPPIMP"; }


    protected:

	typedef std::unordered_map<IPFlow5ID, uint32_t> CPPMap;

	int find(IPFlow5ID &f, int core) override;
	int insert(IPFlow5ID &f, int flowid, int core) override;
	int alloc(int i) override;
	//int delete_flow(FlowControlBlock * fcb, int core) override;
	

};

CLICK_ENDDECLS
#endif
