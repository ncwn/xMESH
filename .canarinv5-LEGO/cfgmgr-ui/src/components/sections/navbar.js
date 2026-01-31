import { Component } from 'react';
import { NavLink } from 'react-router-dom';

export default class Navbar extends Component {
    remove_links() {
        let element = document.getElementById('nav-check');
        element.checked = false;
    }

    render() {
        return (
            <div className='nav'>
                <input type='checkbox' id='nav-check' />
                <div className='nav-header'>
                    <div className='nav-title'>Canarin v5</div>
                </div>
                <div className='nav-btn'>
                    <label htmlFor='nav-check'>
                        <span></span>
                        <span></span>
                        <span></span>
                    </label>
                </div>
                <div className='nav-links'>
                    <NavLink
                        to='/'
                        className={({ isActive }) =>
                            isActive ? 'nav-active' : ''
                        }
                        onClick={this.remove_links}
                    >
                        Status
                    </NavLink>
                    <NavLink
                        to='/device'
                        className={({ isActive }) =>
                            isActive ? 'nav-active' : ''
                        }
                        onClick={this.remove_links}
                    >
                        Device
                    </NavLink>
                    <NavLink
                        to='/networks'
                        className={({ isActive }) =>
                            isActive ? 'nav-active' : ''
                        }
                        onClick={this.remove_links}
                    >
                        Network Interfaces
                    </NavLink>
                    <NavLink
                        to='/sensors'
                        className={({ isActive }) =>
                            isActive ? 'nav-active' : ''
                        }
                        onClick={this.remove_links}
                    >
                        Sensors
                    </NavLink>
                    <NavLink
                        to='/crontab'
                        className={({ isActive }) =>
                            isActive ? 'nav-active' : ''
                        }
                        onClick={this.remove_links}
                    >
                        Crontab
                    </NavLink>
                    {/*<NavLink*/}
                    {/*    to='/logging'*/}
                    {/*    className={({ isActive }) =>*/}
                    {/*        isActive ? 'nav-active' : ''*/}
                    {/*    }*/}
                    {/*    onClick={this.remove_links}*/}
                    {/*>*/}
                    {/*    Logging*/}
                    {/*</NavLink>*/}
                </div>
            </div>
        );
    }
}
