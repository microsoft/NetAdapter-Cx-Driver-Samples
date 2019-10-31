# NetPacketIteratorGetPacket method


## Description



The **NetPacketIteratorGetPacket** method gets the [**NET_PACKET**](https://docs.microsoft.com/windows-hardware/drivers/ddi/packet/ns-packet-_net_packet) structure at a packet iterator's current **Index** in the packet ring.

## Parameters

### Iterator

A pointer to a [**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md) structure.

## Returns

Returns a pointer to the [**NET_PACKET**](https://docs.microsoft.com/windows-hardware/drivers/ddi/packet/ns-packet-_net_packet) structure at the packet iterator's **Index**.

## See Also

[Net ring iterator README](readme.md)

[**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md)

[**NET_PACKET**](https://docs.microsoft.com/windows-hardware/drivers/ddi/packet/ns-packet-_net_packet)
