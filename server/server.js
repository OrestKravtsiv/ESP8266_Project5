//Приєднав модулі
const http = require('http');
const fs = require('fs').promises;
const path = require('path');
// const superagent = require('superagent');
const { program } = require('commander');

//Налаштував потрібні аргументи
program
    .requiredOption('-h, --host <host>', 'address of the server')
    .requiredOption('-p, --port <port>', 'port of the server')
    .parse(process.argv);
//створив змінні для параметрів аргументів
const options = program.opts();

async function handleGetRequest(res, textPath) {
    try {//Витягає дані з кешу
        const fileText = await fs.readFile(textPath, 'utf8');// витягує з кешу
        const lines = fileText.split('\n').filter(line => line.trim() !== ''); // Розбиває файл на рядки, фільтрує порожні рядки
        const data = lines[lines.length - 1]; // Отримує останній непорожній рядок
        res.writeHead(200, { 'Content-Type': 'text/plain' });
        res.end(data);
    } catch {// Частина 3 якщо даних в кеші нема, то надсилає запит на сайт
        // якщо не знайшло на сайті, 404
        res.writeHead(404, { 'Content-Type': 'text/plain' });
        res.end('Not Found');
    }
}

async function handlePostRequest(req, res, textPath) {
    console.log("requested")
    let body = '';

    req.on('data', chunk => {
        body += chunk.toString();
        console.log(chunk.toString())
    });
    req.on('end', () => {
        fs.appendFile(textPath, body + '\n')
        console.log('Received data:', body);
        res.writeHead(200, { 'Content-Type': 'text/plain' });
        res.end(body);
    });
}

//Callback для запиту
const requestListener = async function (req, res) {
    // посилання на файл в директорії кеш
    const cacheFilePath = "./cache/text.txt"
    try {
        if (req.method == 'GET') {
            await handleGetRequest(res, cacheFilePath);
        } else if (req.method == 'POST') {
            await handlePostRequest(req, res, cacheFilePath);
        }
        //  else if (req.method === 'DELETE') {
        //     await handleDeleteRequest(res, cacheFilePath);
        // }
        else {
            console.log("Method Not Allowed")
            res.writeHead(405, { 'Content-Type': 'text/plain' });
            res.end('Method Not Allowed');
        }
    } catch (error) {
        console.error('Error handling request:', error);
        res.writeHead(500, { 'Content-Type': 'text/plain' });
        res.end('Internal Server Error');
    }
}

// Створнення сервера
const server = http.createServer(requestListener);

// Задаємо адресу і порт якиі прослуховує сервер та виводимо в консоль 
server.listen(options.port, options.host, () => {
    console.log(`Server running at http://${options.host}:${options.port}/`);
});

