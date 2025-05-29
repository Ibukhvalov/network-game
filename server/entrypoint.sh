#!/bin/sh

iptables -P INPUT DROP
iptables -P OUTPUT DROP

iptables -A INPUT -p udp --dport 8080 -j ACCEPT
iptables -A OUTPUT -p udp --sport 8080 -j ACCEPT

exec ./server
