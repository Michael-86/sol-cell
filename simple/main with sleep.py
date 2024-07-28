import machine
import time
import network
import socket
import utime
import gc

# Define pins for photoresistor and LED (optional)
photoresistor_pin = machine.ADC(27) # Adjust pin as needed

# Function to read light level from photoresistor
def read_light_level():
    # Adjust scaling factor based on your photoresistor
    light_level = photoresistor_pin.read_u16() * (3.3 / 65535)
    return light_level

# Light threshold for sleep/wake
light_threshold = 0.5 # Adjust based on your environment

# Connect to Wi-Fi
ssid = 'gtfast'
password = 'darktitan01'

wlan = network.WLAN(network.STA_IF)
wlan.active(True)
wlan.connect(ssid, password)

# Function to connect to Wi-Fi
def connect_wifi():
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


# Function to read battery voltage
def read_battery_voltage():
    # Set up ADC
    adc = machine.ADC(26)
    raw_value = adc.read_u16()
    voltage = (raw_value / 65535.0) * 3.3 # Convert to voltage
    battery_voltage = voltage * (23 / 10) # Adjust for voltage divider
    return battery_voltage

# HTML to send to browsers
html = """<!DOCTYPE html>
<html>
<head> <title>Pico W</title> </head>
<body> <h1>Pico W Data</h1>
<p>Battery Voltage: {} V</p>
<p>Light Level: {}</p>
</body>
</html>
"""

# Function to create web server
def create_web_server():
    addr = socket.getaddrinfo('0.0.0.0', 90)[0][-1]
    s = socket.socket()
    s.bind(addr)
    s.listen(1)
    print('listening on', addr)

    while True:
        cl, addr = s.accept()
        print('client connected from', addr)   

        request = cl.recv(1024)
        request = str(request)
        print('Content = {}'.format(request))

        battery_voltage = read_battery_voltage()
        light_level = read_light_level()  # Read light level here
        response = html.format(battery_voltage, light_level)  # Format both values

        cl.send('HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n')
        cl.send(response)
        cl.close()

# Main loop
def main():
    while True:
        light = read_light_level()

        if light < light_threshold:
            print("Dark, going to sleep")
            # Optional: turn off LED
            # Enter deep sleep mode (adjust wakeup time as needed)
            machine.deepsleep(60 * 60) # Sleep for 1 hour

        else:
            print("Light, waking up")
            # Do something when awake
            connect_wifi()
            battery_voltage = read_battery_voltage()
            create_web_server()

            time.sleep(1) # Short delay to avoid rapid switching

            # Garbage collection to free memory
            gc.collect()

if __name__ == "__main__":
    main()
