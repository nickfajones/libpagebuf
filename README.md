[![Build Status](https://travis-ci.org/nickfajones/libpagebuf.svg?branch=master)](https://travis-ci.org/nickfajones/libpagebuf)

libpagebuf
==========

Developers of software that involves IO, for example networking IO, face
the challenge of dealing with large amounts of data.  Whether the data is
passing quickly through the system or not, this data will at the least need
to be stored after it is received from the input side, then that same data
may need to be arranged for writing back to the output side.
Additionally, the data may require processing as it moves in then out of
the system, processing such as parsing and even modification and
manipulation.

When that software system is non-blocking and event driven, additional
challenges exist, such as the need to capture data that arrives in a
piecewise manner, in uncertain size and delay patterns, then the need to
gain access to that data in a linear form to allow proper parsing and
manipulation.

And all of this needs to be done as resource and time efficiently as
possible.

libpagebuf is designed to provide a solution to the data storage challenges
faced in particular by developers of IO oriented, non-blocking event driven
systems.  At its core, it implements a set of data structures and
algorithms for the management of regions of system memory.  On the surface,
through the use of the central pb_buffer class, it provides a means of
reading and manipulating the data that is stored in those memory regions,
that is abstracted away from the underlying arrangement of those memory
regions in the system memory space.

An author may use pb_buffer to receive data from input sources as
fragments, then perform read actions such as searching and copying in
addition to some more intrusive actions such as insertion or truncation on
that data, without any regard for the underlying fragmentation of the
memory regions that the data is stored in.

libpagebuf is also designed with concerns of non-blocking event driven
and multithreaded systems in mind, through the inclusion of interfaces for
the use of custom memory allocators, debugging to check internal structure
integrity and thread exclusivity, interfaces in both C and C++,
C++ interfaces complying to the ASIO ConstBufferSequence,
MutableBufferSequence and DynamicBufferSequence concepts, as well as the
BidirectionanIterator concept used in particular by boost::regex.

libpagebuf is designed for efficiency, using reference counting and
zero-copy semantics at its core, as well as providing a class like
interface in C that provides a path for subclassing and modifying
implementation details
