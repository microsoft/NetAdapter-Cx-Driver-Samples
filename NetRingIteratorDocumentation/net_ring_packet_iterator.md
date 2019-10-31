# NET_RING_PACKET_ITERATOR structure

## Description



A **NET_RING_PACKET_ITERATOR** is a wrapper around a [**NET_RING_ITERATOR**](net_ring_iterator.md) that client drivers use for net packet rings.

## Syntax

```C++
typedef struct _NET_RING_PACKET_ITERATOR
{

    NET_RING_ITERATOR
        Iterator;

} NET_RING_PACKET_ITERATOR;
```

## Fields

### Iterator

The [**NET_RING_ITERATOR**](net_ring_iterator.md) structure around which this structure wraps. 

## Remarks

For packet rings, client drivers must use a **NET_RING_PACKET_ITERATOR** instead of using a [**NET_RING_ITERATOR**](net_ring_iterator.md) directly.

## Requirements

| | |
| --- | --- |
| Header | netringiterator.h |

## See Also

[Net ring iterator README](README.md)

[**NET_RING_ITERATOR**](net_ring_iterator.md)
