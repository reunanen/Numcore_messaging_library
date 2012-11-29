"""
               Copyright 2009 Numcore Ltd

 Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at
          http://www.boost.org/LICENSE_1_0.txt)
"""

import unittest
import slaim
import claim
import numsprew

class TestMessageConversion(unittest.TestCase):

    def setUp(self):
        pass

    def testMsgConversionBackAndForth(self):
        self.amsg = claim.AttributeMessage()
        self.amsg.type = 'Foo'
        self.amsg.attrs['Bar'] = 'Baz'
        self.amsg.attrs['Zup'] = 'Zap'
        self.smsg = self.amsg.to_raw_message()
        self.assertEqual(self.smsg.type, 'Foo')
        self.amsg2 = claim.AttributeMessage(self.smsg)
        self.assertEqual(self.amsg2.type, 'Foo')
        self.assertEqual(self.amsg.attrs['Bar'], self.amsg2.attrs['Bar'])
        self.assertEqual(self.amsg.attrs, self.amsg2.attrs)

if __name__ == '__main__':
    unittest.main()