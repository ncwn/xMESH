const Loader = (props) => {
    let content = <div className='loader'>Loading...</div>;
    if (!props.loading) {
        content = props.children;
    }
    return content || null;
};

export default Loader;
