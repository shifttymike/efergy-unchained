set -eu
mkdir -p /cert
if [ ! -s /cert/server.key ]; then
 /opt/legacy/bin/openssl req -x509 -newkey rsa:1024 -sha1 -nodes -keyout /cert/server.key -out /cert/server.crt -days 30 -subj '/C=GB/ST=London/L=London/O=sensornet/CN=*.sensornet.info/emailAddress=ops@hildebrand.co.uk'
 chmod 600 /cert/server.key
fi
exec /opt/legacy/proxy
