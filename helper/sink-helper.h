/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef SINK_HELPER_H
#define SINK_HELPER_H

#include "ns3/application-container.h"
#include "ns3/ipv4-address.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"

namespace ns3
{

/**
 * \ingroup packetsink
 * \brief A helper to make it easier to instantiate an ns3::PacketSinkApplication
 * on a set of nodes.
 */
class SinkHelper
{
  public:
    /**
     * Create a PacketSinkHelper to make it easier to work with PacketSinkApplications
     *
     * \param protocol the name of the protocol to use to receive traffic
     *        This string identifies the socket factory type used to create
     *        sockets for the applications.  A typical value would be
     *        ns3::TcpSocketFactory.
     * \param address the address of the sink,
     *
     */
    SinkHelper(std::string protocol, Address address);

    ~SinkHelper();

    /**
     * Helper function used to set the underlying application attributes.
     *
     * \param name the name of the application attribute to set
     * \param value the value of the application attribute to set
     */
    void SetAttribute(std::string name, const AttributeValue& value);

    /**
     * Install an ns3::PacketSinkApplication on each node of the input container
     * configured with all the attributes set with SetAttribute.
     *
     * \param c NodeContainer of the set of nodes on which a PacketSinkApplication
     * will be installed.
     * \returns Container of Ptr to the applications installed.
     */
    ApplicationContainer Install(NodeContainer c) const;

    /**
     * Install an ns3::PacketSinkApplication on each node of the input container
     * configured with all the attributes set with SetAttribute.
     *
     * \param node The node on which a PacketSinkApplication will be installed.
     * \returns Container of Ptr to the applications installed.
     */
    ApplicationContainer Install(Ptr<Node> node) const;

    /**
     * Install an ns3::PacketSinkApplication on each node of the input container
     * configured with all the attributes set with SetAttribute.
     *
     * \param nodeName The name of the node on which a PacketSinkApplication will be installed.
     * \returns Container of Ptr to the applications installed.
     */
    ApplicationContainer Install(std::string nodeName) const;

  private:
    /**
     * Install an ns3::PacketSink on the node configured with all the
     * attributes set with SetAttribute.
     *
     * \param node The node on which an PacketSink will be installed.
     * \returns Ptr to the application installed.
     */
    Ptr<Application> InstallPriv(Ptr<Node> node) const;
    ObjectFactory m_factory; //!< Object factory.
};

} // namespace ns3

#endif /* SINK_HELPER_H */
