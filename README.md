[![Build Status](https://travis-ci.org/nickfajones/libpagebuf.svg?branch=master)](https://travis-ci.org/nickfajones/libpagebuf)
[![Coverage Status](https://coveralls.io/repos/github/nickfajones/libpagebuf/badge.svg?branch=master)](https://coveralls.io/github/nickfajones/libpagebuf?branch=master)

libpagebuf
==========

Developers of software that involves IO, for example networking IO, face
the challenge of dealing with large amounts of data.  Whether the data is
passing quickly through the system or not, this data will need to be stored
after it is received from the input side, then arranged for writing back to
the output side.
Additionally, the data may require processing as it moves in then out of
the system.  Processing includes parsing and analysis, and can even include
modification.

When that software system is non-blocking and event driven, an additional
challenge exists in that data may arrive in a piecewise manner with
uncertain size and delay patterns.  Authors of such applications may need
to be access the data in a sequential way, or in a way that deals as little
as possible with the underlying fragmentation.  libpagebuf is designed to
provide a solution to these data storage challenges.

On the surface, through the use of the primary pb_buffer class, libpagbuf
provides a means of writing or copying blocks of data, then a means of
reading and manipulating that data as if it was sequential and unfragmented.
An author may use pb_buffer to receive data from input sources as fragments,
then perform read actions such as searching and copying in addition to some
more intrusive actions such as insertion or truncation on that data, without
any regard for the underlying fragmentation, positioning in system memory
(or other storage) or even ordering in memory of the the data.

libpagebuf is also designed with concerns of non-blocking event driven
and multithreaded systems in mind, through the inclusion of interfaces for
the integration of custom memory allocators, debugging to check internal
structure integrity and thread exclusivity.

The core API and implementation is in C but a thin C++ wrapper is provided.
There exist C++ objects that comply to the ASIO ConstBufferSequence,
MutableBufferSequence and DynamicBufferSequence concepts, as well as the
BidirectionanIterator concept used in particular by boost::regex.

libpagebuf is designed for efficiency, using reference counting and
zero-copy semantics, as well as providing a class like interface in C that
provides a path for subclassing and modifying implementation details.
