import { useEffect, useState } from 'react';
import DynamicForm from '../sections/dynamic_form';
import logo from '../../interlab-logo-2.png';
import board from '../../io-board.png';
import Content from '../sections/content';
import Loader from '../sections/loader';

const Sensors = (props) => {
    const [port_sensor_map, set_port_sensor_map] = useState({});
    const [error, setError] = useState(null);
    const [loading, set_loading] = useState(true);

    const fetch_data = () => {
        set_loading(true);
        fetch('/api/sensors')
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((data) => {
                set_port_sensor_map(data);
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
                setError(error);
            })
            .finally(() => {
                set_loading(false);
            });
    };

    const on_change = (key, val) => {
        const new_port_sensor_map = Object.assign({}, port_sensor_map);
        new_port_sensor_map[key] = val;
        set_port_sensor_map(new_port_sensor_map);
    };

    const on_save = () => {
        if (!window.confirm('Save parameters?')) {
            return;
        }
        set_loading(true);
        fetch('/api/sensors', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(port_sensor_map),
        })
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((value) => {
                set_port_sensor_map(value);
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
                setError(error);
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
            <h1>
                <img alt='logo' src={logo} />
                Sensors
            </h1>
            <Content>
                <Loader loading={loading}>
                    <div className='pure-g'>
                        <div className='pure-u-1 pure-u-md-1-5'>
                            <div className='io-board'>
                                <img alt='io-board' src={board} />
                            </div>
                        </div>

                        <div className='pure-u-1 pure-u-md-1-5'>&nbsp;</div>
                        <div className='pure-u-1 pure-u-md-3-5'>
                            <DynamicForm
                                fields={props.fields}
                                values={port_sensor_map}
                                on_change={on_change}
                                save_text='Save'
                                reset_text='Reset'
                                on_save={on_save}
                                on_reset={on_reset}
                            />
                        </div>
                    </div>
                </Loader>
            </Content>
        </>
    );
};

export default Sensors;
