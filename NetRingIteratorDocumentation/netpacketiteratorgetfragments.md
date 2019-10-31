# NetPacketIteratorGetFragments method


## Description



The **NetPacketIteratorGetFragments** method gets the fragments associated with a packet.

## Syntax

```C++
NET_RING_FRAGMENT_ITERATOR NetPacketIteratorGetFragments(
  NET_RING_PACKET_ITERATOR const *Iterator
);
```

## Parameters

### Iterator

A pointer to a [**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md) structure.

## Returns

Returns a [**NET_RING_FRAGMENT_ITERATOR**](net_ring_fragment_iterator.md) that begins at the first fragment belonging to this packet and ends at the last fragment belonging to this packet.

## Remarks

Client drivers typically call NetPacketIteratorGetFragments to begin the process of posting fragments to hardware for transmitting (Tx).

For a code example of posting fragments to hardware for Tx, see the [Net ring iterator README](README.md).

## Requirements

| | |
| --- | --- |
| Header | netringiterator.h |
| IRQL | Any level as long as target memory is resident |

## See Also

[Net ring iterator README](README.md)

[**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md)

[**NET_RING_FRAGMENT_ITERATOR**](net_ring_fragment_iterator.md)
