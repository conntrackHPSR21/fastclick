#include "flowipmanagerhw.hh"
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

int FlowIPManagerHW::parse(Args *args) {
  Element *e = 0;

  int ret =
      (*args)
          //.read_or_set("NONSYN_ACCEPT", _nonsyn_accept, true)
          //.read_or_set("NONSYN_INSERT", _nonsyn_insert, false)
          .read_or_set("HT_CAPACITY", _ht_size, _table_size)
          .read("HWLIMIT", _flow_rules_limit)
          .read("DEV", e)
          .read_or_set("GROUP", _group, 0)
          .read_or_set(
              "AUTOCLEAN", _autoclean,
              0) // delete from the HT when HW has tagged at least one time?
          .read_or_set("HW_THRESHOLD", _threshold, 0)
          .consume();
  FromDPDKDevice *fd = reinterpret_cast<FromDPDKDevice *>(e);
  _port_id = fd->get_device()->get_port_id();

  if (e == 0)
    click_chatter("Wrong FromDPDKDevice!");

  _reserve += sizeof(uint8_t); // Keep flags
  if (_threshold)
    _reserve += sizeof(uint16_t);
  return ret - (e == 0);
}

int FlowIPManagerHW::alloc(int core) {
  _flags = 0;
  //_flags |= RTE_HASH_EXTRA_FLAGS_NO_FREE_ON_DEL;

  struct rte_hash_parameters hash_params = {0};
  char buf[32];

  hash_params.name = buf;
  hash_params.entries = table_size_per_thread(_ht_size, _passing_weight);
  hash_params.key_len = sizeof(IPFlow5ID);
  hash_params.hash_func = ipv4_hash_crc;
  hash_params.hash_func_init_val = 0;
  hash_params.extra_flag = _flags;
  // hash_params.lifetime = _timeout_ms / _recycle_interval_ms;

  // Flow rules limit are per-thread
  //_flow_rules_limit = table_size_per_thread(_flow_rules_limit, _passing_weight);
  _flow_rules_limit = _flow_rules_limit / _passing_weight;

  VPRINT(0, "[%i] Real capacity for table will be %lu", core,
         hash_params.entries);
  VPRINT(0, "[%i] HW rules will be limited to %lu", core,
         _flow_rules_limit);

  sprintf(buf, "HW-%i", core);
  _tables[core].hash = rte_hash_create(&hash_params);
  if (unlikely(_tables[core].hash == nullptr)) {
    VPRINT(0, "Could not init flow table %d!", 0);
    return 1;
  }

  // TODO: only for the first thread (is it thread safe? who executes the
  // event?) 	rte_eth_dev_callback_register(_port_id,
  //rte_eth_dev_callback_register, aged_flow_callback, click_current_cpu_id());

#if IMP_COUNTERS
  if (_hw_tables == 0) {
    _hw_tables = (hwtable *)rte_malloc(
        "HWTABLES", sizeof(hwtable) * (_tables_count), CLICK_CACHE_LINE_SIZE);
    if (_hw_tables == nullptr) {
      VPRINT(0, "Could not init counters!");
      return 1;
    }
  }
#endif
  return 0;
}
// Insert a new Flow Rule for packet p with tag flowid
int FlowIPManagerHW::insertFlowRule(Packet *p, int32_t flowid, int core) {

  uint8_t rx_q = PAINT_ANNO(p);
  const struct click_ip *iph = p->ip_header();
  const struct click_tcp *tcph = p->tcp_header();

  // The queue is in the PAINT annotation. FromDPDKDevice must be set correctly!
  return insertFlowRule(flowid, iph->ip_src.s_addr, iph->ip_dst.s_addr,
                        tcph->th_sport, tcph->th_dport, p->ip_header()->ip_p,
                        PAINT_ANNO(p), core);
}

int FlowIPManagerHW::insertFlowRule(uint32_t flowid, uint32_t ip_src,
                                    uint32_t ip_dst, uint16_t port_src,
                                    uint16_t port_dst, int proto, uint16_t q,
                                    int core) {

  if (_hw_tables[core].flow_rules_count >= _flow_rules_limit) {
    if (unlikely(_verbose))
      click_chatter("No more flow rules! There are %i rules.",
                    _hw_tables[core].flow_rules_count);
    return 1;
  }

  struct rte_flow_attr attr;
  struct rte_flow_item pattern[4];
  struct rte_flow_action action[4];
  struct rte_flow *flow = NULL;
  struct rte_flow_error error;

  struct rte_flow_action_mark mark = {.id = (uint32_t)flowid};
  // struct rte_flow_action_age age = {.timeout = (uint32_t)_timeout, .reserved
  // = 0, .context = (void *) flowid};
  struct rte_flow_item_ipv4 ip_spec;
  struct rte_flow_item_ipv4 ip_mask;
  struct rte_flow_item_tcp tcp_spec;
  struct rte_flow_item_tcp tcp_mask;
  struct rte_flow_item_udp udp_spec;
  struct rte_flow_item_udp udp_mask;

  struct rte_flow_action_queue queue = {.index = q};
  int res;

  memset(pattern, 0, sizeof(pattern));
  memset(action, 0, sizeof(action));
  memset(&attr, 0, sizeof(struct rte_flow_attr));
  memset(&ip_spec, 0, sizeof(struct rte_flow_item_ipv4));
  memset(&ip_mask, 0, sizeof(struct rte_flow_item_ipv4));

  attr.ingress = 1;
  attr.egress = 0;
  attr.group = _group;

  pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;

  pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
  ip_spec.hdr.dst_addr = ip_dst;
  ip_spec.hdr.src_addr = ip_src;
  ip_mask.hdr.dst_addr = 0xffffffff;
  ip_mask.hdr.src_addr = 0xffffffff;
  pattern[1].spec = &ip_spec;
  pattern[1].mask = &ip_mask;

  if (likely(proto == IP_PROTO_TCP)) {
    memset(&tcp_spec, 0, sizeof(struct rte_flow_item_tcp));
    memset(&tcp_mask, 0, sizeof(struct rte_flow_item_tcp));
    pattern[2].type = RTE_FLOW_ITEM_TYPE_TCP;
    tcp_spec.hdr.src_port = port_src;
    tcp_spec.hdr.dst_port = port_dst;
    tcp_mask.hdr.src_port = 0xffff;
    tcp_mask.hdr.dst_port = 0xffff;
    pattern[2].spec = &tcp_spec;
    pattern[2].mask = &tcp_mask;
  } else if (likely(proto == IP_PROTO_UDP)) {
    memset(&udp_spec, 0, sizeof(struct rte_flow_item_udp));
    memset(&udp_mask, 0, sizeof(struct rte_flow_item_udp));
    pattern[2].type = RTE_FLOW_ITEM_TYPE_UDP;
    udp_spec.hdr.src_port = port_src;
    udp_spec.hdr.dst_port = port_dst;
    udp_mask.hdr.src_port = 0xffff;
    udp_mask.hdr.dst_port = 0xffff;
    pattern[2].spec = &udp_spec;
    pattern[2].mask = &udp_mask;
  } else {
    // Unsupported protocol. Return.
    return 2;
  }
  pattern[3].type = RTE_FLOW_ITEM_TYPE_END;

  action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
  action[0].conf = &queue;
  action[1].type = RTE_FLOW_ACTION_TYPE_MARK;
  action[1].conf = &mark;
  // if (likely(_timeout)) {
  //   action[2].type = RTE_FLOW_ACTION_TYPE_AGE;
  //   action[2].conf = &age;
  //   action[3].type = RTE_FLOW_ACTION_TYPE_END;
  // } else
  action[2].type = RTE_FLOW_ACTION_TYPE_END;

  res = rte_flow_validate(_port_id, &attr, pattern, action, &error);

  if (likely(res == 0)) {
    flow = rte_flow_create(_port_id, &attr, pattern, action, &error);

    if (unlikely(flow == 0)) {
      if (unlikely(_verbose))
        click_chatter("Error while inserting flow rule in the device: (%i) %s",
                      error.type, String(error.message).c_str());
      return error.type;
    }
    _hw_tables[core].flow_rules_count++;
    return 0;
  } else {
    if (unlikely(_verbose))
      click_chatter("Error while validating the rule: (%i) %s", error.type,
                    String(error.message).c_str());
    return error.type;
  }
}

// Verify if p has been tagged, return 0 if not
int32_t FlowIPManagerHW::verifyTag(Packet *p) {
  rte_mbuf *mbuf = 0;

  if (likely(DPDKDevice::is_dpdk_packet(p)) &&
      (mbuf = (struct rte_mbuf *)p->destructor_argument())) {
    if (likely(mbuf->ol_flags & PKT_RX_FDIR_ID)) {
      return mbuf->hash.fdir.hi;
    }
  }
  return 0;
}

bool FlowIPManagerHW::taggable(Packet *p) {
  return p->ip_header()->ip_p == IP_PROTO_UDP ||
         p->ip_header()->ip_p == IP_PROTO_TCP;
}

int FlowIPManagerHW::find(IPFlow5ID &f, int core, Packet *p) {

  auto &tab = _tables[core];
  auto &hwtab = _hw_tables[core];
  FlowControlBlock *fcb;
  rte_hash *ht = reinterpret_cast<rte_hash *>(tab.hash);
  uint64_t this_flow = verifyTag(p);
  VPRINT(3,"verifytag returned %i", this_flow);
  if (this_flow) // The packet has been matched by a flow rule
  {
    VPRINT(3, "flow already tagged with flowid %i", this_flow);
#if IMP_COUNTERS
    hwtab.flows_matches++;
#endif
    fcb = get_fcb(this_flow, core);
    if (unlikely(_autoclean && !IS_DELETED(*get_flags(fcb)))) {
      // to be deleted from HT
      if (unlikely(rte_hash_del_key(ht, &f) < 0)) {
        VPRINT(3, "Error while deleting key %i from HT", *get_flowid(fcb));
      } else {
        *get_flags(fcb) |= DELETEDF;
#if IMP_COUNTERS
        hwtab.deleted_flows++;
#endif
	VPRINT(3, "Succesfully deleted key %i from HT", *get_flowid(fcb));
      }
    } //to be deleted from HT
    return this_flow;
  } // if packet tagged
  else { 
      // packet not tagged: lookup in HT, then check if it has to be inserted in HW

#if IMP_COUNTERS
    hwtab.ht_lookups++;
#endif

    int ret = rte_hash_lookup_data(ht, &f, (void **)&this_flow);
    VPRINT(3, "rte_hash_lookup returned %i",ret);
    if (ret >= 0) {
      fcb = get_fcb(this_flow, core);
      uint8_t *flags = get_flags(fcb);
      (*get_packets(fcb))++;
      VPRINT(4,"Flow %i: seen %i packets so far. Is it enough? %i. Do we need to insert? %i. Flags are %02X", *get_flowid(fcb),
                      *get_packets(fcb),
		      (*get_packets(fcb) > _threshold),
		      !IS_IN_HW(*flags),
		      *flags
		      );
      if (!IS_IN_HW(*flags) &&
	      _hw_tables[core].flow_rules_count < _flow_rules_limit) {
        if (*get_packets(fcb) > _threshold) {
          // insert in HW if we have seen enough packets
          if (likely(taggable(p))) {
            ret = insertFlowRule(p, this_flow, core);
            VPRINT(3, "INSERT RULE RETURNED %i", ret);
            if (ret == 0) {
              *flags = *flags | IN_HWF;
	      _hw_tables[core].ht_count++;
#if IMP_COUNTERS
              _hw_tables[core].flows_inserts++;
#endif
            } // hw insert ok
          } // taggable
        } // packets > threshold
      } // To be inserted in HW
    } // Found in HT
    return ret >= 0 ? this_flow : 0;
  } //Non tagged packet
}

int FlowIPManagerHW::insert(IPFlow5ID &f, int flowid, int core, Packet *p) {
  auto &tab = _tables[core];
  rte_hash *ht = reinterpret_cast<rte_hash *>(tab.hash);

  uint64_t ff = flowid;
  uint32_t ret = rte_hash_add_key_data(ht, &f, (void *)ff);
  if (ret != 0)
    return 0;

  FlowControlBlock *fcb = get_fcb(flowid, core);
  if (_threshold) {
    *get_flags(fcb) = 0; // Just to be safe
    *get_packets(fcb) = 0;
  } else {
    int proto = p->ip_header()->ip_p;
    if (likely(taggable(p))) {
      ret = insertFlowRule(p, flowid, core);
      VPRINT(3, "INSERT RULE RETURNED %i", ret);
      if (ret == 0) {
        *get_flags(fcb) = IN_HWF;
#if IMP_COUNTERS
        _hw_tables[core].flows_inserts++;
#endif
      }
    }
  }

  return flowid; // We are happy anyway, to be compatible with other methods we
                 // cannot consider error when we can't insert in HW
}

int FlowIPManagerHW::delete_flow(FlowControlBlock *fcb, int core) {
  *get_flags(fcb) = 0;
  int ret = rte_hash_del_key(reinterpret_cast<rte_hash *>(_tables[core].hash),
                             get_fid(fcb));
  if(ret >= 0)
    _hw_tables[core].ht_count--;

  VPRINT(3, "Deletion of entry %p, belonging to flow %i , returned %i: %s", fcb,
         *get_flowid(fcb), ret,
         (ret >= 0 ? "OK" : ret == -ENOENT ? "ENOENT" : "EINVAL"));

  return ret;
}

int FlowIPManagerHW::free_pos(int pos, int core) {
  return rte_hash_free_key_with_position(
      reinterpret_cast<rte_hash *>(_tables[core].hash), pos);
}
int FlowIPManagerHW::maintainer(int core) {
  click_chatter("Maintainer not implemented in %s!", class_name());
  return 0;
}

// Here we redefine the handler methods that will override the original ones
enum {
  h_hw_rules_count = 1000,
  h_hw_tot_count
#if IMP_COUNTERS
  ,
  h_hw_flows_inserts,
  h_hw_flows_matches,
  h_hw_flows_matches_ratio,
  h_hw_flows_deleted,
  h_hw_ht_lookups
#endif
};

String FlowIPManagerHW::read_handler(Element *e, void *thunk) {
  FlowIPManagerHW *f = static_cast<FlowIPManagerHW *>(e);
    
  switch ((intptr_t)thunk) {
  case h_hw_rules_count:
  case h_avg_load:
  case h_hw_tot_count:
  case h_count:
#if IMP_COUNTERS
  case h_hw_flows_matches:
  case h_hw_flows_inserts:
  case h_hw_flows_matches_ratio:
  case h_hw_flows_deleted:
  case h_hw_ht_lookups:
#endif
    return String(f->get_hw_counter((intptr_t)thunk));

  default:
    return VirtualFlowIPManagerIMP::read_handler(e, thunk);
  }
}

void FlowIPManagerHW::add_handlers() {
  VirtualFlowIPManagerIMP::add_handlers();
  // We want to handle count and avg_load here
  add_read_handler("count", read_handler, h_count);
  add_read_handler("avg_load", read_handler, h_avg_load);
  add_read_handler("flow_rules_count", read_handler, h_hw_rules_count);
  add_read_handler("total_count", read_handler, h_hw_tot_count);
#if IMP_COUNTERS
  add_read_handler("flow_inserts", read_handler, h_hw_flows_inserts);
  add_read_handler("flow_deleted", read_handler, h_hw_flows_deleted);
  add_read_handler("matched_hw_packets", read_handler, h_hw_flows_matches);
  add_read_handler("matched_hw_packets_ratio", read_handler,
                   h_hw_flows_matches_ratio);
  add_read_handler("ht_lookups", read_handler, h_hw_ht_lookups);
#endif
}

String FlowIPManagerHW::get_hw_counter(int cnt) {
  uint64_t s = 0;
  double sf = 0;
  for (int i = 0; i < _tables_count; i++)
    // if(_hw_tables[i] != 0)
    switch (cnt) {
    case h_hw_rules_count:
      s += _hw_tables[i].flow_rules_count;
      break;
    case h_count:
    case h_avg_load:
      s += _hw_tables[i].ht_count;
      break;
    case h_hw_tot_count:
      s += _hw_tables[i].flow_rules_count + _hw_tables[i].ht_count;
      break;
#if IMP_COUNTERS
    case h_hw_flows_inserts:
      s += _hw_tables[i].flows_inserts;
      break;
    case h_hw_flows_matches:
      s += _hw_tables[i].flows_matches;
      break;
    case h_hw_flows_matches_ratio:
      sf += _tables[i].count_lookups ? (((double)_hw_tables[i].flows_matches) /
                                        _tables[i].count_lookups)
                                     : 0;
      break;
    case h_hw_flows_deleted:
      s += _hw_tables[i].deleted_flows;
      break;
    case h_hw_ht_lookups:
      s += _hw_tables[i].ht_lookups;
      break;
#endif
    }
  if ( cnt == h_avg_load)
      sf = s / (double) _total_size;
  return String(sf > 0 ? sf : s);
}

CLICK_ENDDECLS

ELEMENT_REQUIRES(flow dpdk dpdk19)
EXPORT_ELEMENT(FlowIPManagerHW)
ELEMENT_MT_SAFE(FlowIPManagerHW)
