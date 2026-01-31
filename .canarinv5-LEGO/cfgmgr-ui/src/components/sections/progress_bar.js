export default function ProgressBar(props) {
    const value = props.value;
    let max_width = '100%';

    if (props.max_width != null) {
        max_width = props.max_width;
    }

    const style_container = {
        maxWidth: max_width,
    };

    const style_bar = {
        width: value + '%',
    };
    return (
        <div className='progressbar' style={style_container}>
            <div style={style_bar}>
                <span>{value + '%'}</span>
            </div>
        </div>
    );
}
