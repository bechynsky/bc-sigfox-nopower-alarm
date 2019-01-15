import http.client
import serial
import time


# Configuration
PORT_NAME = "COM9"
WEB_URL = "www.python.org"

# Connect to serial port
device = serial.Serial(PORT_NAME, 115200)

def deviceUpdate(updateCode):
    device.write(updateCode)
    device.flush()
    print(updateCode)

# simple chcek if server responding
def ping():
    output = ""
    try:
        conn = http.client.HTTPSConnection(WEB_URL)
        conn.request("HEAD", "/")
        r1 = conn.getresponse()

        if (r1.status == 200 or r1.status == 204):
            output = b'o' # 0x6f
    except:
        output = b'e' # 0x65
    
    return output

def main():
    try:
        while True:
            r = ping()
            deviceUpdate(r)
            time.sleep(60)
    except KeyboardInterrupt:
        pass
    finally:
        device.close()

if __name__ == "__main__":
    main()
    