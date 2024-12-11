/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef VALUE_DENSE_QUEUE_DISC_H
#define VALUE_DENSE_QUEUE_DISC_H

#include "packet-headers.h"

#include "ns3/object.h"
#include "ns3/packet-filter.h"
#include "ns3/queue-disc.h"

namespace ns3
{

class ValueDenseQueueDisc : public QueueDisc
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);
    /**
     * \brief ValueDenseQueueDisc constructor
     */
    ValueDenseQueueDisc();
    /**
     * \brief ValueDenseQueueDisc Destructor
     */
    ~ValueDenseQueueDisc();

    // Reasons for dropping packets
    static constexpr const char* LIMIT_EXCEEDED_DROP =
        "Queue disc limit exceeded"; //!< Packet dropped due to queue disc limit exceeded

    // Get current queue state
    uint32_t GetQueueStatus();
    uint32_t GetQueueDelay();
    bool CheckConfig(void) override;

  private:
    uint32_t m_fastWeight;
    uint32_t m_normalWeight;
    uint32_t m_slowWeight;
    uint32_t m_currentFastWeight;
    uint32_t m_currentNormalWeight;
    uint32_t m_currentSlowWeight;

    bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    Ptr<QueueDiscItem> DoDequeue(void) override;
    Ptr<const QueueDiscItem> DoPeek(void) override;
    // bool CheckConfig (void) override;
    void InitializeParams(void) override;

    uint32_t EnqueueClassify(Ptr<QueueDiscItem> item);
};

} // namespace ns3

#endif /* DGRv2_QUEUE_DISC_H */
