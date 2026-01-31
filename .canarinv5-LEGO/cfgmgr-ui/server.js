const express = require('express');
const bodyParser = require('body-parser')
const path = require('path');
const { json } = require('express');
const { fileURLToPath } = require('url');
const fs = require('fs');
const { time } = require('console');
const app = express();
app.use(express.static(path.join(__dirname, 'build')));
app.use(express.json());

const sleep = (waitTimeInMs) => new Promise(resolve => setTimeout(resolve, waitTimeInMs));

const status = {
	"battery_percentage":	33,
	"charging":	true,
    "online_sensors":	[{
        "name":	"PM Sensor",
        "manufacturer":	"PlantTower",
        "version":	"1.0",
        "last_reading":	"PM1_0:5,PM2_5:5,PM10:7",
        "type":	"CAN5_SENSORDRIV_TYPE_PM",
        "port":	"UPORT_1",
        "serial_number": "30:43:50:51:46:55:83:fc:25:0"
    }, {
        "name":	"CO2 Sensor",
        "manufacturer":	"Winson",
        "version":	"1.0",
        "last_reading":	"MHZ16_CO2:533",
        "type":	"CAN5_SENSORDRIV_TYPE_CO2",
        "port":	"UPORT_3"
    }, {
        "name":	"ZE03-NO2 Sensor",
        "manufacturer":	"Winson",
        "version":	"1.0",
        "last_reading":	"NO2:0",
        "type":	"CAN5_SENSORDRIV_TYPE_NO2",
        "port":	"UPORT_5"
    }],
    "uptime_sec":	449,
    "system_time": 1653384168,
    "connected":	[{
        "proto":	"CAN5_NETPROTO_HAZEMON",
        "connected":	true,
        "status": "Rssi: -110, Snr: 4, FCnt_Up: 102323, FCnt_Down: 32323",
    }, {
        "proto":	"CAN5_NETPROTO_LORARELAY",
        "connected":	false
    }],
    "app_version": "0.0.2"
}


app.get('/api/status', function (req, res) {
  return res.json(status)
});

app.get('/api/fields', async function(req, res) {
  
  const data = fs.readFileSync('../components/can5_httpserver/src/fields.json');
  //await sleep(2000);
  return res.json(JSON.parse(data));
});

let general = {
	"CFG_DATA_CYCLE_SEC":	"60",
	"CFG_NETWORK_MODE":	"CAN5_NET_TYPE_LORAWAN",
	"CFG_DEVICE_NAME":	"can5-harbinger",
    "CFG_DEVICE_ORGANIZATION":	"interlab",
	"CFG_PROJECT_ID":	"1",
	"CFG_DEVICE_ID":	"39628670564992",
	"CFG_DEVICE_MODE":	"CAN5_MODE_CONFIG",
    "CFG_APP_VERSION":  "0.1.2",
};

app.get('/api/general', async function (req, res) {
  //await sleep(2000);
  return res.json(general);
});

app.post('/api/general', async function (req, res) {
  general = Object.assign({}, req.body);
  //await sleep(2000);
  return res.json(general);
});

let comm = {
    "CFG_DATA_CYCLE_SEC":	"60",
    "CFG_NETWORK_MODE":	"CAN5_NET_TYPE_LORAWAN",
    "CFG_DEVICE_NAME":	"can5-harbinger",
    "CFG_DEVICE_ORGANIZATION":	"interlab",
    "CFG_PROJECT_ID":	"1",
    "CFG_DEVICE_ID":	"39628670564992",
    "CFG_DEVICE_MODE":	"CAN5_MODE_CONFIG",
    "CFG_APP_VERSION":  "0.1.2",
};

app.get('/api/communication', async function (req, res) {
    //await sleep(2000);
    return res.json(comm);
});

app.post('/api/communication', async function (req, res) {
    comm = Object.assign({}, req.body);
    //await sleep(2000);
    return res.json(general);
});

let sensors = {
    "CFG_ADC_0":	"None",
    "CFG_ADC_1":	"None",
    "CFG_ADC_2":	"None",
    "CFG_ADC_3":	"CAN5_SENSORDRIV_TYPE_CO",
    "CFG_I2C_0":	"None",
    "CFG_I2C_1":	"None",
    "CFG_I2C_2":	"None",
    "CFG_I2C_3":	"None",
    "CFG_I2C_4":	"None",
    "CFG_I2C_5":	"None",
    "CFG_I2C_6":	"None",
    "CFG_I2C_7":	"None",
    "CFG_UART_0":	"None",
    "CFG_UART_1":	"None",
    "CFG_UART_2":	"None",
    "CFG_UART_3":	"CAN5_SENSORDRIV_TYPE_CO2",
    "CFG_UART_4":	"CAN5_SENSORDRIV_TYPE_GPS",
    "CFG_UART_5":	"None",
    "CFG_UART_6":	"None",
    "CFG_UART_7":	"None"
};

app.get('/api/sensors', function (req, res) {
  return res.json(sensors);
});

app.post('/api/sensors', function (req, res) {
  sensors = Object.assign({}, req.body);
  return res.json(sensors);
});

let lorawan = {
    "CFG_LWAN_ENABLE": "true",
	"CFG_LWAN_DEVEUI":	"00:00:00:00",
	"CFG_LWAN_APPEUI":	"99:88:77:66:99:88:77:66",
	"CFG_LWAN_ADAPTIVE_DATA_RATE":	"0",
	"CFG_LWAN_DATA_RATE":	"3",
	"CFG_LWAN_TRANSMIT_POWER":	"0",
	"CFG_LWAN_GPS_CYCLE":	"5"
}


app.get('/api/lorawan', function(req, res) {
  return res.json(lorawan);
});

app.post('/api/lorawan', function(req, res) {
  lorawan = Object.assign({}, req.body);
  lwa = Object.assign({}, lorawan);
  lwa['CFG_LWAN_GPS_CYCLE_STATUS'] = "PARAM_INVALID";
  lwa['CFG_LWAN_TRANSMIT_POWER_STATUS'] = "SAVE_ERROR";
  lwa['CFG_LWAN_DATA_RATE_STATUS'] = "SUCCESS";
  return res.json(lwa);
});

let wifi = {
    "CFG_WIFI_STA_SSID":	"can5",
    "CFG_WIFI_AP_PASS":	"interlab",
    "CFG_HAZEMON_IP":	"203.159.6.98",
    "CFG_HAZEMON_PORT":	"60002"
}


app.get('/api/wifi', function(req, res) {
  return res.json(wifi);
});

app.post('/api/wifi', function(req, res) {
  wifi = Object.assign({}, req.body);
  return res.json(wifi);
});

let wifi_ap = {
    "CFG_WIFI_AP_ENABLE": "true",
    "CFG_DEVICE_NAME":	"can5-harbringer",
    "CFG_WIFI_AP_PASS":	"interlab",
};

app.get('/api/wifi_ap', function(req, res) {
    return res.json(wifi_ap);
});

app.post('/api/wifi_ap', function(req, res) {
    wifi_ap = Object.assign({}, req.body);
    return res.json(wifi_ap);
});

let wifi_sta = {
    "CFG_WIFI_STA_ENABLE": "true",
    "CFG_WIFI_STA_SSID":	"can5",
    //"CFG_WIFI_STA_PASS":	"interlab",
};

app.get('/api/wifi_sta', function(req, res) {
    return res.json(wifi_sta);
});

app.post('/api/wifi_sta', function(req, res) {
    wifi_sta = Object.assign({}, req.body);
    return res.json(wifi_sta);
});

let cellular = {
    "CFG_CELL_ENABLE": "true",
    "CFG_CELL_APN":	"internet",
    //"CFG_WIFI_STA_PASS":	"interlab",
};

app.get('/api/cellular', function(req, res) {
    return res.json(cellular);
});

app.post('/api/cellular', function(req, res) {
    cellular = Object.assign({}, req.body);
    return res.json(cellular);
});
/*
let wifi_sta = {
    "CFG_WIFI_STA_SSID":	"can5",
    "CFG_WIFI_STA_PASS":	"interlab",
}    
app.post('/api/wifi_sta', function(req, res) {
  wifi_sta = Object.assign({}, req.body);
  wifi['CFG_WIFI_STA_SSID'] = wifi_sta['CFG_WIFI_STA_PASS']
  return res.json(wifi_sta);
});
*/
const wifi_scanned_ = {
  'CFG_WIFI_STA_SSID': {
      title: 'SSID',
      type: 'dropdown',
      dropdown: {
        'none': 'None',
        'my_ap': 'my_ap',
        'his_ap': 'his_ap',
        'her_ap': 'her_ap',
        'another_ap': 'another_ap',
      }
    },
  'CFG_WIFI_STA_PASS': {
    title: 'Password',
    type: 'password'
  }
};

const wifi_scanned = [
    {
        'ssid': 'None',
        'rssi': '-55'
    },
    {
        'ssid': 'my_ap',
        'rssi': '-56'
    },
    {
        'ssid': 'his_ap',
        'rssi': '-57'
    },
    {
        'ssid': 'her_ap',
        'rssi': '-58'
    },
    {
        'ssid': 'another_ap',
        'rssi': '-59'
    },
]

let logging = {
    "CFG_LOG_TO_SD":	"true",
    "CFG_LOG_TO_NETSOCK":	"true",
    "CFG_LOG_TO_MQTT":	"true",
    "CFG_LOG_TO_NETSOCK_IP":	"192.168.2.2",
    "CFG_LOG_TO_NETSOCK_PORT":	"9745",
    "CFG_LOG_TO_MQTT_URI":	"mqtt://lora.hazemon.in.th",
    "CFG_LOG_TO_MQTT_PORT":	"1883",
    "CFG_LOG_TO_MQTT_TOPIC":	"logs",
    "CFG_LOG_TO_MQTT_USERNAME":	"rmukhia",
    "CFG_LOG_TO_MQTT_PASSWORD":	"12345678",
}

app.get('/api/logging', function(req, res) {
    return res.json(logging);
});

let crontab = [
    "30 59 23 * * *  pause_lwan\n",
    "0 0 0 * * *    reset_lwan_frame_counters\n",
    "0 2 0 * * *    resume_lwan\n"
].join('');

app.get('/api/crontab', function(req, res) {
    return res.json({ 'crontab' : crontab })
});

app.post('/api/crontab', function(req, res) {
    crontab =  req.body['crontab'];
    return res.json( {'crontab': crontab });
});

const ping_result = { "stdout" : "64 bytes from 8.8.8.8 icmp_seq=1 ttl=114 time=37 ms\n64 bytes from 8.8.8.8 icmp_seq=2 ttl=114 time=35 ms\n64 bytes from 8.8.8.8 icmp_seq=3 ttl=114 time=37 ms\n64 bytes from 8.8.8.8 icmp_seq=4 ttl=114 time=38 ms\n64 bytes from 8.8.8.8 icmp_seq=5 ttl=114 time=46 ms\n64 bytes from 8.8.8.8 icmp_seq=6 ttl=114 time=47 ms\n64 bytes from 8.8.8.8 icmp_seq=7 ttl=114 time=44 ms\n64 bytes from 8.8.8.8 icmp_seq=8 ttl=114 time=41 ms\n8 packets transmitted, 8 received, time 325ms\n"};

app.post('/api/cmds', async function(req, res) {
    cmd = req.body
    switch(cmd.command) {
        case 'scan_wifi':
            await sleep(3000);
            return res.json(wifi_scanned);
        case 'reset_lorawan_context':
            return res.json(wifi_scanned);
        case 'remove_wifi_ssid':
            wifi["CFG_WIFI_STA_SSID"] = '';
            return res.json({status: 'ok'})
        case 'ping_wifi':
            await sleep(3000);
            return res.json(ping_result)
        case 'update_time':
            await sleep(1000);
            return res.json({status: 'ok', system_time: cmd.system_time});
        case 'get_cron_jobs':
            return res.json({ status: 'ok', cron_jobs: ["restart", "clear_sensor_cache", "pause_lwan", "reset_lwan_frame_counters", "resume_lwan"]})
    }
    return res.sendStatus(404);
});

app.get('/manifest.json', function (req, res) {
  res.sendFile(path.join(__dirname, 'build', 'manifest.json'));
});

app.get('/*', function (req, res) {
  res.sendFile(path.join(__dirname, 'build', 'index.html'));
});

app.listen(process.env.PORT || 8080);
