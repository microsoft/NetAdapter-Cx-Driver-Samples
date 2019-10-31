# NetFragmentIteratorGetFragment method


## Description



The **NetFragmentIteratorGetFragment** method gets the [**NET_FRAGMENT**](https://docs.microsoft.com/windows-hardware/drivers/ddi/fragment/ns-fragment-_net_fragment) structure at a fragment iterator's current **Index** in the fragment ring.

## Parameters

### Iterator

A pointer to a [**NET_RING_FRAGMENT_ITERATOR**](net_ring_fragment_iterator.md) structure.

## Returns

Returns a pointer to the [**NET_FRAGMENT**](https://docs.microsoft.com/windows-hardware/drivers/ddi/fragment/ns-fragment-_net_fragment) structure at the fragment iterator's **Index**.

## See Also

[Net ring iterator README](readme.md)

[**NET_FRAGMENT**](https://docs.microsoft.com/windows-hardware/drivers/ddi/fragment/ns-fragment-_net_fragment)

[**NET_RING_FRAGMENT_ITERATOR**](net_ring_fragment_iterator.md)
