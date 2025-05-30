#!/bin/sh

iptables -P INPUT DROP
iptables -P OUTPUT DROP

CLIENTS_PORT_RANGE="8081:8085"

iptables -A INPUT -p udp -s 172.25.0.11 --sport "$CLIENTS_PORT_RANGE" -j ACCEPT
iptables -A INPUT -p udp -s 172.25.0.12 --sport "$CLIENTS_PORT_RANGE" -j ACCEPT
iptables -A INPUT -p udp -s 172.25.0.13 --sport "$CLIENTS_PORT_RANGE" -j ACCEPT
iptables -A INPUT -p udp -s 172.25.0.14 --sport "$CLIENTS_PORT_RANGE" -j ACCEPT

iptables -A OUTPUT -p udp -d 172.25.0.11 --dport "$CLIENTS_PORT_RANGE" -j ACCEPT
iptables -A OUTPUT -p udp -d 172.25.0.12 --dport "$CLIENTS_PORT_RANGE" -j ACCEPT
iptables -A OUTPUT -p udp -d 172.25.0.13 --dport "$CLIENTS_PORT_RANGE" -j ACCEPT
iptables -A OUTPUT -p udp -d 172.25.0.14 --dport "$CLIENTS_PORT_RANGE" -j ACCEPT

exec ./server
