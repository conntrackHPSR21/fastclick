#include <click/config.h>
#include <click/glue.hh>
#include <click/args.hh>
#include <click/ipflowid.hh>
#include <click/routervisitor.hh>
#include <click/error.hh>
#include "flowipmanager_robinmap_imp.hh"
#include <rte_hash.h>
#include <click/dpdk_glue.hh>
#include <rte_ethdev.h>


CLICK_DECLS

int
FlowIPManager_RobinMapIMP::alloc(int core)
{
    _tables[core].hash = new TSL_MAPTYPE_ROBIN_IMP();
    if(unlikely(_tables[core].hash == nullptr))
    {
	VPRINT(0,"Could not init flow table %d!", core);
	return 1;
    }

    reinterpret_cast<TSL_MAPTYPE_ROBIN_IMP *>(_tables[core].hash)->max_load_factor(1);
    reinterpret_cast<TSL_MAPTYPE_ROBIN_IMP *>(_tables[core].hash)->reserve(_table_size-1);
    VPRINT(2,"Created hash table with %d buckets\n", 
		reinterpret_cast<TSL_MAPTYPE_ROBIN_IMP *>(_tables[core].hash)->bucket_count());
    return 0;
}

int
FlowIPManager_RobinMapIMP::find(IPFlow5ID &f, int core)
{
    auto& tab = _tables[core];
    auto * _hash = 	reinterpret_cast<TSL_MAPTYPE_ROBIN_IMP *>(tab.hash);
	
    auto ret = _hash->find(f);
    
    return ret != _hash->end() ? ret->second : 0;
}

int
FlowIPManager_RobinMapIMP::insert(IPFlow5ID &f, int flowid, int core)
{
    auto& tab = _tables[core];
    auto * _hash = reinterpret_cast<TSL_MAPTYPE_ROBIN_IMP *> (tab.hash);

    (*_hash)[f] = flowid;

    //TODO: What is the correct return value?
    return flowid;
}


int FlowIPManager_RobinMapIMP::delete_flow(FlowControlBlock * fcb, int core)
{
    IPFlow5ID *f = get_fid(fcb);
    auto * _hash = reinterpret_cast<TSL_MAPTYPE_ROBIN_IMP *>(_tables[core].hash);
    int ret = (*_hash).erase(*f);
    VPRINT(3,"Deletion of entry %p, belonging to flow %i , returned %i: %s", fcb, 
		*get_flowid(fcb), ret, (ret ? "OK" : "ERR" ));

    return ret ? *get_flowid(fcb) : 0;
}

CLICK_ENDDECLS

ELEMENT_REQUIRES(flow)
EXPORT_ELEMENT(FlowIPManager_RobinMapIMP)
ELEMENT_MT_SAFE(FlowIPManager_RobinMapIMP)
