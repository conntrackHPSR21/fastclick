#include <click/config.h>
#include <click/glue.hh>
#include <click/args.hh>
#include <click/ipflowid.hh>
#include <click/routervisitor.hh>
#include <click/error.hh>
#include "flowipmanager_cpp_imp.hh"
#include <rte_hash.h>


CLICK_DECLS

int
FlowIPManager_CPPIMP::alloc(int core)
{
    auto* p = new CPPMap(_table_size);

    _tables[core].hash = p;
    if(unlikely(_tables[core].hash == nullptr))
    {
        VPRINT(0,"Could not init flow table %d!", core);
        return 1;
    }

    return 0;
}

int
FlowIPManager_CPPIMP::find(IPFlow5ID &f, int core)
{
    auto& tab = _tables[core];
    auto *table = 	reinterpret_cast<CPPMap*>(tab.hash);
    
    auto ret = table->find(f);
    return ret != table->end()? ret->second: 0;
}

int
FlowIPManager_CPPIMP::insert(IPFlow5ID &f, int flowid, int core)
{
    auto& tab = _tables[core];
    auto *table = reinterpret_cast<CPPMap*> (tab.hash);

    auto ret = table->insert({f, (uint32_t)flowid});

    return ret.second? flowid : 0;
}

// int FlowIPManager_CuckooPPIMP::delete_flow(FlowControlBlock * fcb, int core)
// {
//     abort();
//     int ret = 0;
//     return ret ? *get_flowid(fcb) : 0;
// }

CLICK_ENDDECLS

ELEMENT_REQUIRES(flow)
EXPORT_ELEMENT(FlowIPManager_CPPIMP)
ELEMENT_MT_SAFE(FlowIPManager_CPPIMP)
