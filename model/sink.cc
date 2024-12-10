/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "sink.h"

#include "packet-tags.h"

#include "ns3/address-utils.h"
#include "ns3/address.h"
#include "ns3/boolean.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/udp-socket.h"

#include <iostream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Sink");

NS_OBJECT_ENSURE_REGISTERED(Sink);

TypeId
Sink::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::Sink")
                            .SetParent<Application>()
                            .SetGroupName("Applications")
                            .AddConstructor<Sink>()
                            .AddAttribute("Local",
                                          "The Address on which to Bind the rx socket.",
                                          AddressValue(),
                                          MakeAddressAccessor(&Sink::m_local),
                                          MakeAddressChecker())
                            .AddAttribute("Protocol",
                                          "The type id of the protocol to use for the rx socket.",
                                          TypeIdValue(UdpSocketFactory::GetTypeId()),
                                          MakeTypeIdAccessor(&Sink::m_tid),
                                          MakeTypeIdChecker())
                            .AddAttribute("EnableSeqTsSizeHeader",
                                          "Enable optional header tracing of SeqTsSizeHeader",
                                          BooleanValue(false),
                                          MakeBooleanAccessor(&Sink::m_enableSeqTsSizeHeader),
                                          MakeBooleanChecker())
                            .AddTraceSource("Rx",
                                            "A packet has been received",
                                            MakeTraceSourceAccessor(&Sink::m_rxTrace),
                                            "ns3::Packet::AddressTracedCallback")
                            .AddTraceSource("RxWithAddresses",
                                            "A packet has been received",
                                            MakeTraceSourceAccessor(&Sink::m_rxTraceWithAddresses),
                                            "ns3::Packet::TwoAddressTracedCallback")
                            .AddTraceSource("RxWithSeqTsSize",
                                            "A packet with SeqTsSize header has been received",
                                            MakeTraceSourceAccessor(&Sink::m_rxTraceWithSeqTsSize),
                                            "ns3::PacketSink::SeqTsSizeCallback");
    return tid;
}

Sink::Sink()
{
    NS_LOG_FUNCTION(this);
    m_socket = 0;
    m_totalRx = 0;
    m_old = 0;
    // uint32_t interval = 10;
    // Simulator::Schedule (MilliSeconds (interval), &MonitorThroughput, interval);
}

Sink::~Sink()
{
    NS_LOG_FUNCTION(this);
}

// void
// Sink::MonitorThroughput (uint32_t interval)
// {
//   uint64_t value_new = GetTotalRx ();
//   Time curTime = Now ();
//   std::ostream* os = m_throughputStream->GetStream ();
//   * os << curTime.GetMilliSeconds () << ""
//        << 8 * (value_new - m_old)/ (1000 * 1000 * interval / 100)
//        << std::endl;
//   Simulator::Schedule (MilliSeconds (interval), &MonitorThroughput, interval);
// }

uint64_t
Sink::GetTotalRx() const
{
    NS_LOG_FUNCTION(this);
    return m_totalRx;
}

Ptr<Socket>
Sink::GetListeningSocket(void) const
{
    NS_LOG_FUNCTION(this);
    return m_socket;
}

std::list<Ptr<Socket>>
Sink::GetAcceptedSockets(void) const
{
    NS_LOG_FUNCTION(this);
    return m_socketList;
}

Time
Sink::GetDelay(const Ptr<Packet>& p) const
{
    NS_LOG_FUNCTION(this);
    TimestampTag txTimeTag;
    p->PeekPacketTag(txTimeTag);
    Time txTime = txTimeTag.GetTimestamp();
    Time delay = Simulator::Now() - txTime;
    return delay;
}

void
Sink::DoDispose(void)
{
    NS_LOG_FUNCTION(this);
    m_socket = 0;
    m_socketList.clear();

    // chain up
    Application::DoDispose();
}

// Application Methods
void
Sink::StartApplication() // Called at time specified by Start
{
    NS_LOG_FUNCTION(this);
    // Create the socket if not already
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket(GetNode(), m_tid);
        if (m_socket->Bind(m_local) == -1)
        {
            NS_FATAL_ERROR("Failed to bind socket");
        }
        m_socket->Listen();
        m_socket->ShutdownSend();
        if (addressUtils::IsMulticast(m_local))
        {
            Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket>(m_socket);
            if (udpSocket)
            {
                // equivalent to setsockopt (MCAST_JOIN_GROUP)
                udpSocket->MulticastJoinGroup(0, m_local);
            }
            else
            {
                NS_FATAL_ERROR("Error: joining multicast on a non-UDP socket");
            }
        }
    }

    m_socket->SetRecvCallback(MakeCallback(&Sink::HandleRead, this));
    m_socket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address&>(),
                                MakeCallback(&Sink::HandleAccept, this));
    m_socket->SetCloseCallbacks(MakeCallback(&Sink::HandlePeerClose, this),
                                MakeCallback(&Sink::HandlePeerError, this));
}

void
Sink::StopApplication() // Called at time specified by Stop
{
    NS_LOG_FUNCTION(this);
    while (!m_socketList.empty()) // these are accepted sockets, close them
    {
        Ptr<Socket> acceptedSocket = m_socketList.front();
        m_socketList.pop_front();
        acceptedSocket->Close();
    }
    if (m_socket)
    {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
}

int i = 0; // count packets

void
Sink::HandleRead(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    Ptr<Packet> packet;
    Address from;
    Address localAddress;

    while ((packet = socket->RecvFrom(from)))
    {
        if (packet->GetSize() == 0)
        { // EOF
            break;
        }
        i++;
        // std::cout<<i<<"packet"<< std::endl;
        // std::cout << "packet received" << std::endl;
        // packet->PrintPacketTags (std::cout);
        // std::cout << std::endl;
        // get packet
        FlagTag flagTag;
        if (packet->PeekPacketTag(flagTag) && flagTag.GetFlag() == true)
        {
            TimestampTag timeTag;
            packet->PeekPacketTag(timeTag);
            std::ostream* os = m_delayStream->GetStream();
            // timeTag.GetSeconds () << " "
            BudgetTag bgtTag;
            packet->PeekPacketTag(bgtTag);

            *os << bgtTag.GetBudget() << std::endl;
            if (GetDelay(packet).GetMicroSeconds() < bgtTag.GetBudget())
            {
                *os << "1" << std::endl;
            }
            else
            {
                *os << "0" << std::endl;
            }

            // *os << GetDelay (packet).GetMicroSeconds ()/1000.0 << std::endl;
        }
        // get delay
        m_totalRx += packet->GetSize();
        if (InetSocketAddress::IsMatchingType(from))
        {
            NS_LOG_INFO("At time " << Simulator::Now().As(Time::S) << " packet sink received "
                                   << packet->GetSize() << " bytes from "
                                   << InetSocketAddress::ConvertFrom(from).GetIpv4() << " port "
                                   << InetSocketAddress::ConvertFrom(from).GetPort() << " total Rx "
                                   << m_totalRx << " bytes");
        }
        else if (Inet6SocketAddress::IsMatchingType(from))
        {
            NS_LOG_INFO("At time " << Simulator::Now().As(Time::S) << " packet sink received "
                                   << packet->GetSize() << " bytes from "
                                   << Inet6SocketAddress::ConvertFrom(from).GetIpv6() << " port "
                                   << Inet6SocketAddress::ConvertFrom(from).GetPort()
                                   << " total Rx " << m_totalRx << " bytes");
        }
        socket->GetSockName(localAddress);
        m_rxTrace(packet, from);
        m_rxTraceWithAddresses(packet, from, localAddress);

        if (m_enableSeqTsSizeHeader)
        {
            PacketReceived(packet, from, localAddress);
        }
    }

    // std::cout<<"total"<<i<<"packets"<< std::endl;
}

void
Sink::PacketReceived(const Ptr<Packet>& p, const Address& from, const Address& localAddress)
{
    SeqTsSizeHeader header;
    Ptr<Packet> buffer;

    auto itBuffer = m_buffer.find(from);
    if (itBuffer == m_buffer.end())
    {
        itBuffer = m_buffer.insert(std::make_pair(from, Create<Packet>(0))).first;
    }

    buffer = itBuffer->second;
    buffer->AddAtEnd(p);
    buffer->PeekHeader(header);

    NS_ABORT_IF(header.GetSize() == 0);

    while (buffer->GetSize() >= header.GetSize())
    {
        NS_LOG_DEBUG("Removing packet of size " << header.GetSize() << " from buffer of size "
                                                << buffer->GetSize());
        Ptr<Packet> complete = buffer->CreateFragment(0, static_cast<uint32_t>(header.GetSize()));
        buffer->RemoveAtStart(static_cast<uint32_t>(header.GetSize()));

        complete->RemoveHeader(header);

        m_rxTraceWithSeqTsSize(complete, from, localAddress, header);

        if (buffer->GetSize() > header.GetSerializedSize())
        {
            buffer->PeekHeader(header);
        }
        else
        {
            break;
        }
    }
}

void
Sink::HandlePeerClose(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
}

void
Sink::HandlePeerError(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
}

void
Sink::HandleAccept(Ptr<Socket> s, const Address& from)
{
    NS_LOG_FUNCTION(this << s << from);
    s->SetRecvCallback(MakeCallback(&Sink::HandleRead, this));
    m_socketList.push_back(s);
}

} // namespace ns3
