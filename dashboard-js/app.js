// npm install express express-handlebars nodemon
// node app.js

const express = require('express');
const app = express();
const { engine } = require('express-handlebars');
app.use(express.static('public'));

app.engine('handlebars', engine());
app.set('view engine', 'handlebars');
app.set('views', './views');

// Dado falso.
let data = [{
    plate: 'ABC-123',
    gps: '123.456',
    speed: '123',
    acceleration: '123',
    risk: '123',
    name: 'John Doe',
    model: 'Tesla',
    year: '2021',
},];

app.get('/', (req, res) => {
    res.render('home', {layout: false, data: data});
});

app.get('/getData', (req, res) => {
   res.json(data);
});

app.post('/updateData', (req, res) => {
   data = req.body;
   res.json({ message: 'Data updated' });
});

app.listen(3000, () => {
   console.log('Server listening on port 3000');
});

// JS ainda precisa funcionar legal, estava pensando nesse Tabulator.
