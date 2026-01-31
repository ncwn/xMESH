import {
    FaBatteryEmpty,
    FaBatteryFull,
    FaBatteryHalf,
    FaBatteryQuarter,
    FaBatteryThreeQuarters,
} from 'react-icons/fa';

export default function BatteryIcon(props) {
    let value = props.value;
    const battery_slots = [
        [0, 10],
        [11, 35],
        [36, 70],
        [71, 90],
        [91, 100],
    ];
    const battery_icons = [
        <FaBatteryEmpty />,
        <FaBatteryQuarter />,
        <FaBatteryHalf />,
        <FaBatteryThreeQuarters />,
        <FaBatteryFull />,
    ];
    for (let i = 0; i < battery_slots.length; i++) {
        if ((value >= battery_slots[i][0]) & (value <= battery_slots[i][1])) {
            return battery_icons[i];
        }
    }
    return battery_icons[2];
}
