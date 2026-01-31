import { useCallback, useEffect, useMemo, useState } from 'react';
import Loader from '../../sections/loader';

const SensorCards = (props) => {
    const [loading, set_loading] = useState(true);
    const [sensors, set_sensors] = useState([]);

    const get_sensors = useCallback(async () => {
        const res = await fetch('/api', {
            method: 'POST',
            body: JSON.stringify({ cmd: 'online_sensors' }),
        });
        if (!res.ok) {
            throw res;
        }
        const data = await res.json();
        set_sensors(data);
    }, []);

    useEffect(() => {
        const update = async () => {
            try {
                await get_sensors();
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
    }, []);

    const sensors_ui = useMemo(() => {
        if (!props.sensors_fields) {
            return <></>;
        }
        const decorate_reading = (reading, is_gps) => {
            let result = <></>;
            try {
                result = reading.split(',').map((read) => {
                    let [label, value] = read.split(':');
                    if (is_gps) {
                        value = parseFloat(parseFloat(value).toFixed(6));
                    } else {
                        value = parseFloat(parseFloat(value).toFixed(2));
                    }
                    return (
                        <div key={label} className='pure-u-1-2 pure-u-md-1-2'>
                            <div className='reading-value'>
                                <h5 className='reading-header'>{label}</h5>
                                {value}
                            </div>
                        </div>
                    );
                });
            } catch (err) {
                console.log(err);
            }

            return result;
        };

        const get_sensor_port = (port) => {
            let result = 'Unknown';
            let needle = '';

            if (port.includes('UPORT')) {
                needle = 'UART' + port.slice('UPORT'.length);
            } else if (port.includes('ADPORT')) {
                needle = 'ADC' + port.slice('ADPORT'.length);
            } else {
                needle = port;
            }

            const val = Object.keys(props.sensors_fields).find((key) => {
                return key.slice('CFG_'.length) === needle;
            });

            if (val) {
                result = props.sensors_fields[val].title;
            }

            return result;
        };

        return sensors.map((sensor) => {
            const sensor_type = sensor.type.slice(
                'CAN5_SENSORDRIV_TYPE_'.length
            );
            const is_gps = sensor_type === 'GPS';
            return (
                <div key={sensor.port} className='pure-u-1-2 pure-u-md-1-4'>
                    <ul className='sensor-card '>
                        <li className='type'>{sensor_type}</li>
                        <li className='pure-g reading'>
                            {decorate_reading(sensor.last_reading, is_gps)}{' '}
                        </li>
                        <li className='port'>
                            <span className='s-header'>Port</span>{' '}
                            {get_sensor_port(sensor.port)}
                        </li>
                        <li className='name'>
                            <span className='s-header'>Name</span> {sensor.name}
                        </li>
                        <li className='manufacturer'>
                            <span className='s-header'>Manufacturer</span>{' '}
                            {sensor.manufacturer}
                        </li>
                        {sensor.serial_number && (
                            <li className='serial-number'>
                                <span className='s-header'>Serial Number</span>{' '}
                                {sensor.serial_number}
                            </li>
                        )}
                    </ul>
                </div>
            );
        });
    }, [sensors, props.sensors_fields]);

    return (
        <div className='pure-g sensor-cards'>
            <Loader loading={loading}>{sensors_ui}</Loader>
        </div>
    );
};

export default SensorCards;
