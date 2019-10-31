# NetFragmentIteratorGetIndex method


## Description



The **NetFragmentIteratorGetIndex** method gets the current **Index** of a fragment iterator in the fragment ring.

## Syntax

```C++
UINT32 NetFragmentIteratorGetIndex(
  NET_RING_FRAGMENT_ITERATOR const *Iterator
);
```

## Parameters

### Iterator

A pointer to a [**NET_RING_FRAGMENT_ITERATOR**](net_ring_fragment_iterator.md) structure.

## Returns

Returns the fragment iterator's current **Index**.

## Requirements

| | |
| --- | --- |
| Header | netringiterator.h |
| IRQL | Any level as long as target memory is resident |

## See Also

[Net ring iterator README](README.md)

[**NET_RING_FRAGMENT_ITERATOR**](net_ring_fragment_iterator.md)
