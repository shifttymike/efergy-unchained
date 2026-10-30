"""OEM bench server. Observation mode by default; no firmware bundled."""
import hashlib,json,os,re,time
from threading import Lock
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
LOG_DIR=Path(os.environ.get('LOG_DIR','/logs'))
SALT='6ya!89'
DELIVERED=set()
DELIVERED_LOCK=Lock()
def host_mac(host):
    first=host.split(':')[0].split('.')[:4]
    # OEM hostname is MAC[5].MAC[4].MAC[3].MAC[0:3], not flat MAC order.
    mac=(first[3]+''.join(reversed(first[:3]))).upper() if len(first)==4 else ''
    return mac if re.fullmatch(r'[0-9A-F]{12}',mac) else None
def command_body(mac,timestamp,version,command='HEXFILE'):
    ts=int(timestamp.split(':')[0],16)
    text=f'{mac.upper()}{SALT}{ts:08X}{SALT}{version}{SALT}{command}{SALT}'
    return hashlib.md5(text.encode('ascii')).hexdigest().encode()+command.encode()
def delivery_key(mac,data):
    return mac,hashlib.sha256(data).digest()
def was_delivered(key):
    with DELIVERED_LOCK:return key in DELIVERED
def mark_delivered(key):
    with DELIVERED_LOCK:DELIVERED.add(key)
class Handler(BaseHTTPRequestHandler):
    protocol_version='HTTP/1.1'
    wbufsize=65536
    def reply(self,body=b'200',code=200,headers=None):
        self.send_response(code);self.send_header('Content-Length',str(len(body)));self.send_header('Content-Type','application/octet-stream');self.send_header('Connection','close')
        for k,v in (headers or {}).items():self.send_header(k,v)
        self.end_headers();self.wfile.write(body);self.close_connection=True
    def handle_request(self):
        try:length=int(self.headers.get('Content-Length','0'))
        except ValueError:return self.reply(b'',400)
        if length<0 or length>65536:return self.reply(b'',413)
        body=self.rfile.read(length);host=self.headers.get('Host','');mac=host_mac(host)
        event={'time':time.time(),'method':self.command,'path':self.path,'host':host,'mac':mac,'headers':dict(self.headers),'body_length':len(body),'body_sha256':hashlib.sha256(body).hexdigest()}
        LOG_DIR.mkdir(exist_ok=True)
        with open(LOG_DIR/'requests.jsonl','a') as f:f.write(json.dumps(event)+'\n')
        print(json.dumps(event),flush=True)
        if self.path=='/health':return self.reply(b'OK')
        image=Path(os.environ.get('IMAGE_PATH','/images/firmware.hex'))
        data=image.read_bytes() if image.is_file() else None
        key=delivery_key(mac,data) if data is not None and mac is not None else None
        delivered=key is not None and was_delivered(key)
        if self.path=='/get_key.html':return self.reply(b'TT|a1bCDEFGHa1zZ\n')
        if self.path.startswith('/check_key.html'):return self.reply(b'success')
        if self.path=='/firmware.hex':
            if data is None:return self.reply(b'',404)
            self.reply(data)
            if key is not None:mark_delivered(key)
            return
        if '.update.' in host and self.command=='GET':
            if data is None:return self.reply(b'')
            # OEM splits URL: at the first slash; no http:// scheme is allowed.
            return self.reply(b'',headers={'URL':host.split(':')[0]+'/firmware.hex','MD5':hashlib.md5(data).hexdigest()})
        if os.environ.get('OEM_COMMAND','')=='REBOOT' and mac is not None and self.path in ['/h3','/h3bulk'] and not (LOG_DIR/'reboot-sent.json').exists():
            try:response=command_body(mac,self.headers['X-TS'],self.headers['X-Version'],'REBOOT')
            except (KeyError,ValueError,UnicodeError):return self.reply()
            self.reply(response);(LOG_DIR/'reboot-sent.json').write_text(json.dumps({'time':time.time(),'mac':mac,'timestamp':self.headers['X-TS'],'version':self.headers['X-Version'],'response':response.decode()}));return
        if data is not None and mac is not None and not delivered and self.path in ['/h3','/h3bulk']:
            try:return self.reply(command_body(mac,self.headers['X-TS'],self.headers['X-Version']))
            except (KeyError,ValueError,UnicodeError):return self.reply(b'Missing update context',400)
        return self.reply()
    do_GET=handle_request
    do_POST=handle_request
if __name__=='__main__':ThreadingHTTPServer(('0.0.0.0',8080),Handler).serve_forever()
