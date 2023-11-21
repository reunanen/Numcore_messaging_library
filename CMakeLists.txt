cmake_minimum_required(VERSION 3.13)
project(Numcore_messaging_library)

add_subdirectory(messaging/numrabw/rabbitmq-c rabbitmq-c_build)
add_subdirectory(messaging/numrabw/crossguid crossguid_build)

include_directories(
  .
  messaging/numrabw/amqpcpp/include
  messaging/numrabw/rabbitmq-c/librabbitmq
  )

set (Sources
  "numcfc/IdGenerator.cpp"
  "numcfc/IniFile.cpp"
  "numcfc/Logger.cpp"
  "numcfc/ThreadRunner.cpp"
  "numcfc/Time.cpp"
  "messaging/slaim/messaging.cpp"
  "messaging/claim/AttributeMessage.cpp"
  "messaging/claim/MessageStreaming.cpp"
  "messaging/claim/PostOffice.cpp"
  "messaging/claim/PostOfficeInitializer.cpp"
  "messaging/numrabw/numrabw_postoffice.cpp"
  "messaging/numrabw/amqpcpp/src/AMQP.cpp"
  "messaging/numrabw/amqpcpp/src/AMQPBase.cpp"
  "messaging/numrabw/amqpcpp/src/AMQPException.cpp"
  "messaging/numrabw/amqpcpp/src/AMQPExchange.cpp"
  "messaging/numrabw/amqpcpp/src/AMQPMessage.cpp"
  "messaging/numrabw/amqpcpp/src/AMQPQueue.cpp"
  )

add_library(NumcoreMessagingLibrary STATIC ${Sources})

target_link_libraries(NumcoreMessagingLibrary rabbitmq-static xg)

target_compile_options(NumcoreMessagingLibrary PUBLIC -Wall -Wextra -Wpedantic -Werror)
