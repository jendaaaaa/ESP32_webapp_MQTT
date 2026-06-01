import paho.mqtt.client as mqtt
import time
import random
import ssl

BROKER_URL = "24d796bf3e444971acbc9fef04cf1ef7.s1.eu.hivemq.cloud"
PORT = 8883
USERNAME = "tester"
PASSWORD = "tester123T"

DEVICE_ID = "C3D4" 
PUB_TOPIC = f"glasshouse/{DEVICE_ID}/moisture"
SUB_TOPIC = f"glasshouse/{DEVICE_ID}/commands"

def on_connect(client, userdata, flags, reason_code, properties=None):
    print(f"Connected to HiveMQ Broker!")
    client.subscribe(SUB_TOPIC)
    print(f"Listening for commands [{SUB_TOPIC}]\n")

def on_message(client, userdata, msg):
    payload = msg.payload.decode()
    print(f"Command received [{msg.topic}]: {payload}")
    if payload == "PUMP_ON":
        print(" > Turning the pump ON!")
    elif payload == "PUMP_OFF":
        print(" > Turning the pump OFF!")

###################################
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2) 
client.username_pw_set(USERNAME, PASSWORD)
client.tls_set(tls_version=ssl.PROTOCOL_TLS)
client.on_connect = on_connect
client.on_message = on_message

print("Connecting to broker...")
client.connect(BROKER_URL, PORT)
client.loop_start()

try:
    while True:
        # READ SENSORS
        fake_moisture = random.randint(30, 80)
        time.sleep(5)
        ##############
        
        print(f"Publishing: {fake_moisture}% to {PUB_TOPIC}")
        client.publish(PUB_TOPIC, fake_moisture)

except KeyboardInterrupt:
    print("\nDisconnecting from broker...")
    client.loop_stop()
    client.disconnect()