import time
from paho.mqtt.client import Client
from paho.mqtt.enums import CallbackAPIVersion
import random
import serial

broker = "broker.emqx.io"
luminosity_topic = "pilyavin/greenhouse/luminosity"

get_value = b'p'
stream = b's'

connection = serial.Serial('COM3', 9600, timeout=1)

time.sleep(3)

if __name__ == "__main__":
    client = Client(
        callback_api_version=CallbackAPIVersion.VERSION2,
        client_id=f'MY_CLIENT_ID_{random.randint(10000, 99999)}'
    )
    client.connect(broker)

    client.loop_start()

    connection.write(stream)

    line = connection.readline()
    print(line.decode())
    
    for itteration in range(1800):
        print(f'iter {itteration}')

        connection.write(get_value)
        print('\tCommand GET_VALUE send to MCU')
        time.sleep(0.1)

        if connection.in_waiting > 0:
            line = connection.readline()
            print(f'\tGot line from MCU: {line}')

            value = line.decode().strip().split('SENSOR_VALUE:', 1)[1]
            client.publish(luminosity_topic, value, qos=2)
            print(
                f"Publish luminosity - {value} to {luminosity_topic}")
        time.sleep(10)

    client.loop_stop() 
    client.disconnect()
    connection.close()