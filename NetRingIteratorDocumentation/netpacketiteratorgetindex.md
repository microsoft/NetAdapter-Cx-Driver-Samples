# NetPacketIteratorGetIndex method


## Description



The **NetPacketIteratorGetIndex** method gets the current **Index** of a packet iterator in the packet ring.

## Syntax

```C++
UINT32 NetPacketIteratorGetIndex(
  NET_RING_PACKET_ITERATOR const *Iterator
);
```

## Parameters

### Iterator

A pointer to a [**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md) structure.

## Returns

Returns the packet iterator's current **Index**.

## Requirements

| | |
| --- | --- |
| Header | netringiterator.h |
| IRQL | Any level as long as target memory is resident |

## See Also

[Net ring iterator README](README.md)

[**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md)
