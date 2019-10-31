# NetRingGetDrainFragments method


## Description



The **NetRingGetDrainFragments** method gets a fragment iterator for the current drain subsection of a fragment ring.

## Syntax

```C++
NET_RING_FRAGMENT_ITERATOR NetRingGetDrainFragments(
  NET_RING_COLLECTION const *Rings
);
```

## Parameters

### Rings

A pointer to the [**NET_RING_COLLECTION**](https://docs.microsoft.com/windows-hardware/drivers/ddi/ringcollection/ns-ringcollection-_net_ring_collection) struture that describes the packet queue's net rings.

## Returns

Returns a [**NET_RING_FRAGMENT_ITERATOR**](net_ring_fragment_iterator.md) that begins at the fragment ring's **BeginIndex** and ends at the fragment ring's **NextIndex**. In other words, the iterator covers the fragment ring's current drain section.

## Remarks

Client drivers call **NetRingGetDrainFragments** to begin the process of draining receive (Rx) fragments from the ring to the OS.

For a code example of draining fragments from the ring back to the OS, see the [Net ring iterator README](README.md).

## Requirements

| | |
| --- | --- |
| Header | netringiterator.h |
| IRQL | Any level as long as target memory is resident |

## See Also

[Net ring iterator README](README.md)

[**NET_RING_COLLECTION**](https://docs.microsoft.com/windows-hardware/drivers/ddi/ringcollection/ns-ringcollection-_net_ring_collection)

[**NET_RING_FRAGMENT_ITERATOR**](net_ring_fragment_iterator.md)

[**NetFragmentIteratorSet**](netfragmentiteratorset.md)
