"""Real HTTP handlers: image-driven delivery and per-device progress."""
import hashlib
import http.client
import importlib.util
import json
import os
import pathlib
import tempfile
import threading
import unittest

spec = importlib.util.spec_from_file_location(
    "oem_http", pathlib.Path(__file__).parents[1] / "server.py"
)
server = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server)


class HTTPService(unittest.TestCase):
    def test_image_delivery_and_tracking(self):
        previous = {key: os.environ.get(key) for key in ["IMAGE_PATH", "OEM_COMMAND"]}
        old_logs = server.LOG_DIR
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            image = root / "image.hex"
            image.write_bytes(b"first-image")
            server.LOG_DIR = root / "logs"
            os.environ.update({"IMAGE_PATH": str(image), "OEM_COMMAND": ""})
            httpd = server.ThreadingHTTPServer(("127.0.0.1", 0), server.Handler)
            thread = threading.Thread(target=httpd.serve_forever, daemon=True)
            thread.start()

            def request(path, service="h3", mac="2A.A9.A5.5410EC"):
                conn = http.client.HTTPConnection("127.0.0.1", httpd.server_port, timeout=3)
                conn.request("GET", path, headers={
                    "Host": mac + "." + service + ".sensornet.info",
                    "X-TS": "0000002A", "X-Version": "3.7.1",
                })
                response = conn.getresponse()
                body = response.read()
                headers = dict(response.getheaders())
                status = response.status
                conn.close()
                return status, body, headers

            try:
                self.assertEqual(request("/health", mac="anything")[0:2], (200, b"OK"))
                self.assertTrue(request("/h3")[1].endswith(b"HEXFILE"))
                _, body, headers = request("/", "update")
                self.assertEqual(body, b"")
                self.assertEqual(headers["MD5"], hashlib.md5(b"first-image").hexdigest())
                self.assertFalse(headers["URL"].startswith("http"))
                self.assertEqual(request("/firmware.hex", "update")[1], b"first-image")
                self.assertEqual(request("/h3")[1], b"200")

                # A different device and a changed image each receive a new offer.
                self.assertTrue(request("/h3", mac="2B.A9.A5.5410EC")[1].endswith(b"HEXFILE"))
                image.write_bytes(b"second-image")
                self.assertTrue(request("/h3")[1].endswith(b"HEXFILE"))

                image.unlink()
                self.assertEqual(request("/firmware.hex", "update")[0], 404)
                _, _, headers = request("/", "update")
                self.assertNotIn("URL", headers)
                self.assertEqual(request("/h3")[1], b"200")
            finally:
                httpd.shutdown()
                httpd.server_close()
                thread.join()
                server.LOG_DIR = old_logs
                for key, value in previous.items():
                    if value is None:
                        os.environ.pop(key, None)
                    else:
                        os.environ[key] = value


if __name__ == "__main__":
    unittest.main()
