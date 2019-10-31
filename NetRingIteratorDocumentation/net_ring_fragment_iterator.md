# NET_RING_FRAGMENT_ITERATOR structure

## Description

A **NET_RING_FRAGMENT_ITERATOR** is a wrapper around a [**NET_RING_ITERATOR**](net_ring_iterator.md) that client drivers use for net fragment rings.

## Syntax

```C++
typedef struct _NET_RING_FRAGMENT_ITERATOR
{

    NET_RING_ITERATOR
        Iterator;

} NET_RING_FRAGMENT_ITERATOR;
```

## Fields

### Iterator

The [**NET_RING_ITERATOR**](net_ring_iterator.md) structure around which this structure wraps. 

## Remarks

For fragment rings, client drivers must use a **NET_RING_FRAGMENT_ITERATOR** instead of using a [**NET_RING_ITERATOR**](net_ring_iterator.md) directly.

## Requirements

| | |
| --- | --- |
| Header | netringiterator.h |

## See Also

[Net ring iterator README](README.md)

[**NET_RING_ITERATOR**](net_ring_iterator.md)
