import { useCallback, useEffect, useState } from 'react';
import Line from '../../../sections/line';
import { FaCube } from 'react-icons/fa';
import Loader from '../../../sections/loader';
import Ping from '../../../sections/ping';

const CellularMode = (props) => {
    const [loading, set_loading] = useState(true);

    const load_data = useCallback(async () => {
        set_loading(true);
        const res = await fetch('/api', {
            method: 'POST',
            body: JSON.stringify({
                cmd: 'get',
                fields: ['CFG_CELL_APN'],
            }),
        });
        if (!res.ok) {
            throw res;
        }
        const data = await res.json();
        props.set_data(data);
    }, []);

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
            <Line
                icon={<FaCube />}
                title='APN'
                value={
                    <input
                        value={props.data.CFG_CELL_APN}
                        onChange={(e) => {
                            props.set_data({
                                ...props.data,
                                CFG_CELL_APN: e.target.value,
                            });
                        }}
                    />
                }
                status={props.status.CFG_CELL_APN_STATUS}
            />
            <Ping />
        </Loader>
    );
};

export default CellularMode;
