# NetPacketIteratorGetCount method


## Description



The **NetPacketIteratorGetCount** method gets the count of packets that a client driver owns in the packet ring.

## Parameters

### Iterator

A pointer to a [**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md) structure.

## Returns

Returns the number of packets between this packet iterator's current **Index** and **EndIndex - 1** inclusive. For example, if the iterator's **Index** is **1** and its **End** index is **5**, the client driver owns four packets: **1**, **2**, **3**, and **4**.

## Remarks

## See Also

[Net ring iterator README](readme.md)

[**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md)
