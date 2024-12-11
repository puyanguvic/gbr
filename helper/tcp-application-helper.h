/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef TCP_APPLICATION_HELPER_H
#define TCP_APPLICATION_HELPER_H

#include "ns3/address.h"
#include "ns3/application-container.h"
#include "ns3/attribute.h"
#include "ns3/net-device.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"

#include <stdint.h>
#include <string>

namespace ns3
{

/**
 * \ingroup bulksend
 * \brief A helper to make it easier to instantiate an ns3::BulkSendApplication
 * on a set of nodes.
 */
class TcpAppHelper
{
  public:
    /**
     * Create an TcpAppHelper to make it easier to work with BulkSendApplications
     *
     * \param protocol the name of the protocol to use to send traffic
     *        by the applications. This string identifies the socket
     *        factory type used to create sockets for the applications.
     *        A typical value would be ns3::UdpSocketFactory.
     * \param address the address of the remote node to send traffic
     *        to.
     */
    TcpAppHelper(std::string protocol, Address address);

    /**
     * Helper function used to set the underlying application attributes,
     * _not_ the socket attributes.
     *
     * \param name the name of the application attribute to set
     * \param value the value of the application attribute to set
     */
    void SetAttribute(std::string name, const AttributeValue& value);

    /**
     * Install an ns3::BulkSendApplication on each node of the input container
     * configured with all the attributes set with SetAttribute.
     *
     * \param c NodeContainer of the set of nodes on which an BulkSendApplication
     * will be installed.
     * \returns Container of Ptr to the applications installed.
     */
    ApplicationContainer Install(NodeContainer c) const;

    /**
     * Install an ns3::BulkSendApplication on the node configured with all the
     * attributes set with SetAttribute.
     *
     * \param node The node on which an BulkSendApplication will be installed.
     * \returns Container of Ptr to the applications installed.
     */
    ApplicationContainer Install(Ptr<Node> node) const;

    /**
     * Install an ns3::BulkSendApplication on the node configured with all the
     * attributes set with SetAttribute.
     *
     * \param nodeName The node on which an BulkSendApplication will be installed.
     * \returns Container of Ptr to the applications installed.
     */
    ApplicationContainer Install(std::string nodeName) const;

  private:
    /**
     * Install an ns3::BulkSendApplication on the node configured with all the
     * attributes set with SetAttribute.
     *
     * \param node The node on which an BulkSendApplication will be installed.
     * \returns Ptr to the application installed.
     */
    Ptr<Application> InstallPriv(Ptr<Node> node) const;

    ObjectFactory m_factory; //!< Object factory.
};

} // namespace ns3

#endif /* ON_OFF_HELPER_H */
