// $Id$

/*
  Copyright (c) 2007-2011, Trustees of The Leland Stanford Junior University
  All rights reserved.

  Redistribution and use in source and binary forms, with or without modification,
  are permitted provided that the following conditions are met:

  Redistributions of source code must retain the above copyright notice, this list
  of conditions and the following disclaimer.
  Redistributions in binary form must reproduce the above copyright notice, this 
  list of conditions and the following disclaimer in the documentation and/or 
  other materials provided with the distribution.
  Neither the name of the Stanford University nor the names of its contributors 
  may be used to endorse or promote products derived from this software without 
  specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR 
  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <sstream>
#include <cmath>
#include <fstream>
#include <limits>
#include <cstdlib>

#include "booksim.hpp"
#include "booksim_config.hpp"
#include "trafficmanager.hpp"
#include "random_utils.hpp" 
#include "vc.hpp"
#include "packet_reply_info.hpp"

#define WATCH_FLID -1
#define MAX(X,Y) (X>Y?(X):(Y))

map<int, vector<int> > gDropStats;

int gExpirationTime = 0;

vector<int> gStatAckReceived;
vector<int> gStatAckEffective;
vector<int> gStatAckSent;

vector<int> gStatNackReceived;
vector<int> gStatNackEffective;
vector<int> gStatNackSent;

vector<int> gStatGrantReceived;
Stats** gStatGrantTimeNow;
Stats** gStatGrantTimeFuture;

vector<int> gStatReservationReceived;
Stats** gStatReservationTimeNow;
Stats** gStatReservationTimeFuture;

vector<int> gStatSpecReceived;
vector<int> gStatSpecDuplicate;

vector<int> gStatNormSent;
vector<int> gStatNormReceived;
vector<int> gStatNormDuplicate;

vector<int> gStatLostPacket;

Stats* gStatROBRange;

Stats* gStatFlowSenderLatency;
Stats* gStatFlowLatency;
Stats* gStatActiveFlowBuffers;
Stats* gStatResponseBuffer;

Stats* gStatPureNetworkLatency;
Stats* gStatAckLatency;
Stats* gStatNackLatency;
Stats* gStatResLatency;
Stats* gStatGrantLatency; 
Stats* gStatSpecLatency;
Stats* gStatNormLatency;
Stats* gStatSourceTrueLatency;
Stats* gStatSourceLatency;
Stats* gStatNackByPacket;


vector<int> gStatFlowStats;

TrafficManager::TrafficManager( const Configuration &config, const vector<Network *> & net )
  : Module( 0, "traffic_manager" ), _net(net), _empty_network(false), _deadlock_timer(0), _last_id(-1), _last_pid(-1), _timed_mode(false), _warmup_time(-1), _drain_time(-1), _cur_id(0), _cur_pid(0), _cur_tid(0), _time(0)
{


  gStatFlowStats.resize(FLOW_STAT_LIFETIME+1+1,0);

  _nodes = _net[0]->NumNodes( );
  _routers = _net[0]->NumRouters( );
  _num_vcs = config.GetInt("num_vcs");

  _flow_buffer_capacity = config.GetInt("flow_buffer_capacity");
  _max_flow_buffers = config.GetInt("flow_buffers");
  _last_receive_flow_buffer.resize(_nodes,0);
  _last_send_flow_buffer.resize(_nodes,0);
  _last_normal_flow_buffer.resize(_nodes,0);
   _last_spec_flow_buffer.resize(_nodes,0);
  _last_normal_vc.resize(_nodes,0);
  _last_spec_vc.resize(_nodes,0);
  _flow_buffer.resize(_nodes);

  for(int i = 0; i<_nodes; i++){
    _flow_buffer[i].resize(_max_flow_buffers,NULL);
  }
  _flow_size = config.GetInt("flow_size");


  _rob.resize(_nodes);
  _reservation_schedule.resize(_nodes, 0);
  _response_packets.resize(_nodes);
  _cur_flid = 0;
  gExpirationTime =  config.GetInt("expiration_time");



  gStatAckReceived.resize(_nodes,0);
  gStatAckEffective.resize(_nodes,0);
  gStatAckSent.resize(_nodes,0);

  gStatNackReceived.resize(_nodes,0);
  gStatNackEffective.resize(_nodes,0);
  gStatNackSent.resize(_nodes,0);

  gStatGrantReceived.resize(_nodes,0);

  gStatReservationReceived.resize(_nodes,0);


  gStatGrantTimeNow=new Stats*[_nodes];
  gStatGrantTimeFuture=new Stats*[_nodes];
  gStatReservationTimeNow=new Stats*[_nodes];
  gStatReservationTimeFuture=new Stats*[_nodes];

  for(int i = 0; i< _nodes; i++){
    ostringstream tmp_name;
    tmp_name << "grant_time_now_" << i;
    gStatGrantTimeNow[i] =  new Stats( this, tmp_name.str() , 1.0, 1000 );
    tmp_name.str("");
    tmp_name << "grant_time_future_" << i;
    gStatGrantTimeFuture[i] =  new Stats( this, tmp_name.str() , 1.0, 1000 );
    tmp_name.str("");
    tmp_name << "res_time_now_" << i;
    gStatReservationTimeNow[i] =  new Stats( this, tmp_name.str() , 1.0, 1000 );
    tmp_name.str("");
    tmp_name << "res_time_future_" << i;
    gStatReservationTimeFuture[i] =  new Stats( this, tmp_name.str() , 1.0, 1000 );
    tmp_name.str("");
  }

  gStatSpecReceived.resize(_nodes,0);
  gStatSpecDuplicate.resize(_nodes,0);

  gStatNormSent.resize(_nodes,0);
  gStatNormReceived.resize(_nodes,0);
  gStatNormDuplicate.resize(_nodes,0);

  gStatLostPacket.resize(_nodes,0);

  gStatROBRange =  new Stats( this, "rob_range" , 1.0, 300 );
  
  gStatFlowSenderLatency =  new Stats( this, "flow_sender_latency" , 1.0, 5000 );
  gStatFlowLatency=  new Stats( this, "flow_latency" , 1.0, 5000 );
  gStatActiveFlowBuffers=  new Stats( this, "sender_active_flows" , 1.0, 10 );
  gStatResponseBuffer=  new Stats( this, "response_range" , 1.0, 10 );

  gStatPureNetworkLatency =  new Stats( this, "net_hist" , 1.0, 5000 );
  gStatAckLatency=  new Stats( this, "ack_hist" , 1.0, 1000 );
  gStatNackLatency=  new Stats( this, "nack_hist" , 1.0, 1000 );
  gStatResLatency=  new Stats( this, "res_hist" , 1.0, 1000 );
  gStatGrantLatency=  new Stats( this, "grant_hist" , 1.0, 1000 );
  gStatSpecLatency=  new Stats( this, "spec_hist" , 1.0, 5000 );
  gStatNormLatency=  new Stats( this, "norm_hist" , 1.0, 5000 );

  gStatSourceLatency=  new Stats( this, "source_queue_hist" , 1.0, 5000 );
  gStatSourceTrueLatency=  new Stats( this, "source_truequeue_hist" , 1.0, 5000 );


  gStatNackByPacket = new Stats(this, "nack_by_sn", 1.0, 1000);
  //nodes higher than limit do not produce or receive packets
  //for default limit = sources

  _limit = config.GetInt( "limit" );
  if(_limit == 0){
    _limit = _nodes;
  }
  assert(_limit<=_nodes);
 
  _subnets = config.GetInt("subnets");
  assert(_subnets == 1);
  _subnet.resize(Flit::NUM_FLIT_TYPES);
  _subnet[Flit::READ_REQUEST] = config.GetInt("read_request_subnet");
  _subnet[Flit::READ_REPLY] = config.GetInt("read_reply_subnet");
  _subnet[Flit::WRITE_REQUEST] = config.GetInt("write_request_subnet");
  _subnet[Flit::WRITE_REPLY] = config.GetInt("write_reply_subnet");

  // ============ Message priorities ============ 

  string priority = config.GetStr( "priority" );

  if ( priority == "class" ) {
    _pri_type = class_based;
  } else if ( priority == "age" ) {
    _pri_type = age_based;
  } else if ( priority == "network_age" ) {
    _pri_type = network_age_based;
  } else if ( priority == "local_age" ) {
    _pri_type = local_age_based;
  } else if ( priority == "queue_length" ) {
    _pri_type = queue_length_based;
  } else if ( priority == "hop_count" ) {
    _pri_type = hop_count_based;
  } else if ( priority == "sequence" ) {
    _pri_type = sequence_based;
  } else if ( priority == "none" ) {
    _pri_type = none;
  } else if ( priority == "other"){
    _pri_type = other;//custom
  } else {
    Error( "Unkown priority value: " + priority );
  }

  _replies_inherit_priority = config.GetInt("replies_inherit_priority");

  // ============ Routing ============ 

  string rf = config.GetStr("routing_function") + "_" + config.GetStr("topology");
  map<string, tRoutingFunction>::const_iterator rf_iter = gRoutingFunctionMap.find(rf);
  if(rf_iter == gRoutingFunctionMap.end()) {
    Error("Invalid routing function: " + rf);
  }
  _rf = rf_iter->second;

  // ============ Traffic ============ 

  _classes = config.GetInt("classes");
  _use_read_write = config.GetIntArray("use_read_write");
  if(_use_read_write.empty()) {
    _use_read_write.push_back(config.GetInt("use_read_write"));
  }
  _use_read_write.resize(_classes, _use_read_write.back());

  _read_request_size = config.GetIntArray("read_request_size");
  if(_read_request_size.empty()) {
    _read_request_size.push_back(config.GetInt("read_request_size"));
  }
  _read_request_size.resize(_classes, _read_request_size.back());

  _read_reply_size = config.GetIntArray("read_reply_size");
  if(_read_reply_size.empty()) {
    _read_reply_size.push_back(config.GetInt("read_reply_size"));
  }
  _read_reply_size.resize(_classes, _read_reply_size.back());

  _write_request_size = config.GetIntArray("write_request_size");
  if(_write_request_size.empty()) {
    _write_request_size.push_back(config.GetInt("write_request_size"));
  }
  _write_request_size.resize(_classes, _write_request_size.back());

  _write_reply_size = config.GetIntArray("write_reply_size");
  if(_write_reply_size.empty()) {
    _write_reply_size.push_back(config.GetInt("write_reply_size"));
  }
  _write_reply_size.resize(_classes, _write_reply_size.back());

  _packet_size = config.GetIntArray( "const_flits_per_packet" );
  if(_packet_size.empty()) {
    _packet_size.push_back(config.GetInt("const_flits_per_packet"));
  }
  _packet_size.resize(_classes, _packet_size.back());
  
  for(int c = 0; c < _classes; ++c)
    if(_use_read_write[c])
      _packet_size[c] = (_read_request_size[c] + _read_reply_size[c] +
			 _write_request_size[c] + _write_reply_size[c]) / 2;

  _load = config.GetFloatArray("injection_rate"); 
  if(_load.empty()) {
    _load.push_back(config.GetFloat("injection_rate"));
  }
  _load.resize(_classes, _load.back());

  if(config.GetInt("injection_rate_uses_flits")) {
    for(int c = 0; c < _classes; ++c){
      _load[c] /= (double)_packet_size[c];
      _load[c] /=_flow_size;
    }
  }

  _traffic = config.GetStrArray("traffic");
  _traffic.resize(_classes, _traffic.back());

  _traffic_function.clear();
  for(int c = 0; c < _classes; ++c) {
    map<string, tTrafficFunction>::const_iterator iter = gTrafficFunctionMap.find(_traffic[c]);
    if(iter == gTrafficFunctionMap.end()) {
      Error("Invalid traffic function: " + _traffic[c]);
    }
    _traffic_function.push_back(iter->second);
  }

  _class_priority = config.GetIntArray("class_priority"); 
  if(_class_priority.empty()) {
    _class_priority.push_back(config.GetInt("class_priority"));
  }
  _class_priority.resize(_classes, _class_priority.back());

  for(int c = 0; c < _classes; ++c) {
    int const & prio = _class_priority[c];
    if(_class_prio_map.count(prio) > 0) {
      _class_prio_map.find(prio)->second.second.push_back(c);
    } else {
      _class_prio_map.insert(make_pair(prio, make_pair(-1, vector<int>(1, c))));
    }
  }

  vector<string> inject = config.GetStrArray("injection_process");
  inject.resize(_classes, inject.back());

  _injection_process.clear();
  for(int c = 0; c < _classes; ++c) {
    map<string, tInjectionProcess>::iterator iter = gInjectionProcessMap.find(inject[c]);
    if(iter == gInjectionProcessMap.end()) {
      Error("Invalid injection process: " + inject[c]);
    }
    _injection_process.push_back(iter->second);
  }

  // ============ Injection VC states  ============ 

  _buf_states.resize(_nodes);
  _last_vc.resize(_nodes);
  _last_interm.resize(_nodes);
  for ( int source = 0; source < _nodes; ++source ) {
    _buf_states[source].resize(_subnets);
    _last_vc[source].resize(_subnets);
    _last_interm[source].resize(_subnets);
    for ( int subnet = 0; subnet < _subnets; ++subnet ) {
      ostringstream tmp_name;
      tmp_name << "terminal_buf_state_" << source << "_" << subnet;
      _buf_states[source][subnet] = new BufferState( config, this, tmp_name.str( ) );
      _last_vc[source][subnet].resize(_classes, 0);
      _last_interm[source][subnet].resize(_classes, -1);
    }
  }

  // ============ Injection queues ============ 

  _qtime.resize(_nodes);
  _qdrained.resize(_nodes);
  //_partial_packets.resize(_nodes);

  for ( int s = 0; s < _nodes; ++s ) {
    _qtime[s].resize(_classes);
    _qdrained[s].resize(_classes);
    //_partial_packets[s].resize(_classes);
  }

  _total_in_flight_flits.resize(_classes);
  _measured_in_flight_flits.resize(_classes);
  _retired_packets.resize(_classes);

  _packets_sent.resize(_nodes);
  _batch_size = config.GetInt( "batch_size" );
  _batch_count = config.GetInt( "batch_count" );
  _repliesPending.resize(_nodes);
  _requestsOutstanding.resize(_nodes);
  _maxOutstanding = config.GetInt ("max_outstanding_requests");  

  // ============ Statistics ============ 

  _plat_stats.resize(_classes);
  _overall_min_plat.resize(_classes);
  _overall_avg_plat.resize(_classes);
  _overall_max_plat.resize(_classes);

  _tlat_stats.resize(_classes);
  _overall_min_tlat.resize(_classes);
  _overall_avg_tlat.resize(_classes);
  _overall_max_tlat.resize(_classes);

  _frag_stats.resize(_classes);
  _overall_min_frag.resize(_classes);
  _overall_avg_frag.resize(_classes);
  _overall_max_frag.resize(_classes);

  _pair_plat.resize(_classes);
  _pair_tlat.resize(_classes);
  
  _hop_stats.resize(_classes);
  
  _sent_flits.resize(_classes);
  _accepted_flits.resize(_classes);
  
  _overall_accepted.resize(_classes);
  _overall_accepted_min.resize(_classes);

  for ( int c = 0; c < _classes; ++c ) {
    ostringstream tmp_name;
    tmp_name << "plat_stat_" << c;
    _plat_stats[c] = new Stats( this, tmp_name.str( ), 1.0, 5000 );
    _stats[tmp_name.str()] = _plat_stats[c];
    tmp_name.str("");

    tmp_name << "overall_min_plat_stat_" << c;
    _overall_min_plat[c] = new Stats( this, tmp_name.str( ), 1.0, 1000 );
    _stats[tmp_name.str()] = _overall_min_plat[c];
    tmp_name.str("");  
    tmp_name << "overall_avg_plat_stat_" << c;
    _overall_avg_plat[c] = new Stats( this, tmp_name.str( ), 1.0, 1000 );
    _stats[tmp_name.str()] = _overall_avg_plat[c];
    tmp_name.str("");  
    tmp_name << "overall_max_plat_stat_" << c;
    _overall_max_plat[c] = new Stats( this, tmp_name.str( ), 1.0, 1000 );
    _stats[tmp_name.str()] = _overall_max_plat[c];
    tmp_name.str("");  

    tmp_name << "tlat_stat_" << c;
    _tlat_stats[c] = new Stats( this, tmp_name.str( ), 1.0, 1000 );
    _stats[tmp_name.str()] = _tlat_stats[c];
    tmp_name.str("");

    tmp_name << "overall_min_tlat_stat_" << c;
    _overall_min_tlat[c] = new Stats( this, tmp_name.str( ), 1.0, 1000 );
    _stats[tmp_name.str()] = _overall_min_tlat[c];
    tmp_name.str("");  
    tmp_name << "overall_avg_tlat_stat_" << c;
    _overall_avg_tlat[c] = new Stats( this, tmp_name.str( ), 1.0, 1000 );
    _stats[tmp_name.str()] = _overall_avg_tlat[c];
    tmp_name.str("");  
    tmp_name << "overall_max_tlat_stat_" << c;
    _overall_max_tlat[c] = new Stats( this, tmp_name.str( ), 1.0, 1000 );
    _stats[tmp_name.str()] = _overall_max_tlat[c];
    tmp_name.str("");  

    tmp_name << "frag_stat_" << c;
    _frag_stats[c] = new Stats( this, tmp_name.str( ), 1.0, 100 );
    _stats[tmp_name.str()] = _frag_stats[c];
    tmp_name.str("");
    tmp_name << "overall_min_frag_stat_" << c;
    _overall_min_frag[c] = new Stats( this, tmp_name.str( ), 1.0, 100 );
    _stats[tmp_name.str()] = _overall_min_frag[c];
    tmp_name.str("");
    tmp_name << "overall_avg_frag_stat_" << c;
    _overall_avg_frag[c] = new Stats( this, tmp_name.str( ), 1.0, 100 );
    _stats[tmp_name.str()] = _overall_avg_frag[c];
    tmp_name.str("");
    tmp_name << "overall_max_frag_stat_" << c;
    _overall_max_frag[c] = new Stats( this, tmp_name.str( ), 1.0, 100 );
    _stats[tmp_name.str()] = _overall_max_frag[c];
    tmp_name.str("");

    tmp_name << "hop_stat_" << c;
    _hop_stats[c] = new Stats( this, tmp_name.str( ), 1.0, 20 );
    _stats[tmp_name.str()] = _hop_stats[c];
    tmp_name.str("");

    _pair_plat[c].resize(_nodes*_nodes);
    _pair_tlat[c].resize(_nodes*_nodes);

    _sent_flits[c].resize(_nodes);
    _accepted_flits[c].resize(_nodes);
    
    for ( int i = 0; i < _nodes; ++i ) {
      tmp_name << "sent_stat_" << c << "_" << i;
      _sent_flits[c][i] = new Stats( this, tmp_name.str( ) );
      _stats[tmp_name.str()] = _sent_flits[c][i];
      tmp_name.str("");    
      
      for ( int j = 0; j < _nodes; ++j ) {
	tmp_name << "pair_plat_stat_" << c << "_" << i << "_" << j;
	_pair_plat[c][i*_nodes+j] = new Stats( this, tmp_name.str( ), 1.0, 250 );
	_stats[tmp_name.str()] = _pair_plat[c][i*_nodes+j];
	tmp_name.str("");
	
	tmp_name << "pair_tlat_stat_" << c << "_" << i << "_" << j;
	_pair_tlat[c][i*_nodes+j] = new Stats( this, tmp_name.str( ), 1.0, 250 );
	_stats[tmp_name.str()] = _pair_tlat[c][i*_nodes+j];
	tmp_name.str("");
      }
    }
    
    for ( int i = 0; i < _nodes; ++i ) {
      tmp_name << "accepted_stat_" << c << "_" << i;
      _accepted_flits[c][i] = new Stats( this, tmp_name.str( ) );
      _stats[tmp_name.str()] = _accepted_flits[c][i];
      tmp_name.str("");    
    }
    
    tmp_name << "overall_acceptance_" << c;
    _overall_accepted[c] = new Stats( this, tmp_name.str( ) );
    _stats[tmp_name.str()] = _overall_accepted[c];
    tmp_name.str("");

    tmp_name << "overall_min_acceptance_" << c;
    _overall_accepted_min[c] = new Stats( this, tmp_name.str( ) );
    _stats[tmp_name.str()] = _overall_accepted_min[c];
    tmp_name.str("");
    
  }

  _batch_time = new Stats( this, "batch_time" );
  _stats["batch_time"] = _batch_time;
  
  _overall_batch_time = new Stats( this, "overall_batch_time" );
  _stats["overall_batch_time"] = _overall_batch_time;
  
  _slowest_flit.resize(_classes, -1);

  // ============ Simulation parameters ============ 

  _total_sims = config.GetInt( "sim_count" );

  _router.resize(_subnets);
  for (int i=0; i < _subnets; ++i) {
    _router[i] = _net[i]->GetRouters();
  }

  //seed the network
  RandomSeed(config.GetInt("seed"));

  string sim_type = config.GetStr( "sim_type" );

  if ( sim_type == "latency" ) {
    _sim_mode = latency;
  } else if ( sim_type == "throughput" ) {
    _sim_mode = throughput;
  }  else if ( sim_type == "batch" ) {
    _sim_mode = batch;
  }  else if (sim_type == "timed_batch"){
    _sim_mode = batch;
    _timed_mode = true;
  }
  else {
    Error( "Unknown sim_type value : " + sim_type );
  }

  _sample_period = config.GetInt( "sample_period" );
  _max_samples    = config.GetInt( "max_samples" );
  _warmup_periods = config.GetInt( "warmup_periods" );

  _measure_stats = config.GetIntArray( "measure_stats" );
  if(_measure_stats.empty()) {
    _measure_stats.push_back(config.GetInt("measure_stats"));
  }
  _measure_stats.resize(_classes, _measure_stats.back());

  _latency_thres = config.GetFloatArray( "latency_thres" );
  if(_latency_thres.empty()) {
    _latency_thres.push_back(config.GetFloat("latency_thres"));
  }
  _latency_thres.resize(_classes, _latency_thres.back());

  _warmup_threshold = config.GetFloatArray( "warmup_thres" );
  if(_warmup_threshold.empty()) {
    _warmup_threshold.push_back(config.GetFloat("warmup_thres"));
  }
  _warmup_threshold.resize(_classes, _warmup_threshold.back());

  _acc_warmup_threshold = config.GetFloatArray( "acc_warmup_thres" );
  if(_acc_warmup_threshold.empty()) {
    _acc_warmup_threshold.push_back(config.GetFloat("acc_warmup_thres"));
  }
  _acc_warmup_threshold.resize(_classes, _acc_warmup_threshold.back());

  _stopping_threshold = config.GetFloatArray( "stopping_thres" );
  if(_stopping_threshold.empty()) {
    _stopping_threshold.push_back(config.GetFloat("stopping_thres"));
  }
  _stopping_threshold.resize(_classes, _stopping_threshold.back());

  _acc_stopping_threshold = config.GetFloatArray( "acc_stopping_thres" );
  if(_acc_stopping_threshold.empty()) {
    _acc_stopping_threshold.push_back(config.GetFloat("acc_stopping_thres"));
  }
  _acc_stopping_threshold.resize(_classes, _acc_stopping_threshold.back());

  _include_queuing = config.GetInt( "include_queuing" );

  _print_csv_results = config.GetInt( "print_csv_results" );
  _print_vc_stats = config.GetInt( "print_vc_stats" );
  _deadlock_warn_timeout = config.GetInt( "deadlock_warn_timeout" );
  _drain_measured_only = config.GetInt( "drain_measured_only" );

  string watch_file = config.GetStr( "watch_file" );
  _LoadWatchList(watch_file);

  vector<int> watch_flits = config.GetIntArray("watch_flits");
  for(size_t i = 0; i < watch_flits.size(); ++i) {
    _flits_to_watch.insert(watch_flits[i]);
  }
  
  vector<int> watch_packets = config.GetIntArray("watch_packets");
  for(size_t i = 0; i < watch_packets.size(); ++i) {
    _packets_to_watch.insert(watch_packets[i]);
  }

  vector<int> watch_transactions = config.GetIntArray("watch_transactions");
  for(size_t i = 0; i < watch_transactions.size(); ++i) {
    _transactions_to_watch.insert(watch_transactions[i]);
  }

  string stats_out_file = config.GetStr( "stats_out" );
  if(stats_out_file == "") {
    _stats_out = NULL;
  } else if(stats_out_file == "-") {
    _stats_out = &cout;
  } else {
    _stats_out = new ofstream(stats_out_file.c_str());
    config.WriteMatlabFile(_stats_out);
  }
  
  string flow_out_file = config.GetStr( "flow_out" );
  if(flow_out_file == "") {
    _flow_out = NULL;
  } else if(flow_out_file == "-") {
    _flow_out = &cout;
  } else {
    _flow_out = new ofstream(flow_out_file.c_str());
  }


}

TrafficManager::~TrafficManager( )
{

  for ( int subnet = 0; subnet < _subnets; ++subnet ) {
    for ( int source = 0; source < _nodes; ++source ) {
      delete _buf_states[source][subnet];
    }
  }
  
  for ( int c = 0; c < _classes; ++c ) {
    delete _plat_stats[c];
    delete _overall_min_plat[c];
    delete _overall_avg_plat[c];
    delete _overall_max_plat[c];

    delete _tlat_stats[c];
    delete _overall_min_tlat[c];
    delete _overall_avg_tlat[c];
    delete _overall_max_tlat[c];

    delete _frag_stats[c];
    delete _overall_min_frag[c];
    delete _overall_avg_frag[c];
    delete _overall_max_frag[c];
    
    delete _hop_stats[c];
    delete _overall_accepted[c];
    delete _overall_accepted_min[c];
    
    for ( int source = 0; source < _nodes; ++source ) {
      delete _sent_flits[c][source];
      
      for ( int dest = 0; dest < _nodes; ++dest ) {
	delete _pair_plat[c][source*_nodes+dest];
	delete _pair_tlat[c][source*_nodes+dest];
      }
    }
    
    for ( int dest = 0; dest < _nodes; ++dest ) {
      delete _accepted_flits[c][dest];
    }
    
  }
  
  delete _batch_time;
  delete _overall_batch_time;
  
  if(gWatchOut && (gWatchOut != &cout)) delete gWatchOut;
  if(_stats_out && (_stats_out != &cout)) delete _stats_out;
  if(_flow_out && (_flow_out != &cout)) delete _flow_out;

  PacketReplyInfo::FreeAll();
  Flit::FreeAll();
  Credit::FreeAll();
}

Flit* TrafficManager::IssueSpecial(int src, Flit* ff){
  Flit * f  = Flit::New();
  f->flid = ff->flid;
  f->sn = ff->sn;
  f->id = _cur_id++;
  f->subnetwork = 0;
  f->src = src;
  f->dest = ff->src;
  f->time = _time;
  f->cl = 0;
  f->type = Flit::ANY_TYPE;
  f->head = true;
  f->tail = true;
  f->vc = 0;
  f->flbid = ff->flbid;
  return f;
}

Flit* TrafficManager::DropPacket(int src, Flit* f){
  Flit* ff = IssueSpecial(f->src, f);
  ff->res_type = RES_TYPE_NACK;
  ff->pri = FLIT_PRI_NACK;
  ff->vc = 1;
  if(f->watch){
    ff->watch=true;
  }
  return ff;
  // _response_packets[src].push_back(ff);
}



void TrafficManager::_RetireFlit( Flit *f, int dest )
{

  Flit* ff;
  FlowROB* receive_rob = NULL;
  FlowBuffer* receive_flow_buffer  = NULL;

  switch(f->res_type){
  case RES_TYPE_ACK:
    gStatAckLatency->AddSample(_time-f->time);
    gStatAckReceived[dest]++;
    receive_flow_buffer = _flow_buffer[dest][f->flbid];
    if( receive_flow_buffer!=NULL &&
	receive_flow_buffer->fl->flid == f->flid){
      gStatAckEffective[dest]+= receive_flow_buffer->ack(f->sn)?1:0;
      if(receive_flow_buffer->done()){
	for(size_t i = 0; i<gStatFlowStats.size()-1; i++){
	  gStatFlowStats[i]+=receive_flow_buffer->_stats[i];
	}
	gStatFlowStats[gStatFlowStats.size()-1]++;

	gStatFlowSenderLatency->AddSample(_time-receive_flow_buffer->fl->create_time);
	delete _flow_buffer[dest][f->flbid];
	_flow_buffer[dest][f->flbid]=NULL;
      }
    }
    f->Free();
    return;
    break;
  case RES_TYPE_NACK:
    gStatNackLatency->AddSample(_time-f->time);
    gStatNackByPacket->AddSample(f->sn);
    gStatNackReceived[dest]++;
    receive_flow_buffer = _flow_buffer[dest][f->flbid];
    if( receive_flow_buffer!=NULL &&
	receive_flow_buffer->fl->flid == f->flid){
      gStatNackEffective[dest]+= receive_flow_buffer->nack(f->sn)?1:0;
    }
    f->Free();
    return;
    break;
  case RES_TYPE_GRANT: 
    gStatGrantLatency->AddSample(_time-f->time);
    gStatGrantReceived[dest]++;
    receive_flow_buffer = _flow_buffer[dest][f->flbid];
    if( receive_flow_buffer!=NULL &&
	receive_flow_buffer->fl->flid == f->flid){
      if(f->payload>_time){
	gStatGrantTimeFuture[dest]->AddSample(f->payload-_time);
      } else {
	gStatGrantTimeNow[dest]->AddSample(_time-f->payload);
      }
      receive_flow_buffer->grant(f->payload);
    }
    f->Free();
    return;
    break;
  case RES_TYPE_RES:
    assert(f->vc==0);
    gStatResLatency->AddSample(_time-f->time);
    gStatReservationReceived[dest]++;
    //find or create a reorder buffer
    if(_rob[dest].count(f->flid)==0){
      receive_rob = new FlowROB();
      _rob[dest].insert(pair<int, FlowROB*>(f->flid, receive_rob));
    } else {
      receive_rob = _rob[dest][f->flid];
    }
    
    f = receive_rob->insert(f);
    
    ff = IssueSpecial(dest,f);
    if(f->flid == WATCH_FLID){
      cout<<"Reservation received"<<endl;
      ff->watch = true;
    }
    if(_reservation_schedule[dest]>_time){
      gStatReservationTimeFuture[dest]->AddSample(_reservation_schedule[dest]-_time);
    } else {
      gStatReservationTimeNow[dest]->AddSample(_time-_reservation_schedule[dest]);
    }
    ff->payload  = MAX(_time, _reservation_schedule[dest]);
    _reservation_schedule[dest] = ff->payload+f->payload;
    ff->res_type = RES_TYPE_GRANT;
    ff->pri = FLIT_PRI_GRANT;
    ff->vc = 1;
    _response_packets[dest].push_back(ff);
    break;

  case RES_TYPE_SPEC:
    if(f->tail){
      gStatSpecLatency->AddSample(_time-f->time);
    }
    gStatSpecReceived[dest]++;
    if(_rob[dest].count(f->flid)==0){
      if(f->flid == WATCH_FLID){
	cout<<"spec destination nack sn "<<f->sn<<"\n";
      }
      //NACK
      if(f->head){
	gStatNackSent[dest]++;
	ff = IssueSpecial(dest,f);
	ff->res_type = RES_TYPE_NACK;
	ff->pri = FLIT_PRI_NACK;
	ff->vc =1;
	_response_packets[dest].push_back(ff);
      } 
      f->Free();
      f = NULL;
    } else {
     if(f->flid == WATCH_FLID){
	cout<<"spec insert sn "<<f->sn<<"\n";
      }
      receive_rob = _rob[dest][f->flid];
      f = receive_rob->insert(f);
      if(f==NULL){
	gStatSpecDuplicate[dest]++;
      }
      //ACK
      if(f!=NULL && f->tail){
	gStatAckSent[dest]++;
	ff = IssueSpecial(dest,f);
	ff->sn = f->head_sn;
	ff->res_type = RES_TYPE_ACK;
	ff->pri  = FLIT_PRI_ACK;
	ff->vc = 1;
	_response_packets[dest].push_back(ff);
      }
    }
    break;
  case RES_TYPE_NORM:
    if(f->tail){
      gStatNormLatency->AddSample(_time-f->time);
    }
    gStatNormReceived[dest]++;
    if(gReservation){
      //find or create a reorder buffer
      if(_rob[dest].count(f->flid)==0){
	receive_rob = new FlowROB();
	_rob[dest].insert(pair<int, FlowROB*>(f->flid, receive_rob));
      } else {
	receive_rob = _rob[dest][f->flid];
      }
      if(f->flid == WATCH_FLID){
	cout<<"normal insert sn "<<f->sn<<"\n";
      }
      f = receive_rob->insert(f);
      if(f==NULL){
	gStatNormDuplicate[dest]++;
      }
    }    
    break;
  default:
    assert(false);
  }
  
  //duplicate packet would not pass this
  if(gReservation){
    if(f){
      assert(receive_rob!=NULL);
      gStatROBRange->AddSample(receive_rob->range());
      if(receive_rob->done()){ //entire flow has been received
	gStatFlowLatency->AddSample(_time-receive_rob->_flow_creation_time);
	delete _rob[dest][f->flid];
	_rob[dest].erase(f->flid); 
      }
      //rest of the code can't process this
      if(f->res_type == RES_TYPE_RES){
	f->Free();
	return;
      }
    } else {
      return;
    }
  }

 
    
  if(f->watch){
    *gWatchOut << GetSimTime() << " | "
	       << "node" << dest << " | "
	       << "retire flit " << f->id
	       << "res type  "<<f->res_type
	       << "." << endl;
  }
  

  //this occurs, when the normal flit retires before the speculative
  if(_total_in_flight_flits[f->cl].count(f->id) == 0){
    gStatLostPacket[dest]++;
    return;
  }

  //Regular retire flit
  _deadlock_timer = 0;

  assert(_total_in_flight_flits[f->cl].count(f->id) > 0);
  _total_in_flight_flits[f->cl].erase(f->id);
  
  if(f->record) {
    assert(_measured_in_flight_flits[f->cl].count(f->id) > 0);
    _measured_in_flight_flits[f->cl].erase(f->id);
  }

  if ( f->watch ) { 
    *gWatchOut << GetSimTime() << " | "
	       << "node" << dest << " | "
	       << "Retiring flit " << f->id 
	       << " (packet " << f->pid
	       << ", src = " << f->src 
	       << ", dest = " << f->dest
	       << ", hops = " << f->hops
	       << ", lat = " << f->atime - f->time
	       << ")." << endl;
  }

  _last_id = f->id;
  _last_pid = f->pid;

  if ( f->head && ( f->dest != dest ) ) {
    ostringstream err;
    err << "Flit " << f->id << " arrived at incorrect output " << dest;
    Error( err.str( ) );
  }

  if ( f->tail ) {
    Flit * head;
    if(f->head) {
      head = f;
    } else {
      //TODO single packets fuck up if a second packet shows up really really late
      //then it will try to be retired again??
      map<int, Flit *>::iterator iter = _retired_packets[f->cl].find(f->pid);
      assert(iter != _retired_packets[f->cl].end());
      head = iter->second;
      _retired_packets[f->cl].erase(iter);
      assert(head->head);
      assert(f->pid == head->pid);
    }
    if ( f->watch ) { 
      *gWatchOut << GetSimTime() << " | "
		 << "node" << dest << " | "
		 << "Retiring packet " << f->pid 
		 << " (lat = " << f->atime - head->time
		 << ", frag = " << (f->atime - head->atime) - (f->id - head->id) // NB: In the spirit of solving problems using ugly hacks, we compute the packet length by taking advantage of the fact that the IDs of flits within a packet are contiguous.
		 << ", src = " << head->src 
		 << ", dest = " << head->dest
		 << ")." << endl;
    }

    //code the source of request, look carefully, its tricky ;)
    if (f->type == Flit::READ_REQUEST || f->type == Flit::WRITE_REQUEST) {
      PacketReplyInfo* rinfo = PacketReplyInfo::New();
      rinfo->source = f->src;
      rinfo->time = f->atime;
      rinfo->ttime = f->ttime;
      rinfo->record = f->record;
      rinfo->type = f->type;
      _repliesDetails[f->id] = rinfo;
      _repliesPending[dest].push_back(f->id);
    } else {
      if ( f->watch ) { 
	*gWatchOut << GetSimTime() << " | "
		   << "node" << dest << " | "
		   << "Completing transation " << f->tid
		   << " (lat = " << f->atime - head->ttime
		   << ", src = " << head->src 
		   << ", dest = " << head->dest
		   << ")." << endl;
      }
      if(f->type == Flit::READ_REPLY || f->type == Flit::WRITE_REPLY  ){
	//received a reply
	_requestsOutstanding[dest]--;
      } else if(f->type == Flit::ANY_TYPE && _sim_mode == batch) {
	//received a reply
	_requestsOutstanding[f->src]--;
      }
      
    }

    // Only record statistics once per packet (at tail)
    // and based on the simulation state
    if ( ( _sim_state == warming_up ) || f->record ) {
      
      _hop_stats[f->cl]->AddSample( f->hops );

      if((_slowest_flit[f->cl] < 0) ||
	 (_plat_stats[f->cl]->Max() < (f->atime - f->time)))
	_slowest_flit[f->cl] = f->id;
      _plat_stats[f->cl]->AddSample( f->atime - f->time);
      gStatPureNetworkLatency->AddSample( f->atime - f->ntime);
      _frag_stats[f->cl]->AddSample( (f->atime - head->atime) - (f->id - head->id) );
      if(f->type == Flit::READ_REPLY || f->type == Flit::WRITE_REPLY || f->type == Flit::ANY_TYPE)
	_tlat_stats[f->cl]->AddSample( f->atime - f->ttime );
   
      _pair_plat[f->cl][f->src*_nodes+dest]->AddSample( f->atime - f->time );
      if(f->type == Flit::READ_REPLY || f->type == Flit::WRITE_REPLY)
	_pair_tlat[f->cl][dest*_nodes+f->src]->AddSample( f->atime - f->ttime );
      else if(f->type == Flit::ANY_TYPE)
	_pair_tlat[f->cl][f->src*_nodes+dest]->AddSample( f->atime - f->ttime );
      
    }
    
    if(f != head) {
      head->Free();
    }
    
  }
  
  if(f->head && !f->tail) {
    _retired_packets[f->cl].insert(make_pair(f->pid, f));
  } else {

    f->Free();  
  }
}

int TrafficManager::_IssuePacket( int source, int cl )
{
  int result;
  if(_sim_mode == batch){ //batch mode
    if(_use_read_write[cl]){ //read write packets
      //check queue for waiting replies.
      //check to make sure it is on time yet
      int pending_time = numeric_limits<int>::max(); //reset to maxtime+1
      if (!_repliesPending[source].empty()) {
	result = _repliesPending[source].front();
	pending_time = _repliesDetails.find(result)->second->time;
      }
      if (pending_time<=_qtime[source][cl]) {
	result = _repliesPending[source].front();
	_repliesPending[source].pop_front();
	
      } else if ((_packets_sent[source] >= _batch_size && !_timed_mode) || 
		 (_requestsOutstanding[source] >= _maxOutstanding)) {
	result = 0;
      } else {
	
	//coin toss to determine request type.
	result = (RandomFloat() < 0.5) ? -2 : -1;
	
	_packets_sent[source]++;
	_requestsOutstanding[source]++;
      } 
    } else { //normal
      if ((_packets_sent[source] >= _batch_size && !_timed_mode) || 
	  (_requestsOutstanding[source] >= _maxOutstanding)) {
	result = 0;
      } else {
	result = _packet_size[cl];
	_packets_sent[source]++;
	//here is means, how many flits can be waiting in the queue
	_requestsOutstanding[source]++;
      } 
    } 
  } else { //injection rate mode
    if(_use_read_write[cl]){ //use read and write
      //check queue for waiting replies.
      //check to make sure it is on time yet
      int pending_time = numeric_limits<int>::max(); //reset to maxtime+1
      if (!_repliesPending[source].empty()) {
	result = _repliesPending[source].front();
	pending_time = _repliesDetails.find(result)->second->time;
      }
      if (pending_time<=_qtime[source][cl]) {
	result = _repliesPending[source].front();
	_repliesPending[source].pop_front();
      } else {

	//produce a packet
	if(_injection_process[cl]( source, _load[cl] )){
	
	  //coin toss to determine request type.
	  result = (RandomFloat() < 0.5) ? -2 : -1;

	} else {
	  result = 0;
	}
      } 
    } else { //normal mode
      return _injection_process[cl]( source, _load[cl] ) ? 1 : 0;
    } 
  }
  return result;
}

void TrafficManager::_GeneratePacket( int source, int stype, 
				      int cl, int time )
{
  assert(stype!=0);

  //refusing to generate packets for nodes greater than limit
  if(source >=_limit){
    return ;
  }
  int packet_destination = _traffic_function[cl](source, _limit);

  
  flow* fl = new flow;
  fl->flid = _cur_flid++;
  //  _flow[source].insert(pair<int, flow*>(fl->flid, fl));
  //_flow_master.insert(pair<int, flow*>(fl->flid, fl));

  int sequence_number =0;
  for(int f_index = 0; f_index<_flow_size; f_index++){
    Flit::FlitType packet_type = Flit::ANY_TYPE;
    int size = _packet_size[cl]; //input size 
    int ttime = time;
    int pid = _cur_pid++;
    assert(_cur_pid);
    int tid = _cur_tid;
    bool record = false;
    bool watch = gWatchOut && ((_packets_to_watch.count(pid) > 0) ||
			       (_transactions_to_watch.count(tid) > 0));
    if(_use_read_write[cl]){
      if(stype < 0) {
	if (stype ==-1) {
	  packet_type = Flit::READ_REQUEST;
	  size = _read_request_size[cl];
	} else if (stype == -2) {
	  packet_type = Flit::WRITE_REQUEST;
	  size = _write_request_size[cl];
	} else {
	  ostringstream err;
	  err << "Invalid packet type: " << packet_type;
	  Error( err.str( ) );
	}
	if ( watch ) { 
	  *gWatchOut << GetSimTime() << " | "
		     << "node" << source << " | "
		     << "Beginning transaction " << tid
		     << " at time " << time
		     << "." << endl;
	}
	++_cur_tid;
	assert(_cur_tid);
      } else  {
	map<int, PacketReplyInfo*>::iterator iter = _repliesDetails.find(stype);
	PacketReplyInfo* rinfo = iter->second;
      
	if (rinfo->type == Flit::READ_REQUEST) {//read reply
	  size = _read_reply_size[cl];
	  packet_type = Flit::READ_REPLY;
	} else if(rinfo->type == Flit::WRITE_REQUEST) {  //write reply
	  size = _write_reply_size[cl];
	  packet_type = Flit::WRITE_REPLY;
	} else {
	  ostringstream err;
	  err << "Invalid packet type: " << rinfo->type;
	  Error( err.str( ) );
	}
	packet_destination = rinfo->source;
	tid = rinfo->tid;
	time = rinfo->time;
	ttime = rinfo->ttime;
	record = rinfo->record;
	_repliesDetails.erase(iter);
	rinfo->Free();
      }
    } else {
      ++_cur_tid;
      assert(_cur_tid);
    }

    if ((packet_destination <0) || (packet_destination >= _nodes)) {
      ostringstream err;
      err << "Incorrect packet destination " << packet_destination
	  << " for stype " << packet_type;
      Error( err.str( ) );
    }

    if ( ( _sim_state == running ) ||
	 ( ( _sim_state == draining ) && ( time < _drain_time ) ) ) {
      record = _measure_stats[cl];
    }

    int subnetwork = ((packet_type == Flit::ANY_TYPE) ? 
		      RandomInt(_subnets-1) :
		      _subnet[packet_type]);
  
    if ( watch ) { 
      *gWatchOut << GetSimTime() << " | "
		 << "node" << source << " | "
		 << "Enqueuing packet " << pid
		 << " at time " << time
		 << "." << endl;
    }
  

    for ( int i = 0; i < size; ++i ) {
      Flit * f  = Flit::New();
      f ->flid = fl->flid;
      f->id     = _cur_id++;
      assert(_cur_id);
      f->pid    = pid;
      f->tid    = tid;
      f->watch  = watch | (gWatchOut && (_flits_to_watch.count(f->id) > 0));
      f->subnetwork = subnetwork;
      f->src    = source;
      f->time   = time;
      f->ttime  = ttime;
      f->record = record;
      f->cl     = cl;
      f->sn = sequence_number++;
      //watchwatch
      if(f->id == -1){
	f->watch=true;;
      }
      _total_in_flight_flits[f->cl].insert(make_pair(f->id, f));
      if(record) {
	_measured_in_flight_flits[f->cl].insert(make_pair(f->id, f));
      }
    
      if(gTrace){
	cout<<"New Flit "<<f->src<<endl;
      }
      f->type = packet_type;

      if ( i == 0 ) { // Head flit
	f->head = true;
	//packets are only generated to nodes smaller or equal to limit
	f->dest = packet_destination;
      } else {
	f->head = false;
	f->dest = -1;
      }
      switch( _pri_type ) {
      case class_based:
	f->pri = _class_priority[cl];
	assert(f->pri >= 0);
	break;
      case age_based:
	f->pri = numeric_limits<int>::max() - (_replies_inherit_priority ? ttime : time);
	assert(f->pri >= 0);
	break;
      case sequence_based:
	f->pri = numeric_limits<int>::max() - _packets_sent[source];
	assert(f->pri >= 0);
	break;
      default:
	f->pri = 0;
      }
      if ( i == ( size - 1 ) ) { // Tail flit
	f->head_sn = f->sn-size+1;
	f->tail = true;
	f->dest =  packet_destination;
      } else {
	f->tail = false;
      }
    
      f->vc  = -1;

      if ( f->watch ) { 
	*gWatchOut << GetSimTime() << " | "
		   << "node" << source << " | "
		   << "Enqueuing flit " << f->id
		   << " flow "<<f->flid
		   << " (packet " << f->pid
		   << ") at time " << time
		   << "." << endl;
      }

      fl->data.push( f );
    }
  }

  fl->rtime = -1;
  fl->vc = -1;
  fl->create_time = time;
  fl->collect = false;
  fl->flow_size = sequence_number;
  fl->data.front()->payload = fl->flow_size;
  fl->data.back()->flow_tail = true;
  int empty_flow = 0;
  for(empty_flow= 0; empty_flow<_max_flow_buffers; empty_flow++){
    if(_flow_buffer[source][empty_flow] == NULL){
      break;
    }
  }
  assert(empty_flow!=_max_flow_buffers);
  _flow_buffer[source][empty_flow] = new FlowBuffer(empty_flow, _flow_buffer_capacity, gReservation, fl);
  if(fl->flid == WATCH_FLID){
    _flow_buffer[source][empty_flow]->_watch = true;
  }
}

void TrafficManager::_Inject(){

  for ( int input = 0; input < _nodes; ++input ) {
    for ( int c = 0; c < _classes; ++c ) {
      int flow_buffer_taken = 0;
      bool any_flow_ready=false;
      for(int i = 0; i<_max_flow_buffers; i++){
	if(_flow_buffer[input][i] != NULL){
	  flow_buffer_taken++;
	  any_flow_ready = any_flow_ready || (_flow_buffer[input][i] ->receive_ready());
	}
      }
      gStatActiveFlowBuffers->AddSample(flow_buffer_taken);
      //if no currently active flow is ready to receive and there is flow buffer available
      if (!any_flow_ready && flow_buffer_taken<_max_flow_buffers) {
	bool generated = false;

	if ( !_empty_network ) {
	  while( !generated && ( _qtime[input][c] <= _time ) ) {
	    int stype = _IssuePacket( input, c );

	    if ( stype != 0 ) { //generate a packet
		_GeneratePacket( input, stype, c, 
				 _include_queuing==1 ? 
				 _qtime[input][c] : _time );
	      
	   
	      generated = true;
	    }
	    //this is not a request packet
	    //don't advance time
	    if(!_use_read_write[c] || (stype <= 0)){
	      ++_qtime[input][c];
	    }
	  }
	  
	  if ( ( _sim_state == draining ) && 
	       ( _qtime[input][c] > _drain_time ) ) {
	    _qdrained[input][c] = true;
	  }
	}
      }
    }
  }
}

void TrafficManager::_Step( )
{

  for ( int input = 0; input < _nodes; ++input ) {
    for(int i = 0; i<_max_flow_buffers; i++){
      if(_flow_buffer[input][i] != NULL){
	_flow_buffer[input][i]->update_stats();
      }
    }
  }

  bool flits_in_flight = false;
  for(int c = 0; c < _classes; ++c) {
    flits_in_flight |= !_total_in_flight_flits[c].empty();
  }
  if(flits_in_flight && (_deadlock_timer++ >= _deadlock_warn_timeout)){
    _deadlock_timer = 0;
    cout << "WARNING: Possible network deadlock.\n";
  }

  for ( int source = 0; source < _nodes; ++source ) {
    for ( int subnet = 0; subnet < _subnets; ++subnet ) {
      Credit * const c = _net[subnet]->ReadCredit( source );
      if ( c ) {
	_buf_states[source][subnet]->ProcessCredit(c);
	c->Free();
      }
    }
  }

  vector<map<int, Flit *> > flits(_subnets);
  
  for ( int subnet = 0; subnet < _subnets; ++subnet ) {
    for ( int dest = 0; dest < _nodes; ++dest ) {
      Flit * const f = _net[subnet]->ReadFlit( dest );
      if ( f ) {
	if(f->watch) {
	  *gWatchOut << GetSimTime() << " | "
		     << "node" << dest << " | "
		     << "Ejecting flit " << f->id
		     << " (packet " << f->pid << ")"
		     << " from VC " << f->vc
		     << "." << endl;
	}
	flits[subnet].insert(make_pair(dest, f));
      }
      if( ( _sim_state == warming_up ) || ( _sim_state == running ) ) {
	for(int c = 0; c < _classes; ++c) {
	  _accepted_flits[c][dest]->AddSample( (f && (f->cl == c)) ? 1 : 0 );
	}
      }
    }
    _net[subnet]->ReadInputs( );
  }

  _Inject();
  vector<int> injected_flits(_subnets*_nodes);


  for(int source = 0; source < _nodes; ++source) {
    

    //from node to flow buffer, 1 flit per cycle
    for(map<int, pair<int, vector<int> > >::reverse_iterator iter = _class_prio_map.rbegin();
	iter != _class_prio_map.rend();
	++iter) {
      int const & base = iter->second.first;
      vector<int> const & classes = iter->second.second;
      int const count = classes.size();
      for(int j = 1; j <= count; ++j) {
	int const offset = (base + j) % count;
	
	//find a flow buffer thats ready to go
	FlowBuffer* ready_flow_buffer=NULL;
	for(int i = 0; i<_max_flow_buffers; i++){
	  int flb_index = (_last_receive_flow_buffer[source]+i)%_max_flow_buffers;
	  if(_flow_buffer[source][flb_index]!=NULL &&
	     _flow_buffer[source][flb_index]->receive_ready()){
	    ready_flow_buffer = _flow_buffer[source][flb_index];

	    //tranfer from flow to flow buffer is happening internally
	    ready_flow_buffer->receive();
	    ++injected_flits[0*_nodes+source]; 
	    iter->second.first = offset;	  
	    _last_receive_flow_buffer[source] = flb_index;
	    break;
	  }
	}
      }
    }
  }
  
  
  //from flow buffer to source router
  for(int source = 0; source < _nodes; ++source) {
    BufferState * const dest_buf = _buf_states[source][0];
    FlowBuffer* ready_flow_buffer = NULL;

    //ack nack grant
    if(gReservation){
      gStatResponseBuffer->AddSample((int)_response_packets[source].size());
      if( !_response_packets[source].empty() ){
	Flit* f = _response_packets[source].front();
	assert(f);
	assert(f->head);//only heads
	if(dest_buf->IsAvailableFor(1) &&
	   dest_buf->HasCreditFor(1)){
	  assert(f->vc ==1);
	  dest_buf->TakeBuffer(f->vc); 
	  dest_buf->SendingFlit(f);
	 
	  _net[0]->WriteSpecialFlit(f, source);
	  _response_packets[source].pop_front();
	}
      }
    }

    int flb_index = _last_send_flow_buffer[source];
    //first check the last flow buffer served
    if(_flow_buffer[source][flb_index]!=NULL && 
       _flow_buffer[source][flb_index]->_vc!=-1 && //this line trip for startup flows
       (_flow_buffer[source][flb_index]->send_norm_ready() ||
	_flow_buffer[source][flb_index]->send_spec_ready())){

      ready_flow_buffer = _flow_buffer[source][flb_index];
  
    } else {

      //then check the normal buffer
      //then check the speculative buffers
    
      //picking a flow buffer and VC assignment
      for(int i = 0; i<_max_flow_buffers; i++){
	flb_index = (_last_normal_flow_buffer[source]+i)%_max_flow_buffers;
	if(_flow_buffer[source][flb_index]!=NULL &&
	   _flow_buffer[source][flb_index]->send_norm_ready()){

	  assert(! _flow_buffer[source][flb_index]->send_spec_ready());
	  ready_flow_buffer = _flow_buffer[source][flb_index];
	  if(ready_flow_buffer->_vc==-1){
	    Flit* f = ready_flow_buffer->front();
	    assert(f);
	    assert(f->head);
	    assert(f->sn==0);
	    OutputSet route_set;
	    _rf(NULL, f, 0, &route_set, true);
	    set<OutputSet::sSetElement> const & os = route_set.GetSet();
	    assert(os.size() == 1);
	    OutputSet::sSetElement const & se = *os.begin();
	    assert(se.output_port == 0);
	    int const & vc_start = se.vc_start;
	    int const & vc_end = se.vc_end;
	    int const vc_count = vc_end - vc_start + 1;
	    for(int i = 1; i <= vc_count; ++i) {
	      int const vc = vc_start + (_last_normal_vc[source] + i) % vc_count;
	      if(dest_buf->IsAvailableFor(vc) && !dest_buf->IsFullFor(vc)) {
		ready_flow_buffer->_vc = vc; 
		_last_normal_vc[source] = vc- vc_start;
		break;
	      }		 
	    }
	  }
	  if(ready_flow_buffer->_vc!=-1){
	    _last_normal_flow_buffer[source]= flb_index;
	    break;
	  }  else {
	    ready_flow_buffer= NULL;
	  }
	}
      }

      //reservation case has a second chance on speculative packets
      if(gReservation && ready_flow_buffer == NULL){
	for(int i = 0; i<_max_flow_buffers; i++){
	  flb_index = (_last_spec_flow_buffer[source]+i)%_max_flow_buffers;
	  if(_flow_buffer[source][flb_index]!=NULL &&
	     _flow_buffer[source][flb_index]->send_spec_ready()){
	    assert(! _flow_buffer[source][flb_index]->send_norm_ready());
	    ready_flow_buffer = _flow_buffer[source][flb_index];
	    if(ready_flow_buffer->_vc==-1){
	      Flit* f = ready_flow_buffer->front();
	      assert(f);
	      assert(f->head);
	      assert(f->sn==0);
	      OutputSet route_set;
	      _rf(NULL, f, 0, &route_set, true);
	      set<OutputSet::sSetElement> const & os = route_set.GetSet();
	      assert(os.size() == 1);
	      OutputSet::sSetElement const & se = *os.begin();
	      assert(se.output_port == 0);
	      int const & vc_start = se.vc_start;
	      int const & vc_end = se.vc_end;
	      int const vc_count = vc_end - vc_start + 1;
	      for(int i = 1; i <= vc_count; ++i) {
		int const vc = vc_start + (_last_spec_vc[source] + i) % vc_count;
		if(dest_buf->IsAvailableFor(vc) && !dest_buf->IsFullFor(vc)) {
		  ready_flow_buffer->_vc = vc; 
		  _last_spec_vc[source] = vc- vc_start;
		  break;
		}		 
	      }
	    }
	    //is this buffer assigned to a VC
	    if(ready_flow_buffer->_vc!=-1){
	      _last_spec_flow_buffer[source]= flb_index;
	      break;
	    } else {
	      ready_flow_buffer= NULL;
	    }
	  }
	}
      }
    }


    if(ready_flow_buffer){
      _last_send_flow_buffer[source] = flb_index;
      int vc = ready_flow_buffer->_vc;
      Flit* f = ready_flow_buffer->front();
      assert(f);
      //vc bookkeeping
      if(f->res_type== RES_TYPE_RES){
	if(dest_buf->IsAvailableFor(0) &&
	   dest_buf->HasCreditFor(0)){
	  dest_buf->TakeBuffer(0); 
	  dest_buf->SendingFlit(f);
	} else {
	  f = NULL;
	}
      } else {
	if(f->head){
	  if(dest_buf->IsAvailableFor(vc) &&
	     dest_buf->HasCreditFor(vc)){
	    dest_buf->TakeBuffer(vc); 
	    f->vc = vc;
	    dest_buf->SendingFlit(f);
	  } else {
	    f = NULL;
	  }
	} else { //body 
	  if( dest_buf->HasCreditFor(vc)){
	    f->vc = vc;
	    dest_buf->SendingFlit(f);
	  } else {
	    f = NULL;
	  }
	}
      
      }    
      //VC bookkeeping completed success
      if(f){
	//actual the actual packet to send
	//this might be slightly different from front
	f =  ready_flow_buffer->send();
	f->exptime = _time+gExpirationTime;
	if(f->res_type==RES_TYPE_RES){
	  gStatSourceTrueLatency->AddSample(_time-ready_flow_buffer->fl->create_time);
	}
	if(f->sn==0 && f->res_type!= RES_TYPE_RES){
	  gStatSourceLatency->AddSample(_time-ready_flow_buffer->fl->create_time);
	}
	if(f->head && f->res_type== RES_TYPE_NORM){
	  gStatNormSent[source]++;
	}
	

	
	//flit network traffic time
	f->ntime = _time;
	_net[0]->WriteSpecialFlit(f, source);
	_sent_flits[0][source]->AddSample(1);
	
	if(ready_flow_buffer->done()){
	  for(size_t i = 0; i<gStatFlowStats.size()-1; i++){
	    gStatFlowStats[i]+=ready_flow_buffer->_stats[i];
	  }
	  gStatFlowStats[gStatFlowStats.size()-1]++;
	  gStatFlowSenderLatency->AddSample(_time-ready_flow_buffer->fl->create_time);
	  delete _flow_buffer[source][f->flbid];
	  _flow_buffer[source][f->flbid]=NULL;
	}
      }
    }
  }

  

  vector<int> ejected_flits(_subnets*_nodes);
  for(int subnet = 0; subnet < _subnets; ++subnet) {
    for(int dest = 0; dest < _nodes; ++dest) {
      map<int, Flit *>::const_iterator iter = flits[subnet].find(dest);
      if(iter != flits[subnet].end()) {
	Flit * const & f = iter->second;
	if(_flow_out) ++ejected_flits[subnet*_nodes+dest];
	f->atime = _time;
	if(f->watch) {
	  *gWatchOut << GetSimTime() << " | "
		     << "node" << dest << " | "
		     << "Injecting credit for VC " << f->vc 
		     << " into subnet " << subnet 
		     << "." << endl;
	}
	Credit * const c = Credit::New();
	c->vc.push_back(f->vc);
	_net[subnet]->WriteCredit(c, dest);
	_RetireFlit(f, dest);
      }
    }
    flits[subnet].clear();
    _net[subnet]->Evaluate( );
    _net[subnet]->WriteOutputs( );
  }
  
  if(_flow_out) {

    vector<vector<int> > received_flits(_subnets*_routers);
    vector<vector<int> > sent_flits(_subnets*_routers);
    vector<vector<int> > stored_flits(_subnets*_routers);
    vector<vector<int> > active_packets(_subnets*_routers);

    for (int subnet = 0; subnet < _subnets; ++subnet) {
      for(int router = 0; router < _routers; ++router) {
	Router * r = _router[subnet][router];
	received_flits[subnet*_routers+router] = r->GetReceivedFlits();
	sent_flits[subnet*_routers+router] = r->GetSentFlits();
	stored_flits[subnet*_routers+router] = r->GetStoredFlits();
	active_packets[subnet*_routers+router] = r->GetActivePackets();
	r->ResetStats();
      }
    }
    *_flow_out << "injected_flits(" << _time + 1 << ",:) = " << injected_flits << ";" << endl;
    *_flow_out << "received_flits(" << _time + 1 << ",:) = " << received_flits << ";" << endl;
    *_flow_out << "stored_flits(" << _time + 1 << ",:) = " << stored_flits << ";" << endl;
    *_flow_out << "sent_flits(" << _time + 1 << ",:) = " << sent_flits << ";" << endl;;
    *_flow_out << "ejected_flits(" << _time + 1 << ",:) = " << ejected_flits << ";" << endl;
    *_flow_out << "active_packets(" << _time + 1 << ",:) = " << active_packets << ";" << endl;
  }

  ++_time;
  //  cout<<"TIME "<<_time<<endl;
  assert(_time);
  if(gTrace){
    cout<<"TIME "<<_time<<endl;
  }

}
  
bool TrafficManager::_PacketsOutstanding( ) const
{
  bool outstanding = false;

  for ( int c = 0; c < _classes; ++c ) {
    
    if ( _measured_in_flight_flits[c].empty() ) {

      for ( int s = 0; s < _nodes; ++s ) {
	if ( _measure_stats[c] && !_qdrained[s][c] ) {
#ifdef DEBUG_DRAIN
	  cout << "waiting on queue " << s << " class " << c;
	  cout << ", time = " << _time << " qtime = " << _qtime[s][c] << endl;
#endif
	  outstanding = true;
	  break;
	}
      }
    } else {
#ifdef DEBUG_DRAIN
      cout << "in flight = " << _measured_in_flight_flits[c].size() << endl;
#endif
      outstanding = true;
    }

    if ( outstanding ) { break; }
  }

  return outstanding;
}

void TrafficManager::_ClearStats( )
{
  _slowest_flit.assign(_classes, -1);

  for ( int c = 0; c < _classes; ++c ) {

    _plat_stats[c]->Clear( );
    _tlat_stats[c]->Clear( );
    _frag_stats[c]->Clear( );
  
    for ( int i = 0; i < _nodes; ++i ) {
      _sent_flits[c][i]->Clear( );
      
      for ( int j = 0; j < _nodes; ++j ) {
	_pair_plat[c][i*_nodes+j]->Clear( );
	_pair_tlat[c][i*_nodes+j]->Clear( );
      }
    }

    for ( int i = 0; i < _nodes; ++i ) {
      _accepted_flits[c][i]->Clear( );
    }
  
    _hop_stats[c]->Clear();

  }
  
  for(int i = 0; i<_routers; i++){
    for(unsigned int j = 0; j<    gDropStats[i].size(); j++){
      gDropStats[i][j] = 0;
    }
  }

  gStatAckReceived.clear();
  gStatAckReceived.resize(_nodes,0);
  gStatAckEffective.clear();
  gStatAckEffective.resize(_nodes,0);
  gStatAckSent.clear();
  gStatAckSent.resize(_nodes,0);

  gStatNackReceived.clear();
  gStatNackReceived.resize(_nodes,0);
  gStatNackEffective.clear();
  gStatNackEffective.resize(_nodes,0);
  gStatNackSent.clear();
  gStatNackSent.resize(_nodes,0);

  gStatGrantReceived.clear();
  gStatGrantReceived.resize(_nodes,0);

  gStatReservationReceived.clear();
  gStatReservationReceived.resize(_nodes,0);

  for(int i = 0; i< _nodes; i++){
    gStatGrantTimeNow[i]->Clear();
    gStatGrantTimeFuture[i]->Clear();
    gStatReservationTimeNow[i]->Clear();
    gStatReservationTimeFuture[i]->Clear();
  }


  gStatSpecReceived.clear();
  gStatSpecReceived.resize(_nodes,0);
  gStatSpecDuplicate.clear();
  gStatSpecDuplicate.resize(_nodes,0);

  gStatNormSent.clear();
  gStatNormSent.resize(_nodes,0);
  gStatNormReceived.clear();
  gStatNormReceived.resize(_nodes,0);
  gStatNormDuplicate.clear();
  gStatNormDuplicate.resize(_nodes,0);

  gStatLostPacket.clear();
  gStatLostPacket.resize(_nodes,0);

  gStatROBRange->Clear();
  
  gStatFlowSenderLatency->Clear();
  gStatFlowLatency->Clear();
  gStatActiveFlowBuffers->Clear();
  gStatResponseBuffer->Clear();

  gStatPureNetworkLatency->Clear();
  gStatAckLatency->Clear();
  gStatNackLatency->Clear();
  gStatResLatency->Clear();
  gStatGrantLatency->Clear();
  gStatSpecLatency->Clear();
  gStatNormLatency->Clear();

  gStatSourceTrueLatency->Clear();
  gStatSourceLatency->Clear();

  gStatNackByPacket->Clear();
  
  gStatFlowStats.clear();
  gStatFlowStats.resize(FLOW_STAT_LIFETIME+1+1,0);
}

int TrafficManager::_ComputeStats( const vector<Stats *> & stats, double *avg, double *min ) const 
{
  int dmin = -1;

  *min = numeric_limits<double>::max();
  *avg = 0.0;

  for ( int d = 0; d < _nodes; ++d ) {
    double curr = stats[d]->Average( );
    if ( curr < *min ) {
      *min = curr;
      dmin = d;
    }
    *avg += curr;
  }

  *avg /= (double)_nodes;

  return dmin;
}

void TrafficManager::_DisplayRemaining( ostream & os ) const 
{
  for(int c = 0; c < _classes; ++c) {

    map<int, Flit *>::const_iterator iter;
    int i;

    os << "Class " << c << ":" << endl;

    os << "Remaining flits: ";
    for ( iter = _total_in_flight_flits[c].begin( ), i = 0;
	  ( iter != _total_in_flight_flits[c].end( ) ) && ( i < 10 );
	  iter++, i++ ) {
      os << iter->first << " ";
    }
    if(_total_in_flight_flits[c].size() > 10)
      os << "[...] ";
    
    os << "(" << _total_in_flight_flits[c].size() << " flits)" << endl;
    
    os << "Measured flits: ";
    for ( iter = _measured_in_flight_flits[c].begin( ), i = 0;
	  ( iter != _measured_in_flight_flits[c].end( ) ) && ( i < 10 );
	  iter++, i++ ) {
      os << iter->first << " ";
    }
    if(_measured_in_flight_flits[c].size() > 10)
      os << "[...] ";
    
    os << "(" << _measured_in_flight_flits[c].size() << " flits)" << endl;
    
  }
}

bool TrafficManager::_SingleSim( )
{
  _time = 0;

  //remove any pending request from the previous simulations
  _requestsOutstanding.assign(_nodes, 0);
  for (int i=0;i<_nodes;i++) {
    _repliesPending[i].clear();
  }

  //reset queuetime for all sources
  for ( int s = 0; s < _nodes; ++s ) {
    _qtime[s].assign(_classes, 0);
    _qdrained[s].assign(_classes, false);
  }

  // warm-up ...
  // reset stats, all packets after warmup_time marked
  // converge
  // draing, wait until all packets finish
  _sim_state    = warming_up;
  
  _ClearStats( );

  bool clear_last = false;
  int total_phases  = 0;
  int converged = 0;

  if (_sim_mode == batch && _timed_mode){
    _sim_state = running;
    while(_time<_sample_period){
      _Step();
      if ( _time % 10000 == 0 ) {
	cout << _sim_state << endl;
	if(_stats_out)
	  *_stats_out << "%=================================" << endl;
	
	for(int c = 0; c < _classes; ++c) {

	  if(_measure_stats[c] == 0) {
	    continue;
	  }

	  double cur_latency = _plat_stats[c]->Average( );
	  double min, avg;
	  int dmin = _ComputeStats( _accepted_flits[c], &avg, &min );
	  
	  cout << "Class " << c << ":" << endl;

	  cout << "Minimum latency = " << _plat_stats[c]->Min( ) << endl;
	  cout << "Average latency = " << cur_latency << endl;
	  cout << "Maximum latency = " << _plat_stats[c]->Max( ) << endl;
	  cout << "Average fragmentation = " << _frag_stats[c]->Average( ) << endl;
	  cout << "Accepted packets = " << min << " at node " << dmin << " (avg = " << avg << ")" << endl;

	  cout << "Total in-flight flits = " << _total_in_flight_flits[c].size() << " (" << _measured_in_flight_flits[c].size() << " measured)" << endl;

	  //c+1 because of matlab arrays starts at 1
	  if(_stats_out)
	    *_stats_out << "lat(" << c+1 << ") = " << cur_latency << ";" << endl
			<< "lat_hist(" << c+1 << ",:) = " << *_plat_stats[c] << ";" << endl
			<< "frag_hist(" << c+1 << ",:) = " << *_frag_stats[c] << ";" << endl;
	} 
      }
    }
    converged = 1;

  } else if(_sim_mode == batch && !_timed_mode){//batch mode   
    while(total_phases < _batch_count) {
      _packets_sent.assign(_nodes, 0);
      _last_id = -1;
      _last_pid = -1;
      _sim_state = running;
      int start_time = _time;
      int min_packets_sent = 0;
      while(min_packets_sent < _batch_size){
	_Step();
	min_packets_sent = _packets_sent[0];
	for(int i = 1; i < _nodes; ++i) {
	  if(_packets_sent[i] < min_packets_sent)
	    min_packets_sent = _packets_sent[i];
	}
	if(_flow_out) {
	  *_flow_out << "packets_sent(" << _time << ",:) = " << _packets_sent << ";" << endl;
	}
      }
      cout << "Batch " << total_phases + 1 << " ("<<_batch_size  <<  " flits) sent. Time used is " << _time - start_time << " cycles." << endl;
      cout << "Draining the Network...................\n";
      _sim_state = draining;
      _drain_time = _time;
      int empty_steps = 0;

      bool packets_left = false;
      for(int c = 0; c < _classes; ++c) {
	if(_drain_measured_only) {
	  packets_left |= !_measured_in_flight_flits[c].empty();
	} else {
	  packets_left |= !_total_in_flight_flits[c].empty();
	}
      }

      while( packets_left ) { 
	_Step( ); 

	++empty_steps;
	
	if ( empty_steps % 1000 == 0 ) {
	  _DisplayRemaining( ); 
	  cout << ".";
	}

	packets_left = false;
	for(int c = 0; c < _classes; ++c) {
	  if(_drain_measured_only) {
	    packets_left |= !_measured_in_flight_flits[c].empty();
	  } else {
	    packets_left |= !_total_in_flight_flits[c].empty();
	  }
	}
      }
      cout << endl;
      cout << "Batch " << total_phases + 1 << " ("<<_batch_size  <<  " flits) received. Time used is " << _time - _drain_time << " cycles. Last packet was " << _last_pid << ", last flit was " << _last_id << "." <<endl;
      _batch_time->AddSample(_time - start_time);
      cout << _sim_state << endl;
      if(_stats_out)
	*_stats_out << "%=================================" << endl;
      double cur_latency = _plat_stats[0]->Average( );
      double min, avg;
      int dmin = _ComputeStats( _accepted_flits[0], &avg, &min );
      
      cout << "Batch duration = " << _time - start_time << endl;
      cout << "Minimum latency = " << _plat_stats[0]->Min( ) << endl;
      cout << "Average latency = " << cur_latency << endl;
      cout << "Maximum latency = " << _plat_stats[0]->Max( ) << endl;
      cout << "Average fragmentation = " << _frag_stats[0]->Average( ) << endl;
      cout << "Accepted packets = " << min << " at node " << dmin << " (avg = " << avg << ")" << endl;
      if(_stats_out) {
	*_stats_out << "batch_time(" << total_phases + 1 << ") = " << _time << ";" << endl
		    << "lat(" << total_phases + 1 << ") = " << cur_latency << ";" << endl
		    << "lat_hist(" << total_phases + 1 << ",:) = "
		    << *_plat_stats[0] << ";" << endl
		    << "frag_hist(" << total_phases + 1 << ",:) = "
		    << *_frag_stats[0] << ";" << endl
		    << "pair_sent(" << total_phases + 1 << ",:) = [ ";
	for(int i = 0; i < _nodes; ++i) {
	  for(int j = 0; j < _nodes; ++j) {
	    *_stats_out << _pair_plat[0][i*_nodes+j]->NumSamples( ) << " ";
	  }
	}
	*_stats_out << "];" << endl
		    << "pair_lat(" << total_phases + 1 << ",:) = [ ";
	for(int i = 0; i < _nodes; ++i) {
	  for(int j = 0; j < _nodes; ++j) {
	    *_stats_out << _pair_plat[0][i*_nodes+j]->Average( ) << " ";
	  }
	}
	*_stats_out << "];" << endl
		    << "pair_tlat(" << total_phases + 1 << ",:) = [ ";
	for(int i = 0; i < _nodes; ++i) {
	  for(int j = 0; j < _nodes; ++j) {
	    *_stats_out << _pair_tlat[0][i*_nodes+j]->Average( ) << " ";
	  }
	}
	*_stats_out << "];" << endl
		    << "sent(" << total_phases + 1 << ",:) = [ ";
	for ( int d = 0; d < _nodes; ++d ) {
	  *_stats_out << _sent_flits[0][d]->Average( ) << " ";
	}
	*_stats_out << "];" << endl
		    << "accepted(" << total_phases + 1 << ",:) = [ ";
	for ( int d = 0; d < _nodes; ++d ) {
	  *_stats_out << _accepted_flits[0][d]->Average( ) << " ";
	}
	*_stats_out << "];" << endl;
      }
      ++total_phases;
    }
    converged = 1;
  } else { 
    //once warmed up, we require 3 converging runs
    //to end the simulation 
    vector<double> prev_latency(_classes, 0.0);
    vector<double> prev_accepted(_classes, 0.0);
    while( ( total_phases < _max_samples ) && 
	   ( ( _sim_state != running ) || 
	     ( converged < 3 ) ) ) {

      if ( clear_last || (( ( _sim_state == warming_up ) && ( ( total_phases % 2 ) == 0 ) )) ) {
	clear_last = false;
	_ClearStats( );
      }
      
      
      for ( int iter = 0; iter < _sample_period; ++iter )
	_Step( );
      
      cout << _sim_state << endl;
      if(_stats_out)
	*_stats_out << "%=================================" << endl;

      int lat_exc_class = -1;
      int lat_chg_exc_class = -1;
      int acc_chg_exc_class = -1;

      for(int c = 0; c < _classes; ++c) {

	if(_measure_stats[c] == 0) {
	  continue;
	}

	double cur_latency = _plat_stats[c]->Average( );
	int dmin;
	double min, avg;
	dmin = _ComputeStats( _accepted_flits[c], &avg, &min );
	double cur_accepted = avg;

	double latency_change = fabs((cur_latency - prev_latency[c]) / cur_latency);
	prev_latency[c] = cur_latency;
	double accepted_change = fabs((cur_accepted - prev_accepted[c]) / cur_accepted);
	prev_accepted[c] = cur_accepted;

	cout << "Class " << c << ":" << endl;

	cout << "Minimum latency = " << _plat_stats[c]->Min( ) << endl;
	cout << "Average latency = " << cur_latency << endl;
	cout << "Maximum latency = " << _plat_stats[c]->Max( ) << endl;
	cout << "Average fragmentation = " << _frag_stats[c]->Average( ) << endl;
	cout << "Accepted packets = " << min << " at node " << dmin << " (avg = " << avg << ")" << endl;
	cout << "Total in-flight flits = " << _total_in_flight_flits[c].size() << " (" << _measured_in_flight_flits[c].size() << " measured)" << endl;
	//c+1 due to matlab array starting at 1
	if(_stats_out) {
	  *_stats_out << "lat(" << c+1 << ") = " << cur_latency << ";" << endl
		      << "lat_hist(" << c+1 << ",:) = " << *_plat_stats[c] << ";" << endl
		      << "frag_hist(" << c+1 << ",:) = " << *_frag_stats[c] << ";" << endl
		      << "pair_sent(" << c+1 << ",:) = [ ";
	  for(int i = 0; i < _nodes; ++i) {
	    for(int j = 0; j < _nodes; ++j) {
	      *_stats_out << _pair_plat[c][i*_nodes+j]->NumSamples( ) << " ";
	    }
	  }
	  *_stats_out << "];" << endl
		      << "pair_lat(" << c+1 << ",:) = [ ";
	  for(int i = 0; i < _nodes; ++i) {
	    for(int j = 0; j < _nodes; ++j) {
	      *_stats_out << _pair_plat[c][i*_nodes+j]->Average( ) << " ";
	    }
	  }
	  *_stats_out << "];" << endl
		      << "pair_lat(" << c+1 << ",:) = [ ";
	  for(int i = 0; i < _nodes; ++i) {
	    for(int j = 0; j < _nodes; ++j) {
	      *_stats_out << _pair_tlat[c][i*_nodes+j]->Average( ) << " ";
	    }
	  }
	  *_stats_out << "];" << endl
		      << "sent(" << c+1 << ",:) = [ ";
	  for ( int d = 0; d < _nodes; ++d ) {
	    *_stats_out << _sent_flits[c][d]->Average( ) << " ";
	  }
	  *_stats_out << "];" << endl
		      << "accepted(" << c+1 << ",:) = [ ";
	  for ( int d = 0; d < _nodes; ++d ) {
	    *_stats_out << _accepted_flits[c][d]->Average( ) << " ";
	  }
	  *_stats_out << "];" << endl;
	  *_stats_out << "inflight(" << c+1 << ") = " << _total_in_flight_flits[c].size() << ";" << endl;
	  
	 
	  for(int i = 0; i<_routers; i++){
	    *_stats_out <<"drop_router("<<i+1<<",:)="
			<<gDropStats[i] 
			<<";"<<endl;
	  }
	  vector<FlitChannel *>  temp = _net[0]->GetChannels();
	  for(unsigned int i = 0; i<temp.size(); i++){
	    *_stats_out <<"chan_idle("<<i+1<<",:)="
			<<temp[i]->GetActivity()
			<<";"<<endl;
	  }
	  *_stats_out <<"run_time = "<<_time<<";"<<endl;
	}
	
	double latency = cur_latency;
	double count = (double)_plat_stats[c]->NumSamples();
	  
	map<int, Flit *>::const_iterator iter;
	for(iter = _total_in_flight_flits[c].begin(); 
	    iter != _total_in_flight_flits[c].end(); 
	    iter++) {
	  latency += (double)(_time - iter->second->time);
	  count++;
	}
	
	if((lat_exc_class < 0) &&
	   (_latency_thres[c] >= 0.0) &&
	   ((latency / count) > _latency_thres[c])) {
	  lat_exc_class = c;
	}
	
	cout << "latency change    = " << latency_change << endl;
	if(lat_chg_exc_class < 0) {
	  if((_sim_state == warming_up) &&
	     (_warmup_threshold[c] >= 0.0) &&
	     (latency_change > _warmup_threshold[c])) {
	    lat_chg_exc_class = c;
	  } else if((_sim_state == running) &&
		    (_stopping_threshold[c] >= 0.0) &&
		    (latency_change > _stopping_threshold[c])) {
	    lat_chg_exc_class = c;
	  }
	}
	
	cout << "throughput change = " << accepted_change << endl;
	if(acc_chg_exc_class < 0) {
	  if((_sim_state == warming_up) &&
	     (_acc_warmup_threshold[c] >= 0.0) &&
	     (accepted_change > _acc_warmup_threshold[c])) {
	    acc_chg_exc_class = c;
	  } else if((_sim_state == running) &&
		    (_acc_stopping_threshold[c] >= 0.0) &&
		    (accepted_change > _acc_stopping_threshold[c])) {
	    acc_chg_exc_class = c;
	  }
	}
	
      }

      // Fail safe for latency mode, throughput will ust continue
      if ( ( _sim_mode == latency ) && ( lat_exc_class >= 0 ) ) {

	cout << "Average latency for class " << lat_exc_class << " exceeded " << _latency_thres[lat_exc_class] << " cycles. Aborting simulation." << endl;
	converged = 0; 
	_sim_state = warming_up;
	break;

      }

      if ( _sim_state == warming_up ) {
	if ( ( _warmup_periods > 0 ) ? 
	     ( total_phases + 1 >= _warmup_periods ) :
	     ( ( ( _sim_mode != latency ) || ( lat_chg_exc_class < 0 ) ) &&
	       ( acc_chg_exc_class < 0 ) ) ) {
	  cout << "Warmed up ..." <<  "Time used is " << _time << " cycles" <<endl;
	  clear_last = true;
	  _sim_state = running;
	}
      } else if(_sim_state == running) {
	if ( ( ( _sim_mode != latency ) || ( lat_chg_exc_class < 0 ) ) &&
	     ( acc_chg_exc_class < 0 ) ) {
	  ++converged;
	} else {
	  converged = 0;
	}
      }
      ++total_phases;
    }
  
    if ( _sim_state == running ) {
      ++converged;

      if ( _sim_mode == latency ) {
	cout << "Draining all recorded packets ..." << endl;
	_sim_state  = draining;
	_drain_time = _time;
	int empty_steps = 0;
	while( _PacketsOutstanding( ) ) { 
	  _Step( ); 

	  ++empty_steps;
	  
	  if ( empty_steps % 1000 == 0 ) {
	    
	    int lat_exc_class = -1;
	    
	    for(int c = 0; c < _classes; c++) {

	      double threshold = _latency_thres[c];

	      if(threshold < 0.0) {
		continue;
	      }

	      double acc_latency = _plat_stats[c]->Sum();
	      double acc_count = (double)_plat_stats[c]->NumSamples();

	      map<int, Flit *>::const_iterator iter;
	      for(iter = _total_in_flight_flits[c].begin(); 
		  iter != _total_in_flight_flits[c].end(); 
		  iter++) {
		acc_latency += (double)(_time - iter->second->time);
		acc_count++;
	      }
	      
	      if((acc_latency / acc_count) > threshold) {
		lat_exc_class = c;
		break;
	      }
	    }
	    
	    if(lat_exc_class >= 0) {
	      cout << "Average latency for class " << lat_exc_class << " exceeded " << _latency_thres[lat_exc_class] << " cycles. Aborting simulation." << endl;
	      converged = 0; 
	      _sim_state = warming_up;
	      break;
	    }
	    
	    _DisplayRemaining( ); 
	    
	  }
	}
      }
    } else {
      cout << "Too many sample periods needed to converge" << endl;
    }

    // Empty any remaining packets
    cout << "Draining remaining packets ..." << endl;
    _empty_network = true;
    int empty_steps = 0;

    bool packets_left = false;
    for(int c = 0; c < _classes; ++c) {
      if(_drain_measured_only) {
	packets_left |= !_measured_in_flight_flits[c].empty();
      } else {
	packets_left |= !_total_in_flight_flits[c].empty();
      }
    }

    while( packets_left ) { 
      _Step( ); 

      ++empty_steps;

      if ( empty_steps % 1000 == 0 ) {
	_DisplayRemaining( ); 
      }
      
      packets_left = false;
      for(int c = 0; c < _classes; ++c) {
	if(_drain_measured_only) {
	  packets_left |= !_measured_in_flight_flits[c].empty();
	} else {
	  packets_left |= !_total_in_flight_flits[c].empty();
	}
      }
    }
    _empty_network = false;
  }

  return ( converged > 0 );
}

bool TrafficManager::Run( )
{
  for ( int sim = 0; sim < _total_sims; ++sim ) {
    if ( !_SingleSim( ) ) {
      cout << "Simulation unstable, ending ..." << endl;
      return false;
    }
    //for the love of god don't ever say "Time taken" anywhere else
    //the power script depend on it
    cout << "Time taken is " << _time << " cycles" <<endl; 
    for ( int c = 0; c < _classes; ++c ) {

      if(_measure_stats[c] == 0) {
	continue;
      }

      _overall_min_plat[c]->AddSample( _plat_stats[c]->Min( ) );
      _overall_avg_plat[c]->AddSample( _plat_stats[c]->Average( ) );
      _overall_max_plat[c]->AddSample( _plat_stats[c]->Max( ) );
      _overall_min_tlat[c]->AddSample( _tlat_stats[c]->Min( ) );
      _overall_avg_tlat[c]->AddSample( _tlat_stats[c]->Average( ) );
      _overall_max_tlat[c]->AddSample( _tlat_stats[c]->Max( ) );
      _overall_min_frag[c]->AddSample( _frag_stats[c]->Min( ) );
      _overall_avg_frag[c]->AddSample( _frag_stats[c]->Average( ) );
      _overall_max_frag[c]->AddSample( _frag_stats[c]->Max( ) );

      double min, avg;
      _ComputeStats( _accepted_flits[c], &avg, &min );
      _overall_accepted[c]->AddSample( avg );
      _overall_accepted_min[c]->AddSample( min );

      if(_sim_mode == batch)
	_overall_batch_time->AddSample(_batch_time->Sum( ));
    }
  }
  
  DisplayStats();
  if(_print_vc_stats) {
    if(_print_csv_results) {
      cout << "vc_stats:";
    }
    VC::DisplayStats(_print_csv_results);
  }
  return true;
}

void TrafficManager::DisplayStats( ostream & os ) {
  for ( int c = 0; c < _classes; ++c ) {

    if(_measure_stats[c] == 0) {
      continue;
    }

    if(_print_csv_results) {
      os << "results:"
	 << c
	 << "," << _traffic[c]
	 << "," << _use_read_write[c]
	 << "," << _packet_size[c]
	 << "," << _load[c]
	 << "," << _overall_min_plat[c]->Average( )
	 << "," << _overall_avg_plat[c]->Average( )
	 << "," << _overall_max_plat[c]->Average( )
	 << "," << _overall_min_tlat[c]->Average( )
	 << "," << _overall_avg_tlat[c]->Average( )
	 << "," << _overall_max_tlat[c]->Average( )
	 << "," << _overall_min_frag[c]->Average( )
	 << "," << _overall_avg_frag[c]->Average( )
	 << "," << _overall_max_frag[c]->Average( )
	 << "," << _overall_accepted[c]->Average( )
	 << "," << _overall_accepted_min[c]->Average( )
	 << "," << _hop_stats[c]->Average( )
	 << endl;
    }

    os << "====== Traffic class " << c << " ======" << endl;
    
    os << "Overall minimum latency = " << _overall_min_plat[c]->Average( )
       << " (" << _overall_min_plat[c]->NumSamples( ) << " samples)" << endl;
    os << "Overall average latency = " << _overall_avg_plat[c]->Average( )
       << " (" << _overall_avg_plat[c]->NumSamples( ) << " samples)" << endl;
    os << "Overall maximum latency = " << _overall_max_plat[c]->Average( )
       << " (" << _overall_max_plat[c]->NumSamples( ) << " samples)" << endl;
    os << "Overall minimum transaction latency = " << _overall_min_tlat[c]->Average( )
       << " (" << _overall_min_tlat[c]->NumSamples( ) << " samples)" << endl;
    os << "Overall average transaction latency = " << _overall_avg_tlat[c]->Average( )
       << " (" << _overall_avg_tlat[c]->NumSamples( ) << " samples)" << endl;
    os << "Overall maximum transaction latency = " << _overall_max_tlat[c]->Average( )
       << " (" << _overall_max_tlat[c]->NumSamples( ) << " samples)" << endl;
    
    os << "Overall minimum fragmentation = " << _overall_min_frag[c]->Average( )
       << " (" << _overall_min_frag[c]->NumSamples( ) << " samples)" << endl;
    os << "Overall average fragmentation = " << _overall_avg_frag[c]->Average( )
       << " (" << _overall_avg_frag[c]->NumSamples( ) << " samples)" << endl;
    os << "Overall maximum fragmentation = " << _overall_max_frag[c]->Average( )
       << " (" << _overall_max_frag[c]->NumSamples( ) << " samples)" << endl;

    os << "Overall average accepted rate = " << _overall_accepted[c]->Average( )
       << " (" << _overall_accepted[c]->NumSamples( ) << " samples)" << endl;
    os << "Overall min accepted rate = " << _overall_accepted_min[c]->Average( )
       << " (" << _overall_accepted_min[c]->NumSamples( ) << " samples)" << endl;
    
    os << "Average hops = " << _hop_stats[c]->Average( )
       << " (" << _hop_stats[c]->NumSamples( ) << " samples)" << endl;

    os << "Slowest flit = " << _slowest_flit[c] << endl;
    
    if(_stats_out){
    *_stats_out<< "ack_received = "
	       <<gStatAckReceived<<";\n";
    *_stats_out<< "ack_eff = "
	       <<gStatAckEffective<<";\n";
    *_stats_out<< "ack_sent = "
	       <<gStatAckSent<<";\n";

    *_stats_out<< "nack_received = "
	       <<gStatNackReceived<<";\n";
    *_stats_out<< "nack_eff = "
	       <<gStatNackEffective<<";\n";
    *_stats_out<< "nack_sent = "
	       <<gStatNackSent<<";\n";

    *_stats_out<< "grant_received = "
	       <<gStatGrantReceived<<";\n";

    *_stats_out<< "res_received = "
	       <<gStatReservationReceived<<";\n";

    *_stats_out<< "spec_received = "
	       <<gStatSpecReceived<<";\n";
    *_stats_out<< "spec_dup = "
	       <<gStatSpecDuplicate<<";\n";

    *_stats_out<< "norm_sent = "
	       <<gStatNormSent<<";\n";
    *_stats_out<< "norm_received = "
	       <<gStatNormReceived<<";\n";
    *_stats_out<< "norm_dup = "
	       <<gStatNormDuplicate<<";\n";

    *_stats_out<< "lost_flits = "
	       <<gStatLostPacket<<";\n";


    for(int i = 0; i<_nodes; i++){
      *_stats_out<<"grant_now ("<<i+1<<",:)="
		 <<*gStatGrantTimeNow[i] <<";\n";
      *_stats_out<<"grant_future ("<<i+1<<",:)="
		 <<*gStatGrantTimeFuture[i] <<";\n";
      *_stats_out<<"res_now ("<<i+1<<",:)="
		 <<*gStatReservationTimeNow[i] <<";\n";
      *_stats_out<<"res_future ("<<i+1<<",:)="
		 <<*gStatReservationTimeFuture[i] <<";\n";
    }
    
    *_stats_out<< "rob_range = "
	       <<*gStatROBRange<<";\n";

    *_stats_out<< "flow_sender_hist = "
	       <<*gStatFlowSenderLatency <<";\n";
    *_stats_out<< "flow_hist = "
	       <<*gStatFlowLatency <<";\n";
    *_stats_out<< "active_flows = "
	       <<*gStatActiveFlowBuffers <<";\n";
    *_stats_out<< "response_range = "
	       <<*gStatResponseBuffer <<";\n";
    
    *_stats_out<< "net_hist = "
	       <<*gStatPureNetworkLatency <<";\n";
    *_stats_out<< "ack_hist = "
	       <<*gStatAckLatency <<";\n";
   *_stats_out<< "nack_hist = "
	       <<*gStatNackLatency <<";\n";
   *_stats_out<< "res_hist = "
	       <<*gStatResLatency <<";\n";
   *_stats_out<< "grant_hist = "
	       <<*gStatGrantLatency <<";\n";
   *_stats_out<< "spec_hist = "
	       <<*gStatSpecLatency <<";\n";
   *_stats_out<< "norm_hist = "
	       <<*gStatNormLatency <<";\n";

   *_stats_out<< "source_queue_hist = "
	      <<*gStatSourceLatency <<";\n";
   *_stats_out<< "source_truequeue_hist = "
	      <<*gStatSourceTrueLatency <<";\n";   

   *_stats_out<< "nack_by_sn = "
	      <<*gStatNackByPacket<<";\n";

   *_stats_out<< "flow_in_spec =" 
	      <<gStatFlowStats[FLOW_STAT_SPEC]<<";\n";
   *_stats_out<< "flow_in_nack =" 
	      <<gStatFlowStats[FLOW_STAT_NACK]<<";\n";
   *_stats_out<< "flow_in_wait =" 
	      <<gStatFlowStats[FLOW_STAT_WAIT]<<";\n";
   *_stats_out<< "flow_in_norm =" 
	      <<gStatFlowStats[FLOW_STAT_NORM ]<<";\n";
   *_stats_out<< "flow_in_norm_ready =" 
	      <<gStatFlowStats[FLOW_STAT_NORM_READY ]<<";\n";
   *_stats_out<< "flow_in_spec_ready =" 
	      <<gStatFlowStats[FLOW_STAT_SPEC_READY]<<";\n";
   *_stats_out<< "flow_in_not_ready =" 
	    <<gStatFlowStats[FLOW_STAT_NOT_READY]<<";\n";
   *_stats_out<< "flow_in_final_not_ready =" 
	    <<gStatFlowStats[FLOW_STAT_FINAL_NOT_READY]<<";\n";
   *_stats_out<< "flow_in_lifetime =" 
	      <<gStatFlowStats[FLOW_STAT_LIFETIME]<<";\n";
   *_stats_out<< "flow_count =" 
	      <<gStatFlowStats[FLOW_STAT_LIFETIME+1]<<";\n";
    }
  }
  
  if(_sim_mode == batch)
    os << "Overall batch duration = " << _overall_batch_time->Average( )
       << " (" << _overall_batch_time->NumSamples( ) << " samples)" << endl;
  
}

//read the watchlist
void TrafficManager::_LoadWatchList(const string & filename){
  ifstream watch_list;
  watch_list.open(filename.c_str());
  
  string line;
  if(watch_list.is_open()) {
    while(!watch_list.eof()) {
      getline(watch_list, line);
      if(line != "") {
	if(line[0] == 'p') {
	  _packets_to_watch.insert(atoi(line.c_str()+1));
	} else if(line[0] == 't') {
	  _transactions_to_watch.insert(atoi(line.c_str()+1));
	} else {
	  _flits_to_watch.insert(atoi(line.c_str()));
	}
      }
    }
    
  } else {
    //cout<<"Unable to open flit watch file, continuing with simulation\n";
  }
}
