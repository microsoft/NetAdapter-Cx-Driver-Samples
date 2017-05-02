
# NetAdapter Class Extension Samples

The Network Adapter Class Extension to WDF (NetAdapterCx) brings together the productivity of WDF with the networking performance of NDIS. The goal of NetAdpaterCx is to make it easy to write a great driver for your NIC.

This repository hosts code you can include into your own NIC driver, based on NetAdapterCx.

# Contents
- [RTL8168D Sample Driver](RtEthSample/README.md)
- [DMA IOHelper](IoHelpers/dma)

## The RTL8168D Sample Driver

This is a complete, working driver for the `PCI\VEN_10EC&DEV_8168&SUBSYS_816810EC&REV_03` device.
The device is based on the PCI bus, uses bus-mastering DMA to transfer data, and uses line-based interrupts.
The hardware supports checksum offload, interupt moderation, and several Wake-on-LAN patterns.

## The IO Helpers

NetAdapter's datapath is designed to be as close to the metal as possible.
However, NetAdapter itself is still generic enough to work with NICs from a variety of hardware busses.
Our IO Helpers are header-only bits of code designed to bridge the gap between NetAdapter's bus-agnostic API and your actual lower edge.
Using an IO Helper means you have even less boilerplate to worry about, and you can focus on showcasing your hardware's features.

These IO Helpers are completely optional: you can decide whether you want to program to the underlying NetAdapter API, or use an IO Helper to do it for you.
If you do decide to use an IO Helper, just download a copy of the relevent headers and include them in your project.

We care deeply about performance, and our IO Helpers have been benchmarked and tuned to be very efficient.
But, if your hardware has an unusual quirk that our code doesn't take into consideration, you can simply modify the code.
The code is licensed under the MIT license, so you have no obligation to publish any changes you make.
But, if you don't want to maintain your own fork of the code, you can submit a pull request with your changes.

We expect to build up a library of several IO Helpers for you.
Currently, we have one IO Helper, for the transmit path of a NIC that uses bus-mastering DMA.
The DMA IO Helper takes over your transmit queue, handling all the details of making efficient use of the operating system's DMA APIs.
The comments at the top of txdmafx.h will show you how to get started.

# Contributing

We welcome your bug reports and feature requests!
File an issue here on GitHub, or email us at NetAdapter@microsoft.com.

We're happy to review pull requests for IO Helpers.
If you're going to make substantial changes, you should coordinate with us first, so we don't inadvertantly end up duplicating our efforts.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Licensing
The code hosted here is licensed under the MIT License.

# See Also

**Source Code to NetAdpaterCx.sys**: [Network-Adapter-Class-Extension](https://github.com/Microsoft/Network-Adapter-Class-Extension)

**API Documentation**: [Network Adapter WDF Class Extension (Cx)](https://aka.ms/netadapter/doc)

