# NetRingGetPostPackets method


## Description



The **NetRingGetPostPackets** method gets a packet iterator for the post subsection of a packet ring.

## Syntax

```C++
NET_RING_PACKET_ITERATOR NetRingGetPostPackets(
  NET_RING_COLLECTION const *Rings
);
```

## Parameters

### Rings

A pointer to the [**NET_RING_COLLECTION**](https://docs.microsoft.com/windows-hardware/drivers/ddi/ringcollection/ns-ringcollection-_net_ring_collection) struture that describes this packet queue's net rings.

## Returns

Returns a [**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md) that begins at the packet ring's **NextIndex** and ends at the packet ring's **EndIndex**. In other words, the iterator covers the packet ring's current post subsection.

## Remarks

Client drivers typically call this method to begin the process of posting packets to hardware. Drivers later complete this process by calling [**NetPacketIteratorSet**](netpacketiteratorset.md).

For a code example of posting packets, see the [Net ring iterator README](README.md).

## Requirements

| | |
| --- | --- |
| Header | netringiterator.h |
| IRQL | Any level as long as target memory is resident |

## See Also

[Net ring iterator README](README.md)

[**NET_RING_COLLECTION**](https://docs.microsoft.com/windows-hardware/drivers/ddi/ringcollection/ns-ringcollection-_net_ring_collection)

[**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md)

[**NetPacketIteratorSet**](netpacketiteratorset.md)
