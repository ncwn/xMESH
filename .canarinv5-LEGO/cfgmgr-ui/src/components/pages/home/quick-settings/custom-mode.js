import Line from '../../../sections/line';
import { FaCube } from 'react-icons/fa';
import { useMemo } from 'react';

const CustomMode = (props) => {
    console.log(props.netif_data);
    const mode = useMemo(() => {
        const _mode = [];
        if (props.netif_data.CFG_WIFI_STA_ENABLE === 'true') _mode.push('WiFi');
        if (props.netif_data.CFG_CELL_ENABLE === 'true') _mode.push('Cellular');
        if (props.netif_data.CFG_LWAN_ENABLE === 'true') _mode.push('LoRa');

        return _mode.join(', ');
    }, [props.netif_data]);

    return (
        <Line
            icon={<FaCube />}
            title='Custom'
            value={<input disabled value={mode} />}
        />
    );
};

export default CustomMode;
