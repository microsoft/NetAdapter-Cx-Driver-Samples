# NetRingGetAllPackets method


## Description



The **NetRingGetAllPackets** method gets a packet iterator for the entire range that a client driver owns in a packet ring.

## Syntax

```C++
NET_RING_PACKET_ITERATOR NetRingGetAllPackets(
  NET_RING_COLLECTION const *Rings
);
```

## Parameters

### Rings

A pointer to the [**NET_RING_COLLECTION**](https://docs.microsoft.com/windows-hardware/drivers/ddi/ringcollection/ns-ringcollection-_net_ring_collection) struture that describes this packet queue's net rings.

## Returns

Returns a [**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md) that begins at the packet ring's **BeginIndex** and ends at the packet ring's **EndIndex**. In other words, the iterator covers all packets in the ring that the driver currently owns, including the post subsection and the drain subsection.

## Remarks

Client drivers typically call **NetRingGetAllPackets** to begin performing operations on all packets that they own in a packet ring. This might include processing a batch of receives that span all available packets in the ring, or draining the ring during data path cancellation.

For a code example of using this method, see the [Net ring iterator README](README.md).

## Requirements

| | |
| --- | --- |
| Header | netringiterator.h |
| IRQL | Any level as long as target memory is resident |

## See Also

[Net ring iterator README](README.md)

[**NET_RING_COLLECTION**](https://docs.microsoft.com/windows-hardware/drivers/ddi/ringcollection/ns-ringcollection-_net_ring_collection)

[**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md)
