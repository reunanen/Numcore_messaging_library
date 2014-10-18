The Numcore messaging library v1.2
==================================

Copyright 2007-2008 Juha Reunanen
          2008-2012 Numcore Ltd
          2014 Juha Reunanen

This library was used in Numcore Ltd's products to send and
receive messages between different applications that may run
on the same host, or on different hosts. The library basically
extends the Spread toolkit (see http://www.spread.org/) in a
limited number of ways:

1) Easy-to-use C++ wrapper classes are provided (together with
   a more or less experimental Python version).
2) The post office objects try to re-open a broken connection
   automatically.
3) Whenever message size exceeds the maximum supported by the
   Spread toolkit, the message is automatically split before
   sending and merged when being received, without client code
   having to worry about it.
4) Threaded and buffered post office implementation means that
   client code does not need to keep calling receive all the
   time.

There are similar libraries out there, for example:
http://www.savarese.com/software/libssrcspread/

The library includes pre-built lib files for Spread Toolkit
version 4.1.0 and Visual Studio 2008. To use in other
environments, you need to install the Spread toolkit and then
compile the Numcore messaging library yourself.

For an example on how to use the library, see the simple
producer-consumer application in the "samples" subdirectory.

Basically the library comprises a number of sublibraries:
1) numcfc: Numcore Foundation Classes
   General-purpose classes for representing time, threads, etc.
2) slaim: Simple Library for Application-Independent Messaging
   A minimalist library defining the messaging API.
3) numsprew: Numcore Spread Wrapper
   A slaim-compliant wrapper for the Spread toolkit.
4) claim: Complicated Library for Application-Independent
   Messaging
   Higher-level concepts such as the threaded and buffered
   post office class and attribute message. Depends on numcfc,
   and takes a lot of benefit from the Boost library (see
   http://www.boost.org/).

Juha Reunanen and Numcore Ltd have decided to release the
library under the Boost Software License (see accompanying file
LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
in order to promote its use in joint projects and elsewhere.

This product uses software developed by Spread Concepts LLC for
use in the Spread toolkit. For more information about Spread
see http://www.spread.org


Version history:

v1.2 (2012-01-20):
- In numcfc::Time, made sure that fraction of second < 1
- In numcfc::ThreadRunner, fixed a potential synchronization
  issue

v1.1 (2011-12-22):
- In numcfc::Time, clarified the distinction between UTC and
  local time; also added some new methods
- In numcfc::IniFile, added method GetFilename()

v1.0 (2011-08-30):
- First released version
