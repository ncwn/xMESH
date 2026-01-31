import { useState } from 'react';
import { FaEye, FaEyeSlash } from 'react-icons/fa';

const PasswordInput = (props) => {
    const [show_password, set_show_password] = useState(false);

    return (
        <div>
            <input
                type={show_password ? 'text' : 'password'}
                onChange={props.on_change}
                value={props.value}
            />
            <button
                style={{
                    borderStyle: 'none',
                    background: '#777',
                    margin: '0 0 0 -2em',
                    padding: 2,
                    width: '2em',
                    cursor: 'pointer',
                }}
                onClick={() => set_show_password(!show_password)}
            >
                {show_password ? (
                    <FaEyeSlash color='#eee' />
                ) : (
                    <FaEye color='#eee' />
                )}
            </button>
        </div>
    );
};

export default PasswordInput;
