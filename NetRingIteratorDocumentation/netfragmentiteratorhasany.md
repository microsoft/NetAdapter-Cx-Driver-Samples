# NetFragmentIteratorHasAny method


## Description



The **NetFragmentIteratorHasAny** method determines whether a fragment iterator has any valid elements to process in the fragment ring.

## Syntax

```C++
BOOLEAN NetFragmentIteratorHasAny(
  NET_RING_FRAGMENT_ITERATOR const *Iterator
);
```

## Parameters

### Iterator

A pointer to a [**NET_RING_FRAGMENT_ITERATOR**](net_ring_fragment_iterator.md) structure.

## Returns

Returns **TRUE** if the iterator's **Index** does not equal its **End** index. In other words, the iterator has a fragment to process. Otherwise, returns **FALSE**.

## Remarks

Client drivers can call **NetFragmentIteratorHasAny** to test if the iterator has any valid elements to process. This method can be used to verify a fragment before processing it, or it can be used as a test condition for a loop when the driver is processing multiple fragments in a batch.

## Requirements

| | |
| --- | --- |
| Header | netringiterator.h |
| IRQL | Any level as long as target memory is resident |

## See Also

[Net ring iterator README](README.md)

[**NET_RING_FRAGMENT_ITERATOR**](net_ring_fragment_iterator.md)
