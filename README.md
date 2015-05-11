libpagebuf
==========

Buffer objects for use in socket programming.

These buffer objects are made with the requirements of socket event based
applications in mind.  The kind of operations that they support include:
- Allocation of storage space to a buffer
- Writing data to storage space
- Transferring (committing) arbitrary amounts of data from one buffer to
  another
- Reading of data

Some other types of operations may be performed on buffered data:
- Seeking (consuming) of arbitrary amounts of buffered data
- Truncation of arbitrary amounts of buffered data
- Rewinding of buffer storage (prepending allocated storage space onto a
  buffer)
- Overwriting of existing buffer data
- Insertion of new data into arbitrary points in buffered data

libpagebuf pb_buffer objects do this with objects representing raw data
and other objects representing views on that data.

pb_buffer objects abstract the underlying arrangement of the raw data through
the data views, so that the underlying data may be operated on as would a
single contiguous array of bytes.  This design leads to pb_buffer being an
effective implementor of zero copy and scatter-gather semantics.

pb_buffer objects also perform resource allocation and de-allocation of
internally managed objects, and storage space (e.g memory) resources on
behalf of the author.

pb_buffer objects further their usefulness in socket event based applications
by allowing adjustment of internal data and data view management behaviour
using creation time strategy configuration interfaces, and also support
pluggable memory and storage space allocators to allow tighter integration
with large data oriented systems.

pb_buffer objects aim to be useful in many situations in application
programming by means of:
- Simple C++ class wrapper for pb_buffer and pb_buffer_iterator
- Adaptors for boost::asio ConstBufferSequence, MutableBufferSequence and
  DynamicBufferSequence concepts (pending)
- An adaptor for boost::regex BidirectionalIterator (pending)
