import { useEffect, useState } from 'react';
import { FaClock, FaDownload, FaInfo, FaWifi } from 'react-icons/fa';
import Line from '../sections/line';
import logo from '../../interlab-logo-2.png';
import YesNoIcon from '../sections/yes_no_icon';
import Content from '../sections/content';
import Loader from '../sections/loader';
import SensorsCards from '../sections/sensors_cards';

const ConnectedProtocols = (props) => {
    const get_title_case = (str) => {
        let res = str.slice('CAN5_NETPROTO_'.length).toLowerCase();
        return res.substring(0, 1).toUpperCase() + res.substring(1);
    };

    const make_tag = (elem) => {
        const sty = elem.connected ? ' connected-success' : 'connected-failure';
        const status = elem.status ? elem.status : '';
        return (
            <div className={sty}>
                <div className='proto'>
                    {get_title_case(elem.proto)}{' '}
                    <YesNoIcon value={elem.connected} />
                </div>{' '}
                <div className='status'>{status}</div>
            </div>
        );
    };

    const result = props.list.map((elem) => make_tag(elem));

    return <>{result}</>;
};

const Status = (props) => {
    const [device, setDevice] = useState(null);
    const [status, setStatus] = useState(null);
    const [error, setError] = useState(null);
    const [loading, set_loading] = useState(true);

    useEffect(() => {
        set_loading(true);

        Promise.all([fetch('/api/status'), fetch('/api/general')])
            .then((responses) => {
                if (responses[0].ok && responses[1].ok) {
                    return Promise.all([
                        responses[0].json(),
                        responses[1].json(),
                    ]);
                }
                throw responses;
            })
            .then((data) => {
                setDevice(data[1]);
                const date = new Date(data[0].system_time * 1000);
                const result = Object.assign({ system_date: date }, data[0]);
                setStatus(result);
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
                setError(error);
            })
            .finally(() => {
                set_loading(false);
            });

        const update_status = setInterval(() => {
            fetch('/api/status')
                .then((response) => {
                    if (response.ok) return response.json();
                    throw response;
                })
                .then((data) => {
                    const date = new Date(data.system_time * 1000);
                    const result = Object.assign({ system_date: date }, data);
                    setStatus(result);
                })
                .catch((error) => {
                    console.error('Error fetching data: ', error);
                    setError(error);
                });
        }, 10000);

        return () => clearInterval(update_status);
    }, []);

    const on_update_time = () => {
        if (!window.confirm('WARNING!!!\nUpdate Time?')) {
            return;
        }
        set_loading(true);
        fetch('/api', {
            method: 'POST',
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
                date.toLocaleTimeString();
                const result = Object.assign({}, status, {
                    system_time: res.system_time,
                    system_date: date,
                });
                setStatus(result);
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
            })
            .finally(() => {
                set_loading(false);
            });
    };

    const get_hms = (seconds) => {
        const h = Math.floor(seconds / 3600);
        const m = Math.floor((seconds % 3600) / 60);
        const s = seconds - h * 3600 - m * 60;
        return `${h} h, ${m} min, ${s} sec`;
    };

    if (error) return 'Error...';

    return (
        <>
            <h1>
                <img alt='logo' src={logo} />
                Status
            </h1>
            <Content>
                <Loader loading={loading}>
                    {!loading && (
                        <div className='pure-g'>
                            <Line
                                icon={<FaInfo />}
                                title='Device Name'
                                value={device.CFG_DEVICE_NAME}
                            />
                            <Line
                                icon={<FaDownload />}
                                title='App Version'
                                value={status.app_version}
                            />
                            <Line
                                icon={<FaWifi />}
                                title='Network Connected'
                                value={
                                    <ConnectedProtocols
                                        list={status.connected}
                                    />
                                }
                            />
                            <Line
                                icon={<FaClock />}
                                title='Uptime'
                                value={get_hms(status.uptime_sec)}
                            />
                            <Line
                                icon={<FaClock />}
                                title='System Time'
                                value={
                                    <div className='pure-g'>
                                        <div className='pure-u-1-2 pure-u-md-1-2 local-time'>
                                            {status.system_date.toDateString() +
                                                ', ' +
                                                status.system_date.toLocaleTimeString()}
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
                                }
                            />
                        </div>
                    )}
                </Loader>
            </Content>

            {!loading && (
                <>
                    <h3>Sensors Online</h3>
                    <SensorsCards
                        online_sensors={status.online_sensors}
                        sensors_fields={props.sensor_fields}
                    />
                </>
            )}
        </>
    );
};

export default Status;
