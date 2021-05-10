#ifndef CLICK_FLOWIPMANAGER_HSIMP_HH
#define CLICK_FLOWIPMANAGER_HSIMP_HH
#include <click/batchbuilder.hh>
#include <click/config.h>
#include <click/fixed_growth_hs.hh>
#include <click/flow/common.hh>
#include <click/flow/flowelement.hh>
#include <click/multithread.hh>
#include <click/pair.hh>
#include <click/string.hh>
#include <click/timer.hh>
#include <click/timerwheel.hh>
#include <click/vector.hh>
#include <click/virtualflowipmanagerimp.hh>
#include <tsl/hopscotch_map.h>
using tsl::hopscotch_map;






// Hopscotch map, store the hash, 30 buckets as neighborhood size
// Per core duplication, indipendent tables

#ifndef HS_VARIANTS
    #define HS_VARIANTS 0
#endif

#if HS_VARIANTS
// Declare the different Hopscotch variants
typedef hopscotch_map<IPFlow5ID, int, VirtualFlowIPManagerIMP<>::hash_flow_T, std::equal_to<>,
                      std::allocator<std::pair<IPFlow5ID, int>>, 10, false,
                      tsl::fixed_size_policy>
    hs_map_10;
typedef hopscotch_map<IPFlow5ID, int, VirtualFlowIPManagerIMP<>::hash_flow_T, std::equal_to<>,
                      std::allocator<std::pair<IPFlow5ID, int>>, 10, true,
                      tsl::fixed_size_policy>
    hs_map_10_store;
typedef hopscotch_map<IPFlow5ID, int, VirtualFlowIPManagerIMP<>::hash_flow_T, std::equal_to<>,
                      std::allocator<std::pair<IPFlow5ID, int>>, 20, false,
                      tsl::fixed_size_policy>
    hs_map_20;
typedef hopscotch_map<IPFlow5ID, int, VirtualFlowIPManagerIMP<>::hash_flow_T, std::equal_to<>,
                      std::allocator<std::pair<IPFlow5ID, int>>, 20, true,
                      tsl::fixed_size_policy>
    hs_map_20_store;
typedef hopscotch_map<IPFlow5ID, int, VirtualFlowIPManagerIMP<>::hash_flow_T, std::equal_to<>,
                      std::allocator<std::pair<IPFlow5ID, int>>, 30, false,
                      tsl::fixed_size_policy>
    hs_map_30;

typedef hopscotch_map<IPFlow5ID, int, VirtualFlowIPManagerIMP<>::hash_flow_T, std::equal_to<>,
                      std::allocator<std::pair<IPFlow5ID, int>>, 40, false,
                      tsl::fixed_size_policy>
    hs_map_40;
typedef hopscotch_map<IPFlow5ID, int, VirtualFlowIPManagerIMP<>::hash_flow_T, std::equal_to<>,
                      std::allocator<std::pair<IPFlow5ID, int>>, 50, false,
                      tsl::fixed_size_policy>
    hs_map_50;
typedef hopscotch_map<IPFlow5ID, int, VirtualFlowIPManagerIMP<>::hash_flow_T, std::equal_to<>,
                      std::allocator<std::pair<IPFlow5ID, int>>, 60, false,
                      tsl::fixed_size_policy>
    hs_map_60;

typedef hopscotch_map<IPFlow5ID, int, VirtualFlowIPManagerIMP<>::hash_flow_T, std::equal_to<>,
                      std::allocator<std::pair<IPFlow5ID, int>>, 30, true,
                      tsl::fixed_mod_growth_policy>
    hs_map_mod_30_store;


typedef hopscotch_map<IPFlow5ID, int, VirtualFlowIPManagerIMP<>::hash_flow_T, std::equal_to<>,
                      std::allocator<std::pair<IPFlow5ID, int>>, 60, false,
                      tsl::fixed_mod_growth_policy>
    hs_map_mod_60;

#endif

typedef hopscotch_map<IPFlow5ID, int, VirtualFlowIPManagerIMP<>::hash_flow_T, std::equal_to<>,
                      std::allocator<std::pair<IPFlow5ID, int>>, 30, true,
                      tsl::fixed_size_policy>
    hs_map_30_store;

// Template class for everything
template <typename HS_MAP>
class FlowIPManagerHS_IMPbase : public VirtualFlowIPManagerIMP<> {
protected:
  int find(IPFlow5ID &f, int core = 0) override {
    auto &tab = _tables[core];
    auto *_hash = reinterpret_cast<HS_MAP *>(tab.hash);
    auto ret = _hash->find(f);
    return ret != _hash->end() ? ret->second : 0;
  }
  int insert(IPFlow5ID &f, int flowid, int core = 0) override {
    auto &tab = _tables[core];
    auto *_hash = reinterpret_cast<HS_MAP *>(tab.hash);
    (*_hash)[f] = flowid;

    // TODO: What is the correct return value?
    return flowid;
  }

  int alloc(int i) override {
    _tables[i].hash = new HS_MAP();
    if (unlikely(_tables[i].hash == nullptr)) {
      VPRINT(0,"Could not init flow table %d!", i);
      return 1;
    }

    // Request with -1, it should already be a power of two and the policy will
    // retrieve the correct one (+1)
    reinterpret_cast<HS_MAP *>(_tables[i].hash)->max_load_factor(1);
    reinterpret_cast<HS_MAP *>(_tables[i].hash)->reserve(_table_size - 1);
    VPRINT(1,"Created hash table with %d buckets\n",
          reinterpret_cast<HS_MAP *>(_tables[i].hash)->bucket_count());

    return 0;
  }

  int delete_flow(FlowControlBlock *fcb, int core) override {
    IPFlow5ID *f = get_fid(fcb);
    auto *_hash = reinterpret_cast<HS_MAP *>(_tables[core].hash);
    int ret = (*_hash).erase(*f);

    VPRINT(2,
          "Deletion of entry %p, belonging to flow %i , returned %i: %s", fcb,
          *get_flowid(fcb), ret, (ret ? "OK" : "ERR"));

    return ret ? *get_flowid(fcb) : 0;
  }

  // int free_pos(int pos, int core) override;
};

// Specialize the class


class FlowIPManagerHS_IMP : public FlowIPManagerHS_IMPbase<hs_map_30_store> {
  const char *class_name() const override { return "FlowIPManagerHS_IMP"; }
};

#if HS_VARIANTS

// class FlowIPManagerHS_IMP_XX_STORE : public FlowIPManagerHS_IMPbase<hs_map_XX_STORE> {
//   const char *class_name() const override { return "FlowIPManagerHS_IMP_XX_STORE"; }
// };
class FlowIPManagerHS_IMP_10 : public FlowIPManagerHS_IMPbase<hs_map_10> {
  const char *class_name() const override { return "FlowIPManagerHS_IMP_10"; }
};
class FlowIPManagerHS_IMP_10_STORE : public FlowIPManagerHS_IMPbase<hs_map_10_store> {
  const char *class_name() const override { return "FlowIPManagerHS_IMP_10_STORE"; }
};
class FlowIPManagerHS_IMP_20 : public FlowIPManagerHS_IMPbase<hs_map_20> {
  const char *class_name() const override { return "FlowIPManagerHS_IMP_20"; }
};
class FlowIPManagerHS_IMP_20_STORE : public FlowIPManagerHS_IMPbase<hs_map_20_store> {
  const char *class_name() const override { return "FlowIPManagerHS_IMP_20_STORE"; }
};
class FlowIPManagerHS_IMP_30 : public FlowIPManagerHS_IMPbase<hs_map_30> {
  const char *class_name() const override { return "FlowIPManagerHS_IMP_30"; }
};
class FlowIPManagerHS_IMP_30_STORE : public FlowIPManagerHS_IMPbase<hs_map_30_store> {
  const char *class_name() const override { return "FlowIPManagerHS_IMP_30_STORE"; }
};

class FlowIPManagerHS_IMP_40 : public FlowIPManagerHS_IMPbase<hs_map_40> {
  const char *class_name() const override { return "FlowIPManagerHS_IMP_40"; }
};
class FlowIPManagerHS_IMP_50 : public FlowIPManagerHS_IMPbase<hs_map_50> {
  const char *class_name() const override { return "FlowIPManagerHS_IMP_50"; }
};
class FlowIPManagerHS_IMP_60 : public FlowIPManagerHS_IMPbase<hs_map_60> {
  const char *class_name() const override { return "FlowIPManagerHS_IMP_60"; }

};
class FlowIPManagerHS_IMP_MOD_60 : public FlowIPManagerHS_IMPbase<hs_map_mod_60> {
  const char *class_name() const override { return "FlowIPManagerHS_IMP_MOD_60"; }
};

class FlowIPManagerHS_IMP_MOD : public FlowIPManagerHS_IMPbase<hs_map_mod_30_store> {
  const char *class_name() const override { return "FlowIPManagerHS_IMP_MOD"; }
};

#endif

CLICK_ENDDECLS
#endif
