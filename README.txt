The Numcore messaging library v3.0
==================================

Copyright 2007-2008 Juha Reunanen
          2008-2012 Numcore Ltd
          2014,2016,2018 Juha Reunanen

This library was used in Numcore Ltd's products to send and
receive messages between different applications that may run
on the same host, or on different hosts.

The first versions of the library basically extended the
Spread toolkit (see http://www.spread.org/) in a limited
number of ways.

The present version (3+) replaces the Spread toolkit (which
has a somewhat nasty license) with RabbitMQ. The implementation
is not really much more than a convenience wrapper allowing
applications to get rid of the Spread toolkit's license,
without requiring any changes in the code.

For an example on how to use the library, see the simple
producer-consumer application in the "samples" subdirectory.

Basically the library comprises a number of sublibraries:
1) numcfc: Numcore Foundation Classes
   General-purpose classes for representing time, threads, etc.
2) slaim: Simple Library for Application-Independent Messaging
   A minimalist library defining the messaging API.
3) numrabw: Numcore RabbitMQ Wrapper
   A slaim-compliant wrapper for use with RabbitMQ.
4) claim: Complicated Library for Application-Independent
   Messaging
   Some higher-level concepts, such as claim::AttributeMessage.

Juha Reunanen and Numcore Ltd have decided to release the
library under the Boost Software License (see accompanying file
LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
in order to promote its use in joint projects and elsewhere.


Version history:

v3.0 (2018-03-25):
- Switch from ZeroMQ to RabbitMQ

v2.0 (2016-07-25):
- Switch from Spread toolkit to ZeroMQ and custom broker

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
