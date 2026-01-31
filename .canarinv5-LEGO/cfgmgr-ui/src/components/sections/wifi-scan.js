import Line from './line';
import { FaCube } from 'react-icons/fa';
import SelectInput from './select_input';
import Loader from './loader';
import { useState } from 'react';

const WifiScan = (props) => {
    // two states, before scan, and after scan
    const [scan_state, set_scan_state] = useState('before-scan');
    const [loading, set_loading] = useState(false);
    const [wifi_available, set_wifi_available] = useState([]);
    const [selected, set_selected] = useState('');

    const scan_wifi = () => {
        set_loading(true);
        fetch('/api', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ cmd: 'wifi_scan' }),
        })
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((val) => {
                let params = {};
                val.forEach((elem) => {
                    params[elem['ssid']] =
                        elem['ssid'] + ' [rssi: ' + elem['rssi'] + ']';
                });
                set_wifi_available(params);
                set_selected(Object.keys(params)[0]);
                set_scan_state('after-scan');
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
            })
            .finally(() => {
                set_loading(false);
            });
    };

    const on_change_select = (key) => {
        set_selected(key);
    };

    const on_select = () => {
        props.on_select(selected);
    };

    let display;

    if (scan_state === 'before-scan') {
        display = (
            <Line
                icon={<FaCube />}
                title='Scan WiFi'
                value={
                    <button
                        type='button'
                        className='pure-button'
                        onClick={scan_wifi}
                    >
                        Scan
                    </button>
                }
            />
        );
    } else {
        display = (
            <Line
                icon={<FaCube />}
                title='WiFi Available'
                value={
                    <>
                        <SelectInput
                            options={wifi_available}
                            selected={selected}
                            on_change={(e) => on_change_select(e.target.value)}
                        />
                        <button
                            type='button'
                            className='pure-button'
                            onClick={on_select}
                        >
                            Select
                        </button>
                        <button
                            type='button'
                            className='pure-button'
                            onClick={scan_wifi}
                        >
                            Re-Scan
                        </button>
                    </>
                }
            />
        );
    }

    return <Loader loading={loading}>{display}</Loader>;
};

export default WifiScan;
