"""Probe an OEM update server using a synthetic MAC."""
import http.client,json,sys
host=sys.argv[1] if len(sys.argv)>1 else '192.168.23.30'
fake_mac='01.00.00.020000'  # maps to locally administered 02:00:00:00:00:01
def request(method,path,service='h3',body=None,mac=fake_mac):
 conn=http.client.HTTPConnection(host,80,timeout=5)
 conn.request(method,path,body=body,headers={'Host':mac+'.'+service+'.sensornet.info','X-Version':'3.7.1','X-TS':'0000002A','Content-Type':'application/eh-ping'})
 r=conn.getresponse();data=r.read();headers=dict(r.getheaders());status=r.status;conn.close();assert int(headers['Content-Length'])==len(data);return status,data,headers
assert request('GET','/health')[0:2]==(200,b'OK')
assert request('GET','/get_key.html','keys')[0:2]==(200,b'TT|a1bCDEFGHa1zZ\n')
assert request('GET','/check_key.html?p=TT&ts=0000002A&h=fixture','keys')[0:2]==(200,b'success')
assert request('POST','/h3',body=b'',mac=fake_mac)[1].endswith(b'HEXFILE')
s,d,h=request('GET','/','update',mac=fake_mac);assert s==200 and d==b'' and 'URL' in h and 'MD5' in h
s,image,_=request('GET','/firmware.hex','update',mac=fake_mac);assert s==200 and image
assert request('POST','/h3',body=b'',mac=fake_mac)[1]==b'200'
assert request('POST','/h3bulk',body=b'\0'*4096,mac=fake_mac)[0:2]==(200,b'200')
conn=http.client.HTTPConnection(host,80,timeout=5);conn.request('GET','/',headers={'Host':'other.example'});r=conn.getresponse();assert r.status==200;r.read();conn.close()
print(json.dumps({'host':host,'HTTP':'passed','key_endpoints':'passed','telemetry':'passed','bulk':'passed','synthetic_device_update':'passed','unrestricted_local_requests':'passed'}))
