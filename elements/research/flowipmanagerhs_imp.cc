#include <click/config.h>
#include <click/glue.hh>
#include <click/args.hh>
#include <click/ipflowid.hh>
#include <click/routervisitor.hh>
#include <click/error.hh>
#include "flowipmanagerhs_imp.hh"
#include <rte_hash.h>
#include <click/dpdk_glue.hh>
#include <rte_ethdev.h>



CLICK_DECLS

CLICK_ENDDECLS

ELEMENT_REQUIRES(flow)
EXPORT_ELEMENT(FlowIPManagerHS_IMP)
ELEMENT_MT_SAFE(FlowIPManagerHS_IMP)

#if HS_VARIANTS
// EXPORT_ELEMENT(FlowIPManagerHS_IMP_10)
// ELEMENT_MT_SAFE(FlowIPManagerHS_IMP_10)
// EXPORT_ELEMENT(FlowIPManagerHS_IMP_10_STORE)
// ELEMENT_MT_SAFE(FlowIPManagerHS_IMP_10_STORE)

// EXPORT_ELEMENT(FlowIPManagerHS_IMP_20)
// ELEMENT_MT_SAFE(FlowIPManagerHS_IMP_20)
// EXPORT_ELEMENT(FlowIPManagerHS_IMP_20_STORE)
// ELEMENT_MT_SAFE(FlowIPManagerHS_IMP_20_STORE)

// EXPORT_ELEMENT(FlowIPManagerHS_IMP_30)
// ELEMENT_MT_SAFE(FlowIPManagerHS_IMP_30)
// EXPORT_ELEMENT(FlowIPManagerHS_IMP_30_STORE)
// ELEMENT_MT_SAFE(FlowIPManagerHS_IMP_30_STORE)

// EXPORT_ELEMENT(FlowIPManagerHS_IMP_40)
// EXPORT_ELEMENT(FlowIPManagerHS_IMP_50)
// EXPORT_ELEMENT(FlowIPManagerHS_IMP_60)
// ELEMENT_MT_SAFE(FlowIPManagerHS_IMP_40)
// ELEMENT_MT_SAFE(FlowIPManagerHS_IMP_50)
// ELEMENT_MT_SAFE(FlowIPManagerHS_IMP_60)

// EXPORT_ELEMENT(FlowIPManagerHS_IMP_MOD_60)
// ELEMENT_MT_SAFE(FlowIPManagerHS_IMP_MOD_60)
// EXPORT_ELEMENT(FlowIPManagerHS_IMP_MOD)
// ELEMENT_MT_SAFE(FlowIPManagerHS_IMP_MOD)
#endif
