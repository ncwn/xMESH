import logo from '../../interlab-logo-2.png';
import Content from '../sections/content';
import Loader from '../sections/loader';
import DynamicForm from '../sections/dynamic_form';
import { useEffect, useState } from 'react';
import Line from '../sections/line';
import { FaCube } from 'react-icons/fa';
import Ping from '../sections/ping';
import WifiScan from '../sections/wifi-scan';

const WifiAP = (props) => {
    const [wifi_ap_params, set_wifi_ap_params] = useState({});
    const [loading, set_loading] = useState(false);
    const [error, set_error] = useState(null);

    const fetch_data = () => {
        set_loading(true);
        fetch('/api/wifi_ap')
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((data) => {
                set_wifi_ap_params(data);
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
                set_error(error);
            })
            .finally(() => {
                set_loading(false);
            });
    };

    const on_change = (key, val) => {
        const _data = Object.assign({}, wifi_ap_params);
        _data[key] = val;
        set_wifi_ap_params(_data);
    };

    const on_save = () => {
        if (!window.confirm('Save parameters?')) {
            return;
        }
        set_loading(true);
        fetch('/api/wifi_ap', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(wifi_ap_params),
        })
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((value) => {
                set_wifi_ap_params(value);
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
                set_error(error);
            })
            .finally(() => {
                set_loading(false);
            });
    };

    const on_reset = () => {
        if (!window.confirm('Reset parameters?')) {
            return;
        }
        fetch_data();
    };

    useEffect(() => {
        fetch_data();
    }, []);

    if (error) return 'Error...';

    return (
        <>
            <h2>Wifi Access Point</h2>
            <Content>
                <Loader loading={loading}>
                    <DynamicForm
                        fields={props.fields}
                        values={wifi_ap_params}
                        on_change={on_change}
                        save_text='Save'
                        reset_text='Reset'
                        on_save={on_save}
                        on_reset={on_reset}
                    />
                </Loader>
            </Content>
        </>
    );
};

const PingSection = (props) => {
    return (
        <>
            <h2>Ping</h2>
            <Content>
                <Ping />
            </Content>
        </>
    );
};

const WifiSTA = (props) => {
    const [wifi_params, set_wifi_params] = useState({});
    const [error, set_error] = useState(null);
    const [loading, set_loading] = useState(false);

    const fetch_data = () => {
        set_loading(true);
        fetch('/api/wifi_sta')
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((data) => {
                set_wifi_params(data);
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
                set_error(error);
            })
            .finally(() => {
                set_loading(false);
            });
    };

    const on_change = (key, val) => {
        const new_wifi_params = Object.assign({}, wifi_params);
        new_wifi_params[key] = val;
        set_wifi_params(new_wifi_params);
    };

    const on_save = () => {
        if (!window.confirm('Save parameters?')) {
            return;
        }
        set_loading(true);
        fetch('/api/wifi_sta', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(
                Object.assign({ CFG_WIFI_STA_PASS: '' }, wifi_params)
            ),
        })
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((value) => {
                set_wifi_params(value);
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
                set_error(error);
            })
            .finally(() => {
                set_loading(false);
            });
    };

    const on_reset = () => {
        if (!window.confirm('Reset parameters?')) {
            return;
        }
        fetch_data();
    };

    const is_enabled =
        wifi_params['CFG_WIFI_STA_ENABLE'] &&
        wifi_params['CFG_WIFI_STA_ENABLE'] === 'true';

    const on_select_scanned_wifi = (val) => {
        const new_wifi_params = Object.assign({}, wifi_params);
        new_wifi_params['CFG_WIFI_STA_SSID'] = val;
        set_wifi_params(new_wifi_params);
    };

    useEffect(() => {
        fetch_data();
    }, []);

    if (error) return 'Error...';

    return (
        <>
            <h2>Wifi Client</h2>
            <Content>
                <Loader loading={loading}>
                    <DynamicForm
                        fields={props.fields}
                        values={wifi_params}
                        on_change={on_change}
                    />
                    {is_enabled && (
                        <WifiScan on_select={on_select_scanned_wifi} />
                    )}

                    <div className='pure-u-1' style={{ textAlign: 'center' }}>
                        <button
                            type='button'
                            className='pure-button pure-button-primary'
                            onClick={on_save}
                        >
                            Save
                        </button>
                        <button
                            type='button'
                            className='pure-button button-warning'
                            onClick={on_reset}
                        >
                            Reset
                        </button>
                    </div>
                </Loader>
            </Content>
        </>
    );
};

const Cellular = (props) => {
    const [cellular_params, set_cellular_params] = useState({});
    const [loading, set_loading] = useState(false);
    const [error, set_error] = useState(null);

    const fetch_data = () => {
        set_loading(true);
        fetch('/api/cellular')
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((data) => {
                set_cellular_params(data);
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
                set_error(error);
            })
            .finally(() => {
                set_loading(false);
            });
    };

    const on_change = (key, val) => {
        const _data = Object.assign({}, cellular_params);
        _data[key] = val;
        set_cellular_params(_data);
    };

    const on_save = () => {
        if (!window.confirm('Save parameters?')) {
            return;
        }
        set_loading(true);
        fetch('/api/cellular', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(cellular_params),
        })
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((value) => {
                set_cellular_params(value);
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
                set_error(error);
            })
            .finally(() => {
                set_loading(false);
            });
    };

    const on_reset = () => {
        if (!window.confirm('Reset parameters?')) {
            return;
        }
        fetch_data();
    };

    useEffect(() => {
        fetch_data();
    }, []);

    if (error) return 'Error...';

    return (
        <>
            <h2>Cellular</h2>
            <Content>
                <Loader loading={loading}>
                    <DynamicForm
                        fields={props.fields}
                        values={cellular_params}
                        on_change={on_change}
                        save_text='Save'
                        reset_text='Reset'
                        on_save={on_save}
                        on_reset={on_reset}
                    />
                </Loader>
            </Content>
        </>
    );
};

const LoRaWAN = (props) => {
    const [lorawan_params, set_lorawan_params] = useState({});
    const [loading, set_loading] = useState(false);
    const [error, set_error] = useState(null);

    const fetch_data = () => {
        set_loading(true);
        fetch('/api/lorawan')
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((data) => {
                set_lorawan_params(data);
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
                set_error(error);
            })
            .finally(() => {
                set_loading(false);
            });
    };

    const on_change = (key, val) => {
        const _data = Object.assign({}, lorawan_params);
        _data[key] = val;
        set_lorawan_params(_data);
    };

    const on_save = () => {
        if (!window.confirm('Save parameters?')) {
            return;
        }
        set_loading(true);
        fetch('/api/lorawan', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(lorawan_params),
        })
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((value) => {
                set_lorawan_params(value);
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
                set_error(error);
            })
            .finally(() => {
                set_loading(false);
            });
    };

    const on_reset = () => {
        if (!window.confirm('Reset parameters?')) {
            return;
        }
        fetch_data();
    };

    const on_remove_lorawan_context = () => {
        if (
            !window.confirm(
                'WARNING!!!\nReset LoRaWAN context?\nOnce reset device needs to rejoin again.'
            )
        ) {
            return;
        }
        fetch('/api', {
            method: 'POST',
            body: JSON.stringify({ cmd: 'reset_lorawan_context' }),
        })
            .then((response) => {
                if (!response.ok) {
                    set_error(error);
                }
                window.alert(
                    'Cleared LoRaWAN context. Device needs to be rejoined.'
                );
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
            })
            .finally(() => {
                set_loading(false);
            });
    };

    useEffect(() => {
        fetch_data();
    }, []);

    if (error) return 'Error...';

    const is_enabled =
        lorawan_params['CFG_LWAN_ENABLE'] &&
        lorawan_params['CFG_LWAN_ENABLE'] === 'true';

    return (
        <>
            <h2>LoRaWAN</h2>
            <Content>
                <Loader loading={loading}>
                    <DynamicForm
                        fields={props.fields}
                        values={lorawan_params}
                        on_change={on_change}
                        save_text='Save'
                        reset_text='Reset'
                        on_save={on_save}
                        on_reset={on_reset}
                    />
                    {is_enabled && (
                        <Line
                            icon={<FaCube />}
                            title='Remove LoRaWAN Mac Context'
                            value={
                                <button
                                    type='button'
                                    className='pure-button'
                                    onClick={on_remove_lorawan_context}
                                >
                                    Remove
                                </button>
                            }
                        />
                    )}
                </Loader>
            </Content>
        </>
    );
};

const Networks = (props) => (
    <>
        <h1>
            <img alt='logo' src={logo} />
            Networks
        </h1>
        <WifiAP fields={props.fields['wifi_ap']} />
        <WifiSTA fields={props.fields['wifi_sta']} />
        <Cellular fields={props.fields['cellular']} />
        <PingSection fields={props.fields['ping']} />
        <LoRaWAN fields={props.fields['lorawan']} />
    </>
);

export default Networks;
