"""
               Copyright 2009 Numcore Ltd

 Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at
          http://www.boost.org/LICENSE_1_0.txt)
"""

class Message:
    def __init__(self, type = "", text = ""):
        self.type = type
        self.text = text

class PostOffice:
    def __init__(self):
        self.clientName = "unkpy"

    def subscribe(self, type):
        pass
        
    def unsubscribe(self, type):
        pass
        
    def send(self, msg):
        pass
        
    def receive(self, maxSecondsToWait):
        pass
        
    def set_connect_info(self, connectString):
        pass
        
    def set_client_identifier(self, clientIdentifier):
        pass
        
    def get_client_address(self):
        pass
        
    def get_version(self):
        pass


def convert_message_list_to_single_message(lst):
    text = ''
    for smsg in lst:
        text = text + serialize_message(smsg)
    s = Message('MessageList', text)
    return s.text


def serialize_message(smsg):
    s = '['
    contentsLength = len(smsg.type) + len(smsg.text) + 4
    s = s + str(contentsLength)
    s = s + ' ('
    s = s + smsg.type
    s = s + ' '
    s = s + smsg.text
    s = s + ')\n]'
    return s
