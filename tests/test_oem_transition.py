"""Carrier bounds and compressed input validation against the linked images."""
import random,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import package_oem_transition as transition
from package_firmware import read_hex,APP_BASE,BOOT_BASE,APP_SIZE

class Transition(unittest.TestCase):
    def test_reject_damaged_package(self):
        original=(ROOT/'build/efergy-update.efu').read_bytes()
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'damaged.efu'
            for offset in [8,12,16,20,56,63,64,len(original)-1]:
                data=bytearray(original);data[offset]^=1;path.write_bytes(data)
                with self.assertRaises(ValueError):
                    transition.prepare(ROOT/'build/efergy-factory.hex',path,Path(directory)/'out')

    def test_codec_boundaries(self):
        rng=random.Random(42)
        samples=[b'',b'\xff',b'\xff'*128,b'\xff'*129,bytes(range(256))*5]
        samples += [bytes(rng.choice([255,255,rng.randrange(256)]) for _ in range(n)) for n in [63,64,127,128,129,4096]]
        for data in samples:self.assertEqual(transition.decode(transition.encode(data),len(data)),data)
        for blob,size in [(b'\x01a',2),(b'\xff',127),(b'\x80',2)]:
            with self.assertRaises(ValueError):transition.decode(blob,size)

    def test_actual_carrier(self):
        folder=ROOT/'build/transition';carrier=read_hex(folder/'efergy-oem-transition.hex')
        factory=read_hex(ROOT/'build/efergy-factory.hex')
        app=(ROOT/'build/efergy-update.efu').read_bytes()
        blob=(folder/'transition-blob.bin').read_bytes()
        self.assertLessEqual(transition.BLOB_BASE+len(blob),transition.ENTRY_STUB)
        self.assertLess((folder/'efergy-oem-transition.hex').stat().st_size+4096,0x4f000)
        self.assertLess(max(carrier),transition.ENTRY_STUB)
        self.assertEqual(bytes(carrier[x] for x in range(4)),bytes.fromhex('00ef02f0'))
        packed_app=transition.encode(app)
        self.assertEqual(transition.decode(blob[:len(packed_app)],64+APP_SIZE),app)
        loader=transition.decode(blob[len(packed_app):],0x20000-BOOT_BASE)
        self.assertEqual(loader,bytes(factory.get(x,255) for x in range(BOOT_BASE,0x20000)))
        # Model the stock vector relocation, then the converter's loader write
        # and the clean loader's active-slot application install.
        installed=dict(carrier)
        installed.update({transition.ENTRY_STUB+i:carrier[i] for i in range(4)})
        installed.update({i:v for i,v in enumerate(bytes.fromhex('00efe6f0'))})
        installed.update({BOOT_BASE+i:v for i,v in enumerate(loader)})
        installed.update({APP_BASE+i:v for i,v in enumerate(app[64:])})
        for address,value in factory.items():self.assertEqual(installed.get(address,255),value,hex(address))

if __name__=='__main__':unittest.main()
