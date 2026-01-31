import InterlabLogo from '../../sections/interlab-logo';
import { useState } from 'react';
import SensorsOnline from './sensors-online';
import QuickSettings from './quick-settings';
import Status from './status';

const Home = (props) => {
    const [loading, set_loading] = useState(false);
    const [device_data, set_device_data] = useState({});

    return (
        <>
            <h1>
                <InterlabLogo variation={2} />
                Status
            </h1>
            <Status />
            <h3>Sensors Online</h3>
            <SensorsOnline sensors_fields={props.sensors_fields} />
            <h3>Quick Settings</h3>
            <QuickSettings sensors_fields={props.sensors_fields} />
        </>
    );
};

export default Home;
