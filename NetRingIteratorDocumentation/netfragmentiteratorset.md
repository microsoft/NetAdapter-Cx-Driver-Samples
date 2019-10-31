# NetFragmentIteratorSet method


## Description



The **NetFragmentIteratorSet** method completes a client driver's post or drain operation on the fragment ring.

## Parameters

### Iterator

A pointer to a [**NET_RING_FRAGMENT_ITERATOR**](net_ring_fragment_iterator.md) structure.

## Returns

None.

## Remarks

**NetFragmentIteratorSet** sets the fragment iterator's **IndexToSet** to its **Index**, which indicates to the OS that the client driver has finished processing the fragments from **IndexToSet** to **Index - 1** inclusive. Client drivers call this method to finish posting fragments to hardware, or to finish draining fragments to the OS.

For a code example of draining fragments back to the OS, see [Net ring iterator README](readme.md).

## See Also

[Net ring iterator README](readme.md)

[**NET_RING_FRAGMENT_ITERATOR**](net_ring_fragment_iterator.md)
