/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef ROUTING_TABLE_ENTRY_H
#define ROUTING_TABLE_ENTRY_H

#include "ns3/ipv4-address.h"

#include <list>
#include <ostream>
#include <vector>

namespace ns3
{

/**
 * \ingroup ipv4Routing
 *
 * A record of an IPv4 routing table entry for Ipv4GlobalRouting and
 * Ipv4StaticRouting.  This is not a reference counted object.
 */
class RoutingTableEntry
{
  public:
    /**
     * \brief This constructor does nothing
     */
    RoutingTableEntry();
    /**
     * \brief Copy Constructor
     * \param route The route to copy
     */
    RoutingTableEntry(const RoutingTableEntry& route);
    /**
     * \brief Copy Constructor
     * \param route The route to copy
     */
    RoutingTableEntry(const RoutingTableEntry* route);
    /**
     * \return True if this route is a host route (mask of all ones); false otherwise
     */
    bool IsHost(void) const;
    /**
     * \return True if this route is not a host route (mask is not all ones); false otherwise
     *
     * This method is implemented as !IsHost ().
     */
    bool IsNetwork(void) const;
    /**
     * \return True if this route is a default route; false otherwise
     */
    bool IsDefault(void) const;
    /**
     * \return True if this route is a gateway route; false otherwise
     */
    bool IsGateway(void) const;
    /**
     * \return address of the gateway stored in this entry
     */
    Ipv4Address GetGateway(void) const;
    /**
     * \return The IPv4 address of the destination of this route
     */
    Ipv4Address GetDest(void) const;
    /**
     * \return The IPv4 network number of the destination of this route
     */
    Ipv4Address GetDestNetwork(void) const;
    /**
     * \return The IPv4 network mask of the destination of this route
     */
    Ipv4Mask GetDestNetworkMask(void) const;
    /**
     * \return The Ipv4 interface number used for sending outgoing packets
     */
    uint32_t GetInterface(void) const;
    /**
     * \return the distance
     */
    uint32_t GetDistance(void) const;
    /**
     * \return The Ipv4 interface number used for sending outgoing packets in the next hop
     */
    uint32_t GetNextInterface(void) const;
    /**
     * \return An Ipv4RoutingTableEntry object corresponding to the input parameters.
     * \param dest Ipv4Address of the destination
     * \param nextHop Ipv4Address of the next hop
     * \param interface Outgoing interface
     */
    static RoutingTableEntry CreateHostRouteTo(Ipv4Address dest,
                                               Ipv4Address nextHop,
                                               uint32_t interface);
    /**
     * \return An Ipv4RoutingTableEntry object corresponding to the input parameters.
     * \param dest Ipv4Address of the destination
     * \param interface Outgoing interface
     */
    static RoutingTableEntry CreateHostRouteTo(Ipv4Address dest, uint32_t interface);

    /**
     * \author Pu Yang
     * \return An Ipv4RoutingTableEntry object corresponding to the input parameters.
     * \param dest Ipv4Address of the destination
     * \param nextHop the Ipv4Address the nextHop
     * \param interface Outgoing interface
     * \param  nextInterface Outgoing interface in next hop
     * \param distance The distance between root and destination
     */
    static RoutingTableEntry CreateHostRouteTo(Ipv4Address dest,
                                               Ipv4Address nextHop,
                                               uint32_t interface,
                                               uint32_t nextInterface,
                                               uint32_t distance);
    /**
     * \return An Ipv4RoutingTableEntry object corresponding to the input parameters.
     * \param network Ipv4Address of the destination network
     * \param networkMask Ipv4Mask of the destination network mask
     * \param nextHop Ipv4Address of the next hop
     * \param interface Outgoing interface
     */
    static RoutingTableEntry CreateNetworkRouteTo(Ipv4Address network,
                                                  Ipv4Mask networkMask,
                                                  Ipv4Address nextHop,
                                                  uint32_t interface);
    /**
     * \return An Ipv4RoutingTableEntry object corresponding to the input parameters.
     * \param network Ipv4Address of the destination network
     * \param networkMask Ipv4Mask of the destination network mask
     * \param interface Outgoing interface
     */
    static RoutingTableEntry CreateNetworkRouteTo(Ipv4Address network,
                                                  Ipv4Mask networkMask,
                                                  uint32_t interface);
    /**
     * \return An Ipv4RoutingTableEntry object corresponding to the input
     * parameters.  This route is distinguished; it will match any
     * destination for which a more specific route does not exist.
     * \param nextHop Ipv4Address of the next hop
     * \param interface Outgoing interface
     */
    static RoutingTableEntry CreateDefaultRoute(Ipv4Address nextHop, uint32_t interface);

  private:
    /**
     * \brief Constructor.
     * \param network network address
     * \param mask network mask
     * \param gateway the gateway
     * \param interface the interface index
     */
    RoutingTableEntry(Ipv4Address network, Ipv4Mask mask, Ipv4Address gateway, uint32_t interface);
    /**
     * \brief Constructor.
     * \param dest destination address
     * \param mask network mask
     * \param interface the interface index
     */
    RoutingTableEntry(Ipv4Address dest, Ipv4Mask mask, uint32_t interface);
    /**
     * \brief Constructor.
     * \param dest destination address
     * \param gateway the gateway
     * \param interface the interface index
     */
    RoutingTableEntry(Ipv4Address dest, Ipv4Address gateway, uint32_t interface);
    /**
     * \brief Constructor.
     * \param dest destination address
     * \param interface the interface index
     */
    RoutingTableEntry(Ipv4Address dest, uint32_t interface);

    /**
     * \brief Constructor.
     * \param dest destination address
     * \param gateway gateway address
     * \param interface the interface index
     * \param nextInterface the interface index in next hop
     * \param distance the distance between root and destination
     */
    RoutingTableEntry(Ipv4Address dest,
                      Ipv4Address gateway,
                      uint32_t interface,
                      uint32_t nextInterface,
                      uint32_t distance);

    Ipv4Address m_dest;         //!< destination address
    Ipv4Mask m_destNetworkMask; //!< destination network mask
    Ipv4Address m_gateway;      //!< gateway
    uint32_t m_interface;       //!< output interface
    uint32_t m_nextInterface;   //!< output interface in next hop
    uint32_t m_distance;        //!< the distance between root and destination
};

/**
 * \brief Stream insertion operator.
 *
 * \param os the reference to the output stream
 * \param route the Ipv4 routing table entry
 * \returns the reference to the output stream
 */
std::ostream& operator<<(std::ostream& os, const RoutingTableEntry& route);

/**
 * \brief Equality operator.
 *
 * \param a lhs
 * \param b rhs
 * \returns true if operands are equal, false otherwise
 */
bool operator==(const RoutingTableEntry a, const RoutingTableEntry b);

} // namespace ns3

#endif /* ROUTING_TABLE_ENTRY_H */
