// -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*-
//
// Copyright (c) 2008 University of Washington
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation;
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "ipv4-gbr-routing.h"

#include "dgrv2-queue-disc.h"
#include "packet-headers.h"
#include "packet-tags.h"
#include "route-manager.h"

#include "ns3/boolean.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/ipv4-packet-info-tag.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/log.h"
#include "ns3/loopback-net-device.h"
#include "ns3/names.h"
#include "ns3/network-module.h"
#include "ns3/node.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/point-to-point-module.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/traffic-control-module.h"
#include "ns3/udp-header.h"
#include "ns3/udp-socket-factory.h"

#include <iomanip>
#include <string>
#include <vector>

#define DGR_PORT 666
#define DGR_BROAD_CAST "224.0.0.13"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("GBR");

NS_OBJECT_ENSURE_REGISTERED(GBR);

TypeId
GBR::GetTypeId(void)
{
    static TypeId tid =
        TypeId("ns3::GBR")
            .SetParent<Ipv4RoutingProtocol>()
            .SetGroupName("DGR-routing")
            .AddConstructor<GBR>()
            .AddAttribute("RandomEcmpRouting",
                          "Set to true if packets are randomly routed among ECMP; set to false for "
                          "using only one route consistently",
                          BooleanValue(false),
                          MakeBooleanAccessor(&GBR::m_randomEcmpRouting),
                          MakeBooleanChecker())
            .AddAttribute("RespondToInterfaceEvents",
                          "Set to true if you want to dynamically recompute the global routes upon "
                          "Interface notification events (up/down, or add/remove address)",
                          BooleanValue(false),
                          MakeBooleanAccessor(&GBR::m_respondToInterfaceEvents),
                          MakeBooleanChecker())
            .AddAttribute("SamplePeriod",
                          "Time between two Unsolicited Neighbor State Updates.",
                          TimeValue(MilliSeconds(10)),
                          MakeTimeAccessor(&GBR::m_unsolicitedUpdate),
                          MakeTimeChecker())
            .AddAttribute("RouteSelectMode",
                          "Routing Select Mode",
                          EnumValue(NONE),
                          MakeEnumAccessor(&GBR::m_routeSelectMode),
                          MakeEnumChecker(NONE, "ECMP", KSHORT, "KSHORT", DGR, "DGR", DDR, "DDR"));
    return tid;
}

GBR::GBR()
    : m_randomEcmpRouting(false),
      m_respondToInterfaceEvents(false),
      m_nsdb()
{
    NS_LOG_FUNCTION(this);
    m_rand = CreateObject<UniformRandomVariable>();
}

GBR::~GBR()
{
    NS_LOG_FUNCTION(this);
}

void
GBR::AddHostRouteTo(Ipv4Address dest, Ipv4Address nextHop, uint32_t interface)
{
    NS_LOG_FUNCTION(this << dest << nextHop << interface);
    RoutingTableEntry* route = new RoutingTableEntry();
    *route = RoutingTableEntry::CreateHostRouteTo(dest, nextHop, interface);
    m_hostRoutes.push_back(route);
}

void
GBR::AddHostRouteTo(Ipv4Address dest, uint32_t interface)
{
    NS_LOG_FUNCTION(this << dest << interface);
    RoutingTableEntry* route = new RoutingTableEntry();
    *route = RoutingTableEntry::CreateHostRouteTo(dest, interface);
    m_hostRoutes.push_back(route);
}

/**
 * \author Pu Yang
 * \brief Add a host route to the global routing table with the distance
 * between root and destination
 * \param dest The Ipv4Address destination for this route.
 * \param nextHop The next hop Ipv4Address
 * \param interface The network interface index used to send packets to the
 *  destination
 * \param distance The distance between root and destination
 */
void
GBR::AddHostRouteTo(Ipv4Address dest,
                    Ipv4Address nextHop,
                    uint32_t interface,
                    uint32_t nextInterface,
                    uint32_t distance)
{
    NS_LOG_FUNCTION(this << dest << nextHop << interface << nextInterface << distance);
    RoutingTableEntry* route = new RoutingTableEntry();
    *route =
        RoutingTableEntry::CreateHostRouteTo(dest, nextHop, interface, nextInterface, distance);
    m_hostRoutes.push_back(route);
}

void
GBR::AddNetworkRouteTo(Ipv4Address network,
                       Ipv4Mask networkMask,
                       Ipv4Address nextHop,
                       uint32_t interface)
{
    NS_LOG_FUNCTION(this << network << networkMask << nextHop << interface);
    RoutingTableEntry* route = new RoutingTableEntry();
    *route = RoutingTableEntry::CreateNetworkRouteTo(network, networkMask, nextHop, interface);
    m_networkRoutes.push_back(route);
}

void
GBR::AddNetworkRouteTo(Ipv4Address network, Ipv4Mask networkMask, uint32_t interface)
{
    NS_LOG_FUNCTION(this << network << networkMask << interface);
    RoutingTableEntry* route = new RoutingTableEntry();
    *route = RoutingTableEntry::CreateNetworkRouteTo(network, networkMask, interface);
    m_networkRoutes.push_back(route);
}

void
GBR::AddASExternalRouteTo(Ipv4Address network,
                          Ipv4Mask networkMask,
                          Ipv4Address nextHop,
                          uint32_t interface)
{
    NS_LOG_FUNCTION(this << network << networkMask << nextHop << interface);
    RoutingTableEntry* route = new RoutingTableEntry();
    *route = RoutingTableEntry::CreateNetworkRouteTo(network, networkMask, nextHop, interface);
    m_ASexternalRoutes.push_back(route);
}

Ptr<Ipv4Route>
GBR::LookupECMPRoute(Ipv4Address dest, Ptr<NetDevice> oif)
{
    /**
     * Get the shortest path in the routing table
     */
    NS_LOG_FUNCTION(this << dest << oif);
    NS_LOG_LOGIC("Looking for route for destination " << dest);

    Ptr<Ipv4Route> rtentry = 0;
    // store all available routes that bring packets to their destination
    typedef std::vector<RoutingTableEntry*> RouteVec_t;
    RouteVec_t allRoutes;

    NS_LOG_LOGIC("Number of m_hostRoutes = " << m_hostRoutes.size());
    for (HostRoutesCI i = m_hostRoutes.begin(); i != m_hostRoutes.end(); i++)
    {
        NS_ASSERT((*i)->IsHost());
        if ((*i)->GetDest() == dest)
        {
            if (oif)
            {
                if (oif != m_ipv4->GetNetDevice((*i)->GetInterface()))
                {
                    NS_LOG_LOGIC("Not on requested interface, skipping");
                    continue;
                }
            }
            allRoutes.push_back(*i);
            NS_LOG_LOGIC(allRoutes.size() << "Found DGR host route" << *i);
        }
    }
    if (allRoutes.size() > 0) // if route(s) is found
    {
        uint32_t routRef = 0;
        uint32_t shortestDist = allRoutes.at(0)->GetDistance();
        for (uint32_t i = 0; i < allRoutes.size(); i++)
        {
            if (allRoutes.at(i)->GetDistance() < shortestDist)
            {
                routRef = i;
                shortestDist = allRoutes.at(i)->GetDistance();
            }
        }
        RoutingTableEntry* route = allRoutes.at(routRef);

        // create a Ipv4Route object from the selected routing table entry
        rtentry = Create<Ipv4Route>();
        rtentry->SetDestination(route->GetDest());
        /// \todo handle multi-address case
        rtentry->SetSource(m_ipv4->GetAddress(route->GetInterface(), 0).GetLocal());
        rtentry->SetGateway(route->GetGateway());
        uint32_t interfaceIdx = route->GetInterface();
        rtentry->SetOutputDevice(m_ipv4->GetNetDevice(interfaceIdx));
        return rtentry;
    }
    else
    {
        return 0;
    }
}

Ptr<Ipv4Route>
GBR::LookupDDRRoute(Ipv4Address dest, Ptr<Packet> p, Ptr<const NetDevice> idev)
{
    // std::cout <<"DGR routing" << std::endl;
    BudgetTag bgtTag;
    TimestampTag timeTag;
    p->PeekPacketTag(bgtTag);
    p->PeekPacketTag(timeTag);
    // avoid loop
    DistTag distTag;
    uint32_t dist = UINT32_MAX;
    dist -= 1;
    if (p->PeekPacketTag(distTag))
    {
        dist = distTag.GetDistance();
    }

    // budget in microseconds
    uint32_t bgt;
    if (bgtTag.GetBudget() + timeTag.GetTimestamp().GetMicroSeconds() <
        Simulator::Now().GetMicroSeconds())
    {
        bgt = 0;
    }
    else
    {
        bgt = (bgtTag.GetBudget() + timeTag.GetTimestamp().GetMicroSeconds() -
               Simulator::Now().GetMicroSeconds());
    }
    NS_LOG_FUNCTION(this << dest << idev);
    NS_LOG_LOGIC("Looking for route for destination " << dest);
    Ptr<Ipv4Route> rtentry = 0;
    // store all available routes that bring packets to their destination
    typedef std::vector<RoutingTableEntry*> RouteVec_t;
    // typedef std::vector<RoutingTableEntry *>::const_iterator RouteVecCI_t;
    RouteVec_t allRoutes;

    NS_LOG_LOGIC("Number of m_hostRoutes = " << m_hostRoutes.size());
    for (HostRoutesCI i = m_hostRoutes.begin(); i != m_hostRoutes.end(); i++)
    {
        NS_ASSERT((*i)->IsHost());
        if ((*i)->GetDest() == dest)
        {
            if (idev)
            {
                if (idev == m_ipv4->GetNetDevice((*i)->GetInterface()))
                {
                    NS_LOG_LOGIC("Not on requested interface, skipping");
                    continue;
                }
            }

            // if interface is down, continue
            if (!m_ipv4->IsUp((*i)->GetInterface()))
                continue;

            // get the local queue delay in microsecond
            Ptr<NetDevice> dev_local = m_ipv4->GetNetDevice((*i)->GetInterface());
            // get the queue disc on the device
            Ptr<QueueDisc> disc = m_ipv4->GetObject<Node>()
                                      ->GetObject<TrafficControlLayer>()
                                      ->GetRootQueueDiscOnDevice(dev_local);
            Ptr<DGRv2QueueDisc> dvq = DynamicCast<DGRv2QueueDisc>(disc);
            // uint32_t status_local = dvq->GetQueueStatus ();
            // uint32_t delay_local = status_local * 2000;
            uint32_t delay_local = dvq->GetQueueDelay();

            // Get the neighbor queue status in microsecond
            uint32_t delay_neighbor = 0;
            if ((*i)->GetNextInterface() != 0xffffffff)
            {
                uint32_t iface = (*i)->GetInterface();
                uint32_t niface = (*i)->GetNextInterface();
                NeighborStatusEntry* entry = m_nsdb.GetNeighborStatusEntry(iface);
                StatusUnit* su = entry->GetStatusUnit(niface);
                delay_neighbor = su->GetEstimateDelayDDR();
                // std::cout << "Neighbor delay: " << delay_neighbor << std::endl;
            }
            // in microsecond
            uint32_t estimate_delay =
                ((*i)->GetDistance() + 1) * 1000 + delay_local + delay_neighbor;

            if (estimate_delay > bgt)
            {
                NS_LOG_LOGIC("Too far to the destination, skipping");
                continue;
            }

            if ((*i)->GetDistance() > dist)
            {
                NS_LOG_LOGIC("Loop avoidance, skipping");
                continue;
            }

            allRoutes.push_back(*i);
            NS_LOG_LOGIC(allRoutes.size()
                         << "Found DGR host route" << *i << " with Cost: " << (*i)->GetDistance());
        }
    }
    if (allRoutes.size() > 0) // if route(s) is found
    {
        // random select
        uint32_t selectIndex = 0;
        uint32_t shortestDist = allRoutes.at(0)->GetDistance();
        for (uint32_t i = 0; i < allRoutes.size(); i++)
        {
            if (allRoutes.at(i)->GetDistance() < shortestDist)
            {
                selectIndex = i;
                shortestDist = allRoutes.at(i)->GetDistance();
            }
        }

        RoutingTableEntry* route = allRoutes.at(selectIndex);
        uint32_t interfaceIdx = route->GetInterface();

        rtentry = Create<Ipv4Route>();
        rtentry->SetDestination(route->GetDest());
        /// \todo handle multi-address case
        rtentry->SetSource(m_ipv4->GetAddress(interfaceIdx, 0).GetLocal());
        rtentry->SetGateway(route->GetGateway());
        rtentry->SetOutputDevice(m_ipv4->GetNetDevice(interfaceIdx));

        distTag.SetDistance(route->GetDistance());
        p->ReplacePacketTag(distTag);
        return rtentry;
    }
    else
    {
        return LookupECMPRoute(dest);
    }
}

Ptr<Ipv4Route>
GBR::LookupDGRRoute(Ipv4Address dest, Ptr<Packet> p, Ptr<const NetDevice> idev)
{
    // std::cout <<"DGR routing" << std::endl;
    BudgetTag bgtTag;
    TimestampTag timeTag;
    p->PeekPacketTag(bgtTag);
    p->PeekPacketTag(timeTag);
    // avoid loop
    DistTag distTag;
    uint32_t dist = UINT32_MAX;
    dist -= 1;
    if (p->PeekPacketTag(distTag))
    {
        dist = distTag.GetDistance();
    }

    // budget in microseconds
    uint32_t bgt;
    if (bgtTag.GetBudget() + timeTag.GetTimestamp().GetMicroSeconds() <
        Simulator::Now().GetMicroSeconds())
    {
        bgt = 0;
    }
    else
    {
        bgt = (bgtTag.GetBudget() + timeTag.GetTimestamp().GetMicroSeconds() -
               Simulator::Now().GetMicroSeconds());
    }
    NS_LOG_FUNCTION(this << dest << idev);
    NS_LOG_LOGIC("Looking for route for destination " << dest);
    Ptr<Ipv4Route> rtentry = 0;
    // store all available routes that bring packets to their destination
    typedef std::vector<RoutingTableEntry*> RouteVec_t;
    // typedef std::vector<RoutingTableEntry *>::const_iterator RouteVecCI_t;
    RouteVec_t allRoutes;

    NS_LOG_LOGIC("Number of m_hostRoutes = " << m_hostRoutes.size());
    for (HostRoutesCI i = m_hostRoutes.begin(); i != m_hostRoutes.end(); i++)
    {
        NS_ASSERT((*i)->IsHost());
        if ((*i)->GetDest() == dest)
        {
            if (idev)
            {
                if (idev == m_ipv4->GetNetDevice((*i)->GetInterface()))
                {
                    NS_LOG_LOGIC("Not on requested interface, skipping");
                    continue;
                }
            }

            // if interface is down, continue
            if (!m_ipv4->IsUp((*i)->GetInterface()))
                continue;

            // get the local queue delay in microsecond
            Ptr<NetDevice> dev_local = m_ipv4->GetNetDevice((*i)->GetInterface());
            // get the queue disc on the device
            Ptr<QueueDisc> disc = m_ipv4->GetObject<Node>()
                                      ->GetObject<TrafficControlLayer>()
                                      ->GetRootQueueDiscOnDevice(dev_local);
            Ptr<DGRv2QueueDisc> dvq = DynamicCast<DGRv2QueueDisc>(disc);
            // uint32_t status_local = dvq->GetQueueStatus ();
            // uint32_t delay_local = status_local * 2000;
            uint32_t delay_local = dvq->GetQueueDelay();

            // Get the neighbor queue status in microsecond
            uint32_t delay_neighbor = 0;
            if ((*i)->GetNextInterface() != 0xffffffff)
            {
                uint32_t iface = (*i)->GetInterface();
                uint32_t niface = (*i)->GetNextInterface();
                NeighborStatusEntry* entry = m_nsdb.GetNeighborStatusEntry(iface);
                StatusUnit* su = entry->GetStatusUnit(niface);
                delay_neighbor = su->GetEstimateDelayDGR();
            }
            // in microsecond
            uint32_t estimate_delay = (*i)->GetDistance() * 1000 + delay_local + delay_neighbor;

            if (estimate_delay > bgt)
            {
                NS_LOG_LOGIC("Too far to the destination, skipping");
                continue;
            }

            if ((*i)->GetDistance() > dist)
            {
                NS_LOG_LOGIC("Loop avoidance, skipping");
                continue;
            }

            allRoutes.push_back(*i);
            NS_LOG_LOGIC(allRoutes.size()
                         << "Found DGR host route" << *i << " with Cost: " << (*i)->GetDistance());
        }
    }
    if (allRoutes.size() > 0) // if route(s) is found
    {
        // random select
        uint32_t selectIndex = m_rand->GetInteger(0, allRoutes.size() - 1);

        RoutingTableEntry* route = allRoutes.at(selectIndex);
        uint32_t interfaceIdx = route->GetInterface();

        rtentry = Create<Ipv4Route>();
        rtentry->SetDestination(route->GetDest());
        /// \todo handle multi-address case
        rtentry->SetSource(m_ipv4->GetAddress(interfaceIdx, 0).GetLocal());
        rtentry->SetGateway(route->GetGateway());
        rtentry->SetOutputDevice(m_ipv4->GetNetDevice(interfaceIdx));

        distTag.SetDistance(route->GetDistance());
        p->ReplacePacketTag(distTag);
        return rtentry;
    }
    else
    {
        return 0;
    }
}

Ptr<Ipv4Route>
GBR::LookupKShortRoute(Ipv4Address dest, Ptr<Packet> p, Ptr<const NetDevice> idev)
{
    // avoid loop
    DistTag distTag;
    uint32_t dist = UINT32_MAX;
    dist -= 1;
    if (p->PeekPacketTag(distTag))
    {
        dist = distTag.GetDistance();
    }

    NS_LOG_FUNCTION(this << dest << idev);
    NS_LOG_LOGIC("Looking for route for destination " << dest);
    Ptr<Ipv4Route> rtentry = 0;
    // store all available routes that bring packets to their destination
    typedef std::vector<RoutingTableEntry*> RouteVec_t;
    // typedef std::vector<RoutingTableEntry *>::const_iterator RouteVecCI_t;
    RouteVec_t allRoutes;

    NS_LOG_LOGIC("Number of m_hostRoutes = " << m_hostRoutes.size());
    for (HostRoutesCI i = m_hostRoutes.begin(); i != m_hostRoutes.end(); i++)
    {
        NS_ASSERT((*i)->IsHost());
        if ((*i)->GetDest() == dest)
        {
            if (idev)
            {
                if (idev == m_ipv4->GetNetDevice((*i)->GetInterface()))
                {
                    NS_LOG_LOGIC("Not on requested interface, skipping");
                    continue;
                }
            }
            if ((*i)->GetDistance() > dist)
            {
                NS_LOG_LOGIC("Loop avoidance, skipping");
                continue;
            }

            allRoutes.push_back(*i);
            NS_LOG_LOGIC(allRoutes.size()
                         << "Found route" << *i << " with Cost: " << (*i)->GetDistance());
        }
    }
    if (allRoutes.size() > 0) // if route(s) is found
    {
        // random select
        uint32_t selectIndex = m_rand->GetInteger(0, allRoutes.size() - 1);
        RoutingTableEntry* route = allRoutes.at(selectIndex);
        uint32_t interfaceIdx = route->GetInterface();

        rtentry = Create<Ipv4Route>();
        rtentry->SetDestination(route->GetDest());
        /// \todo handle multi-address case
        rtentry->SetSource(m_ipv4->GetAddress(interfaceIdx, 0).GetLocal());
        rtentry->SetGateway(route->GetGateway());
        rtentry->SetOutputDevice(m_ipv4->GetNetDevice(interfaceIdx));

        distTag.SetDistance(route->GetDistance());
        p->ReplacePacketTag(distTag);
        return rtentry;
    }
    else
    {
        return 0;
    }
}

// Ptr<Ipv4Route>
// GBR::LookupDDRRoute (Ipv4Address dest, Ptr<Packet> p, Ptr<const NetDevice> idev)
// {
//   // std::cout << "DDR routing" << std::endl;
//   BudgetTag bgtTag;
//   TimestampTag timeTag;
//   p->PeekPacketTag (bgtTag);
//   p->PeekPacketTag (timeTag);
//   // avoid loop
//   DistTag distTag;
//   uint32_t dist = UINT32_MAX;
//   dist -= 1;
//   if (p->PeekPacketTag (distTag))
//     {
//       dist = distTag.GetDistance ();
//     }

//   // budget in microseconds
//   uint32_t bgt;
//   if (bgtTag.GetBudget ()*1000 + timeTag.GetTimestamp ().GetMicroSeconds () < Simulator::Now
//   ().GetMicroSeconds ())
//     {
//       bgt = 0;
//     }
//   else
//     bgt = (bgtTag.GetBudget ()*1000 + timeTag.GetTimestamp ().GetMicroSeconds () - Simulator::Now
//     ().GetMicroSeconds ());

//   NS_LOG_FUNCTION (this << dest << idev);
//   NS_LOG_LOGIC ("Looking for route for destination " << dest);
//   Ptr<Ipv4Route> rtentry = 0;
//   // store all available routes that bring packets to their destination
//   typedef std::vector<RoutingTableEntry*> RouteVec_t;
//   // typedef std::vector<RoutingTableEntry *>::const_iterator RouteVecCI_t;
//   RouteVec_t allRoutes;

//   NS_LOG_LOGIC ("Number of m_hostRoutes = " << m_hostRoutes.size ());
//   for (HostRoutesCI i = m_hostRoutes.begin ();
//        i != m_hostRoutes.end ();
//        i++)
//     {
//       NS_ASSERT ((*i)->IsHost ());
//       if ((*i)->GetDest () == dest)
//         {
//           if (idev)
//             {
//               if (idev == m_ipv4->GetNetDevice ((*i)->GetInterface ()))
//                 {
//                   NS_LOG_LOGIC ("Not on requested interface, skipping");
//                   continue;
//                 }
//             }

//           // if interface is down, continue
//           if (!m_ipv4->IsUp ((*i)->GetInterface ())) continue;

//           // // get the local queue delay in microsecond
//           // Ptr <NetDevice> dev_local = m_ipv4->GetNetDevice ((*i)->GetInterface ());
//           // //get the queue disc on the device
//           // Ptr<QueueDisc> disc = m_ipv4->GetObject<Node> ()->GetObject<TrafficControlLayer>
//           ()->GetRootQueueDiscOnDevice (dev_local);
//           // Ptr<DGRv2QueueDisc> dvq = DynamicCast <DGRv2QueueDisc> (disc);
//           // uint32_t status_local = dvq->GetQueueStatus ();
//           // uint32_t delay_local = dvq->GetQueueDelay ();

//           // // Get the neighbor queue status in microsecond
//           // uint32_t delay_neighbor = 0;
//           // if ((*i)->GetNextInterface () != 0xffffffff)
//           //   {
//           //     uint32_t iface = (*i)->GetInterface ();
//           //     uint32_t niface = (*i)->GetNextInterface ();
//           //     NeighborStatusEntry *entry = m_nsdb.GetNeighborStatusEntry (iface);
//           //     StatusUnit *su = entry->GetStatusUnit (niface);
//           //     delay_neighbor = su->GetEstimateDelayDDR ();
//           //   }
//           // // in microsecond
//           // uint32_t estimate_delay = ((*i)->GetDistance () + 1) * 1000 + delay_local +
//           delay_neighbor;

//           // get the local queue delay in microsecond
//           Ptr <NetDevice> dev_local = m_ipv4->GetNetDevice ((*i)->GetInterface ());
//           //get the queue disc on the device
//           Ptr<QueueDisc> disc = m_ipv4->GetObject<Node> ()->GetObject<TrafficControlLayer>
//           ()->GetRootQueueDiscOnDevice (dev_local); Ptr<DGRv2QueueDisc> dvq = DynamicCast
//           <DGRv2QueueDisc> (disc); uint32_t status_local = dvq->GetQueueStatus (); uint32_t
//           delay_local = status_local * 2000;

//           // Get the neighbor queue status in microsecond
//           uint32_t delay_neighbor = 0;
//           if ((*i)->GetNextInterface () != 0xffffffff)
//             {
//               uint32_t iface = (*i)->GetInterface ();
//               uint32_t niface = (*i)->GetNextInterface ();
//               NeighborStatusEntry *entry = m_nsdb.GetNeighborStatusEntry (iface);
//               StatusUnit *su = entry->GetStatusUnit (niface);
//               delay_neighbor = su->GetEstimateDelayDGR ();
//             }
//           // in microsecond
//           uint32_t estimate_delay = ((*i)->GetDistance () + 1) * 1000 + delay_local +
//           delay_neighbor;

//           if (estimate_delay > bgt)
//             {
//               NS_LOG_LOGIC ("Too far to the destination, skipping");
//               continue;
//             }

//           if ((*i)->GetDistance () > dist)
//             {
//               NS_LOG_LOGIC ("Loop avoidance, skipping");
//               continue;
//             }

//           allRoutes.push_back (*i);
//           NS_LOG_LOGIC (allRoutes.size () << "Found DGR host route" << *i << " with Cost: " <<
//           (*i)->GetDistance ());
//         }
//     }
//   if (allRoutes.size () > 0 ) // if route(s) is found
//     {
//       // random select
//       uint32_t selectIndex = m_rand->GetInteger (0, allRoutes.size ()-1);

//       RoutingTableEntry* route = allRoutes.at (selectIndex);
//       uint32_t interfaceIdx = route->GetInterface ();

//       rtentry = Create<Ipv4Route> ();
//       rtentry->SetDestination (route->GetDest ());
//       /// \todo handle multi-address case
//       rtentry->SetSource (m_ipv4->GetAddress (interfaceIdx, 0).GetLocal ());
//       rtentry->SetGateway (route->GetGateway ());
//       rtentry->SetOutputDevice (m_ipv4->GetNetDevice (interfaceIdx));

//       distTag.SetDistance (route->GetDistance ());
//       p->ReplacePacketTag (distTag);
//       return rtentry;
//     }
//   else
//     {
//       return 0;
//       // return LookupECMPRoute (dest);
//     }
// }

uint32_t
GBR::GetNRoutes(void) const
{
    NS_LOG_FUNCTION(this);
    uint32_t n = 0;
    n += m_hostRoutes.size();
    n += m_networkRoutes.size();
    n += m_ASexternalRoutes.size();
    return n;
}

RoutingTableEntry*
GBR::GetRoute(uint32_t index) const
{
    NS_LOG_FUNCTION(this << index);
    if (index < m_hostRoutes.size())
    {
        uint32_t tmp = 0;
        for (HostRoutesCI i = m_hostRoutes.begin(); i != m_hostRoutes.end(); i++)
        {
            if (tmp == index)
            {
                return *i;
            }
            tmp++;
        }
    }
    index -= m_hostRoutes.size();
    uint32_t tmp = 0;
    if (index < m_networkRoutes.size())
    {
        for (NetworkRoutesCI j = m_networkRoutes.begin(); j != m_networkRoutes.end(); j++)
        {
            if (tmp == index)
            {
                return *j;
            }
            tmp++;
        }
    }
    index -= m_networkRoutes.size();
    tmp = 0;
    for (ASExternalRoutesCI k = m_ASexternalRoutes.begin(); k != m_ASexternalRoutes.end(); k++)
    {
        if (tmp == index)
        {
            return *k;
        }
        tmp++;
    }
    NS_ASSERT(false);
    // quiet compiler.
    return 0;
}

void
GBR::RemoveRoute(uint32_t index)
{
    NS_LOG_FUNCTION(this << index);
    if (index < m_hostRoutes.size())
    {
        uint32_t tmp = 0;
        for (HostRoutesI i = m_hostRoutes.begin(); i != m_hostRoutes.end(); i++)
        {
            if (tmp == index)
            {
                NS_LOG_LOGIC("Removing route " << index << "; size = " << m_hostRoutes.size());
                delete *i;
                m_hostRoutes.erase(i);
                NS_LOG_LOGIC("Done removing host route "
                             << index << "; host route remaining size = " << m_hostRoutes.size());
                return;
            }
            tmp++;
        }
    }
    index -= m_hostRoutes.size();
    uint32_t tmp = 0;
    for (NetworkRoutesI j = m_networkRoutes.begin(); j != m_networkRoutes.end(); j++)
    {
        if (tmp == index)
        {
            NS_LOG_LOGIC("Removing route " << index << "; size = " << m_networkRoutes.size());
            delete *j;
            m_networkRoutes.erase(j);
            NS_LOG_LOGIC("Done removing network route "
                         << index << "; network route remaining size = " << m_networkRoutes.size());
            return;
        }
        tmp++;
    }
    index -= m_networkRoutes.size();
    tmp = 0;
    for (ASExternalRoutesI k = m_ASexternalRoutes.begin(); k != m_ASexternalRoutes.end(); k++)
    {
        if (tmp == index)
        {
            NS_LOG_LOGIC("Removing route " << index << "; size = " << m_ASexternalRoutes.size());
            delete *k;
            m_ASexternalRoutes.erase(k);
            NS_LOG_LOGIC("Done removing network route "
                         << index << "; network route remaining size = " << m_networkRoutes.size());
            return;
        }
        tmp++;
    }
    NS_ASSERT(false);
}

int64_t
GBR::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    m_rand->SetStream(stream);
    return 1;
}

void
GBR::DoDispose(void)
{
    NS_LOG_FUNCTION(this);
    // TODO: Realise memorys
    for (HostRoutesI i = m_hostRoutes.begin(); i != m_hostRoutes.end(); i = m_hostRoutes.erase(i))
    {
        delete (*i);
    }
    for (NetworkRoutesI j = m_networkRoutes.begin(); j != m_networkRoutes.end();
         j = m_networkRoutes.erase(j))
    {
        delete (*j);
    }
    for (ASExternalRoutesI l = m_ASexternalRoutes.begin(); l != m_ASexternalRoutes.end();
         l = m_ASexternalRoutes.erase(l))
    {
        delete (*l);
    }

    Ipv4RoutingProtocol::DoDispose();
}

void
GBR::DoInitialize()
{
    NS_LOG_FUNCTION(this);
    // bool addedGlobal = false;
    m_initialized = true;

    // To Check: An random value is needed to initialize the protocol?
    Time delay = m_unsolicitedUpdate;
    m_nextUnsolicitedUpdate = Simulator::Schedule(delay, &GBR::SendUnsolicitedUpdate, this);

    uint32_t nodeId = m_ipv4->GetNetDevice(1)->GetNode()->GetId();
    std::stringstream ss;
    ss << nodeId;
    std::string strNodeId = ss.str();

    // // std::string node = "Node " + (std::string)nodeId;
    // m_outStream = Create<OutputStreamWrapper> ("Node" + strNodeId + "queueStatusErr.txt",
    // std::ios::out);

    m_nextUnsolicitedUpdate = Simulator::Schedule(delay, &GBR::SendUnsolicitedUpdate, this);

    // Initialize the sockets for every netdevice
    for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); i++)
    {
        Ptr<LoopbackNetDevice> check = DynamicCast<LoopbackNetDevice>(m_ipv4->GetNetDevice(i));
        if (check)
        {
            continue;
        }

        bool activeInterface = false;
        if (m_interfaceExclusions.find(i) == m_interfaceExclusions.end())
        {
            activeInterface = true;
            m_ipv4->SetForwarding(i, true);
        }

        for (uint32_t j = 0; j < m_ipv4->GetNAddresses(i); j++)
        {
            Ipv4InterfaceAddress address = m_ipv4->GetAddress(i, j);
            NS_LOG_LOGIC("For interface: " << i << "the " << j << "st Address is " << address);
            // std::cout << "For interface: " << i << " the " << j << "st Address is ";

            if (address.GetScope() != Ipv4InterfaceAddress::HOST && activeInterface == true)
            {
                NS_LOG_LOGIC("DGR: add socket to " << address.GetLocal());
                TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
                Ptr<Node> theNode = m_ipv4->GetObject<Node>();
                Ptr<Socket> socket = Socket::CreateSocket(theNode, tid);

                InetSocketAddress local = InetSocketAddress(address.GetLocal(), DGR_PORT);
                socket->BindToNetDevice(m_ipv4->GetNetDevice(i));
                int ret = socket->Bind(local);
                NS_ASSERT_MSG(ret == 0, "Bind unsuccessful");

                socket->SetRecvCallback(MakeCallback(&GBR::Receive, this));
                socket->SetIpRecvTtl(true);
                socket->SetRecvPktInfo(true);

                m_unicastSocketList[socket] = i;
            }
        }
    }

    if (!m_multicastRecvSocket)
    {
        NS_LOG_LOGIC("DGR: adding receiving socket");
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        Ptr<Node> theNode = m_ipv4->GetObject<Node>();
        m_multicastRecvSocket = Socket::CreateSocket(theNode, tid);
        InetSocketAddress local = InetSocketAddress(DGR_BROAD_CAST, DGR_PORT);
        m_multicastRecvSocket->Bind(local);
        m_multicastRecvSocket->SetRecvCallback(MakeCallback(&GBR::Receive, this));
        m_multicastRecvSocket->SetIpRecvTtl(true);
        m_multicastRecvSocket->SetRecvPktInfo(true);
    }

    // if (addedGlobal)
    //   {
    //     Time delay = Seconds (m_rand->GetValue (m_minTriggeredUpdateDelay.GetSeconds (),
    //                                             m_maxTriggeredUpdateDelay.GetSeconds ()));
    //     m_nextTriggeredUpdate = Simulator::Schedule (delay,
    //     &GBR::DoSendNeighborStatusUpdate, this, false);
    //   }

    // delay = Seconds (m_rand->GetValue (0.01, m_startupDelay.GetSeconds ()));
    // m_nextTriggeredUpdate = Simulator::Schedule (delay,
    // &GBR::SendNeighborStatusRequest, this);

    Ipv4RoutingProtocol::DoInitialize();
}

// Formatted like output of "route -n" command
void
GBR::PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
    NS_LOG_FUNCTION(this << stream);
    std::ostream* os = stream->GetStream();
    // Copy the current ostream state
    std::ios oldState(nullptr);
    oldState.copyfmt(*os);

    *os << "Node: " << m_ipv4->GetObject<Node>()->GetId() << ", Time: " << Now().As(unit)
        << ", Local time: " << m_ipv4->GetObject<Node>()->GetLocalTime().As(unit) << ", GBR table"
        << std::endl;

    if (GetNRoutes() > 0)
    {
        *os << "  Destination     Gateway    Flags   Metric  Iface   NextIface" << std::endl;
        for (uint32_t j = 0; j < GetNRoutes(); j++)
        {
            std::ostringstream dest, gw, mask, flags, metric;
            RoutingTableEntry route = GetRoute(j);
            dest << route.GetDest();
            *os << std::setw(13) << dest.str();
            gw << route.GetGateway();
            *os << std::setw(13) << gw.str();
            flags << "U";
            if (route.IsHost())
            {
                flags << "H";
            }
            else if (route.IsGateway())
            {
                flags << "G";
            }
            *os << std::setiosflags(std::ios::left) << std::setw(6) << flags.str();
            metric << route.GetDistance();
            if (route.GetDistance() == 0xffffffff)
            {
                *os << std::setw(9) << "-";
            }
            else
            {
                *os << std::setw(9) << metric.str();
            }

            if (Names::FindName(m_ipv4->GetNetDevice(route.GetInterface())) != "")
            {
                *os << Names::FindName(m_ipv4->GetNetDevice(route.GetInterface()));
            }
            else
            {
                *os << std::setw(7) << route.GetInterface();
                if (route.GetNextInterface() != 0xffffffff)
                {
                    *os << std::setw(8) << route.GetNextInterface();
                }
                else
                {
                    *os << std::setw(8) << "-";
                }
            }
            *os << std::endl;
        }
    }
    *os << std::endl;
    (*os).copyfmt(oldState);
}

Ptr<Ipv4Route>
GBR::RouteOutput(Ptr<Packet> p,
                 const Ipv4Header& header,
                 Ptr<NetDevice> oif,
                 Socket::SocketErrno& sockerr)
{
    // std::cout << "at Node: " << m_ipv4->GetNetDevice (0)->GetNode ()->GetId () << "RouteOutput"
    // << std::endl;
    NS_LOG_FUNCTION(this << p << &header << oif << &sockerr);
    //
    // First, see if this is a multicast packet we have a route for.  If we
    // have a route, then send the packet down each of the specified interfaces.
    //
    if (header.GetDestination().IsMulticast())
    {
        NS_LOG_LOGIC("Multicast destination-- returning false");
        return 0; // Let other routing protocols try to handle this
    }

    //
    // See if this is a Delay-Guarenteed packet we have a route for.
    //
    NS_LOG_LOGIC("Delay-Guarenteed destination- looking up");
    Ptr<Ipv4Route> rtentry;
    BudgetTag budgetTag;
    if (!p)
    {
        rtentry = LookupECMPRoute(header.GetDestination(), oif);
    }
    else if (p->PeekPacketTag(budgetTag))
    {
        switch (m_routeSelectMode)
        {
        case NONE:
            rtentry = LookupECMPRoute(header.GetDestination(), oif);
            break;
        case KSHORT:
            rtentry = LookupKShortRoute(header.GetDestination(), p, oif);
            break;
        case DGR:
            rtentry = LookupDGRRoute(header.GetDestination(), p, oif);
            break;
        case DDR:
            rtentry = LookupDDRRoute(header.GetDestination(), p, oif);
            break;
        default:
            rtentry = LookupECMPRoute(header.GetDestination(), oif);
        }
        // rtentry = LookupDGRRoute (header.GetDestination (), p, oif);
    }
    else
    {
        rtentry = LookupECMPRoute(header.GetDestination(), oif);
    }

    if (rtentry)
    {
        sockerr = Socket::ERROR_NOTERROR;
    }
    else
    {
        sockerr = Socket::ERROR_NOROUTETOHOST;
    }
    return rtentry;
}

bool
GBR::RouteInput(Ptr<const Packet> p,
                const Ipv4Header& header,
                Ptr<const NetDevice> idev,
                UnicastForwardCallback ucb,
                MulticastForwardCallback mcb,
                LocalDeliverCallback lcb,
                ErrorCallback ecb)
{
    NS_LOG_FUNCTION(this << p << header << header.GetSource() << header.GetDestination() << idev
                         << &lcb << &ecb);
    // Check if input device supports IP
    NS_ASSERT(m_ipv4->GetInterfaceForDevice(idev) >= 0);
    uint32_t iif = m_ipv4->GetInterfaceForDevice(idev);

    if (m_ipv4->IsDestinationAddress(header.GetDestination(), iif))
    {
        if (!lcb.IsNull())
        {
            NS_LOG_LOGIC("Local delivery to " << header.GetDestination());
            // std::cout << "Local delivery to " << header.GetDestination () << std::endl;
            lcb(p, header, iif);
            return true;
        }
        else
        {
            // The local delivery callback is null.  This may be a multicast
            // or broadcast packet, so return false so that another
            // multicast routing protocol can handle it.  It should be possible
            // to extend this to explicitly check whether it is a unicast
            // packet, and invoke the error callback if so
            // std::cout << "ERROR !!!!" << std::endl;
            return false;
        }
    }
    // Check if input device supports IP forwarding
    if (m_ipv4->IsForwarding(iif) == false)
    {
        NS_LOG_LOGIC("Forwarding disabled for this interface");
        // std::cout << "RI: Forwarding disabled for this interface" << std::endl;
        ecb(p, header, Socket::ERROR_NOROUTETOHOST);
        return true;
    }
    // Next, try to find a route
    NS_LOG_LOGIC("Unicast destination- looking up global route");
    Ptr<Ipv4Route> rtentry;
    BudgetTag budgetTag;
    Ptr<Packet> p_copy;
    if (p->PeekPacketTag(budgetTag))
    {
        p_copy = p->Copy();
        switch (m_routeSelectMode)
        {
        case NONE:
            rtentry = LookupECMPRoute(header.GetDestination());
            break;
        case KSHORT:
            rtentry = LookupKShortRoute(header.GetDestination(), p_copy, idev);
            break;
        case DGR:
            rtentry = LookupDGRRoute(header.GetDestination(), p_copy, idev);
            break;
        case DDR:
            rtentry = LookupDDRRoute(header.GetDestination(), p_copy, idev);
            break;
        default:
            rtentry = LookupECMPRoute(header.GetDestination());
        }
        // rtentry = LookupDGRRoute (header.GetDestination (), p_copy, idev);
    }
    else
    {
        rtentry = LookupECMPRoute(header.GetDestination());
    }
    if (rtentry)
    {
        // std::cout << "find a way" << std::endl;
        if (p_copy)
        {
            p = p_copy->Copy();
        }
        NS_LOG_LOGIC("Found unicast destination- calling unicast callback");
        ucb(rtentry, p, header);
        return true;
    }
    else
    {
        NS_LOG_LOGIC("Did not find unicast destination- returning false");
        return false; // Let other routing protocols try to handle this
                      // route request.
    }
}

void
GBR::NotifyInterfaceUp(uint32_t i)
{
    NS_LOG_FUNCTION(this << i);
    if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0) // avoid startup events
    {
        RouteManager::DeleteDGRRoutes();
        RouteManager::BuildDGRRoutingDatabase();
        RouteManager::InitializeRoutes();
    }
}

void
GBR::NotifyInterfaceDown(uint32_t i)
{
    NS_LOG_FUNCTION(this << i);
    if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0) // avoid startup events
    {
        RouteManager::DeleteDGRRoutes();
        RouteManager::BuildDGRRoutingDatabase();
        RouteManager::InitializeRoutes();
    }
}

void
GBR::NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address)
{
    NS_LOG_FUNCTION(this << interface << address);
    if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0) // avoid startup events
    {
        RouteManager::DeleteDGRRoutes();
        RouteManager::BuildDGRRoutingDatabase();
        RouteManager::InitializeRoutes();
    }
}

void
GBR::NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address)
{
    NS_LOG_FUNCTION(this << interface << address);
    if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0) // avoid startup events
    {
        RouteManager::DeleteDGRRoutes();
        RouteManager::BuildDGRRoutingDatabase();
        RouteManager::InitializeRoutes();
    }
}

void
GBR::SetIpv4(Ptr<Ipv4> ipv4)
{
    NS_LOG_FUNCTION(this << ipv4);
    NS_ASSERT(!m_ipv4 && ipv4);
    m_ipv4 = ipv4;
}

void
GBR::Receive(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    Address sender;
    Ptr<Packet> packet = socket->RecvFrom(sender);
    InetSocketAddress senderAddr = InetSocketAddress::ConvertFrom(sender);
    NS_LOG_INFO("Received " << *packet << " from " << senderAddr.GetIpv4() << ":"
                            << senderAddr.GetPort());
    Ipv4Address senderAddress = senderAddr.GetIpv4();
    // uint32_t senderPort = senderAddr.GetPort ();

    if (socket == m_multicastRecvSocket)
    {
        NS_LOG_LOGIC("Received a packet from the multicast socket");
    }
    else
    {
        NS_LOG_LOGIC("Received a packet from one of the unicast sockets");
    }

    Ipv4PacketInfoTag interfaceInfo;
    if (!packet->RemovePacketTag(interfaceInfo))
    {
        NS_ABORT_MSG("No incoming interface on This message, aborting,");
    }
    uint32_t incomingIf = interfaceInfo.GetRecvIf();
    Ptr<Node> node = m_ipv4->GetObject<Node>();
    Ptr<NetDevice> dev = node->GetDevice(incomingIf);
    uint32_t ipInterfaceIndex = m_ipv4->GetInterfaceForDevice(dev);

    SocketIpTtlTag hoplimitTag;
    if (!packet->RemovePacketTag(hoplimitTag))
    {
        NS_ABORT_MSG("No incoming Hop count on message, aborting");
    }
    uint8_t hopLimit = hoplimitTag.GetTtl();

    int32_t interfaceForAddress = m_ipv4->GetInterfaceForAddress(senderAddress);
    if (interfaceForAddress != -1)
    {
        NS_LOG_LOGIC("Ignoring a packet sent by myself.");
        return;
    }

    DgrHeader hdr;
    packet->RemoveHeader(hdr);

    if (hdr.GetCommand() == DgrHeader::RESPONSE)
    {
        NS_LOG_LOGIC("The message is a Response from " << senderAddr.GetIpv4() << ":"
                                                       << senderAddr.GetPort());
        HandleResponses(hdr, senderAddress, ipInterfaceIndex, hopLimit);
    }
    // else if (hdr.GetCommand () == DgrHeader::REQUEST)
    //   {
    //     Todo: Handle request in the future
    //     NS_LOG_LOGIC ("This message is a Request from " << senderAddr.GetIpv4 () << ":"
    //                                                     << senderAddr.GetPort ());
    //     HandleRequests (hdr, senderAddress, senderPort, ipInterfaceIndex, hopLimit);
    //   }
    else
    {
        NS_LOG_LOGIC("Ignoring message with unknown command: " << int(hdr.GetCommand()));
    }
}

void
GBR::SendUnsolicitedUpdate()
{
    NS_LOG_FUNCTION(this);
    if (m_nextTriggeredUpdate.IsRunning())
    {
        m_nextTriggeredUpdate.Cancel();
    }
    DoSendNeighborStatusUpdate(true);
    // todo : update the delay, do we need some random in the delay
    Time delay = m_unsolicitedUpdate;
    m_nextUnsolicitedUpdate = Simulator::Schedule(delay, &GBR::SendUnsolicitedUpdate, this);
}

void
GBR::DoSendNeighborStatusUpdate(bool periodic)
{
    NS_LOG_FUNCTION(this << (periodic ? " periodic" : " triggered"));
    for (SocketListI iter = m_unicastSocketList.begin(); iter != m_unicastSocketList.end(); iter++)
    {
        uint32_t interface = iter->second;
        if (m_interfaceExclusions.find(interface) == m_interfaceExclusions.end())
        {
            uint16_t mtu = m_ipv4->GetMtu(interface);
            uint16_t maxNse = (mtu - Ipv4Header().GetSerializedSize() -
                               UdpHeader().GetSerializedSize() - DgrHeader().GetSerializedSize()) /
                              DgrNse().GetSerializedSize();
            Ptr<Packet> p = Create<Packet>();
            SocketIpTtlTag ttlTag;
            ttlTag.SetTtl(1);
            p->AddPacketTag(ttlTag);

            DgrHeader hdr;
            hdr.SetCommand(DgrHeader::RESPONSE);
            // Find the Status of every netdevice and put it in
            // TODO: Finish this function when finish the NSE definiation
            for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); i++)
            {
                if (!m_ipv4->IsUp(i))
                    continue;
                Ptr<LoopbackNetDevice> check =
                    DynamicCast<LoopbackNetDevice>(m_ipv4->GetNetDevice(i));
                if (check)
                {
                    continue;
                }
                // get the device
                Ptr<NetDevice> dev = m_ipv4->GetNetDevice(i);
                // get the queue disc on devic
                Ptr<QueueDisc> disc = m_ipv4->GetObject<Node>()
                                          ->GetObject<TrafficControlLayer>()
                                          ->GetRootQueueDiscOnDevice(dev);
                Ptr<DGRv2QueueDisc> qdisc = DynamicCast<DGRv2QueueDisc>(disc);
                DgrNse nse;
                nse.SetInterface(i);
                nse.SetState(qdisc->GetQueueStatus());
                hdr.AddNse(nse);
                if (hdr.GetNseNumber() == maxNse)
                {
                    p->AddHeader(hdr);
                    NS_LOG_DEBUG("SendTo: " << *p);
                    iter->first->SendTo(
                        p,
                        0,
                        InetSocketAddress(DGR_BROAD_CAST,
                                          DGR_PORT)); // Todo : defind the port for DGR routing
                    p->RemoveHeader(hdr);
                    hdr.ClearNses();
                }
            }
            if (hdr.GetNseNumber() > 0)
            {
                p->AddHeader(hdr);
                NS_LOG_DEBUG("SendTo: " << *p);
                iter->first->SendTo(
                    p,
                    0,
                    InetSocketAddress(DGR_BROAD_CAST, DGR_PORT)); // Todo: Defined the DGR port
            }
        }
    }
}

void
GBR::HandleResponses(DgrHeader hdr,
                     Ipv4Address senderAddress,
                     uint32_t incomingInterface,
                     uint8_t hopLimit)
{
    NS_LOG_FUNCTION(this << senderAddress << incomingInterface << int(hopLimit) << hdr);
    if (m_interfaceExclusions.find(incomingInterface) != m_interfaceExclusions.end())
    {
        NS_LOG_LOGIC(
            "Ignoring an update message from an excluded interface: " << incomingInterface);
        return;
    }
    if (hdr.GetSerializedSize() == 4)
    {
        NS_LOG_LOGIC("Ignoring an update message without neighbor state entries!");
    }

    NeighborStatusEntry* entry = m_nsdb.HandleNeighborStatusEntry(incomingInterface);
    if (entry == nullptr)
    {
        entry = new NeighborStatusEntry();
        m_nsdb.Insert(incomingInterface, entry);
    }

    std::list<DgrNse> nses = hdr.GetNseList();
    for (std::list<DgrNse>::iterator iter = nses.begin(); iter != nses.end(); iter++)
    {
        uint32_t n_iface = (*iter).GetInterface();
        int n_state = (*iter).GetState();
        StatusUnit* su = entry->GetStatusUnit(n_iface);
        if (su == nullptr)
        {
            su = new StatusUnit();
            entry->Insert(n_iface, su);
        }
        su->Update(n_state);
        // std::ostream* os = m_outStream->GetStream ();
        // *os << "Iface: " << n_iface << " Predict Err: " << abs(n_state - su->GetCurrentState ())
        // << std::endl; Print the su su->Print (std::cout);
    }
}

// void
// GBR::HandleRequests (DgrHeader hdr,
//                                 Ipv4Address senderAddress,
//                                 uint16_t senderPort,
//                                 uint32_t incomingInterface,
//                                 uint8_t hopLimit)
// {
//   NS_LOG_FUNCTION (this << senderAddress << senderPort
//                         << incomingInterface << hopLimit << hdr);
//   std::cout << ">> Handle requests of Address: ";
//   senderAddress.Print (std::cout);
//   std::cout << " hopLimit: " << hopLimit << std::endl;

//   if (m_interfaceExclusions.find (incomingInterface) == m_interfaceExclusions.end ())
//     {
//       // We use one of the sending sockets, as they're bound to the right interface
//       // and the local address might be used on different interfaces.
//       Ptr<Socket> sendingSocket;
//       for (SocketListI iter = m_unicastSocketList.begin ();
//             iter != m_unicastSocketList.end ();
//             iter ++)
//         {
//           if (iter->second == incomingInterface)
//             {
//               sendingSocket = iter->first;
//             }
//         }
//       NS_ASSERT_MSG (sendingSocket,
//                       "HandleRequest - Impossible to find a socket to send the reply");

//       uint16_t mtu = m_ipv4->GetMtu (incomingInterface);
//       uint16_t maxNse = (mtu - Ipv4Header().GetSerializedSize () -
//                           UdpHeader ().GetSerializedSize () - DgrHeader ().GetSerializedSize
//                           ())/DgrNse ().GetSerializedSize ();

//       Ptr<Packet> p = Create<Packet> ();
//       SocketIpTtlTag ttlTag;
//       ttlTag.SetTtl (1);
//       p->AddPacketTag (ttlTag);

//       // Serialize the current Device Status
//       DgrHeader hdr;
//       hdr.SetCommand (DgrHeader::RESPONSE);

//       for (uint32_t i = 0; i < m_ipv4->GetNInterfaces (); i ++)
//         {
//           if (!m_ipv4->IsUp (i)) continue;

//           // get the device
//           Ptr<NetDevice> dev = m_ipv4->GetNetDevice (i);

//           // get the queue disc on devic
//           Ptr<QueueDisc> disc = m_ipv4->GetObject<Node> ()->
//                                 GetObject<TrafficControlLayer> ()->
//                                 GetRootQueueDiscOnDevice (dev);
//           if (disc == nullptr)
//             {
//               NS_LOG_LOGIC ("loopback devices don't have queue disc!");
//               continue;
//             }

//           Ptr<DGRv2QueueDisc> dgr_disc = DynamicCast <DGRv2QueueDisc> (disc);

//           if (dgr_disc == nullptr)
//             {
//               NS_LOG_LOGIC ("No DGRv2QueueDisc find!");
//               continue;
//             }

//           DgrNse nse;
//           nse.SetInterface (i);
//           hdr.AddNse (nse);
//           if (hdr.GetNseNumber () == maxNse)
//             {
//               p->AddHeader (hdr);
//               NS_LOG_DEBUG ("SendTo: " << *p);
//               sendingSocket->SendTo (p, 0, InetSocketAddress (senderAddress, DGR_PORT)); // to
//               neighbor p->RemoveHeader (hdr); hdr.ClearNses ();
//             }
//         }
//         if (hdr.GetNseNumber () > 0)
//           {
//             p->AddHeader (hdr);
//             NS_LOG_DEBUG ("SendTo: " << *p);
//             sendingSocket->SendTo(p, 0, InetSocketAddress (senderAddress, DGR_PORT)); // Todo:
//             Defined the RIP port
//           }
//     }
// }

} // namespace ns3