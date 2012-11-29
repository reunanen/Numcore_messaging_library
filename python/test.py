"""
               Copyright 2009 Numcore Ltd

 Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at
          http://www.boost.org/LICENSE_1_0.txt)
"""

#from slaim import amsg
import slaim
import claim
import numsprew

def recvloop(counter):
    try:
        po = claim.PostOffice()
        print('version: ', po.get_version())
        po.subscribe("__claim_MsgStatus")
        po.subscribe("Foo")
        fooCounter = 0
        while counter > 0:
            msg = po.receive(1)
            if msg is not None:
                if msg.type == '__claim_MsgStatus':
                    amsg = claim.AttributeMessage(msg)
                    print(amsg.attrs['client_address'],amsg.attrs['time_current_utc'])
                    counter = counter - 1
                    msg.type = 'Foo'
                    msg.text = str(counter)
                    po.send(msg)
                elif msg.type == 'Foo':
                    fooCounter = fooCounter + 1
                    pass #print(msg.text)                    
        po.unsubscribe("__claim_MsgStatus")       
        print('my address: ', po.get_client_address())
        print('fooCounter: ', fooCounter)
    finally:
        po.kill()
    
if __name__ == "__main__":
    import sys
    counter = 10
    if len(sys.argv) > 1:
        counter = int(sys.argv[1])
    print(counter)
    recvloop(counter)

