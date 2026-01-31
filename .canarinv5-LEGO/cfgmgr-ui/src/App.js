import { useEffect, useState } from 'react';
import 'purecss/build/base.css';
import 'purecss/build/grids-responsive.css';
import 'purecss/build/buttons.css';
import './App.css';
import { BrowserRouter as Router, Route, Routes } from 'react-router-dom';
import { IconContext } from 'react-icons';
import Navbar from './components/sections/navbar';
import Footer from './components/sections/footer';
import Loader from './components/sections/loader';
import Device from './components/pages/device';
import Networks from './components/pages/network';
import Sensors from './components/pages/sensors';
import Crontab from './components/pages/crontab';
import Home from './components/pages/home';

export default function App() {
    const [fields, set_fields] = useState({
        ports: {},
        lorawan: {},
        wifi: {},
    });
    const [loading, set_loading] = useState(true);

    //const [error, setError] = useState(null);

    useEffect(() => {
        set_loading(true);
        fetch('/api/fields')
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((values) => {
                set_fields(values);
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
                //setError(error);
            })
            .finally(() => {
                set_loading(false);
            });
    }, []);

    return (
        <Router>
            <Navbar />
            <Loader loading={loading}>
                <IconContext.Provider value={{ color: '#666' }}>
                    <div className='wrapper'>
                        <Routes>
                            <Route
                                path='/device'
                                element={<Device fields={fields['device']} />}
                            />
                            <Route
                                path='/networks'
                                element={
                                    <Networks fields={fields['networks']} />
                                }
                            />
                            <Route
                                path='/sensors'
                                element={<Sensors fields={fields['sensors']} />}
                            />
                            <Route path='/crontab' element={<Crontab />} />
                            {/*<Route*/}
                            {/*    path='/logging'*/}
                            {/*    element={<Logging fields={fields['logging']} />}*/}
                            {/*/>*/}
                            <Route
                                path='/'
                                element={
                                    <Home sensors_fields={fields['sensors']} />
                                }
                            />
                        </Routes>
                    </div>
                </IconContext.Provider>
            </Loader>
            <Footer />
        </Router>
    );
}
