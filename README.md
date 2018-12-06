
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

