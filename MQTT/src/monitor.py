from paho.mqtt.client import Client, MQTTMessage
from paho.mqtt.enums import CallbackAPIVersion
import random
import time


broker = "broker.emqx.io"

luminosity_topic = "pilyavin/greenhouse/luminosity"
light_status_topic = "pilyavin/greenhouse/light_state"

topics = [
    luminosity_topic,
    light_status_topic
]

def on_connect(client: Client, userdata, flags, reason_code, properties):
    if reason_code == 0:
        print("Connected to MQTT Broker!")
    else:
        print("Failed to connect, return code %d\n", reason_code)


def on_message(client: Client, userdata, message: MQTTMessage):
    data = str(message.payload.decode("utf-8"))
    topic = message.topic
    print(f"Received message {data} from topic {topic}")

if __name__ == "__main__":
    try:
        client = Client(
            callback_api_version=CallbackAPIVersion.VERSION2,
            client_id=f'MY_CLIENT_ID_{random.randint(10000, 99999)}'
        )
        client.on_connect = on_connect
        client.on_message = on_message
        client.connect(broker)

        client.loop_start()
        
        for topic in topics:
            print(f"Subcribing to {topic}")
            client.subscribe(topic, qos=2)

        while True:
            time.sleep(1)

    except KeyboardInterrupt:
        print("Monitoring finisshed....")
    
    finally:
        client.loop_stop()
        client.disconnect()
