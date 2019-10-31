# NetPacketIteratorSet method


## Description



The **NetPacketIteratorSet** method completes a client driver's post or drain operation on the packet ring.

## Syntax

```C++
void NetPacketIteratorSet(
  NET_RING_PACKET_ITERATOR const *Iterator
);
```

## Parameters

### Iterator

A pointer to a [**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md) structure.

## Returns

None.

## Remarks

**NetPacketIteratorSet** sets the packet iterator's **IndexToSet** to its **Index**, which indicates to the OS that the client driver has finished processing the packets from **IndexToSet** to **Index - 1** inclusive. Client drivers call this method to finish posting packets to hardware, or to finish draining packets to the OS.

For a code example of draining packets back to the OS, see [Net ring iterator README](README.md).

## Requirements

| | |
| --- | --- |
| Header | netringiterator.h |
| IRQL | Any level as long as target memory is resident |

## See Also

[Net ring iterator README](README.md)

[**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md)
