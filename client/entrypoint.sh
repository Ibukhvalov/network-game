#!/bin/sh

SERVER_IP="172.25.0.10"

iptables -P INPUT DROP
iptables -P OUTPUT DROP

iptables -A OUTPUT -p udp -d "$SERVER_IP" --dport 8080 -j ACCEPT
iptables -A INPUT -p udp -s "$SERVER_IP" --sport 8080 -j ACCEPT

exec ./client Player_$(hostname)
