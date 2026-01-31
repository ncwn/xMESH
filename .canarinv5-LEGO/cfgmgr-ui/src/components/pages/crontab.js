import { useEffect, useState } from 'react';
import logo from '../../interlab-logo-2.png';
import Content from '../sections/content';
import Loader from '../sections/loader';

const Crontab = (props) => {
    const [cron_job_text, set_cron_job_text] = useState('');
    const [error, setError] = useState(null);
    const [loading, set_loading] = useState(true);
    const [cron_job_list, set_cron_job_list] = useState([]);

    const fetch_job_list = () => {
        fetch('/api', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ cmd: 'get_cron_jobs' }),
        })
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((data) => {
                set_cron_job_list(data['cron_jobs']);
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
            });
    };

    const fetch_data = () => {
        set_loading(true);
        fetch('/api/crontab')
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((data) => {
                set_cron_job_text(data['crontab']);
            })
            .catch((error) => {
                console.error('Error fetching data: ', error);
                setError(error);
            })
            .finally(() => {
                set_loading(false);
            });
    };

    const on_change = (evt) => {
        set_cron_job_text(evt.target.value);
    };

    const on_save = () => {
        if (!window.confirm('Save crontab?')) {
            return;
        }
        set_loading(true);
        fetch('/api/crontab', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ crontab: cron_job_text }),
        })
            .then((response) => {
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then((value) => {
                set_cron_job_text(value['crontab']);
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
        if (!window.confirm('Reset crontab?')) {
            return;
        }
        fetch_data();
    };

    useEffect(() => {
        fetch_job_list();
        fetch_data();
    }, []);

    if (error) return 'Error...';

    const cjob_list = (
        <ul>
            {cron_job_list.map((elem, index) => {
                return <li key={index}>{elem}</li>;
            })}
        </ul>
    );

    return (
        <>
            <h1>
                <img alt='logo' src={logo} />
                Crontab
            </h1>
            <Content>
                <Loader loading={loading}>
                    <div className='crontab'>
                        <div className='help-section'>
                            <p>
                                Schedule jobs using cron expressions with
                                seconds resolution. The first field is seconds
                                followed by the standard cron expression.
                            </p>
                            <p>
                                For example, to schedule a job every 30 second
                                would be "30 * * * * * job".
                            </p>
                            {cjob_list}
                        </div>
                        <textarea onChange={on_change} value={cron_job_text} />
                    </div>
                    <div className='pure-u-1' style={{ textAlign: 'center' }}>
                        <button
                            type='button'
                            className='pure-button pure-button-primary'
                            onClick={on_save}
                        >
                            Save
                        </button>
                        <button
                            type='button'
                            className='pure-button button-warning'
                            onClick={on_reset}
                        >
                            Reset
                        </button>
                    </div>
                </Loader>
            </Content>
        </>
    );
};

export default Crontab;
