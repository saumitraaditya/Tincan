/*
* ipop-project
* Copyright 2016, University of Florida
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/
#include "control_listener.h"
#include "webrtc/base/nethelpers.h"
namespace tincan
{
using namespace rtc;
ControlListener::ControlListener(unique_ptr<ControlDispatch> control_dispatch) :
  ctrl_dispatch_(move(control_dispatch)),
  packet_options_(DSCP_DEFAULT)
{
  ctrl_dispatch_->SetDispatchToListenerInf(this);
}

ControlListener::~ControlListener()
{}

void
ControlListener::ReadPacketHandler(
  AsyncPacketSocket *,
  const char * data,
  size_t len,
  const SocketAddress &,
  const PacketTime &)
{
  try {
    TincanControl ctrl(data, len);
    LOG(LS_VERBOSE) << "Received CONTROL: " << ctrl.StyledString();
    (*ctrl_dispatch_)(ctrl);
  }
  catch(exception & e) {
    LOG(LS_WARNING) << "A control failed to execute." << endl
      << string(data, len) << endl
      << e.what();
  }
}
//
//IpopControllerLink interface implementation
void 
ControlListener::Deliver(
  TincanControl & ctrl_resp)
{
  std::string msg = ctrl_resp.StyledString();
  lock_guard<mutex> lg(skt_mutex_);
  snd_socket_->SendTo(msg.c_str(), msg.length(), *ctrl_addr_, packet_options_);
}
void
ControlListener::Deliver(
  unique_ptr<TincanControl> ctrl_resp)
{
  Deliver(*ctrl_resp.get());
}
//
//DispatchtoListener interface implementation
void 
ControlListener::CreateIpopControllerLink(
  unique_ptr<SocketAddress> controller_addr)
{
  lock_guard<mutex> lg(skt_mutex_);
  ctrl_addr_ = move(controller_addr);
  SocketFactory* sf = Thread::Current()->socketserver();
  snd_socket_ = make_unique<AsyncUDPSocket>(
    sf->CreateAsyncSocket(ctrl_addr_->family(), SOCK_DGRAM));
}

void
ControlListener::Run(Thread* thread) {
  BasicPacketSocketFactory packet_factory;
  in6_addr addr6;
  if(rtc::inet_pton(AF_INET6, kLocalHost6, &addr6) != 0)
    rcv_socket_.reset(packet_factory.CreateUdpSocket(
      SocketAddress(kLocalHost6, kUdpPort), 0, 0));
  else
    rcv_socket_.reset(packet_factory.CreateUdpSocket(
      SocketAddress(kLocalHost, kUdpPort), 0, 0));
  rcv_socket_->SignalReadPacket.connect(this,
    &ControlListener::ReadPacketHandler);
  thread->ProcessMessages(-1); //run until stopped
}
}  // namespace tincan
