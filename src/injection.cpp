// $Id$

/*
Copyright (c) 2007-2010, Trustees of The Leland Stanford Junior University
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

#include <iostream>
#include <cassert>
#include "random_utils.hpp"
#include "injection.hpp"

using namespace std;

InjectionProcess::InjectionProcess(double rate)
: _rate(rate)
{
  if((rate < 0.0) || (rate > 1.0)) {
    cout << "Error: Injection process must have load between 0.0 and 1.0." << endl;
    exit(-1);
  }
}

InjectionProcess * InjectionProcess::New(string const & inject, double load)
{
  string process_name;
  string param_str;
  size_t left = inject.find_first_of('(');
  if(left == string::npos) {
    process_name = inject;
    param_str = "{}";
  } else {
    process_name = inject.substr(0, left);
    size_t right = inject.find_last_of(')');
    if(right == string::npos) {
      param_str = inject.substr(left+1);
    } else {
      param_str = inject.substr(left+1, right-left-1);
    }
  }
  vector<string> params = tokenize('{' + param_str + '}');

  InjectionProcess * result = NULL;
  if(process_name == "bernoulli") {
    result = new BernoulliInjectionProcess(load);
  } else if(process_name == "on_off") {
    if(params.size() < 2) {
      cout << "Missing parameters for injection process: " << inject << endl;
      exit(-1);
    }
    double alpha = strtod(params[0].c_str(), NULL);
    double beta = strtod(params[1].c_str(), NULL);
    bool initial = (params.size() > 2) ? atoi(params[2].c_str()) : false;
    result = new OnOffInjectionProcess(load, alpha, beta, initial);
  } else {
    cout << "Invalid injection process: " << inject << endl;
    exit(-1);
  }
  return result;
}

vector<InjectionProcess *> InjectionProcess::Load(Configuration const & config)
{
  int const classes = config.GetInt("classes");
  assert(classes > 0);

  vector<int> packet_size = config.GetIntArray( "packet_size" );
  if(packet_size.empty()) {
    packet_size.push_back(config.GetInt("packet_size"));
  }
  packet_size.resize(classes, packet_size.back());
  
  vector<double> load = config.GetFloatArray("injection_rate"); 
  if(load.empty()) {
    load.push_back(config.GetFloat("injection_rate"));
  }
  load.resize(classes, load.back());

  if(config.GetInt("injection_rate_uses_flits")) {
    for(int c = 0; c < classes; ++c)
      load[c] /= (double)packet_size[c];
  }

  vector<string> inject = config.GetStrArray("injection_process");
  inject.resize(classes, inject.back());
  
  vector<InjectionProcess *> result(classes);
  for(int c = 0; c < classes; ++c) {
    result[c] = InjectionProcess::New(inject[c], load[c]);
  }
  return result;
}

//=============================================================

BernoulliInjectionProcess::BernoulliInjectionProcess(double rate)
  : InjectionProcess(rate)
{

}

bool BernoulliInjectionProcess::test()
{
  return (RandomFloat() < _rate);
}

//=============================================================

OnOffInjectionProcess::OnOffInjectionProcess(double rate, double alpha, 
					     double beta, bool initial)
  : InjectionProcess(rate), _alpha(alpha), _beta(beta), _state(initial)
{
  assert((alpha >= 0.0) && (alpha <= 1.0));
  assert((beta >= 0.0) && (beta <= 1.0));
}

bool OnOffInjectionProcess::test()
{

  // advance state

  if(!_state) {
    if(RandomFloat() < _alpha) { // from off to on
      _state = true;
    }
  } else {
    if(RandomFloat() < _beta) { // from on to off
      _state = false;
    }
  }

  // generate packet

  if(_state) { // on?
    double r1 = _rate * (_alpha + _beta) / _alpha;
    return (RandomFloat() < r1);
  }

  return false;
}
