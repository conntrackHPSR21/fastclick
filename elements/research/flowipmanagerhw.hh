#ifndef CLICK_FLOWIPMANAGERHW_HH
#define CLICK_FLOWIPMANAGERHW_HH
#include <click/config.h>
#include <click/string.hh>
#include <click/timer.hh>
#include <click/vector.hh>
#include <click/multithread.hh>
#include <click/pair.hh>
#include <click/flow/flowelement.hh>
#include <click/flow/common.hh>
#include <click/batchbuilder.hh>
#include <click/virtualflowipmanagerimp.hh>
#include <rte_hash.h>
#include <elements/userlevel/fromdpdkdevice.hh>
#include <click/tcphelper.hh>


CLICK_DECLS

// DPK hash tables, per core duplication
// Use HW to classify before Click 

class DPDKDevice;
struct rte_hash;

#define DELETEDF 0b0000001
#define IN_HWF   0b0000010

#define IS_DELETED(flags) (flags & DELETEDF)
#define IS_IN_HW(flags)	  (flags & IN_HWF)



class FlowIPManagerHW: public VirtualFlowIPManagerIMP<nolock,true,false,maintainerArgNone>, public TCPHelper {
    public:

        const char *class_name() const { return "FlowIPManagerHW"; }
	FlowIPManagerHW(): VirtualFlowIPManagerIMP() {}
	int parse(Args * args) override;

	// Allow to define additional handlers
	static String read_handler(Element *e, void *thunk);
	static int write_handler(const String &input, Element *e, void *thunk,
				 ErrorHandler *errh);
	virtual void add_handlers() override;
	String get_hw_counter(int cnt);

    protected:

	struct hwtable {
	    hwtable(): flow_rules_count(0)
	    {
#if IMP_COUNTERS
		flows_inserts = 0;
		flows_matches = 0;
		deleted_flows = 0;
		ht_lookups = 0;
#endif
	    }
#if IMP_COUNTERS
	    uint32_t flows_inserts, flows_matches, deleted_flows, ht_lookups;
#endif
	    uint32_t flow_rules_count;
	    uint32_t ht_count;
	};

	hwtable* _hw_tables=0;

	int find(IPFlow5ID &f, int core, Packet *p) override;
	int insert(IPFlow5ID &f, int flowid, int core, Packet * p) override;
	int maintainer(int core = 0) override;
	int alloc(int i) override;
	int delete_flow(FlowControlBlock * fcb, int core) override;
	int free_pos(int pos, int core) override;

	int _flags = 0;
	int _flow_rules_limit; // How many rules to issue to NIC
	int _ht_size;	    // Effective size of the HT, can be smaller than the FCBS

	uint16_t _threshold;

	int _autoclean;
    
	//int _timeout;
	portid_t _port_id;
	uint32_t _group;	

	int insertFlowRule(Packet *p, int32_t flowid, int core = 0);
	int insertFlowRule(uint32_t flowid, uint32_t ip_src,
			   uint32_t ip_dst, uint16_t port_src,
			   uint16_t port_dst, int proto, uint16_t q, int core = 0);
	//int remove_aged_flows(int n);
	int32_t verifyTag(Packet *p);
	inline bool taggable(Packet *p);
	
      inline uint8_t* get_flags(FlowControlBlock *fcb) {
	  return (uint8_t*) FCB_DATA(fcb,reserve_size() );
	  //return FCB_DATA(fcb, 4 + 2 + 4); 
	  //return reinterpret_cast<IPFlow5ID *> (((uint8_t *) &(fcb->data_32[1])) + sizeof(uint16_t));
    };
      inline uint16_t* get_packets(FlowControlBlock *fcb) {
	  return (uint16_t*) FCB_DATA(fcb, reserve_size() + sizeof(uint8_t) );
      }
    inline static FlowControlBlock** get_next(FlowControlBlock *fcb) {
    };
	

};

CLICK_ENDDECLS
#endif
