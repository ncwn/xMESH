import { FaCube } from 'react-icons/fa';
import Line from '../sections/line';
import SelectInput from '../sections/select_input';
import YesNoIcon from './yes_no_icon';
import PasswordInput from './password_input';

// fields
// values
const DynamicForm = (props) => {
    const fields = Object.keys(props.fields).map((key) => {
        const entry = props.fields[key];
        const status_entry = key + '_STATUS';
        let input = '';
        let val = '';

        if (entry.depends) {
            if (
                !entry.depends.some(
                    (depends) => props.values[depends.field] === depends.value
                )
            ) {
                return (
                    <input
                        key={key}
                        type='hidden'
                        value={props.values[key] || ''}
                    />
                );
            }
        }

        switch (entry.type) {
            case 'text':
                val =
                    (props.values[key] === '__NONE__'
                        ? ''
                        : props.values[key]) || val;
                input = (
                    <input
                        type='text'
                        key={key}
                        onChange={(e) => props.on_change(key, e.target.value)}
                        value={val}
                    />
                );
                break;
            case 'readonlytext':
                val =
                    (props.values[key] === '__NONE__'
                        ? ''
                        : props.values[key]) || val;
                input = <input type='text' key={key} disabled value={val} />;
                break;
            case 'password':
                val =
                    (props.values[key] === '__NONE__'
                        ? ''
                        : props.values[key]) || val;
                input = (
                    <PasswordInput
                        key={key}
                        on_change={(e) => props.on_change(key, e.target.value)}
                        value={val}
                    ></PasswordInput>
                );
                break;
            case 'dropdown':
                input = (
                    <SelectInput
                        key={key}
                        options={entry.dropdown}
                        selected={props.values[key] || ''}
                        on_change={(e) => props.on_change(key, e.target.value)}
                    />
                );
                break;
            case 'boolean':
                val =
                    (props.values[key] === '__NONE__'
                        ? ''
                        : props.values[key]) || val;
                input = <YesNoIcon key={key} value={val} />;
                break;
            case 'textarea':
                val =
                    (props.values[key] === '__NONE__'
                        ? ''
                        : props.values[key]) || val;
                input = (
                    <textarea
                        key={key}
                        value={val}
                        onChange={(e) => props.on_change(key, e.target.value)}
                        className='field textarea'
                        rows='16'
                        cols='60'
                    />
                );
                break;

            default:
        }

        let status = null;
        if (status_entry in props.values) {
            status = props.values[status_entry];
        }
        return (
            <Line
                key={key}
                icon={<FaCube />}
                title={entry.title}
                value={input}
                status={status}
            />
        );
    });

    return (
        <div className='pure-g'>
            {fields}
            <div className='pure-u-1' style={{ textAlign: 'center' }}>
                {props.save_text && (
                    <button
                        type='button'
                        className='pure-button pure-button-primary'
                        onClick={props.on_save}
                    >
                        {props.save_text}
                    </button>
                )}
                {props.reset_text && (
                    <button
                        type='button'
                        className='pure-button button-warning'
                        onClick={props.on_reset}
                    >
                        {props.reset_text}
                    </button>
                )}
            </div>
        </div>
    );
};

export default DynamicForm;
