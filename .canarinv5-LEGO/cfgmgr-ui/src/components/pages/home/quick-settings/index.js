import Content from '../../../sections/content';
import Loader from '../../../sections/loader';
import { useCallback, useEffect, useState } from 'react';
import equal from 'deep-equal';
import Line from '../../../sections/line';
import SelectInput from '../../../sections/select_input';
import { FaCube } from 'react-icons/fa';
import WifiMode from './wifi-mode';
import CellularMode from './cellular-mode';
import LoRaMode from './lora-mode';
import CustomMode from './custom-mode';

const wifi_mode = {
    CFG_WIFI_STA_ENABLE: 'true',
    CFG_CELL_ENABLE: 'false',
    CFG_LWAN_ENABLE: 'false',
    CFG_MQTT_CONFIGURATION_ENABLE: 'true',
    CFG_MQTT_DATA_ENABLE: 'true',
    CFG_LORARELAY_ENABLE: 'false',
};

const cell_mode = {
    CFG_WIFI_STA_ENABLE: 'false',
    CFG_CELL_ENABLE: 'true',
    CFG_LWAN_ENABLE: 'false',
    CFG_MQTT_CONFIGURATION_ENABLE: 'true',
    CFG_MQTT_DATA_ENABLE: 'true',
    CFG_LORARELAY_ENABLE: 'false',
};

const lora_mode = {
    CFG_WIFI_STA_ENABLE: 'false',
    CFG_CELL_ENABLE: 'false',
    CFG_LWAN_ENABLE: 'true',
    CFG_MQTT_CONFIGURATION_ENABLE: 'false',
    CFG_MQTT_DATA_ENABLE: 'false',
    CFG_LORARELAY_ENABLE: 'true',
};

const GetMode = (netif_data) => {
    if (equal(netif_data, wifi_mode)) return 'WiFi';
    if (equal(netif_data, cell_mode)) return 'Cellular';
    if (equal(netif_data, lora_mode)) return 'LoRa';
    return 'Custom';
};

const Index = (props) => {
    const [loading, set_loading] = useState(true);
    const [netif_data, set_netif_data] = useState({});
    const [netif_mode, set_netif_mode] = useState({});

    const [mode_error, set_mode_error] = useState(null);

    const [data, set_data] = useState({});
    const [status, set_status] = useState({});

    const load_mode = useCallback(async () => {
        const res = await fetch('/api', {
            method: 'POST',
            body: JSON.stringify({
                cmd: 'get',
                fields: [
                    'CFG_WIFI_STA_ENABLE',
                    'CFG_CELL_ENABLE',
                    'CFG_LWAN_ENABLE',
                    'CFG_MQTT_CONFIGURATION_ENABLE',
                    'CFG_MQTT_DATA_ENABLE',
                    'CFG_LORARELAY_ENABLE',
                ],
            }),
        });
        if (!res.ok) {
            throw res;
        }

        const data = await res.json();

        set_netif_data(data);
        return data;
    }, []);

    useEffect(() => {
        set_loading(true);
        load_mode()
            .catch((err) => {
                console.error(err);
            })
            .finally(() => {
                set_loading(false);
            });
    }, []);

    useEffect(() => {
        set_netif_mode(GetMode(netif_data));
    }, [netif_data]);

    const mode_options = {
        WiFi: 'WiFi',
        Cellular: 'Cellular',
        LoRa: 'LoRa',
    };

    if (netif_mode === 'Custom') {
        mode_options['Custom'] = 'Custom';
    }

    const on_save = useCallback(async () => {
        const fields = [];
        switch (netif_mode) {
            case 'WiFi':
                Object.keys(wifi_mode).forEach((key) => {
                    fields.push({
                        key: key,
                        value: wifi_mode[key].toString(),
                    });
                });
                break;
            case 'Cellular':
                Object.keys(cell_mode).forEach((key) => {
                    fields.push({ key: key, value: cell_mode[key].toString() });
                });
                break;
            case 'LoRa':
                Object.keys(lora_mode).forEach((key) => {
                    fields.push({
                        key: key,
                        value: lora_mode[key].toString(),
                    });
                });
                break;
        }

        Object.keys(data).forEach((key) => {
            fields.push({ key: key, value: data[key].toString() });
        });

        const request = { cmd: 'set', fields };

        const req = await fetch('/api', {
            method: 'POST',
            body: JSON.stringify(request),
        });

        if (!req.ok) {
            throw req;
        }

        const res = await req.json();

        const _data_key = Object.keys(res).filter(
            (k) => !k.endsWith('_STATUS')
        );
        const _status_key = Object.keys(res).filter((k) =>
            k.endsWith('_STATUS')
        );

        const _data = _data_key.reduce((acc, key) => {
            if (!Object.keys(wifi_mode).find((k) => k === key)) {
                acc[key] = res[key];
            }
            return acc;
        }, {});

        const mode_status_keys = Object.keys(wifi_mode).map(
            (key) => key + '_STATUS'
        );

        let _error = null;

        const _status = _status_key.reduce((acc, key) => {
            if (!mode_status_keys.find((k) => k === key)) {
                acc[key] = res[key];
            }
            if (res[key] !== 'SUCCESS') {
                _error = res[key];
            }
            return acc;
        }, {});

        set_mode_error(_error);
        set_data(_data);
        set_status(_status);

        if (
            !_error &&
            window.confirm('Settings saved.\nProceed with reboot?')
        ) {
            const res = await fetch('/api', {
                method: 'POST',
                body: JSON.stringify({
                    cmd: 'reboot',
                }),
            });
            if (!res.ok) {
                throw res;
            }

            alert('Rebooting...');
        }
    }, [data]);
    const on_reset = useCallback(async () => {}, []);

    return (
        <Content>
            <Loader loading={loading}>
                <Line
                    icon={<FaCube />}
                    title='Mode'
                    value={
                        <>
                            <SelectInput
                                options={mode_options}
                                selected={netif_mode}
                                on_change={(e) => {
                                    set_netif_mode(e.target.value);
                                }}
                            />
                        </>
                    }
                    status={mode_error}
                />
                {netif_mode === 'WiFi' && (
                    <WifiMode data={data} set_data={set_data} status={status} />
                )}
                {netif_mode === 'Cellular' && (
                    <CellularMode
                        data={data}
                        set_data={set_data}
                        status={status}
                    />
                )}
                {netif_mode === 'LoRa' && (
                    <LoRaMode data={data} set_data={set_data} status={status} />
                )}
                {netif_mode === 'Custom' && (
                    <CustomMode data={data} netif_data={netif_data} />
                )}

                {netif_mode !== 'Custom' && (
                    <div className='pure-u-1' style={{ textAlign: 'center' }}>
                        <button
                            type='button'
                            className='pure-button pure-button-primary'
                            onClick={() => {
                                set_loading(true);
                                on_save().then(() => {
                                    set_loading(false);
                                });
                            }}
                        >
                            Save and Reboot
                        </button>
                        <button
                            type='button'
                            className='pure-button button-warning'
                            onClick={on_reset}
                        >
                            Reset
                        </button>
                    </div>
                )}
            </Loader>
        </Content>
    );
};

export default Index;
