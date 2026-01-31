import time
import math

from bottle import route, run, template, post, request
import json
import struct
import binascii
import threading
import requests


THETHINGNETWORK = 'thethingsnetwork'
KERLINK = 'kerlink'
TTN3PROTOCOL = 'http://'

ACK_BYTES_SIZE = 7


class CurrentPacket(object):
    uplink_format = 'BBB{}s'
    downlink_format = 'BB{}s'

    def __init__(self):
        self.pkt_id = 9000 # power level 9000
        self.num_seq = 0
        self.fragments = []
        self.bytes_ack = [0] * ACK_BYTES_SIZE
        self.is_new_pkt = False

    def _process_payload(self, pkt_id, seq_id, num_seq, bin_data):
        # check if the current packet id is the same
        if pkt_id != self.pkt_id:
            # initialize for new packet id
            self.is_new_pkt = True
            self.pkt_id = pkt_id
            self.num_seq = num_seq
            # Basic datatype so no reference, all pointing to different instances
            self.fragments = [None] * num_seq
            self.bytes_ack = [0] * math.ceil(num_seq/8)
        else:
            self.is_new_pkt = False
            self.num_seq = num_seq

        self.fragments[seq_id] = bin_data
        self.bytes_ack[int(seq_id / 8)] |= (1 << (seq_id % 8))

        for i in range(self.num_seq):
            if self.fragments[i] is None:
                return False
        return True

    def encode_downlink(self):
        magic = 0xCA
        ack = bytes()
        for i in range(len(self.bytes_ack)):
            ack += self.bytes_ack[i].to_bytes(1, byteorder='big')
        payload = struct.pack(self.downlink_format.format(len(ack)), magic, self.pkt_id, ack)
        payload = binascii.b2a_base64(payload).decode()
        return payload

    def decode_payload(self, encoded_payload, callback = None):
        payload = binascii.a2b_base64(encoded_payload + '===')  # add padding to avoid size issue
        payload_len = len(payload)
        header_len = struct.calcsize(self.uplink_format.format(0))
        data_len = payload_len - header_len
        fmt = self.uplink_format.format(data_len)
        pkt_id, seq_id, num_seq, bin_data = struct.unpack(fmt, payload)
        print('Received pkt_id: {} seq_id: {}'.format(pkt_id, seq_id))
        if self._process_payload(pkt_id, seq_id, num_seq, bin_data):
            if callback is not None:
                data = bytes()
                for i in range(len(self.fragments)):
                    data += self.fragments[i]
                callback(data)

    def get_is_new_packet(self):
        return self.is_new_pkt


current_packet = CurrentPacket()


def complete_callback(data):
    print("received data ", data)


def send_ack(data):
    time.sleep(0.5)

    current_packet.decode_payload(data['payload'], complete_callback)

    downlink_url = data['meta']['downlink_url_push']

    if current_packet.get_is_new_packet():
        downlink_url = data['meta']['downlink_url_replace']

    payload = current_packet.encode_downlink()

    obj = {
        "downlinks": [
            {
                'frm_payload': payload,
                'f_port': data['meta']['port'],
                'priority': 'NORMAL'
            }
        ]
    }
    headers = {'Authorization': 'Bearer %s' % data['meta']['downlink_key']}
    downlink = requests.post(downlink_url, headers=headers, json=obj)
    print('Downlink', downlink.text)


@post('/lora/uplink')
def lora_test():
    msg = request.json
    payload = msg['uplink_message']['frm_payload']
    device = None
    if not payload:
        return json.dumps({ 'status' : 'error'})
    #payload = binascii.a2b_base64(payload + '===')  # add padding to avoid size issue
    # payload = base64.decodestring(msg['payload_raw'].encode('UTF-8')

    # Channel can be missiong
    channel = 999
    try:
        channel = msg['uplink_message']['rx_metadata'][0]['channel_index']
    except:
        pass

    # snr may be missing
    snr = 999
    try:
        snr = msg['uplink_message']['rx_metadata'][0]['snr']
    except:
        pass

    # rssi may be missing
    rssi = 999
    try:
        rssi = msg['uplink_message']['rx_metadata'][0]['rssi']
    except:
        pass

    data = {
        'meta': {
            'vendor': THETHINGNETWORK,
            'deveui': msg['end_device_ids']['dev_eui'],
            'port': msg['uplink_message']['f_port'],
            'seqno': msg['uplink_message']['f_cnt'],
            'dev_id': msg['end_device_ids']['device_id'],
            'downlink_url_push': TTN3PROTOCOL + request.headers['X-Downlink-Push'],
            'downlink_url_replace': TTN3PROTOCOL + request.headers['X-Downlink-Replace'],
            'downlink_key': request.headers['X-Downlink-Apikey'],
            'frequency': msg['uplink_message']['settings']['frequency'],
            'data_rate': msg['uplink_message']['settings']['data_rate_index'],
            'coding_rate': msg['uplink_message']['settings']['coding_rate'],
            'snr': snr,
            'rssi': rssi,
            'channel': channel,
        },
        'payload': payload,
    }
    t1 = threading.Thread(target=send_ack, args=(data,))
    t1.start()
    return json.dumps({'status': 'ok'})


run(host='0.0.0.0', port=8080)