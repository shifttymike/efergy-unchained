#!/bin/sh
set -eu

: "${LAN_INTERFACE:?Set LAN_INTERFACE in oem-server/.env}"
: "${UPDATE_SERVER_IP:?Set UPDATE_SERVER_IP in oem-server/.env}"
: "${DHCP_RANGE_START:?Set DHCP_RANGE_START in oem-server/.env}"
: "${DHCP_RANGE_END:?Set DHCP_RANGE_END in oem-server/.env}"
: "${DHCP_NETMASK:?Set DHCP_NETMASK in oem-server/.env}"
: "${DHCP_ROUTER:?Set DHCP_ROUTER in oem-server/.env}"
: "${UPSTREAM_DNS:?Set UPSTREAM_DNS in oem-server/.env}"

mkdir -p /var/lib/misc
cat >/run/dnsmasq.conf <<EOF
interface=${LAN_INTERFACE}
bind-interfaces
listen-address=${UPDATE_SERVER_IP}
port=53
no-resolv
server=${UPSTREAM_DNS}
domain-needed
bogus-priv
address=/sensornet.info/${UPDATE_SERVER_IP}
dhcp-authoritative
dhcp-range=${DHCP_RANGE_START},${DHCP_RANGE_END},${DHCP_NETMASK},12h
dhcp-option=option:router,${DHCP_ROUTER}
dhcp-option=option:dns-server,${UPDATE_SERVER_IP}
dhcp-leasefile=/var/lib/misc/dnsmasq.leases
log-dhcp
log-facility=-
EOF

dnsmasq --test --conf-file=/run/dnsmasq.conf
exec dnsmasq --keep-in-foreground --conf-file=/run/dnsmasq.conf
