import {Tabulator} from 'https://cdnjs.cloudflare.com/ajax/libs/tabulator/5.4.4/js/tabulator_esm.min.js';
import axios from 'https://cdn.skypack.dev/axios';

const allCarsDataset = async () => {
    const response = await axios.get('/getData');
    return response.data;
}

const createTable = async () => new Tabulator("#main-table", {
    height:205, // set height of table (in CSS or here), this enables the Virtual DOM and improves render speed dramatically (can be any valid css height value)
    data: allCarsDataset, //assign data to table
    layout:"fitColumns", //fit columns to width of table (optional)
    columns:[ //Define Table Columns
        {title:"Plate", field:"plate", width:150},
        {title:"GPS", field:"gps", align:"left"},
        {title:"Speed", field:"speed", align:"left"},
        {title:"Acceleration", field:"acceleration", align:"left"},
        {title:"Risk", field:"risk", align:"left"},
        {title:"Name", field:"name", align:"left"},
        {title:"Model", field:"model", align:"left"},
        {title:"Year", field:"year", align:"left"},
    ],
});
