import hashlib,importlib.util,pathlib,unittest
spec=importlib.util.spec_from_file_location('oem_server',pathlib.Path(__file__).parents[1]/'server.py');server=importlib.util.module_from_spec(spec);spec.loader.exec_module(server)
class Protocol(unittest.TestCase):
 def test_mac(self):
  for service in ['h3','keys','update']:
   self.assertEqual(server.host_mac('2A.A9.A5.5410EC.'+service+'.sensornet.info'),'5410ECA5A92A')
  self.assertEqual(server.host_mac('2A.A9.A5.5410EC.keys.sensornet.info'),'5410ECA5A92A')
  self.assertIsNone(server.host_mac('unrelated.example'))
 def test_live_request_hash(self):
  # Captured OEM request: confirms actual MAC order independently of command handling.
  mac=server.host_mac('2A.A9.A5.5410EC.h3.sensornet.info')
  digest=hashlib.md5(f'{mac}|6AE31677|a1bCDEFGHa1zZ'.encode()).hexdigest()
  self.assertEqual(digest[-16:],'d8d026e650743f44')
 def test_recovered_command_fixture(self):
  # Recovered literal format and argument order from PIC addresses 0x5e6a,
  # 0x7e64 and 0x11ce7; the live OEM exchange is still required validation.
  self.assertEqual(server.command_body('5410eca5a92a','0000002A','3.7.1'),b'9f0d85ad60a9d260c881f51a0a402508HEXFILE')
  self.assertEqual(server.command_body('5410ECA5A92A','0000002A:123','3.7.1'),server.command_body('5410ECA5A92A','0000002A','3.7.1'))
  self.assertNotEqual(server.command_body('5410ECA5A92A','0000002B','3.7.1'),server.command_body('5410ECA5A92A','0000002A','3.7.1'))

class DeliveryProgress(unittest.TestCase):
 def test_device_and_image_progress_without_allowlist(self):
  a=server.delivery_marker('5410ECA5A92A',b'one')
  self.assertNotEqual(a,server.delivery_marker('5410ECA5A92B',b'one'))
  self.assertNotEqual(a,server.delivery_marker('5410ECA5A92A',b'two'))

if __name__=='__main__':unittest.main()
