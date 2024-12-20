//Приєднав модулі
const express = require('express');
const app = express();
// const fs = require('fs').promises;
const bodyParser = require('body-parser');
const { program } = require('commander');


//Налаштував потрібні аргументи
program
    .requiredOption('-h, --host <host>', 'address of the server')
    .requiredOption('-p, --port <port>', 'port of the server')
    .parse(process.argv);
//створив змінні для параметрів аргументів
const options = program.opts();

app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));


var dataArray = [];


app.post('/data', (req, res) => {
    const jsonData = req.body;
    console.log(jsonData);
    if (jsonData.crossPeriod) {
        console.log(`Received data - Time of crossing: ${jsonData.offTime}, crossing period: ${jsonData.crossPeriod} ms, Speed: ${jsonData.speed} km/h`);
        dataArray.push(jsonData);

        res.status(200).send('Data received');
    } else {
        res.status(400).send('Invalid data');
    }
});

app.get('/data', (req, res) => {
    const json = dataArray;
    res.status(200).json(json);
})

app.get('/time', (req, res) => {
    date = new Date();
    date.setHours(date.getHours() + 2);
    const timeString = date.toISOString();
    res.json({ timestamp: timeString });
})


// Задаємо адресу і порт якиі прослуховує сервер та виводимо в консоль 
app.listen(options.port, options.host, () => {
    console.log(`Server is running on http://${options.host}:${options.port}/`);
});


