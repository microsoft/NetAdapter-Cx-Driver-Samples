# NetFragmentIteratorAdvanceToTheEnd method


## Description



The **NetFragmentIteratorAdvanceToTheEnd** method advances the current **Index** of a fragment iterator to its **End** index.

## Syntax

```C++
void NetFragmentIteratorAdvanceToTheEnd(
  NET_RING_FRAGMENT_ITERATOR *Iterator
);
```

## Parameters

### Iterator

A pointer to a [**NET_RING_FRAGMENT_ITERATOR**](net_ring_fragment_iterator.md) structure.

## Returns

None.

## Remarks

After calling **NetFragmentIteratorAdvanceToTheEnd**, the fragment iterator's current **Index** advances to the iterator's **End** index. Therefore, the fragments between the old value of iterator's **Index** and the iterator's **End - 1** inclusive are transferred to the OS.

Client drivers typically call **NetFragmentIteratorAdvanceToTheEnd** to cancel all fragments in the ring or perform other operations that drain all the ring's fragments.

For a code example of using this method, see [Net ring iterator README](README.md).

## Requirements

| | |
| --- | --- |
| Header | netringiterator.h |
| IRQL | Any level as long as target memory is resident |

## See Also

[Net ring iterator README](README.md)

[**NET_RING_FRAGMENT_ITERATOR**](net_ring_fragment_iterator.md)
