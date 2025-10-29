import time
from paho.mqtt.client import Client, MQTTMessage
from paho.mqtt.enums import CallbackAPIVersion
import random
import serial

broker = "broker.emqx.io"
luminosity_topic = "pilyavin/greenhouse/luminosity"
light_status_topic = "pilyavin/greenhouse/light_state"

light_on = b'u'
light_off = b'd'

connection = serial.Serial('COM5', 9600, timeout=1)
time.sleep(2)
connection.write(light_on)

light_state = light_on

def set_light_on() -> str:
    global light_state

    light_state = light_on
    connection.write(light_on)


def set_light_off() -> str:
    global light_state

    light_state = light_off
    connection.write(light_off)


def process_luminosity_data(value: int, client: Client):
    if value > 30 and light_state == light_on:
        print("Setting light off")
        set_light_off()

    elif value <= 30 and light_state == light_off:
        print("Setting light on")
        set_light_on()


def on_connect(client: Client, userdata, flags, reason_code, properties):
    if reason_code == 0:
        print("Connected to MQTT Broker!")
    else:
        print("Failed to connect, return code %d\n", reason_code)


def on_message(client: Client, userdata, message: MQTTMessage):
    val = int(message.payload.decode("utf-8"))
    topic = message.topic
    print(f"Received luminosity value {val} from topic {topic}")
    if topic == luminosity_topic:
        process_luminosity_data(val, client)

    if connection.in_waiting > 0:
        line = connection.readline().decode().strip()
        print(f'Got line from MCU: {line}')

        print(f"Published light responce - {line} to {light_status_topic}")
        client.publish(light_status_topic, line, qos=2)


if __name__ == "__main__":
    client = Client(
        callback_api_version=CallbackAPIVersion.VERSION2,
        client_id=f'MY_CLIENT_ID_{random.randint(10000, 99999)}'
    )
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(broker)

    client.connect(broker)
    client.loop_start()
    print(f"Subcribing to {luminosity_topic}")
    client.subscribe(luminosity_topic, qos=2)

    time.sleep(1800)
    client.disconnect()
    client.loop_stop()
