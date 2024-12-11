/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2007 University of Washington
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
 * Author: Tom Henderson (tomhend@u.washington.edu)
 */

#include "route-manager.h"

#include "route-manager-impl.h"

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/simulation-singleton.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RouteManager");

// ---------------------------------------------------------------------------
//
// DGRRoutingManager Implementation
//
// ---------------------------------------------------------------------------

void
RouteManager::DeleteRoutes()
{
    NS_LOG_FUNCTION_NOARGS();
    SimulationSingleton<RouteManagerImpl>::Get()->DeleteRoutes();
}

void
RouteManager::BuildRoutingDatabase(void)
{
    NS_LOG_FUNCTION_NOARGS();
    SimulationSingleton<RouteManagerImpl>::Get()->BuildRoutingDatabase();
}

void
RouteManager::InitializeRoutes(void)
{
    NS_LOG_FUNCTION_NOARGS();
    SimulationSingleton<RouteManagerImpl>::Get()->InitializeRoutes();
}

uint32_t
RouteManager::AllocateRouterId(void)
{
    NS_LOG_FUNCTION_NOARGS();
    static uint32_t routerId = 0;
    return routerId++;
}

} // namespace ns3
