# NetPacketIteratorGetCount method


## Description



The **NetPacketIteratorGetCount** method gets the count of packets that a client driver owns in the packet ring.

## Syntax

```C++
UINT32 NetPacketIteratorGetCount(
  NET_RING_PACKET_ITERATOR const *Iterator
);
```

## Parameters

### Iterator

A pointer to a [**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md) structure.

## Returns

Returns the number of packets between this packet iterator's current **Index** and **EndIndex - 1** inclusive. For example, if the iterator's **Index** is **1** and its **End** index is **5**, the client driver owns four packets: **1**, **2**, **3**, and **4**.

## Remarks

## Requirements

| | |
| --- | --- |
| Header | netringiterator.h |
| IRQL | Any level as long as target memory is resident |

## See Also

[Net ring iterator README](README.md)

[**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md)
