import Content from '../../../sections/content';
import Loader from '../../../sections/loader';
import { useCallback, useEffect, useMemo, useState } from 'react';
import {
    FaBroadcastTower,
    FaClock,
    FaDownload,
    FaInbox,
    FaInfo,
    FaLocationArrow,
    FaSignal,
    FaWifi,
} from 'react-icons/fa';
import Line from '../../../sections/line';

const Status = () => {
    const [loading, set_loading] = useState(true);
    const [meta_data, set_meta_data] = useState({});
    const [error, setError] = useState(null);

    const get_meta_data = useCallback(async () => {
        const res = await fetch('/api', {
            method: 'POST',
            body: JSON.stringify({
                cmd: 'get_status',
            }),
        });
        if (!res.ok) {
            throw res;
        }
        const data = await res.json();
        const date = new Date(data.system_time * 1000);
        const gps_date = new Date(data.CFG_LAST_G_TIME * 1000);
        const result = Object.assign({ system_date: date, gps_date }, data);
        set_meta_data(result);
    }, []);

    useEffect(() => {
        const update = async () => {
            try {
                await get_meta_data();
            } catch (err) {
                console.error(err);
            }

            setTimeout(update, 10000);
        };

        set_loading(true);
        update()
            .catch((err) => {
                console.log(err);
            })
            .finally(() => {
                set_loading(false);
            });
    }, [get_meta_data]);

    const data_mode = useMemo(() => {
        const ref = {
            CAN5_NETPROTO_MQTT: 'MQTT',
            CAN5_NETPROTO_LORARELAY: 'LoRaWAN',
        };
        if (!meta_data.connected) {
            return null;
        }
        const data_mode = meta_data.connected
            .map((item) => ref[item.proto])
            .join(', ');
        const status = meta_data.connected.some((item) => item.connected);

        let stats = meta_data.connected.reduce((acc, item) => {
            if (!item.status) {
                return acc;
            }
            const s = item.status.split(',');

            acc[ref[item.proto]] = acc[ref[item.proto]] || [];
            s.forEach((i) => {
                if (i.trim().startsWith('Last_Rx')) {
                    let t = i.split(':');
                    let date = new Date(parseInt(t[1]) * 1000);
                    t[1] = date.toLocaleString();
                    acc[ref[item.proto]].push(t.join(': '));
                } else {
                    acc[ref[item.proto]].push(i);
                }
            });
            return acc;
        }, {});

        return {
            data_mode,
            status,
            stats,
        };
    }, [meta_data.connected]);

    const on_update_time = () => {
        if (!window.confirm('WARNING!!!\nUpdate Time?')) {
            return;
        }
        set_loading(true);
        fetch('/api', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                cmd: 'update_time',
                system_time: (Date.now() / 1000).toFixed(),
            }),
        })
            .then((response) => {
                if (!response.ok) {
                    setError(error);
                } else {
                    return response.json();
                }
            })
            .then((res) => {
                const date = new Date(res.system_time * 1000);
                const result = Object.assign({}, meta_data, {
                    system_time: res.system_time,
                    system_date: date,
                });
                set_meta_data(result);
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
            })
            .finally(() => {
                set_loading(false);
            });
    };

    const get_location = new Promise((resolve, reject) =>
        navigator.geolocation.getCurrentPosition(
            (position) => {
                resolve(position);
            },
            (error) => {
                reject(error);
            }
        )
    );

    const on_update_gps = () => {
        if (!window.confirm('WARNING!!!\nUpdate GPS?')) {
            return;
        }
        set_loading(true);
        get_location
            .then((position) => {
                fetch('/api', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json',
                    },
                    body: JSON.stringify({
                        cmd: 'set',
                        fields: [
                            {
                                key: 'CFG_LAST_G_LAT',
                                value: (
                                    position.coords.latitude || 0
                                ).toString(),
                            },
                            {
                                key: 'CFG_LAST_G_LNG',
                                value: (
                                    position.coords.longitude || 0
                                ).toString(),
                            },
                            {
                                key: 'CFG_LAST_G_ALT',
                                value: (
                                    position.coords.altitude || 0
                                ).toString(),
                            },
                            {
                                key: 'CFG_LAST_G_TIME',
                                value: (Date.now() / 1000).toFixed(),
                            },
                        ],
                    }),
                })
                    .then((response) => {
                        if (!response.ok) {
                            setError(error);
                        } else {
                            return response.json();
                        }
                    })
                    .then((res) => {
                        const date = new Date(res.CFG_LAST_G_TIME * 1000);
                        const result = Object.assign({}, meta_data, {
                            CFG_LAST_G_LAT: res.CFG_LAST_G_LAT,
                            CFG_LAST_G_LNG: res.CFG_LAST_G_LNG,
                            CFG_LAST_G_ALT: res.CFG_LAST_G_ALT,
                            CFG_LAST_G_TIME: res.CFG_LAST_G_TIME,
                            gps_date: date,
                        });
                        set_meta_data(result);
                    })
                    .catch((error) => {
                        console.error('Error fetching data: ', error);
                    })
                    .finally(() => {
                        set_loading(false);
                    });
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
            })
            .finally(() => {
                set_loading(false);
            });
    };

    const get_hms = useCallback((seconds) => {
        const h = Math.floor(seconds / 3600);
        const m = Math.floor((seconds % 3600) / 60);
        const s = seconds - h * 3600 - m * 60;
        return `${h} h, ${m} min, ${s} sec`;
    }, []);

    return (
        <Content>
            <Loader loading={loading}>
                <Line icon={<FaInfo />} title='Device'>
                    <div className='pure-g'>
                        <div
                            style={{
                                fontWeight: 'bold',
                            }}
                            className='pure-u-1-1 pure-u-md-1-2'
                        >
                            {meta_data.CFG_DEVICE_NAME}
                        </div>
                        <div
                            style={{
                                fontStyle: 'italic',
                            }}
                            className='pure-u-1-1 pure-u-md-1-2'
                        >
                            {meta_data.CFG_DEVICE_ID}
                            {meta_data.CFG_DEVICE_ID_POSTFIX}
                        </div>
                    </div>
                </Line>
                <Line
                    icon={<FaDownload />}
                    title='App Version'
                    value={meta_data.CFG_APP_VERSION}
                />
                <Line
                    icon={<FaClock />}
                    title='Uptime'
                    value={get_hms(meta_data.uptime_sec)}
                />
                <Line icon={<FaClock />} title='System Time'>
                    {meta_data.system_date && (
                        <div className='pure-g'>
                            <div className='pure-u-1-2 pure-u-md-1-2 local-time'>
                                {meta_data.system_date.toDateString() +
                                    ', ' +
                                    meta_data.system_date.toLocaleTimeString()}
                            </div>
                            <div className='pure-u-1-2 pure-u-md-1-2'>
                                <button
                                    type='button'
                                    className='pure-button'
                                    onClick={on_update_time}
                                >
                                    Update Time
                                </button>
                            </div>
                        </div>
                    )}
                </Line>
                <Line icon={<FaLocationArrow />} title='GPS Location'>
                    <div className='pure-g'>
                        <div className='pure-u-1-1 pure-u-md-1-4'>
                            <b>Lat:</b> {meta_data.CFG_LAST_G_LAT}
                        </div>
                        <div className='pure-u-1-1 pure-u-md-1-4'>
                            <b>Lng:</b> {meta_data.CFG_LAST_G_LNG}
                        </div>
                        <div className='pure-u-1-1 pure-u-md-1-4'>
                            <b>Alt:</b> {meta_data.CFG_LAST_G_ALT} m
                        </div>
                        {meta_data.gps_date && (
                            <div className='pure-u-1-1 pure-u-md-1-4 small-local-time'>
                                {meta_data.gps_date.toLocaleDateString() +
                                    ', ' +
                                    meta_data.gps_date.toLocaleTimeString()}
                            </div>
                        )}
                        <div className='pure-u-1-2 pure-u-md-1-2'>
                            <button
                                type='button'
                                className='pure-button'
                                onClick={on_update_gps}
                            >
                                Update GPS
                            </button>
                        </div>
                    </div>
                </Line>
                <Line icon={<FaWifi />} title='Wifi'>
                    {meta_data.wifi && (
                        <ul className='home-network-status'>
                            <li>
                                <h5>Mac Address:</h5> {meta_data.wifi.mac}
                            </li>
                            <li>
                                <h5>Status:</h5> Wifi{' '}
                                {meta_data.wifi.connected
                                    ? 'Connected'
                                    : 'Disconnected'}
                            </li>
                            {meta_data.wifi.connected && (
                                <>
                                    <li>
                                        <h5>SSID:</h5>{' '}
                                        <b>{meta_data.wifi.ssid}</b>
                                    </li>
                                    <li>
                                        <h5>BSSID:</h5> {meta_data.wifi.bssid}
                                    </li>
                                    <li>
                                        <h5>Frequency</h5>: 2.412 GHz (Channel{' '}
                                        {meta_data.wifi.channel})
                                    </li>
                                    <li>
                                        <h5>RSSI:</h5>
                                        {meta_data.wifi.rssi} dBm
                                    </li>
                                    <li>
                                        <h5>IP:</h5> <b>{meta_data.wifi.ip}</b>
                                    </li>
                                </>
                            )}
                        </ul>
                    )}
                </Line>
                <Line icon={<FaSignal />} title='Cellular'>
                    {meta_data.cellular && (
                        <ul className='home-network-status'>
                            <li>
                                <h5>Status:</h5> Cellular{' '}
                                {meta_data.cellular.connected
                                    ? 'Connected'
                                    : 'Disconnected'}
                            </li>
                            {meta_data.cellular.connected && (
                                <>
                                    <li>
                                        <h5>Operator:</h5>{' '}
                                        <b>
                                            {
                                                meta_data.cellular
                                                    .CFG_CELL_OPERATOR
                                            }
                                        </b>
                                    </li>
                                    <li>
                                        <h5>APN:</h5>{' '}
                                        {meta_data.cellular.CFG_CELL_APN}
                                    </li>
                                    <li>
                                        <h5>RSSI:</h5> {meta_data.cellular.rssi}{' '}
                                        dBm
                                    </li>
                                    <li>
                                        <h5>IP:</h5> {meta_data.cellular.ip}
                                    </li>
                                </>
                            )}
                        </ul>
                    )}
                </Line>
                <Line icon={<FaBroadcastTower />} title='LoRa'>
                    {meta_data.lora && (
                        <ul className='home-network-status'>
                            <li>
                                <h5>Status:</h5> LoRa{' '}
                                {meta_data.lora.enabled
                                    ? 'Enabled'
                                    : 'Disabled'}
                            </li>
                            {meta_data.lora.enabled && (
                                <>
                                    <li>
                                        <h5>SNR:</h5>{' '}
                                        {meta_data.lora.CFG_LWAN_STATS_SNR}
                                    </li>
                                    <li>
                                        <h5>RSSI:</h5>{' '}
                                        {meta_data.lora.CFG_LWAN_STATS_RSSI}
                                    </li>
                                </>
                            )}
                        </ul>
                    )}
                </Line>
                <Line icon={<FaInbox />} title='Data Mode'>
                    {data_mode && (
                        <ul className='home-network-status'>
                            <li>
                                <h5>Enabled Protocol:</h5> {data_mode.data_mode}
                            </li>
                            <li>
                                <h5>Status:</h5>{' '}
                                {data_mode.status
                                    ? 'Connected'
                                    : 'Disconnected'}
                            </li>
                            <li>
                                {Object.keys(data_mode.stats).map((key) => (
                                    <ul
                                        key={key}
                                        className='home-network-status'
                                    >
                                        <li>
                                            <h5>{key} Stats:</h5>
                                        </li>
                                        {data_mode.stats[key].map((item) => (
                                            <li className='padded' key={item}>
                                                {item}
                                            </li>
                                        ))}
                                    </ul>
                                ))}
                            </li>
                        </ul>
                    )}
                </Line>
            </Loader>
        </Content>
    );
};

export default Status;
