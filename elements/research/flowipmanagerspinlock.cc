#include <click/config.h>
#include <click/glue.hh>
#include <click/args.hh>
#include <click/ipflowid.hh>
#include <click/routervisitor.hh>
#include <click/error.hh>
#include <rte_hash.h>
#include "flowipmanagerspinlock.hh"
#include <click/dpdk_glue.hh>
#include <rte_ethdev.h>

CLICK_DECLS

Spinlock FlowIPManagerSpinlock::hash_table_lock;

int
FlowIPManagerSpinlock::alloc(int i)
{
    if (_tables[0].hash == 0)
    {

	struct rte_hash_parameters hash_params = {0};
	char buf[32];

	hash_params.name = buf;
	hash_params.entries = _table_size;
	hash_params.key_len = sizeof(IPFlow5ID);
	hash_params.hash_func = ipv4_hash_crc;
	hash_params.hash_func_init_val = 0;
	hash_params.extra_flag = RTE_HASH_EXTRA_FLAGS_NO_FREE_ON_DEL;

	VPRINT(1,"Real capacity for each table will be %lu", _table_size);


	sprintf(buf, "SPIN-%i", i);
	_tables[0].hash = rte_hash_create(&hash_params);

	if(unlikely(_tables[0].hash == nullptr))
	{
	    VPRINT(0,"Could not init flow table %d!", 0);
	    return 1;
	}
    }
    else
	VPRINT(1,"alloc on thread %i: ignore");
    return 0;
}

int
FlowIPManagerSpinlock::find(IPFlow5ID &f, int core)
{
    core = 0;

    auto& tab = _tables[core];
    rte_hash * ht = reinterpret_cast<rte_hash*>(tab.hash);
    uint64_t this_flow=0;

    //int ret = rte_hash_lookup_data(ht, &f, (void **)&this_flow);
    
    FlowIPManagerSpinlock::hash_table_lock.acquire();
    int ret = rte_hash_lookup_data(ht, &f, (void **)&this_flow);
    FlowIPManagerSpinlock::hash_table_lock.release();
    //click_chatter("FIND: %u -> %i -> %c", hash, ret, (ret>0) ?'Y' : 'N');
    return ret>=0 ? this_flow : 0;

}

int
FlowIPManagerSpinlock::insert(IPFlow5ID &f, int flowid, int core)
{
    core = 0;

    auto& tab = _tables[core];
    rte_hash * ht = reinterpret_cast<rte_hash *> (tab.hash);

    uint64_t ff = flowid;
    FlowIPManagerSpinlock::hash_table_lock.acquire();
    uint32_t ret = rte_hash_add_key_data(ht, &f, (void *) ff);
    FlowIPManagerSpinlock::hash_table_lock.release();
    ff = 1234;

    //TODO: What is the correct return value?
    return ret == 0 ? flowid : 0;
}


int FlowIPManagerSpinlock::delete_flow(FlowControlBlock * fcb, int core)
{
    core = 0;
    FlowIPManagerSpinlock::hash_table_lock.acquire();
    int ret = rte_hash_del_key(reinterpret_cast<rte_hash *> (_tables[core].hash), get_fid(fcb));
    FlowIPManagerSpinlock::hash_table_lock.release();
    VPRINT(2,"Deletion of entry %p, belonging to flow %i , returned %i: %s", fcb, 
		*get_flowid(fcb), ret, (ret >=0 ? "OK" : ret == -ENOENT ? "ENOENT" : "EINVAL" ));

    return ret;
}

int FlowIPManagerSpinlock::free_pos(int pos, int core)
{
    core = 0;
    FlowIPManagerSpinlock::hash_table_lock.acquire();
    return rte_hash_free_key_with_position(reinterpret_cast<rte_hash *> (_tables[core].hash), pos);
    FlowIPManagerSpinlock::hash_table_lock.release();
}

CLICK_ENDDECLS

ELEMENT_REQUIRES(flow dpdk dpdk19)
EXPORT_ELEMENT(FlowIPManagerSpinlock)
ELEMENT_MT_SAFE(FlowIPManagerSpinlock)
