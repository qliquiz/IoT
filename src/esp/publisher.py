import time
import paho.mqtt.client as client
import random

broker = "broker.emqx.io"

client = client.Client(client.CallbackAPIVersion.VERSION1, 'isu10012300')

print("Connecting to broker", broker)
print(client.connect(broker))
client.loop_start()
print("Publishing")

for _ in range(10):
    state = "on" if random.randint(0, 1) else "off"
    print(f"state is {state}")
    client.publish("esp8266/command", state)
    time.sleep(random.randint(4, 10))

client.disconnect()
client.loop_stop()
