# NetRingGetAllFragments method


## Description



The **NetRingGetAllFragments** method gets a fragment iterator for the entire range that a client driver owns in a fragment ring.

## Syntax

```C++
NET_RING_FRAGMENT_ITERATOR NetRingGetAllFragments(
  NET_RING_COLLECTION const *Rings
);
```

## Parameters

### Rings

A pointer to the [**NET_RING_COLLECTION**](https://docs.microsoft.com/windows-hardware/drivers/ddi/ringcollection/ns-ringcollection-_net_ring_collection) struture that describes the packet queue's net rings.

## Returns

Returns a [**NET_RING_FRAGMENT_ITERATOR**](net_ring_fragment_iterator.md) that begins at the fragment ring's **BeginIndex** and ends at the fragment ring's **EndIndex**. In other words, the iterator covers both the ring's post subsection and its drain subsection, or all fragments in the ring that the driver currently owns.

## Remarks

Client drivers typically call **NetRingGetAllFragments** to begin performing operations on all fragments that they own in a fragment ring. This might include processing a batch of receives that span all available fragments in the ring, or draining the ring during data path cancellation.

For a code example of using this method, see the [Net ring iterator README](README.md).

## Requirements

| | |
| --- | --- |
| Header | netringiterator.h |
| IRQL | Any level as long as target memory is resident |

## See Also

[Net ring iterator README](README.md)

[**NET_RING_COLLECTION**](https://docs.microsoft.com/windows-hardware/drivers/ddi/ringcollection/ns-ringcollection-_net_ring_collection)

[**NET_RING_FRAGMENT_ITERATOR**](net_ring_fragment_iterator.md)
