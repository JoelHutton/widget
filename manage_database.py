#!/usr/bin/python

import sqlite3
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

while True:
    command=raw_input("what would you like to add/remove?(p/s/h/a/e/q):")
    if command=="p":
        print "properties(name,property_type,contact,notes)"
        for row in c.execute("SELECT * FROM properties"):
            print row
        if raw_input("add or delete (a/d):")=="d":
            c.execute("DELETE FROM properties WHERE name=?", (raw_input("name:"),)) 
        else:
            print "property types:"
            for prop in c.execute("SELECT * FROM property_types"):
                print "{0}".format(prop[0])
            c.execute("INSERT INTO properties(name,property_type,contact,notes) VALUES (?,?,?,?)", (raw_input("name:"),raw_input("property type:"),raw_input("contact:"),raw_input("notes:"))) 
        conn.commit()
        print "properties(name,property_type,contact,notes)"
        for row in c.execute("SELECT * FROM properties"):
            print row
    elif command=="s":
        print "sensors(mac,property,name,room_type)"
        for row in c.execute("SELECT * FROM sensors"):
            print row
        if raw_input("add or delete (a/d):")=="d":
            c.execute("DELETE FROM sensors WHERE mac=?", (raw_input("mac:"),)) 
        else:
            print "properties:"
            for prop in c.execute("SELECT * FROM unregistered_macs"):
                print "{0}".format(prop[0])
            print "room types:"
            for prop in c.execute("SELECT * FROM room_types"):
                print "{0}".format(prop[0])
            unregistered_macs=c.execute("SELECT * FROM unregistered_macs")
            mac_array=[]
            print "unregistered_macs:"
            i=0
            for row in unregistered_macs:
                print "{0}:{1}".format(i,row[0])
                mac_array.append(row[0])
                i+=1
            response=raw_input("to register one of these macs enter its index, simply hit return to use a different mac:")
            use_existing=False
            mac_index=""
            for char in response:
                if char.isdigit():
                    mac_index += char
                    use_existing=True
            mac_index=int(mac_index)
            if use_existing and mac_index<i:
                mac=mac_array[mac_index]
                print "using {0} as mac".format(mac)
                c.execute("INSERT INTO sensors(mac,property,name,room_type) VALUES (?,?,?,?)", (mac,raw_input("property:"),raw_input("sensor name:"),raw_input("room type:"))) 
                c.execute("DELETE FROM unregistered_macs WHERE mac=?",(mac,))
            else:
                print "you will need to enter a mac manually"
                c.execute("INSERT INTO sensors(mac,property,name,room_type) VALUES (?,?,?,?)", (raw_input("mac:"),raw_input("property:"),raw_input("sensor name:"),raw_input("room type:"))) 
            conn.commit()
        print "sensors(property,name,room_type)"
        for row in c.execute("SELECT * FROM sensors"):
            print row
        conn.commit()
    elif command=="a":
        print "properties(name,property_type,contact,notes)"
        for row in c.execute("SELECT * FROM properties"):
            print row
        print "sensors(property,name,room_type)"
        for row in c.execute("SELECT * FROM sensors"):
            print row
    elif command=="q":
        for row in conn.execute(raw_input("query:")):
            print row
        conn.commit()
    elif command=="e":
        conn.close()
        exit(0)
    else:
        print "type p for property,r for room,s for sensor, e to exit, a to display all, q to directly type query or  h to see this"
    
