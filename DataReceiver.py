import paho.mqtt.client as cli
import threading
from DbManager import sensor_data_handler
import urllib.request

MQTT_Topic = "/impetus/rtb/#"
HOST = urllib.request.urlopen("http://169.254.169.254/latest/meta-data/public-ipv4").read().decode('utf-8')
PORT = 1883
KEEP_ALIVE = 60


def msg_handler(mosq, obj, msg):
	t1 = threading.Thread(target=sensor_data_handler, args=(msg,))
	t1.start()


client = cli.Client()
client.on_message = msg_handler
client.connect(HOST, PORT, KEEP_ALIVE)
client.subscribe(MQTT_Topic, 2)
client.loop_forever()
