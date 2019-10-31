# NetPacketIteratorAdvance method


## Description



The **NetPacketIteratorAdvance** method advances the **Index** of a packet iterator by one.

## Parameters

### Iterator

A pointer to a [**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md) structure.

## Returns

None.

## Remarks

If the packet iterator's current **Index** is at the end of the packet ring, this method automatically handles wrapping around to the beginning of the ring.

## See Also

[Net ring iterator README](readme.md)

[**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md)
