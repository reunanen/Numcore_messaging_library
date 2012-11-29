"""
               Copyright 2009 Numcore Ltd

 Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at
          http://www.boost.org/LICENSE_1_0.txt)
"""

import slaim
import spread

class PostOffice(slaim.PostOffice):
    def __init__(self):
        self.clientId = "unkpy"
        self._mailbox = -1
        self.connectString = "4803@localhost"
        self._privateGroup = ""
        self._maxSingleMessageLength = 65536

    def __del__(self):
        if self.is_mailbox_ok():
            self._mailbox.disconnect()

    def subscribe(self, type):
        self._regular_operations()
        if self.is_mailbox_ok():
            self._mailbox.join(type)
        pass
        
    def unsubscribe(self, type):
        self._regular_operations()
        if self.is_mailbox_ok():
            self._mailbox.leave(type)
        
    def send(self, msg):
        self._regular_operations()
        msgLen = len(msg.type) + 1 + len(msg.text)
        if msgLen <= self._maxSingleMessageLength:
            rawMsg = msg.type + "\t" + msg.text
            self._mailbox.multicast(spread.FIFO_MESS, msg.type, rawMsg, 1)
        else:
            raise _maxSingleMessageLength

    def receive(self, maxSecondsToWait):
        self._regular_operations()
        if self._mailbox.poll():
            rawMsg = self._mailbox.receive()
            if rawMsg.msg_type == 1: # normal
                pos = rawMsg.message.find('\t')
                type = rawMsg.message[:pos]
                text = rawMsg.message[pos+1:]
                return slaim.Message(type, text)
            elif rawMsg.msg_type == 2: # multi
                msg = _multiMessagesBeingReceived[rawMsg.sender]
                msg.text = msg.text.append(rawMsg.message)
                return None
            elif rawMsg.msg_type == 4: # multi-start
                pos = rawMsg.message.find('\t')
                type = rawMsg.message[:pos]
                text = rawMsg.message[pos+1:]
                _multiMessagesBeingReceived[rawMsg.sender] = Slaim.message(type, text)
                return None
            elif rawMsg.msg_type == 8: # multi-end
                msg = _multiMessagesBeingReceived[rawMsg.sender]
                msg.text = msg.text.append(rawMsg.message)
                _multiMessagesBeingReceived[rawMsg.sender] = None
                return msg
        else:
            return None
        
    def set_connect_info(self, connectString):
        self.connectString = connectString
        
    def set_client_identifier(self, clientIdentifier):
        self.clientId = clientIdentifier
        
    def get_client_address(self):
        return self._privateGroup
        
    def get_version(self):
        return '$Id: numsprew.py,v 1.4 2011-08-30 13:24:44 juha Exp $'

    def is_mailbox_ok(self):
        return self._mailbox != -1
        
    def _regular_operations(self):
        if self.is_mailbox_ok() == False:
            self._mailbox = spread.connect(
                self.connectString, self.clientId, 0, 0)
            self._privateGroup = self._mailbox.private_group
            