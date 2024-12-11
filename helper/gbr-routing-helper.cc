/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#include "gbr-routing-helper.h"

#include "ns3/ipv4-gbr-routing.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/log.h"
#include "ns3/router-interface.h"
#include "ns3/traffic-control-layer.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DGRRoutingHelper");

GBRHelper::GBRHelper()
{
}

GBRHelper::GBRHelper(const GBRHelper& o)
{
}

GBRHelper*
GBRHelper::Copy(void) const
{
    return new GBRHelper(*this);
}

Ptr<Ipv4RoutingProtocol>
GBRHelper::Create(Ptr<Node> node) const
{
    NS_LOG_LOGIC("Adding DGRRouter interface to node " << node->GetId());
    // install DGRv2 Queue to netdevices

    // install DGR router to node.
    Ptr<Router> dgrRouter = CreateObject<Router>();
    node->AggregateObject(dgrRouter);

    NS_LOG_LOGIC("Adding DGRRouting Protocol to node " << node->GetId());
    Ptr<GBR> dgrRouting = CreateObject<GBR>();
    dgrRouter->SetRoutingProtocol(dgrRouting);

    return dgrRouting;
}

void
GBRHelper::PopulateRoutingTables(void)
{
    std::clock_t t;
    t = clock();
    RouteManager::BuildRoutingDatabase();
    RouteManager::InitializeRoutes();

    t = clock() - t;
    uint32_t time_init_ms = 1000000.0 * t / CLOCKS_PER_SEC;
    std::cout << "CPU time used for Init: " << time_init_ms << " ms\n";
}

void
GBRHelper::RecomputeRoutingTables(void)
{
    RouteManager::DeleteRoutes();
    RouteManager::BuildRoutingDatabase();
    RouteManager::InitializeRoutes();
}

QueueDiscContainer
GBRHelper::Install(Ptr<Node> node)
{
    NetDeviceContainer container;
    for (uint32_t i = 0; i < node->GetNDevices(); i++)
    {
        container.Add(node->GetDevice(i));
    }
    return Install(container);
}

QueueDiscContainer
GBRHelper::Install(NetDeviceContainer c)
{
    QueueDiscContainer container;
    for (NetDeviceContainer::Iterator i = c.Begin(); i != c.End(); ++i)
    {
        container.Add(Install(*i));
    }
    return container;
}

QueueDiscContainer
GBRHelper::Install(Ptr<NetDevice> d)
{
    QueueDiscContainer container;
    // A TrafficControlLayer object is aggregated by the InternetStackHelper, but check
    // anyway because a queue disc has no effect without a TrafficControlLayer object
    Ptr<TrafficControlLayer> tc = d->GetNode()->GetObject<TrafficControlLayer>();
    NS_ASSERT(tc);

    // Generate the DGRv2Qeueu Object
    ObjectFactory queueFactory;
    queueFactory.SetTypeId("ns3::ValueDenseQueueDisc");
    Ptr<ValueDenseQueueDisc> qdisc = queueFactory.Create<ValueDenseQueueDisc>();
    tc->SetRootQueueDiscOnDevice(d, qdisc);
    container.Add(qdisc);
    return container;
}

} // namespace ns3
