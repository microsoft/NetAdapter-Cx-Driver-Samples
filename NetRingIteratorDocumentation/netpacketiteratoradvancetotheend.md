# NetPacketIteratorAdvanceToTheEnd method


## Description



The **NetPacketIteratorAdvanceToTheEnd** method advances the current **Index** of a packet iterator to its **End** index.

## Syntax

```C++
void NetPacketIteratorAdvanceToTheEnd(
  NET_RING_PACKET_ITERATOR *Iterator
);
```

## Parameters

### Iterator

A pointer to a [**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md) structure.

## Returns

None.

## Remarks

After calling **NetPacketIteratorAdvanceToTheEnd**, the packet iterator's current **Index** advances to the iterator's **End** index. Therefore, the packets between the old value of iterator's **Index** and the iterator's **End - 1** inclusive are transferred to the OS.

Client drivers typically call **NetPacketIteratorAdvanceToTheEnd** to cancel all packets in the ring or perform other operations that drain all the ring's packets.

For a code example of using this method, see [Net ring iterator README](README.md).

## Requirements

| | |
| --- | --- |
| Header | netringiterator.h |
| IRQL | Any level as long as target memory is resident |

## See Also

[Net ring iterator README](README.md)

[**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md)
