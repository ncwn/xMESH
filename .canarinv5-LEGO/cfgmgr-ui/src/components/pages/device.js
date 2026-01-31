import { useEffect, useState } from 'react';
import { FaCube } from 'react-icons/fa';
import DynamicForm from '../sections/dynamic_form';
import Line from '../sections/line';
import logo from '../../interlab-logo-2.png';
import Content from '../sections/content';
import Loader from '../sections/loader';

const General = (props) => {
    const [general_params, set_general_params] = useState({});
    const [error, set_error] = useState(null);
    const [loading, set_loading] = useState(true);

    const fetch_data = () => {
        set_loading(true);
        fetch('/api/general')
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((data) => {
                set_general_params(data);
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
        const new_port_sensor_map = Object.assign({}, general_params);
        new_port_sensor_map[key] = val;
        set_general_params(new_port_sensor_map);
    };

    const on_save = () => {
        if (!window.confirm('Save parameters?')) {
            return;
        }
        set_loading(true);
        fetch('/api/general', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(general_params),
        })
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((value) => {
                set_general_params(value);
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
            <h3>General</h3>
            <Content>
                <Loader loading={loading}>
                    <DynamicForm
                        fields={props.fields}
                        values={general_params}
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

const Communication = (props) => {
    const [comm_params, set_comm_params] = useState({});
    const [error, set_error] = useState(null);
    const [loading, set_loading] = useState(true);

    const fetch_data = () => {
        set_loading(true);
        fetch('/api/communication')
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((data) => {
                set_comm_params(data);
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
        const new_port_sensor_map = Object.assign({}, comm_params);
        new_port_sensor_map[key] = val;
        set_comm_params(new_port_sensor_map);
    };

    const on_save = () => {
        if (!window.confirm('Save parameters?')) {
            return;
        }
        set_loading(true);
        fetch('/api/communication', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(comm_params),
        })
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((value) => {
                set_comm_params(value);
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
            <h3>Communication</h3>
            <Content>
                <Loader loading={loading}>
                    <DynamicForm
                        fields={props.fields}
                        values={comm_params}
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

const DeviceAdvanceSettings = (props) => {
    const [upgrade_loading, set_upgrade_loading] = useState(false);
    const [remove_sensor_data_loading, set_remove_sensor_data_loading] =
        useState(false);
    const [error, set_error] = useState(null);

    const on_reboot = () => {
        if (!window.confirm('WARNING!!!\nReboot Device?')) {
            return;
        }
        fetch('/api', {
            method: 'POST',
            body: JSON.stringify({ cmd: 'reboot' }),
        })
            .then((response) => {
                if (!response.ok) {
                    set_error(error);
                }
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
            })
            .finally(() => {});
    };

    const on_firmware_upgrade = () => {
        if (!window.confirm('WARNING!!!\nUpgrade Firmware?')) {
            return;
        }
        set_upgrade_loading(true);
        fetch('/api', {
            method: 'POST',
            body: JSON.stringify({ cmd: 'firmware_upgrade' }),
        })
            .then((response) => {
                if (!response.ok) {
                    set_error(error);
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
                set_upgrade_loading(false);
            });
    };

    const on_remove_sensor_data = () => {
        if (!window.confirm('WARNING!!!\nRemove sensor data?')) {
            return;
        }
        set_remove_sensor_data_loading(true);
        fetch('/api', {
            method: 'POST',
            body: JSON.stringify({ cmd: 'remove_sensor_data' }),
        })
            .then((response) => {
                if (!response.ok) {
                    set_error(error);
                    alert('Error in removing sensor data cache!');
                } else {
                    alert('Sensor data cache removed!');
                }
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
            })
            .finally(() => {
                set_remove_sensor_data_loading(false);
            });
    };

    return (
        <>
            <h3>Advance Settings</h3>
            <Content>
                <Loader loading={remove_sensor_data_loading}>
                    <Line
                        icon={<FaCube />}
                        title='Remove Sensor Data Cache'
                        value={
                            <button
                                type='button'
                                className='pure-button'
                                onClick={on_remove_sensor_data}
                            >
                                Remove
                            </button>
                        }
                    />
                </Loader>
                <Line
                    icon={<FaCube />}
                    title='Reboot'
                    value={
                        <button
                            type='button'
                            className='pure-button'
                            onClick={on_reboot}
                        >
                            Reboot
                        </button>
                    }
                />
                <Loader loading={upgrade_loading}>
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
            </Content>
        </>
    );
};

const Device = (props) => {
    return (
        <>
            <h1>
                <img alt='logo' src={logo} />
                Device
            </h1>
            <General fields={props.fields['general']} />
            <Communication fields={props.fields['communication']} />
            <DeviceAdvanceSettings />
        </>
    );
};

export default Device;
