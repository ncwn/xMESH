import { useEffect, useState } from 'react';
import DynamicForm from '../sections/dynamic_form';
import logo from '../../interlab-logo-2.png';
import Content from '../sections/content';
import Loader from '../sections/loader';

const Logging = (props) => {
    const [logging_params, set_logging_params] = useState({});
    const [error, setError] = useState(null);
    const [loading, set_loading] = useState(true);

    const fetch_data = () => {
        set_loading(true);
        fetch('/api/logging')
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((data) => {
                set_logging_params(data);
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
        const new_logging_params = Object.assign({}, logging_params);
        new_logging_params[key] = val;
        set_logging_params(new_logging_params);
    };

    const on_save = () => {
        if (!window.confirm('Save parameters?')) {
            return;
        }
        set_loading(true);
        fetch('/api/logging', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(logging_params),
        })
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((value) => {
                set_logging_params(value);
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
                Logging
            </h1>
            <Content>
                <Loader loading={loading}>
                    <DynamicForm
                        fields={props.fields}
                        values={logging_params}
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

export default Logging;
