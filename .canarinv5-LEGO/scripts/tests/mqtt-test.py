import binascii
import socket
import time
from datetime import datetime

from paho.mqtt import client as mqtt_client
import json

from canarin_loramsg2 import canarin_loramsg as cl2

broker = 'ttn.hazemon.in.th'
port = 1883
node = 'can5-prototype'
topic = 'v3/canarinv5-dev/devices/{}/up'.format(node)
client_id = 'mqtt-test'

publish_topic = 'v3/canarinv5-dev/devices/{}/down/replace'.format(node)

username = 'canarinv5-dev'
password = 'NNSXS.KOHDOJAT5PV7BOLNXEGQABN6DNJQOA4VTQBSJLI.5I26M4ZQJKHMQMP5J7ZWIYCGXMAMAUBZXTDPBBJWLOTLEIKSVMWQ'

seq_ids = []
gtimestamp = 0
client = None


def on_connect(_client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT Broker!")
    else:
        print("Failed to connect, return code %d\n", rc)


def on_message(_client, userdata, msg):
    msg_json = json.loads(msg.payload.decode())
    # received_at = msg_json['received_at']
    # print(received_at)
    if 'frm_payload' not in msg_json['uplink_message']:
        return
    payload = msg_json['uplink_message']['frm_payload']
    print(payload)
    payload = binascii.a2b_base64(payload + '===')  # add padding to avoid size issue
    print(msg_json['end_device_ids'])
    parse(payload)


def get_data(sensor_type, _id, timestamp, value):
    print("#1:  ts: {}\t sensor_type: {}\t id: {}\t value:{}".format(timestamp, sensor_type, _id, value))
    print("#2:  ", datetime.fromtimestamp(timestamp))


def publish_ack():
    num_data = len(seq_ids)
    id_array = cl2.intArray(num_data)
    print(seq_ids)
    for i, d in enumerate(seq_ids):
        print(i, d)
        id_array[i] = d

    ack_pkt = cl2.make_ack_packet(num_data, id_array)
    cl2.hton_s_packet_ack(ack_pkt)
    _bin = cl2.cdata(ack_pkt, cl2.get_packet_ack_length(ack_pkt))
    reply_payload = binascii.b2a_base64(_bin).decode()

    payload = {
        "downlinks": [{
            "f_port": 21,
            "frm_payload": reply_payload,
            "priority": "NORMAL"
        }]
    }
    client.publish(publish_topic, json.dumps(payload))


def parse(payload):
    global gtimestamp
    global seq_ids
    pkt = cl2.bin_to_s_packet(payload)

    if cl2.ntoh_s_packet(pkt, len(payload)) == 0:
        return

    n_data = cl2.get_packet_n_data(pkt)
    timestamp = cl2.get_packet_timestamp(pkt)
    for i in range(n_data):
        s_data = cl2.parse_packet(pkt, i)
        value = 0
        if s_data.sensor_type in [cl2.ST_GLAT, cl2.ST_GLNG]:
            value = cl2.parse_sensor_data_float(s_data)
        else:
            value = cl2.parse_sensor_data_int(s_data)

        if timestamp != gtimestamp:
            gtimestamp = timestamp
            seq_ids = [s_data.seq]
        elif s_data.seq not in seq_ids:
            seq_ids.append(s_data.seq)

        get_data(s_data.sensor_type, s_data.seq, timestamp, value)
        if len(seq_ids) > 0:
            publish_ack()


if __name__ == '__main__':
    client = mqtt_client.Client(client_id)
    client.username_pw_set(username, password)
    client.on_connect = on_connect
    client.connect(broker, port)
    client.subscribe(topic)
    client.on_message = on_message
    client.loop_forever()
