#ifndef _FLOWBUFFER_H_
#define _FLOWBUFFER_H_
#include "flit.hpp"
#include "reservation.hpp"
#include <map>
#include <queue>
#include "globals.hpp"

class TrafficManager;

#define FLOW_STAT_SPEC 0
#define FLOW_STAT_NACK 1
#define FLOW_STAT_WAIT 2
#define FLOW_STAT_NORM 3
#define FLOW_STAT_NORM_READY 4
#define FLOW_STAT_SPEC_READY 5
#define FLOW_STAT_NOT_READY 6
#define FLOW_STAT_FINAL_NOT_READY 7
#define FLOW_STAT_LIFETIME 9

#define FLOW_DONE_NOT 0
#define FLOW_DONE_DONE 1
#define FLOW_DONE_MORE 2

struct flow{
  int flid;
  short vc;
  int flow_size;
  int create_time;
  int data_to_generate;
  short src;
  short dest;
  short cl;

  int sn;
  flow(){
    buffer=NULL;
  }
  queue<Flit*>* buffer;
  ~flow(){
    if(buffer){
      while(!buffer->empty()){
	buffer->front()->Free();
	buffer->pop();
      }
      delete buffer;
      buffer = NULL;
    } else {
      assert(false);
    }
  }
};

class FlowBuffer{
public:
  FlowBuffer(TrafficManager* p, int src, int id, int mode, flow* fl);
  ~FlowBuffer();
  //activate directly behaves as the constructor
  void Activate(int src, int id, int mode, flow* fl);
  void Deactivate();
  //init is called by activate and reset and only modifed some state for a new flow
  void Init( flow* fl);
  void Reset();

  bool ack(int sn);
  bool nack(int sn);
  void grant(int time, int try_again, int lat);
  void Reactivate();
  void ReconstructFlit();

  bool eligible();
  bool send_norm_ready();
  bool send_spec_ready();
  int done();


  Flit* send();
  Flit* front();

  void active_update();
  void ecn_update();

  inline bool active(){
    return _active;
  }
  inline int priority(){
    if(_status== FLOW_STATUS_NORM){
      return 1;
    } else {
      return 0;
    }
  }

  bool _active;
  bool _was_reset;
  bool _was_reset_sometime;
  int _time_to_send_res;

  int  _last_payload;

  TrafficManager* parent;
  map<int, int> _flit_status;
  map<int, Flit*> _flit_buffer;
  queue<flow*> _flow_queue;
  flow* fl;
  Flit* _reservation_flit;
  int _reserved_time;
  int _future_reserved_time;//reservation_tail_reserve=on;
  int _expected_latency;//deduct this amount fromt he reserved time 
  int _src;
  int _dest;
  int _id;
  int _mode;

  //spec mode
  int _status;
  bool _tail_sent; //if the last flit sent was a tail
  int _last_sn;
  int _guarantee_sent;
  int _ready;
  bool _res_sent; //I meant res_sent
  int _vc;
  int _reserved_slots; //this variable is needed because _readyonly reflect the number of packets generated, since I use lazy packet generation, I need to keep the numbers serparate
  int _total_reserved_slots;//none chunk limit multiple flows
  int _spec_outstanding;
  bool _res_outstanding;
  int _sleep_time; //pause the buffer before issuing next res to the same dest


  //ECN mode
  int _IRD;
  int _IRD_timer; 
  int _IRD_wait;

  //these variables for stat collection
  vector<int> _stats;
  int _no_retransmit_loss;
  int _fast_retransmit;
  bool _watch;
  long _total_wait;
  bool _reservation_check;
  int _last_send_time;
  int _last_nack_time;
};

#endif
