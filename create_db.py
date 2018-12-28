#!/usr/bin/python

import sqlite3

conn = sqlite3.connect('measurements.db')
c = conn.cursor()

conn.execute('pragma foreign_keys=ON')

c.execute('''CREATE TABLE room_types
                     (
                     name TEXT,
                     PRIMARY KEY(name)
                     )''')
c.execute('''CREATE TABLE property_types
                     (
                     name TEXT,
                     PRIMARY KEY(name)
                     )''')
c.execute('''CREATE TABLE measurement_types
                     (
                     name TEXT,
                     PRIMARY KEY(name)
                     )''')
c.execute('''CREATE TABLE properties
                     (
                     name TEXT,
                     property_type TEXT,
                     contact TEXT,
                     notes TEXT,
                     FOREIGN KEY(property_type) REFERENCES property_types(name),
                     PRIMARY KEY(name)
                     )''')
c.execute('''CREATE TABLE sensors
                     (mac TEXT,
                     property TEXT, 
                     name TEXT,
                     room_type TEXT,
                     PRIMARY KEY(mac),
                     FOREIGN KEY(room_type) REFERENCES room_types(name),
                     CONSTRAINT namespace UNIQUE(name,property), 
                     FOREIGN KEY(property) REFERENCES properties(name) ON DELETE CASCADE)''')
c.execute('''CREATE TABLE measurements
                     (time INTEGER,
                     sensor TEXT, 
                     measurement_type TEXT, 
                     measurement REAL,
                     CONSTRAINT namespace UNIQUE(time,sensor,measurement_type,measurement),
                     FOREIGN KEY(measurement_type) REFERENCES measurement_types(name),
                     FOREIGN KEY(sensor) REFERENCES sensors(mac) ON DELETE CASCADE
                    )''')
c.execute('''CREATE TABLE unregistered_macs
                     (mac TEXT
                    )''')

# Insert a row of data
c.execute("INSERT INTO room_types VALUES ('bathroom')") 
c.execute("INSERT INTO room_types VALUES ('bedroom')") 
c.execute("INSERT INTO room_types VALUES ('kitchen')") 
c.execute("INSERT INTO room_types VALUES ('hall')") 
c.execute("INSERT INTO room_types VALUES ('living room')") 
c.execute("INSERT INTO property_types VALUES ('house')") 
c.execute("INSERT INTO property_types VALUES ('flat')") 
c.execute("INSERT INTO measurement_types VALUES ('temperature')") 
c.execute("INSERT INTO measurement_types VALUES ('motion')") 
c.execute("INSERT INTO measurement_types VALUES ('light')") 
c.execute("INSERT INTO measurement_types VALUES ('humidity')") 
c.execute("INSERT INTO properties(name,property_type,contact,notes) VALUES ('Joels Flat','flat','','')") 
#c.execute("INSERT INTO sensors(mac, property,name,room_type) VALUES ('Joels Flat','bedroom 1','bedroom')") 
#c.execute("INSERT INTO measurements(time, sensor, measurement_type, measurement) VALUES ('1477753027',1,'temperature',15.0)") 
# Save (commit) the changes
conn.commit()

# We can also close the connection if we are done with it.
# Just be sure any changes have been committed or they will be lost.
conn.close()
