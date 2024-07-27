import network
import machine
import utime
import socket

# Connect to Wi-Fi
ssid = 'gtfast'
password = 'darktitan01'

wlan = network.WLAN(network.STA_IF)
wlan.active(True)

def connect_to_wifi():
    wlan.connect(ssid, password)
    max_wait = 10
    while max_wait > 0:
        if wlan.status() < 0 or wlan.status() >= 3:
            break
        max_wait -= 1
        print('Waiting for connection...')
        utime.sleep(1)

    if wlan.status() != 3:
        print('Network connection failed. Retrying...')
        connect_to_wifi()
    else:
        print('Connected')
        status = wlan.ifconfig()
        print('IP address:', status[0])

# Initial connection
connect_to_wifi()

# Set up ADC
adc = machine.ADC(26)

# Function to read voltage
def read_voltage():
    raw_value = adc.read_u16()
    voltage = (raw_value / 65535.0) * 3.3  # Convert to voltage
    battery_voltage = voltage * (23 / 10)  # Adjust for voltage divider
    return battery_voltage

# HTML to send to browsers
html = """<!DOCTYPE html>
<html>
    <head> <title>Pico W</title> </head>
    <body> <h1>Pico W Battery Voltage</h1>
        <p>Battery Voltage: {}</p>
    </body>
</html>
"""

# Initialize connection count
connection_count = 0

# Open socket
addr = socket.getaddrinfo('0.0.0.0', 90)[0][-1]
s = socket.socket()
s.bind(addr)
s.listen(1)

print('Listening on', addr)

# Listen for connections
while True:
    if wlan.status() != 3:
        print('Wi-Fi disconnected. Reconnecting...')
        connect_to_wifi()

    cl, addr = s.accept()
    print('Client connected from', addr)
    request = cl.recv(1024)
    request = str(request)
    print('Content = {}'.format(request))

    # Increment connection count
    connection_count += 1

    battery_voltage = read_voltage()
    response = html.format(battery_voltage)

    cl.send('HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n')
    cl.send(response)
    cl.close()

    # Decrement connection count
    connection_count -= 1

    # If no active connections, enter sleep mode
    if connection_count == 0:
        print('Entering sleep mode...')
        wlan.active(False)  # Disable Wi-Fi
        machine.deepsleep()  # Enter deep sleep
