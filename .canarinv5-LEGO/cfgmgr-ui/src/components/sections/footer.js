import logo from '../../interlab-logo-1.png';

const Footer = () => (
    <div className='footer'>
        <div className='status-line-border'></div>
        <div className='items'>
            <a
                href='https://interlab.ait.ac.th/'
                target='_blank'
                rel='noreferrer'
            >
                <img alt='logo' src={logo} />
            </a>
        </div>
        <div className='items'>2021 &copy;</div>
    </div>
);

export default Footer;
