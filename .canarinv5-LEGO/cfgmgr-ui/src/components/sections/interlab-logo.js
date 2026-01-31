import logo1 from '../../interlab-logo-1.png';
import logo2 from '../../interlab-logo-2.png';
import logo3 from '../../interlab-logo-3.png';

const InterlabLogo = (props) => {
    const { variation, ...other } = props;

    let logo;
    switch (variation) {
        case 1:
            logo = logo1;
            break;
        case 2:
            logo = logo2;
            break;
        case 3:
            logo = logo3;
            break;
        default:
            logo = logo1;
            break;
    }

    return <img alt='interlab-logo' src={logo} {...props} />;
};

export default InterlabLogo;
