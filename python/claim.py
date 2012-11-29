"""
               Copyright 2009 Numcore Ltd

 Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at
          http://www.boost.org/LICENSE_1_0.txt)
"""

import slaim
import numsprew

import time
import threading
import Queue


class PostOffice(slaim.PostOffice,threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)
        self._postOffice = numsprew.PostOffice()
        self.isKilled = Queue.Queue(1)
        self._pendingSubscribeCommands = Queue.Queue(64)
        self._messagesReceived = Queue.Queue(1024)
        self._messagesToBeSent = Queue.Queue(1024)
        self._lock = threading.Lock()
        self.start()

    def __del__(self):
        self.kill()
        self.join()

    def run(self):
        ''' loop until killed '''
        while self.isKilled.empty():
            didSomething = False
            if not self._pendingSubscribeCommands.empty():
                didSomething = True
                type, s = self._pendingSubscribeCommands.get()
                if s:
                    self._postOffice.subscribe(type)
                else:
                    self._postOffice.unsubscribe(type)

            msg = self._postOffice.receive(0)
            if msg != None:
                didSomething = True
                self._messagesReceived.put(msg)

            if not self._messagesToBeSent.empty():
                didSomething = True
                msg = self._messagesToBeSent.get()
                self._postOffice.send(msg)

            if not didSomething:
                time.sleep(0.1)

    def kill(self):
        if self.isKilled.empty():
            self.isKilled.put(True)

    def subscribe(self, type):
        self._pendingSubscribeCommands.put((type, True))
        
    def unsubscribe(self, type):
        self._pendingSubscribeCommands.put((type, False))
        
    def send(self, msg):
        self._messagesToBeSent.put(msg)
        
    def receive(self, maxSecondsToWait):
        try:
            return self._messagesReceived.get(True, maxSecondsToWait)
        except Queue.Empty:
            return None
        
    def set_connect_info(self, connectString):
        try:
            self._lock.acquire()
            self._postOffice.set_connect_info(connectString)
        finally:
            self._lock.release()
        
    def set_client_identifier(self, clientIdentifier):
        try:
            self._lock.acquire()
            self._postOffice.set_client_identifier(clientIdentifier)
        finally:
            self._lock.release()
        
    def get_client_address(self):
        address = ''
        try:
            self._lock.acquire()
            return self._postOffice.get_client_address()
        finally:
            self._lock.release()
        
    def get_version(self):
        try:
            self._lock.acquire()
            return 'claimed<' + self._postOffice.get_version() + ">"
        finally:
            self._lock.release()


class AttributeMessage:
    def __init__(self, smsg = None):
        if smsg != None:
            self.from_raw_message(smsg)
        else:
            self.type = ''
            self.attrs = {}
            self.body = ''
            
    def from_raw_message(self, smsg):
        """ Convert a slaim.Message to a claim.AttributeMessage. """
        self.type = smsg.type
        self.attrs = {}
        self.body = ''
        rest = smsg.text
        while len(rest) > 0:
            pos = rest.find(' ')        
            data_length = int(rest[1:pos])        
            data_end_pos = pos + 2 + data_length;
            type_end_pos = rest.find(' ',pos+1)
            name = rest[pos+2:type_end_pos]
            value = rest[type_end_pos+1:data_end_pos-3]
            if name == 'm_body':
                self.body = value
            else:
                self.attrs[name] = value
            rest = rest[data_end_pos:]

    def to_raw_message(self):
        smsg = slaim.Message(self.type)
        msgList = []
        bodyMsg = slaim.Message('m_body', self.body)
        msgList.append(bodyMsg)
        for name in self.attrs:
            value = self.attrs[name]
            attrMsg = slaim.Message(name, value)
            msgList.append(attrMsg)
        smsg.text = slaim.convert_message_list_to_single_message(msgList)
        return smsg
