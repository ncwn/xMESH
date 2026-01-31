import { FaCheck, FaTimes } from 'react-icons/fa';

const YesNo = (props) => {
    let value = props.value;
    if (value === true) {
        return <FaCheck color='lime' />;
    } else {
        return <FaTimes color='red' />;
    }
};

export default YesNo;
