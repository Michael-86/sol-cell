import network
import socket
import machine
import utime
from ota import OTAUpdater
from WIFI_CONFIG import SSID, PASSWORD

# Ota update source
firmware_url = "https://github.com/Michael-86/sol-cell/tree/main"

wlan = network.WLAN(network.STA_IF)
wlan.active(True)
wlan.connect(SSID, PASSWORD)

ota_updater = OTAUpdater(SSID, PASSWORD, firmware_url, "main.py")
ota_updater.download_and_install_update_if_available()

# Wait for connection
max_wait = 10
while max_wait > 0:
    if wlan.status() < 0 or wlan.status() >= 3:
        break
    max_wait -= 1
    print('waiting for connection...')
    utime.sleep(1)

if wlan.status() != 3:
    raise RuntimeError('network connection failed')
else:
    print('connected')
    status = wlan.ifconfig()
    print('ip = ' + status[0])

# Set up ADC
adc = machine.ADC(26)

# Function to read voltage
def read_voltage():
    raw_value = adc.read_u16()
    voltage = (raw_value / 65535.0) * 3.3  # Convert to voltage
    battery_voltage = voltage * (20 / (10 + 20))  # Adjust for voltage divider
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
addr = socket.getaddrinfo('0.0.0.0', 80)[0][-1]
s = socket.socket()
s.bind(addr)
s.listen(1)

print('listening on', addr)

# Listen for connections
while True:
    cl, addr = s.accept()
    print('client connected from', addr)
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
