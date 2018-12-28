#!/usr/bin/python

import socket
import sqlite3
import time
import signal
import sys

def signal_handler(signal, frame):
        print('exitting')
        conn.close()
        sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)

conn = sqlite3.connect('measurements.db')
c = conn.cursor()
conn.execute('pragma foreign_keys=ON')

UDP_IP = ""
UDP_PORT = 1234

sock = socket.socket(socket.AF_INET, # Internet
                             socket.SOCK_DGRAM) # UDP
sock.bind((UDP_IP, UDP_PORT))

while True:
        data, addr = sock.recvfrom(1024) # buffer size is 1024 bytes
        print "received message from {0}:{1}".format(data,addr)
        parts=data.split('-')
        sock.sendto("ACK",addr)
        if len(parts)!=3:
            print "Malformed message"
            print data
        else:
            mac=parts[0]
            measurement_type=parts[1]
            measurement=parts[2]
            if mac=="" or measurement_type=="" or measurement=="":
                print "Malformed message"
                print data
            else:
                found=False
                for row in c.execute(" SELECT * FROM sensors WHERE sensors.mac=? ",(mac,)):
                    found=True
                    break
                if not(found):
                    print "could not find sensor with mac {0}".format(mac)
                    exists=False
                    for row in c.execute("SELECT * FROM unregistered_macs WHERE mac=?",(mac,)):
                        exists=True
                    if exists:
                        print "unregistered mac, already in unregistered_macs db, ignoring"
                    else:
                        c.execute("INSERT INTO unregistered_macs(mac) VALUES(?)", (mac,))
                        print "unregistered mac entering into unregistered_mac table"
                        conn.commit()
                else:
                    c.execute("INSERT INTO measurements(time, sensor, measurement, measurement_type) VALUES(?,?,?,?)", (int(round(time.time())), mac, measurement, measurement_type))
                    print "inserted measurement:\n\ttime:{0}\n\tsensor:{1}\n\tmeasurement_type:{2}\n\tmeasurement:{3}".format(int(round(time.time())), mac, measurement_type, measurement)
                    conn.commit()
# We can also close the connection if we are done with it.
# Just be sure any changes have been committed or they will be lost.
conn.close()
