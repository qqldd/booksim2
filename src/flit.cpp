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

/*flit.cpp
 *
 *flit struct is a flit, carries all the control signals that a flit needs
 *Add additional signals as necessary. Flits has no concept of length
 *it is a singluar object.
 *
 *When adding objects make sure to set a default value in this constructor
 */

#include "booksim.hpp"
#include "flit.hpp"

stack<Flit *> Flit::_all;
stack<Flit *> Flit::_free;

ostream& operator<<( ostream& os, const Flit& f )
{
  os << "  Flit ID: " << f.id << " (" << &f << ")" 
     << " Packet ID: " << f.pid
     << " Transaction ID: " << f.tid
     << " Class: " << f.cl 
     << " Head: " << f.head
     << " Tail: " << f.tail << endl;
  os << "  Source: " << f.src << "  Dest: " << f.dest << " Intm: "<<f.intm<<endl;
  os << "  Injection time: " << f.time << " Transaction start: " << f.ttime << " Arrival time: " << f.atime << " Phase: "<<f.ph<< endl;
  os << "  VC: " << f.vc << endl;
  return os;
}

Flit::Flit() 
{  
  Reset();
}  

void Flit::Reset() 
{  
  vc        = -1 ;
  cl        = -1 ;
  head      = false ;
  tail      = false ;
  true_tail = false ;
  time      = -1 ;
  ttime     = -1 ;
  atime     = -1 ;
  sn        = 0 ;
  rob_time  = 0 ;
  id        = -1 ;
  pid       = -1 ;
  tid       = -1 ;
  hops      = 0 ;
  watch     = false ;
  record    = false ;
  intm = 0;
  src = -1;
  dest = -1;
  pri = 0;
  intm =-1;
  ph = -1;
  dr = -1;
  minimal = 1;
  ring_par = -1;
  data = 0;
}  

Flit * Flit::New() {
  Flit * f;
  if(_free.empty()) {
    f = new Flit;
    _all.push(f);
  } else {
    f = _free.top();
    f->Reset();
    _free.pop();
  }
  return f;
}

void Flit::Free() {
  _free.push(this);
}

void Flit::FreeAll() {
  while(!_all.empty()) {
    delete _all.top();
    _all.pop();
  }
}
