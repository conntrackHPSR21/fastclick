#include "flowipmanagermp_lazy.hh"
#include <click/args.hh>
#include <click/config.h>
#include <click/dpdk_glue.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/ipflowid.hh>
#include <click/routervisitor.hh>
#include <rte_ethdev.h>
#include <rte_hash.h>

CLICK_DECLS

int FlowIPManagerMP_lazy::parse(Args *args) {
    return (*args)
        .read_or_set("LF", _lf, true)
        .read_or_set("ALWAYS_RECYCLE", _always_recycle, 1)
        .consume();
}

int FlowIPManagerMP_lazy::alloc(int core) {
    if (_tables[0].hash == 0) {
        _flags = 0;
        if (_lf)
            _flags = RTE_HASH_EXTRA_FLAGS_MULTI_WRITER_ADD |
                     RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY_LF;
        else
            _flags = RTE_HASH_EXTRA_FLAGS_MULTI_WRITER_ADD |
                     RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY;

        if (_always_recycle) _flags |= RTE_HASH_EXTRA_FLAGS_ALWAYS_RECYCLE;

        _flags |= RTE_HASH_EXTRA_FLAGS_NO_FREE_ON_DEL;

        struct rte_hash_parameters hash_params = {0};
        char buf[32];

        hash_params.name = buf;
        hash_params.entries = _table_size;
        hash_params.key_len = sizeof(IPFlow5ID);
        hash_params.hash_func = ipv4_hash_crc;
        hash_params.hash_func_init_val = 0;
        hash_params.extra_flag = _flags;
        hash_params.lifetime = _timeout_ms / _recycle_interval_ms;

        VPRINT(0, "[%i] Real capacity for table will be %lu", core,
               _table_size);

        sprintf(buf, "MP_lazy-%i", core);
        _tables[0].hash = rte_hash_create(&hash_params);

        if (unlikely(_tables[0].hash == nullptr)) {
            VPRINT(0, "Could not init flow table %d!", 0);
            return 1;
        }
    } else
        VPRINT(1, "alloc on thread %i: ignore");
    if (!_timer) _timer = new Timer(this);

    if (_timer && !_timer->initialized()) {
        _timer->initialize(this, true);
        VPRINT(0, "Updating recycle epoch every %d ms", _recycle_interval_ms);
        _timer->schedule_after_msec(_recycle_interval_ms);
    }
    return 0;
}

int FlowIPManagerMP_lazy::find(IPFlow5ID &f, int core) {
    core = 0;

    auto &tab = _tables[core];
    rte_hash *ht = reinterpret_cast<rte_hash *>(tab.hash);
    uint64_t this_flow = 0;

    //_recent is updated by timer

    // int ret = rte_hash_lookup_data(ht, &f, (void **)&this_flow);
    int ret = rte_hash_lookup_data_t(ht, &f, (void **)&this_flow, _recent);

    return ret >= 0 ? this_flow : 0;
}

int FlowIPManagerMP_lazy::insert(IPFlow5ID &f, int flowid, int core) {
    core = 0;

    auto &tab = _tables[core];
    rte_hash *ht = reinterpret_cast<rte_hash *>(tab.hash);

    int64_t old_flow;
    IPFlow5ID old_key;
    int64_t ret;
    old_flow = flowid;

    // We assume that _recent would have been refreshed during the find
    // So it is already "recent"
    ret = rte_hash_add_key_data_recycle_keepdata_t(
        ht, &f, (void *)flowid, (void *)&old_key, (void *)&old_flow, _recent);

    VPRINT(3, "HT insert returned %i for flowid %i. old_flow is %i.", ret,
           flowid, old_flow);
    if (ret >= 0) {
#if IMP_COUNTERS
#endif
        if (flowid != old_flow) {
            VPRINT(3, "Recycled entry! %i %i", flowid, old_flow);
#if IMP_COUNTERS
            _recycled_entries++;
#endif
            flows_push(core, old_flow);
        }
	else
	{
#if IMP_COUNTERS
	    _total_flows++;
#endif
	}
    } else
        flowid = 0;

    // TODO: What is the correct return value?
    return flowid;
}

int FlowIPManagerMP_lazy::delete_flow(FlowControlBlock *fcb, int core) {
    core = 0;
    int ret = rte_hash_del_key(reinterpret_cast<rte_hash *>(_tables[core].hash),
                               get_fid(fcb));
    VPRINT(2, "Deletion of entry %p, belonging to flow %i , returned %i: %s",
           fcb, *get_flowid(fcb), ret,
           (ret >= 0 ? "OK" : ret == -ENOENT ? "ENOENT" : "EINVAL"));

    return ret;
}

int FlowIPManagerMP_lazy::free_pos(int pos, int core) {
    core = 0;
    return rte_hash_free_key_with_position(
        reinterpret_cast<rte_hash *>(_tables[core].hash), pos);
}

enum {
#if IMP_COUNTERS
    h_recycled_entries = 2000,
    h_total_flows
#endif
};

String FlowIPManagerMP_lazy::read_handler(Element *e, void *thunk) {
    FlowIPManagerMP_lazy *f = static_cast<FlowIPManagerMP_lazy *>(e);

    switch ((intptr_t)thunk) {
#if IMP_COUNTERS
    case h_recycled_entries:
    case h_total_flows:
        return String(f->get_lazy_counter((intptr_t)thunk));
#endif
    default:
        return VirtualFlowIPManagerIMP::read_handler(e, thunk);
    }
}

void FlowIPManagerMP_lazy::add_handlers() {
    VirtualFlowIPManagerIMP::add_handlers();
#if IMP_COUNTERS
    add_read_handler("recycled_entries", read_handler, h_recycled_entries);
    add_read_handler("total_flows", read_handler, h_total_flows);
#endif
}

String FlowIPManagerMP_lazy::get_lazy_counter(int cnt) {
    switch (cnt) {
#if IMP_COUNTERS
    case h_recycled_entries:
        return String(_recycled_entries);
    case h_total_flows:
        return String(_total_flows);
#endif
    }
    return String(0);
}
void FlowIPManagerMP_lazy::run_timer(Timer *t) {
    _recent = click_jiffies() / _recycle_interval_ms;
    _timer->schedule_after_msec(_recycle_interval_ms);
}

int FlowIPManagerMP_lazy::maintainer(int core) {
    // In lazy deletion, the mantainer does nothing!
    // click_chatter("Mantainer on Lazy deletion HT: doing nothing!");
    return 0;
}


CLICK_ENDDECLS

ELEMENT_REQUIRES(flow dpdk dpdk19 dpdk_lazyht)
EXPORT_ELEMENT(FlowIPManagerMP_lazy)
ELEMENT_MT_SAFE(FlowIPManagerMP_lazy)

