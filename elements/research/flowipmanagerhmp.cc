/*
 * flowipmanagerhmp.{cc,hh} - Flow classification using HashtableMP
 *
 * Copyright (c) 2019-2020 Tom Barbette, KTH Royal Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.

 */

#include <click/config.h>
#include <click/glue.hh>
#include <click/args.hh>
#include <click/ipflowid.hh>
#include <click/routervisitor.hh>
#include <click/error.hh>
#include <rte_hash.h>
#include "flowipmanagerhmp.hh"
#include <click/dpdk_glue.hh>
#include <rte_ethdev.h>

CLICK_DECLS


int
FlowIPManagerHMP::alloc(int i)
{
    if (_tables[0].hash == 0)
    {
	_tables[0].hash = new HashTableMP<IPFlow5ID,int>();
    reinterpret_cast<HashTableMP <IPFlow5ID, int> *> (_tables[0].hash)->resize_clear(_table_size);
	VPRINT(2,"Created HMP table by thread %i", i);
    }
    else
	VPRINT(1,"alloc on thread %i: ignore");
    return 0;

}

int
FlowIPManagerHMP::find(IPFlow5ID &f, int core)
{
    HashTableMP<IPFlow5ID, int> * ht = reinterpret_cast<HashTableMP <IPFlow5ID, int> *> (_tables[0].hash);

    uint64_t this_flow=0;
    auto ret = ht->find(f);
    return ret!=0 ? (int) *ret : 0;

}

int
FlowIPManagerHMP::insert(IPFlow5ID &f, int flowid, int core)
{

    HashTableMP<IPFlow5ID, int> * ht = reinterpret_cast<HashTableMP <IPFlow5ID, int> *> (_tables[0].hash);
    ht->find_insert(f, flowid);
    return flowid;
}

int FlowIPManagerHMP::delete_flow(FlowControlBlock * fcb, int core)
{
    reinterpret_cast<HashTableMP <IPFlow5ID, int> *> (_tables[0].hash)->erase(*get_fid(fcb));
    return 1;
}

CLICK_ENDDECLS

ELEMENT_REQUIRES(flow dpdk dpdk19)
EXPORT_ELEMENT(FlowIPManagerHMP)
ELEMENT_MT_SAFE(FlowIPManagerHMP)
