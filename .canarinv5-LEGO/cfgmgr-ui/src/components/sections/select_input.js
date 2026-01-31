import { useMemo } from 'react';

const SelectInput = (props) => {
    const options_component = useMemo(() => {
        const options = props.options == null ? { 0: 0 } : props.options;
        return Object.keys(options).map((key) => {
            const val = options[key];
            return (
                <option key={key} value={key}>
                    {val}
                </option>
            );
        });
    }, [props.options]);

    const selected = props.selected == null ? 0 : props.selected;

    return (
        <select onChange={props.on_change} value={selected}>
            {options_component}
        </select>
    );
};

export default SelectInput;
