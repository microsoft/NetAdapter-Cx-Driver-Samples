# NetPacketIteratorAdvance method


## Description



The **NetPacketIteratorAdvance** method advances the **Index** of a packet iterator by one.

## Syntax

```C++
void NetPacketIteratorAdvance(
  NET_RING_PACKET_ITERATOR *Iterator
);
```

## Parameters

### Iterator

A pointer to a [**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md) structure.

## Returns

None.

## Remarks

If the packet iterator's current **Index** is at the end of the packet ring, this method automatically handles wrapping around to the beginning of the ring.

## Requirements

| | |
| --- | --- |
| Header | netringiterator.h |
| IRQL | Any level as long as target memory is resident |

## See Also

[Net ring iterator README](README.md)

[**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md)
