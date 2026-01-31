import Loader from './loader';
import Line from './line';
import { FaCube } from 'react-icons/fa';
import { useState } from 'react';

const Ping = (props) => {
    const [ping_data, set_ping_data] = useState(null);
    // prettier-ignore
    const [ping_params, set_ping_params] = useState("8.8.8.8");
    const [loading, set_loading] = useState(false);

    const ping_wifi = () => {
        set_loading(true);
        set_ping_data(null);
        fetch('/api', {
            method: 'POST',
            body: JSON.stringify({
                cmd: 'internet_ping',
                address: ping_params,
            }),
        })
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((val) => {
                set_ping_data(val['stdout'].split('\n'));
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
            })
            .finally(() => {
                set_loading(false);
            });
    };

    const on_change = (key, val) => {
        const new_ping_params = Object.assign({}, ping_params);
        new_ping_params[key] = val;
        set_ping_params(new_ping_params);
    };

    let content = (
        <Loader loading={loading}>
            {ping_data && (
                <div className='terminal'>
                    {ping_data &&
                        ping_data.map((line, idx) => <li key={idx}>{line}</li>)}
                </div>
            )}
        </Loader>
    );

    return (
        <>
            <Line
                icon={<FaCube />}
                title='Ping'
                value={
                    <>
                        <input
                            value={ping_params}
                            onChange={(e) => {
                                set_ping_params(e.target.value);
                            }}
                        />
                        <button
                            className='pure-button pure-button-primary'
                            onClick={ping_wifi}
                        >
                            Ping
                        </button>
                    </>
                }
                fields={
                    //prettier-ignore
                    { "0": { type: "text", title: "Ping" } }
                }
                values={ping_params}
                on_change={on_change}
                save_text='Ping'
                on_save={ping_wifi}
            />
            <br></br>
            {content}
        </>
    );
};

export default Ping;
