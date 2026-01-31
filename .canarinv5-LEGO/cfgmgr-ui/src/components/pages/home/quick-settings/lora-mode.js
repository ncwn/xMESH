import Loader from '../../../sections/loader';
import Line from '../../../sections/line';
import { FaCube } from 'react-icons/fa';
import { useCallback, useEffect, useState } from 'react';
import SelectInput from '../../../sections/select_input';

const LoraMode = (props) => {
    const [loading, set_loading] = useState(false);

    const load_data = useCallback(async () => {
        set_loading(true);
        const res = await fetch('/api', {
            method: 'POST',
            body: JSON.stringify({
                cmd: 'get',
                fields: ['CFG_LWAN_DATA_RATE'],
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

    console.log(props.data.CFG_LWAN_DATA_RATE);

    return (
        <Loader loading={loading}>
            <Line
                icon={<FaCube />}
                title='Data Rate'
                value={
                    <SelectInput
                        //prettier-ignore
                        options={{
              "2": "2",
              "3": "3",
              "4": "4",
              "5": "5"
            }}
                        selected={props.data.CFG_LWAN_DATA_RATE}
                        on_change={(e) => {
                            props.set_data({
                                ...props.data,
                                CFG_LWAN_DATA_RATE: e.target.value,
                            });
                        }}
                    />
                }
                status={props.status.CFG_LWAN_DATA_RATE_STATUS}
            />
        </Loader>
    );
};

export default LoraMode;
