# Net ring iterators

## Overview

The net ring iterator interface is an internal helper API that client drivers can optionally call to perform post and drain operations on a net ring. Netringiterator.h is not included in the WDK.

A [**NET_RING_ITERATOR**](net_ring_iterator.md) is a small structure that contains references to the post and drain indices of a [**NET_RING**](https://docs.microsoft.com/windows-hardware/drivers/ddi/ring/ns-ring-_net_ring) to which it belongs.

Each **NET_RING** can have multiple iterators. For example, the packet ring might have an iterator that covers the drain section of the ring (a drain iterator), an iterator that covers the post section of the ring (a post iterator), and an iterator that covers both the post and drain sections. Likewise, the fragment ring can have the same iterators.

To make it easy for client drivers to control each iterator for each ring, the net ring iterator interface separates iterators into two categories: [**NET_RING_PACKET_ITERATOR**](net_ring_packet_iterator.md) and [**NET_RING_FRAGMENT_ITERATOR**](net_ring_fragment_iterator.md). These are wrapper structures around the **NET_RING_ITERATOR** and are used in all net ring iterator interface API calls. Client drivers should not use a **NET_RING_ITERATOR** directly. Instead, they should use the appropriate type of iterator for a given net ring (either packet or fragment).

By getting, advancing, and setting net ring iterators, client drivers send and receive network data in their packet queues. Client drivers also call methods on net ring iterators to access the ring's elements.

For more information about net rings, see [Introduction to net rings](https://docs.microsoft.com/windows-hardware/drivers/netcx/introduction-to-net-rings).

## Netringiterator.h

The Netringiterator.h header contains the following APIs:

### Methods

| Title | Description |
| --- | --- |
| [NetFragmentIteratorAdvance](netfragmentiteratoradvance.md) | The NetFragmentIteratorAdvance method advances the Index of a fragment iterator by one. |
| [NetFragmentIteratorAdvanceToTheEnd](netfragmentiteratoradvancetotheend.md) | The NetFragmentIteratorAdvanceToTheEnd method advances the current Index of a fragment iterator to its End index. |
| [NetFragmentIteratorGetCount](netfragmentiteratorgetcount.md) | The NetFragmentIteratorGetCount method gets the count of fragments that a client driver owns in the fragment ring. |
| [NetFragmentIteratorGetFragment](netfragmentiteratorgetfragment.md) | The NetFragmentIteratorGetFragment method gets the NET_FRAGMENT structure at a fragment iterator's current Index in the fragment ring. |
| [NetFragmentIteratorGetIndex](netfragmentiteratorgetindex.md) | The NetFragmentIteratorGetIndex method gets the current Index of a fragment iterator in the fragment ring. |
| [NetFragmentIteratorHasAny](netfragmentiteratorhasany.md) | The NetFragmentIteratorHasAny method determines whether a fragment iterator has any valid elements to process in the fragment ring. |
| [NetFragmentIteratorSet](netfragmentiteratorset.md) | The NetFragmentIteratorSet method completes a client driver's post or drain operation on the fragment ring. |
| [NetPacketIteratorAdvance](netpacketiteratoradvance.md) | The NetPacketIteratorAdvance method advances the Index of a packet iterator by one. |
| [NetPacketIteratorAdvanceToTheEnd](netpacketiteratoradvancetotheend.md) | The NetPacketIteratorAdvanceToTheEnd method advances the current Index of a packet iterator to its End index. |
| [NetPacketIteratorGetCount](netpacketiteratorgetcount.md) | The NetPacketIteratorGetCount method gets the count of packets that a client driver owns in the packet ring. |
| [NetPacketIteratorGetFragments](netpacketiteratorgetfragments.md) | The NetPacketIteratorGetFragments method gets the fragments associated with a packet. |
| [NetPacketIteratorGetIndex](netpacketiteratorgetindex.md) | The NetPacketIteratorGetIndex method gets the current Index of a packet iterator in the packet ring. |
| [NetPacketIteratorGetPacket](netpacketiteratorgetpacket.md) | The NetPacketIteratorGetPacket method gets the NET_PACKET structure at a packet iterator's current Index in the packet ring. |
| [NetPacketIteratorHasAny](netpacketiteratorhasany.md) | The NetPacketIteratorHasAny method determines whether a packet iterator has any valid elements to process in the packet ring. |
| [NetPacketIteratorSet](netpacketiteratorset.md) | The NetPacketIteratorSet method completes a client driver's post or drain operation on the packet ring. |
| [NetRingGetAllFragments](netringgetallfragments.md) | The NetRingGetAllFragments method gets a fragment iterator for the entire range that a client driver owns in a fragment ring. |
| [NetRingGetAllPackets](netringgetallpackets.md) | The NetRingGetAllPackets method gets a packet iterator for the entire range that a client driver owns in a packet ring. |
| [NetRingGetDrainFragments](netringgetdrainfragments.md) | The NetRingGetDrainFragments method gets a fragment iterator for the current drain subsection of a fragment ring. |
| [NetRingGetDrainPackets](netringgetdrainpackets.md) | The NetRingGetDrainPackets method gets a packet iterator for the drain subsection of a packet ring. |
| [NetRingGetPostFragments](netringgetpostfragments.md) | The NetRingGetPostFragments method gets a fragment iterator for the current post subsection of a fragment ring. |
| [NetRingGetPostPackets](netringgetpostpackets.md) | The NetRingGetPostPackets method gets a packet iterator for the post subsection of a packet ring. |

### Structures

| Title | Description |
| --- | --- |
| [NET_RING_FRAGMENT_ITERATOR](net_ring_fragment_iterator.md) | A NET_RING_FRAGMENT_ITERATOR is a wrapper around a NET_RING_ITERATOR that client drivers use for net fragment rings. |
| [NET_RING_ITERATOR](net_ring_iterator.md) | A NET_RING_ITERATOR is a small structure that contains references to the post and drain indices of a NET_RING to which it belongs. |
| [NET_RING_PACKET_ITERATOR](net_ring_packet_iterator.md) | A NET_RING_PACKET_ITERATOR is a wrapper around a NET_RING_ITERATOR that client drivers use for net packet rings. |

## Examples

### Sending data with net ring iterators

Here is a typical post and drain sequence for a driver whose device transmits data in order, such as a simple PCI NIC.

1. Call **NetTxQueueGetRingCollection** to retrieve the transmit queue's ring collection structure. You can store this in the queue's context space to reduce calls out of the driver. 
2. Post data to hardware:    
    1. Use the ring collection to retrieve the post iterator for the transmit queue's packet ring by calling [**NetRingGetPostPackets**](netringgetpostpackets.md).
    2. Do the following in a loop:
        1. Get a packet by calling [**NetPacketIteratorGetPacket**](netpacketiteratorgetpacket.md) with the packet iterator.
        2. Check if this packet should be ignored. If it should be ignored, skip to step 6 of this loop. If not, continue.
        3. Get a fragment iterator for this packet's fragments by calling [**NetPacketIteratorGetFragments**](netpacketiteratorgetfragments.md).
        4. Do the doing the following in a loop:
            1. Call [**NetFragmentIteratorGetFragment**](netfragmentiteratorgetfragment.md) with the fragment iterator to get a fragment.
            2. Translate the **NET_FRAGMENT** descriptor into the associated hardware fragment descriptor.
            3. Call [**NetFragmentIteratorAdvance**](netfragmentiteratoradvance.md) to move to the next fragment for this packet.
        5. Update the fragment ring's **Next** index to match the fragment iterator's current **Index**, which indicates that posting to hardware is complete.
        6. Call [**NetPacketIteratorAdvance**](netpacketiteratoradvance.md) to move to the next packet.
    3. Call [**NetPacketIteratorSet**](netpacketiteratorset.md) to finalize posting packets to hardware.
3. Drain completed transmit packets to the OS:
    1. Use the ring collection to retrieve the drain iterator for the transmit queue's packet ring by calling [**NetRingGetDrainPackets**](netringgetdrainpackets.md).
    2. Do the following in a loop:
        1. Get a packet by calling [**NetPacketIteratorGetPacket**](netpacketiteratorgetpacket.md).
        2. Check if the packet has finished transmitting. If it has not, break out of the loop.
        2. Call [**NetPacketIteratorAdvance**](netpacketiteratoradvance.md) to move to the next packet.
    3. Call [**NetPacketIteratorSet**](netpacketiteratorset.md) to finalize draining packets to the OS.

These steps might look like this in code. Note that hardware-specific details such as how to post descriptors to hardware or flushing a successful post transaction are left out for clarity.

```C++
void
MyEvtTxQueueAdvance(
    NETPACKETQUEUE TxQueue
)
{
    // Get the transmit queue's context to retrieve the net ring collection
    PMY_TX_QUEUE_CONTEXT txQueueContext = MyGetTxQueueContext(TxQueue);
    NET_RING_COLLECTION const * Rings = txQueueContext->Rings;

    //
    // Post data to hardware
    //
    NET_RING_PACKET_ITERATOR packetIterator = NetRingGetPostPackets(Rings);
    while(NetPacketIteratorHasAny(&packetIterator))
    {
        NET_PACKET* packet = NetPacketIteratorGetPacket(&packetIterator);        
        if(!packet->Ignore)
        {
            NET_FRAGMENT_ITERATOR fragmentIterator = NetPacketIteratorGetFragments(&packetIterator);
            UINT32 packetIndex = NetPacketIteratorGetIndex(&packetIterator);
            
            for(txQueueContext->PacketTransmitControlBlocks[packetIndex]->numTxDescriptors = 0; 
                NetFragmentIteratorHasAny(&fragmentIterator); 
                txQueueContext->PacketTransmitControlBlocks[packetIndex]->numTxDescriptors++)
            {
                NET_FRAGMENT* fragment = NetFragmentIteratorGetFragment(&fragmentIterator);

                // Post fragment descriptor to hardware
                ...
                //

                NetFragmentIteratorAdvance(&fragmentIterator);
            }

            //
            // Update the fragment ring's Next index to indicate that posting is complete and prepare for draining
            //
            fragmentIterator.Iterator.Rings->Rings[NET_RING_TYPE_FRAGMENT]->NextIndex = NetFragmentIteratorGetIndex(&fragmentIterator);
        }
        NetPacketIteratorAdvance(&packetIterator);
    }
    NetPacketIteratorSet(&packetIterator);

    //
    // Drain packets if completed
    //
    packetIterator = NetRingGetDrainPackets(Rings);
    while(NetPacketIteratorHasAny(&packetIterator))
    {        
        NET_PACKET* packet = NetPacketIteratorGetPacket(&packetIterator);
        
        // Test packet for transmit completion by checking hardware ownership flags in the packet's last fragment
        ..
        //
        
        NetPacketIteratorAdvance(&packetIterator);
    }
    NetPacketIteratorSet(&packetIterator);
}
```

### Receiving data with net ring iterators

Here is a typical sequence for a driver that receives data in order, with one fragment per packet.

1. Call **NetRxQueueGetRingCollection** to retrieve the receive queue's ring collection structure. You can store this in the queue's context space to reduce calls out of the driver. 
2. Indicate received data to the OS by draining the net rings:
    1. Use the ring collection to retrieve the drain iterator for the receive queue's fragment ring through a call to [**NetRingGetDrainFragments**](netringgetdrainfragments.md).
    2. Get a packet iterator for all available packets in the packet ring by calling [**NetRingGetAllPackets**](netringgetallpackets.md).
    3. Do the following in a loop:
        1. Check if the fragment has been received by the hardware. If not, break out of the loop.
        2. Get the fragment iterator's current fragment by calling [**NetFragmentIteratorGetFragment**](netfragmentiteratorgetfragment.md).
        3. Fill in the fragment's information, such as its **ValidLength**, based on its matching hardware descriptor.
        4. Get a packet for this fragment by calling [**NetPacketIteratorGetPacket**](netpacketiteratorgetpacket.md).
        5. Bind the fragment to the packet by setting the packet's **FragmentIndex** to the fragment's current index in the fragment ring and setting the number of fragments appropriately (in this example, it is set to **1**). 
        6. Optionally, fill in any other packet information such as checksum info.
        7. Call [**NetFragmentIteratorAdvance**](netfragmentiteratoradvance.md) to move to the next fragment.
        7. Call [**NetPacketIteratorAdvance**](netpacketiteratoradvance.md) to move to the next packet.
    4. Call [**NetFragmentIteratorSet**](netfragmentiteratorset.md) and [**NetPacketIteratorSet**](netpacketiteratorset.md) to finalize indicating received packets and their fragments to the OS.
3. Post fragment buffers to hardware for the next receives:    
    1. Use the ring collection to retrieve the post iterator for the receive queue's fragment ring by calling [**NetRingGetPostFragments**](netringgetpostfragments.md).
    2. Do the following in a loop:
        1. Get the fragment iterator's current index by calling [**NetFragmentIteratorGetIndex**](netfragmentiteratorgetindex.md).
        2. Post the fragment's information to the matching hardware descriptor.
        3. Call [**NetFragmentIteratorAdvance**](netfragmentiteratoradvance.md) to move to the next fragment.
    3. Call [**NetFragmentIteratorSet**](netfragmentiteratorset.md) to finalize posting fragments to hardware.

These steps might look like this in code:

```C++
void
MyEvtRxQueueAdvance(
    NETPACKETQUEUE RxQueue
)
{
    // Get the receive queue's context to retrieve the net ring collection
    PMY_RX_QUEUE_CONTEXT rxQueueContext = MyGetRxQueueContext(RxQueue);
    NET_RING_COLLECTION const * Rings = rxQueueContext->Rings;

    //
    // Indicate receives by draining the rings
    //
    NET_RING_FRAGMENT_ITERATOR fragmentIterator = NetRingGetDrainFragments(Rings);
    NET_RING_PACKET_ITERATOR packetIterator = NetRingGetAllPackets(Rings);
    while(NetFragmentIteratorHasAny(&fragmentIterator))
    {
        UINT32 currentFragmentIndex = NetFragmentIteratorGetIndex(&fragmentIterator);

        // Test for fragment reception
        ...
        //

        NET_FRAGMENT* fragment = NetFragmentIteratorGetFragment(&fragmentIterator);
        fragment->ValidLength = ... ;
        NET_PACKET* packet = NetPacketIteratorGetPacket(&packetIterator);
        packet->FragmentIndex = currentFragmentIndex;
        packet->FragmentCount = 1;

        if(rxQueueContext->IsChecksumExtensionEnabled)
        {
            // Fill in checksum info
            ...
            //
        }        

        NetFragmentIteratorAdvance(&fragmentIterator);
        NetPacketIteratorAdvance(&packetIterator);
    }
    NetFragmentIteratorSet(&fragmentIterator);
    NetFragmentIteratorSet(&packetIterator);

    //
    // Post fragment buffers to hardware
    //
    fragmentIterator = NetRingGetPostFragments(Rings);
    while(NetFragmentIteratorHasAny(&fragmentIterator))
    {
        UINT32 currentFragmentIndex = NetFragmentIteratorGetIndex(&fragmentIterator);

        // Post fragment information to hardware descriptor
        ...
        //

        NetFragmentIteratorAdvance(&fragmentIterator);
    }
    NetFragmentIteratorSet(&fragmentIterator);
}
```

### Canceling data with net ring iterators

#### Canceling a transmit queue

If your hardware supports canceling in-flight transmits, you should also advance the net ring's post packet iterator past all canceled packets. This might look like the following example:

```C++
void
MyEvtTxQueueCancel(
    NETPACKETQUEUE TxQueue
)
{
    // Get the transmit queue's context to retrieve the net ring collection
    PMY_TX_QUEUE_CONTEXT txQueueContext = MyGetTxQueueContext(TxQueue);
    NET_RING_COLLECTION const * rings = txQueueContext->Rings;

    NET_RING_PACKET_ITERATOR packetIterator = NetRingGetPostPackets(rings);
    while (NetPacketIteratorHasAny(&packetIterator))
    {
        // Mark this packet as canceled with the scratch field, then move past it
        NetPacketIteratorGetPacket(&packetIterator)->Scratch = 1;
        NetPacketIteratorAdvance(&packetIterator);
    }
    NetPacketIteratorSet(&packetIterator);
}
```

#### Canceling a receive queue

To return packets, you should first attempt to indicate any receives that might have been indicated while the receive path was being disabled, then set all packets to be ignored and return all fragments to the OS. This might look like the following code example.

> [!NOTE]
> This example leaves out details for indicating receives. For a code sample of receiving data, see [receiving network data with net ring iterators](#receiving-data-with-net-ring-iterators).

```C++
void
MyEvtRxQueueCancel(
    NETPACKETQUEUE RxQueue
)
{
    // Get the receive queue's context to retrieve the net ring collection
    PMY_RX_QUEUE_CONTEXT rxQueueContext = MyGetRxQueueContext(RxQueue);
    NET_RING_COLLECTION const * rings = rxQueueContext->Rings;

    // Set hardware register for cancellation
    ...
    //

    // Indicate receives
    ...
    //

    // Get all packets and mark them for ignoring
    NET_RING_PACKET_ITERATOR packetIterator = NetRingGetAllPackets(rings);
    while(NetPacketIteratorHasAny(&packetIterator))
    {
        NetPacketIteratorGetPacket(&packetIterator)->Ignore = 1;
        NetPacketIteratorAdvance(&packetIterator);
    }
    NetPacketIteratorSet(&packetIterator);

    // Return all fragments to the OS
    NET_RING_FRAGMENT_ITERATOR fragmentIterator = NetRingGetAllFragments(rings);
    NetFragmentIteratorAdvanceToTheEnd(&fragmentIterator);
    NetFragmentIteratorSet(&fragmentIterator);
}
```