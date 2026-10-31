import pathlib
import struct
import sys
import unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/"tools"))
import userdisk

class DataDiskTests(unittest.TestCase):
    def test_structure(self):
        image=userdisk.create()
        self.assertEqual(len(image),32*1024*1024)
        self.assertEqual(image[510:512],b"\x55\xaa")
        self.assertEqual(struct.unpack_from("<H",image,11)[0],512)
        self.assertEqual(struct.unpack_from("<H",image,19)[0],userdisk.SECTORS)
        self.assertEqual(struct.unpack_from("<H",image,22)[0],userdisk.FAT_SECTORS)
        root=(userdisk.RESERVED+2*userdisk.FAT_SECTORS)*512
        self.assertEqual(image[root:root+11],b"QUSER   DAT")
        self.assertEqual(struct.unpack_from("<I",image,root+28)[0],len(userdisk.MARKER))
        self.assertEqual(image[userdisk.START*512:userdisk.START*512+len(userdisk.MARKER)],userdisk.MARKER)
        for sector in (userdisk.RESERVED,userdisk.RESERVED+userdisk.FAT_SECTORS):
            self.assertEqual(struct.unpack_from("<HHH",image,sector*512),(0xfff8,0xffff,0xffff))
        self.assertEqual(image[root+32:root+64],bytes(32))

if __name__=="__main__":
    unittest.main()
