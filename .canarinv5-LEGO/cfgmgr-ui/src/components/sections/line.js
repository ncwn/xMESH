export default function Line(props) {
    let value_class = 'pure-u-1 pure-u-md-2-3';
    if (props.status) {
        value_class = 'pure-u-1 pure-u-md-1-3';
    }

    let status_class = 'status-ok';

    if (props.status !== 'SUCCESS') {
        status_class = 'status-error';
    }

    let status = 'Saved.';

    switch (props.status) {
        case 'SUCCESS':
            status = 'Saved.';
            break;
        case 'PARAM_INVALID':
            status = 'Invalid input.';
            break;
        case 'SAVE_ERROR':
            status = 'Cannot save to device.';
            break;
        default:
    }

    let value = props.value;

    if (!value) {
        value = props.children;
    }

    return (
        <>
            <div className='pure-u-1 pure-u-md-1-3'>
                <div className='header'>
                    {props.icon} &nbsp;{props.title}
                </div>
            </div>
            <div className={value_class}>
                <div className='value'>{value}</div>
            </div>
            {props.status && (
                <div className='pure-u-1 pure-u-md-1-3'>
                    <div className={status_class}>{status}</div>
                </div>
            )}
            <div className='pure-u-1 pure-u-md-1 status-line-border'></div>
        </>
    );
}
