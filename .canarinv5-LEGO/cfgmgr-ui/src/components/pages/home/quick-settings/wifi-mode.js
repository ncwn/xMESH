import { useCallback, useEffect, useState } from 'react';
import Line from '../../../sections/line';
import { FaCube } from 'react-icons/fa';
import PasswordInput from '../../../sections/password_input';
import Loader from '../../../sections/loader';
import Ping from '../../../sections/ping';
import WifiScan from '../../../sections/wifi-scan';

const WifiMode = (props) => {
    const [loading, set_loading] = useState(false);
    const load_data = useCallback(async () => {
        set_loading(true);
        const res = await fetch('/api', {
            method: 'POST',
            body: JSON.stringify({
                cmd: 'get',
                fields: ['CFG_WIFI_STA_SSID', 'CFG_WIFI_STA_PASS'],
            }),
        });
        if (!res.ok) {
            throw res;
        }
        const data = await res.json();
        props.set_data(data);
    }, []);
    const on_firmware_upgrade = () => {
        if (!window.confirm('WARNING!!!\nUpgrade Firmware?')) {
            return;
        }
        set_loading(true);
        fetch('/api', {
            method: 'POST',
            body: JSON.stringify({ cmd: 'firmware_upgrade' }),
        })
            .then((response) => {
                if (!response.ok) {
                    alert('Upgrade not successful!');
                } else {
                    alert(
                        'Upgrade underway! Please check device after sometime.'
                    );
                }
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
            })
            .finally(() => {
                set_loading(false);
            });
    };

    useEffect(() => {
        load_data()
            .catch((err) => {
                console.error(err);
            })
            .finally(() => {
                set_loading(false);
            });
    }, []);

    return (
        <Loader loading={loading}>
            <WifiScan
                on_select={(ssid) =>
                    props.set_data({ ...props.data, CFG_WIFI_STA_SSID: ssid })
                }
            />
            <Line
                icon={<FaCube />}
                title='WiFi SSID'
                value={
                    <input
                        value={props.data.CFG_WIFI_STA_SSID}
                        onChange={(e) => {
                            props.set_data({
                                ...props.data,
                                CFG_WIFI_STA_SSID: e.target.value,
                            });
                        }}
                    />
                }
                status={props.status.CFG_WIFI_STA_SSID_STATUS}
            />
            <Line
                icon={<FaCube />}
                title='WiFi Password'
                value={
                    <PasswordInput
                        value={props.data.CFG_WIFI_STA_PASS}
                        on_change={(e) => {
                            props.set_data({
                                ...props.data,
                                CFG_WIFI_STA_PASS: e.target.value,
                            });
                        }}
                    />
                }
                status={props.status.CFG_WIFI_STA_PASS_STATUS}
            />
            <Ping />
            <Line
                icon={<FaCube />}
                title={'Update Firmware'}
                value={
                    <button
                        type='button'
                        className='pure-button'
                        onClick={on_firmware_upgrade}
                    >
                        Update Firmware
                    </button>
                }
            />
        </Loader>
    );
};

export default WifiMode;
