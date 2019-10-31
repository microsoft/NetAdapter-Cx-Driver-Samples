# NET_RING_ITERATOR structure

## Description



A **NET_RING_ITERATOR** is a small structure that contains references to the post and drain indices of a [**NET_RING**](https://docs.microsoft.com/windows-hardware/drivers/ddi/ring/ns-ring-_net_ring) to which it belongs.

## Syntax

```C++
typedef struct _NET_RING_ITERATOR
{

    NET_RING_COLLECTION const *
        Rings;

    UINT32* const
        IndexToSet;

    UINT32
        Index;

    UINT32 const
        End;

} NET_RING_ITERATOR;
```

## Fields

### Rings

A [**NET_RING_COLLECTION**](https://docs.microsoft.com/windows-hardware/drivers/ddi/ringcollection/ns-ringcollection-_net_ring_collection) structure that describes the [**NET_RING**](https://docs.microsoft.com/windows-hardware/drivers/ddi/ring/ns-ring-_net_ring) structure to which this net ring iterator belongs.
 
### IndexToSet

The beginning of the range of elements that are covered by this iterator. This index does not move until after the client driver has finished processing elements in the ring and sets the iterator, which advances **IndexToSet** to **Index**.

Client drivers call [**NetPacketIteratorSet**](netpacketiteratorset.md) to set a packet iterator, or [**NetFragmentIteratorSet**](netfragmentiteratorset.md) to set a fragment iterator.
 
### Index

The iterator's current index in the [**NET_RING**](https://docs.microsoft.com/windows-hardware/drivers/ddi/ring/ns-ring-_net_ring). This index is advanced as the client driver processes elements in the ring.

Client drivers typically call [**NetPacketIteratorAdvance**](netpacketiteratoradvance.md) to advance a packet iterator, or [**NetFragmentIteratorAdvance**](netfragmentiteratoradvance.md) to advance a fragment iterator.
 
### End
 
The end of the range of elements in the net ring that is covered by this iterator.

## Remarks

NetAdapterCx client drivers should not use the **NET_RING_ITERATOR** structure directly. Instead, they should either use a [**NET_RING_PACKET_ITERATOR*](net_ring_packet_iterator.md) for a packet ring or a [**NET_RING_FRAGMENT_ITERATOR**](net_ring_fragment_iterator.md) for a fragment ring.

## Requirements

| | |
| --- | --- |
| Header | netringiterator.h |

## See Also

[Net ring iterator README](README.md)

[**NET_RING_PACKET_ITERATOR*](net_ring_packet_iterator.md)

[**NET_RING_FRAGMENT_ITERATOR**](net_ring_fragment_iterator.md)
