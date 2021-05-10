#ifndef CLICK_FLOWIPMANAGER_RMIMP_HH
#define CLICK_FLOWIPMANAGER_RMIMP_HH
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
#include <tsl/robin_map.h>
#include <click/fixed_growth_hs.hh>


using tsl::robin_map;

// Robin Hood hash map, store the hash

#define TSL_MAPTYPE_ROBIN_IMP \
    robin_map\
	<IPFlow5ID, \
	int, \
	hash_flow_T, \
	std::equal_to<>, \
	std::allocator<std::pair<IPFlow5ID, int>>, \
	true, \
	tsl::fixed_size_policy>


class FlowIPManager_RobinMapIMP: public VirtualFlowIPManagerIMP<nolock,true,false,maintainerArgMP> {
    public:

        const char *class_name() const override { return "FlowIPManager_RobinMapIMP"; }


    protected:
	int find(IPFlow5ID &f, int core) override;
	int insert(IPFlow5ID &f, int flowid, int core) override;
	int alloc(int i) override;
	int delete_flow(FlowControlBlock * fcb, int core) override;
	

};

CLICK_ENDDECLS
#endif
